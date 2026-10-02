#include "services/VideoAssetImporter.h"
#include "services/ProjectStore.h"
#include "infrastructure/JsonProcess.h"
#include "infrastructure/RuntimePaths.h"
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <cmath>
#include <memory>

namespace qvw::services {
namespace {
constexpr qint64 hardByteLimit=64*1024*1024;
constexpr qint64 chunkBytes=256*1024;
double number(const QString &text){bool ok=false;const auto n=text.toDouble(&ok);return ok&&std::isfinite(n)?n:-1;}
double rate(const QString &text){const auto parts=text.split('/');if(parts.size()!=2)return -1;const auto a=number(parts[0]),b=number(parts[1]);return a>0&&b>0?a/b:-1;}
bool metadata(const QJsonObject &object,domain::Asset *asset,QString *error){
    const auto format=object["format"].toObject();const auto brand=format["tags"].toObject()["major_brand"].toString().trimmed();
    const QStringList mp4Brands={"isom","iso2","iso3","iso4","iso5","iso6","avc1","mp41","mp42","M4V","MSNV","dash"};
    if(!object["streams"].isArray()||!format["format_name"].toString().split(',').contains("mp4")||!mp4Brands.contains(brand)){
        *error=QStringLiteral("仅支持 H.264 编码的 MP4 视频（不接受 MOV）。");return false;
    }
    if(number(format["duration"].toString())<8){*error=QStringLiteral("视频时长至少需要 8 秒。");return false;}
    int count=0;
    for(const auto &entry:object["streams"].toArray()){
        const auto stream=entry.toObject();if(stream["codec_type"]!="video")continue;++count;
        const auto width=stream["width"].toDouble(-1),height=stream["height"].toDouble(-1);
        if(stream["codec_name"]!="h264"||std::abs(rate(stream["avg_frame_rate"].toString())-30)>1e-9||std::abs(rate(stream["r_frame_rate"].toString())-30)>1e-9){*error=QStringLiteral("视频必须为 H.264、恒定 30 fps。");return false;}
        if(!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0||std::floor(width)!=width||std::floor(height)!=height||width*height>40'000'000){*error=QStringLiteral("视频尺寸无效或超过 4000 万像素上限。");return false;}
        const auto frames=number(stream["nb_read_frames"].toString());
        if(number(stream["duration"].toString())<8||frames<240||std::floor(frames)!=frames){*error=QStringLiteral("视频流至少需要 8 秒和 240 个可读取帧。");return false;}
        asset->width=static_cast<int>(width);asset->height=static_cast<int>(height);
    }
    if(count!=1){*error=QStringLiteral("视频必须含且仅含一个视频流。");return false;}return true;
}
bool frameTimestamps(const QJsonObject &object,QString *error){
    const auto frames=object["frames"].toArray();
    if(!object["frames"].isArray()||frames.size()!=240){*error=QStringLiteral("视频前 8 秒必须提供恰好 240 个帧时间戳。");return false;}
    const auto first=number(frames.first().toObject()["best_effort_timestamp_time"].toString());
    // ffprobe prints six decimal places; two microseconds covers rounding of both timestamps.
    constexpr double tolerance=0.000002;
    // The template begins sampling at sourceFrame0, so the accepted video must begin at time zero.
    if(first<0||std::abs(first)>tolerance){*error=QStringLiteral("视频帧时间戳必须从 0 秒开始。");return false;}
    double previous=first;
    for(int i=0;i<frames.size();++i){
        const auto timestamp=number(frames[i].toObject()["best_effort_timestamp_time"].toString());
        if(timestamp<0||std::abs(timestamp-(first+i/30.0))>tolerance||(i>0&&std::abs((timestamp-previous)-1.0/30.0)>tolerance)){
            *error=QStringLiteral("视频前 240 帧必须按恒定 30 fps 排列；检测到缺失或不规则的帧时间戳。");return false;
        }
        previous=timestamp;
    }
    return true;
}
}
class VideoAssetImporter::State {
public:
    VideoAssetImporter *owner;infra::JsonProcess probe;QPointer<QProcess> decode;QTimer deadline;
    QProcessEnvironment environment=infra::RuntimePaths::processEnvironment();domain::Project candidate;domain::Asset asset;
    QString ffprobe,ffmpeg,source,destination;std::unique_ptr<QTemporaryDir> stage;QFile input,output,comparison;
    QCryptographicHash hash{QCryptographicHash::Sha256};QByteArray stdoutBytes,stderrBytes;
    qint64 limit=0,copied=0;bool active=false,stopping=false,checkingFrames=false;quint64 generation=0;
    explicit State(VideoAssetImporter *o):owner(o),probe(o){deadline.setSingleShot(true);}
    QString stagedPath() const{return stage?stage->filePath("source.mp4"):QString();}
    void dispose(QProcess *p){deadline.stop();decode=nullptr;p->disconnect(owner);if(p->state()!=QProcess::NotRunning){p->kill();p->waitForFinished(500);}p->deleteLater();}
    void clean(){input.close();output.close();comparison.close();stage.reset();}
    void stop(){stopping=true;probe.cancel();if(decode){auto *p=decode.data();const auto program=p->program();const auto args=p->arguments();dispose(p);emit owner->commandFinished(program,args,-1);}deadline.stop();clean();stopping=false;}
    void fail(const QString &text){if(!active)return;const auto gen=generation;active=false;stop();if(gen!=generation)return;emit owner->stateChanged(false);if(gen==generation)emit owner->failed(text);}
    void enqueue(void(State::*method)()){const auto gen=generation;QTimer::singleShot(0,owner,[this,gen,method]{if(active&&gen==generation)(this->*method)();});}
    void success(const domain::Asset &accepted){const auto result=candidate;const auto acceptedCopy=accepted;const auto gen=generation;active=false;clean();emit owner->imported(result,acceptedCopy);if(gen==generation)emit owner->stateChanged(false);}
    void copyChunk(){
        const auto bytes=input.read(chunkBytes);if(input.error()!=QFileDevice::NoError){fail(QStringLiteral("读取视频失败：%1").arg(input.errorString()));return;}
        if(!bytes.isEmpty()){
            copied+=bytes.size();if(copied>limit){fail(QStringLiteral("视频超过素材大小上限。"));return;}
            if(output.write(bytes)!=bytes.size()){fail(QStringLiteral("写入视频暂存副本失败：%1").arg(output.errorString()));return;}hash.addData(bytes);enqueue(&State::copyChunk);return;
        }
        if(copied<=0){fail(QStringLiteral("视频不能为空。"));return;}
        if(!output.flush()){fail(QStringLiteral("无法完成视频暂存副本。"));return;}
        input.close();output.close();if(!QFile::setPermissions(stagedPath(),QFile::ReadOwner)){fail(QStringLiteral("无法保护视频暂存副本。"));return;}
        asset.hash=QString::fromLatin1(hash.result().toHex());asset.path=QStringLiteral("assets/%1.mp4").arg(asset.hash);asset.size=copied;
        probe.start(ffprobe,{"-v","error","-count_frames","-show_streams","-show_format","-of","json",stagedPath()},stage->path(),30000);
    }
    bool consume(QProcess *p){p->setReadChannel(QProcess::StandardOutput);stdoutBytes+=p->read(64*1024-stdoutBytes.size()+1);p->setReadChannel(QProcess::StandardError);stderrBytes+=p->read(64*1024-stderrBytes.size()+1);if(stdoutBytes.size()>64*1024||stderrBytes.size()>64*1024){fail(QStringLiteral("视频验证输出超过 64 KiB 上限。"));return false;}return true;}
    void startFrameProbe(){
        if(!active)return;checkingFrames=true;auto *p=new QProcess(owner);decode=p;stdoutBytes.clear();stderrBytes.clear();p->setProcessEnvironment(environment);p->setProgram(ffprobe);
        // -show_entries must follow -show_frames on pinned ffprobe9, otherwise its filter is overwritten.
        p->setArguments({"-v","error","-read_intervals","%+#240","-select_streams","v:0","-show_frames","-show_entries","frame=best_effort_timestamp_time","-of","json",stagedPath()});p->setWorkingDirectory(stage->path());
        QObject::connect(p,&QProcess::readyReadStandardOutput,owner,[this,p]{if(decode==p)consume(p);});QObject::connect(p,&QProcess::readyReadStandardError,owner,[this,p]{if(decode==p)consume(p);});
        QObject::connect(p,&QProcess::errorOccurred,owner,[this,p](QProcess::ProcessError error){if(decode==p&&error==QProcess::FailedToStart)fail(QStringLiteral("ffprobe 帧检查无法启动：%1").arg(p->errorString()));});
        QObject::connect(p,&QProcess::finished,owner,[this,p](int code,QProcess::ExitStatus status){
            if(decode!=p||!active||!consume(p))return;const auto program=p->program();const auto args=p->arguments();const auto gen=generation;
            QJsonParseError parseError;const auto document=QJsonDocument::fromJson(stdoutBytes,&parseError);const auto diagnostics=QString::fromUtf8(stderrBytes).trimmed();
            dispose(p);emit owner->commandFinished(program,args,status==QProcess::NormalExit?code:-1);if(gen!=generation||!active)return;
            if(code!=0||status!=QProcess::NormalExit||!diagnostics.isEmpty()||parseError.error!=QJsonParseError::NoError||!document.isObject()){fail(QStringLiteral("ffprobe 帧时间戳检查失败：%1").arg(diagnostics));return;}
            QString error;if(!frameTimestamps(document.object(),&error)){fail(error);return;}startDecode();
        });deadline.start(30000);p->start();
    }
    void startDecode(){
        if(!active)return;checkingFrames=false;auto *p=new QProcess(owner);decode=p;stdoutBytes.clear();stderrBytes.clear();p->setProcessEnvironment(environment);p->setProgram(ffmpeg);
        p->setArguments({"-v","error","-xerror","-nostdin","-i",stagedPath(),"-map","0:v:0","-f","null","-"});p->setWorkingDirectory(stage->path());
        QObject::connect(p,&QProcess::readyReadStandardOutput,owner,[this,p]{if(decode==p)consume(p);});QObject::connect(p,&QProcess::readyReadStandardError,owner,[this,p]{if(decode==p)consume(p);});
        QObject::connect(p,&QProcess::errorOccurred,owner,[this,p](QProcess::ProcessError error){if(decode==p&&error==QProcess::FailedToStart)fail(QStringLiteral("ffmpeg 无法启动：%1").arg(p->errorString()));});
        QObject::connect(p,&QProcess::finished,owner,[this,p](int code,QProcess::ExitStatus status){
            if(decode!=p||!active||!consume(p))return;const auto error=QString::fromUtf8(stderrBytes).trimmed();const auto program=p->program();const auto args=p->arguments();const auto gen=generation;
            dispose(p);emit owner->commandFinished(program,args,status==QProcess::NormalExit?code:-1);if(gen!=generation||!active)return;
            if(code!=0||status!=QProcess::NormalExit||!error.isEmpty()){fail(QStringLiteral("ffmpeg 全片解码失败：%1").arg(error));return;}beginCommit();
        });deadline.start(120000);p->start();
    }
    void beginCommit(){
        QString error;if(!ProjectStore::resolvePath(candidate,asset.path,&destination,&error,false)){fail(error);return;}
        for(const auto &existing:candidate.assets){if(existing.hash!=asset.hash)continue;
            if(existing.mime!=asset.mime||existing.width!=asset.width||existing.height!=asset.height||existing.size!=asset.size||existing.path!=asset.path){fail(QStringLiteral("已登记素材元数据与实际视频不符。"));return;}
            if(!ProjectStore::resolvePath(candidate,existing.path,&destination,&error)){fail(error);return;}asset=existing;beginComparison();return;
        }
        if(!QDir().mkpath(QFileInfo(destination).absolutePath())){fail(QStringLiteral("无法创建视频素材目录。"));return;}
        if(!ProjectStore::resolvePath(candidate,asset.path,&destination,&error,false)){fail(error);return;}
        if(QFileInfo::exists(destination)||QFileInfo(destination).isSymLink()){beginComparison();return;}
        commit(false);
    }
    void beginComparison(){
        const QFileInfo info(destination);if(!info.isFile()||info.isSymLink()||info.size()!=asset.size){fail(QStringLiteral("目标视频已存在且内容不符，已保留原文件。"));return;}
        comparison.setFileName(destination);input.setFileName(stagedPath());if(!comparison.open(QIODevice::ReadOnly)||!input.open(QIODevice::ReadOnly)){fail(QStringLiteral("无法校验已存在的视频副本。"));return;}enqueue(&State::compareChunk);
    }
    void compareChunk(){
        const auto expected=input.read(chunkBytes),existing=comparison.read(chunkBytes);
        if(input.error()!=QFileDevice::NoError||comparison.error()!=QFileDevice::NoError||expected!=existing){fail(QStringLiteral("已存在视频内容与哈希不符，已保留原文件。"));return;}
        if(!expected.isEmpty()){enqueue(&State::compareChunk);return;}input.close();comparison.close();commit(true);
    }
    void commit(bool exists){
        // No queued callbacks occur between ownership transfer and manifest commit.
        QString error,checked;if(!ProjectStore::resolvePath(candidate,asset.path,&checked,&error,false)||checked!=destination||QFileInfo(destination).isSymLink()){fail(error.isEmpty()?QStringLiteral("视频目标路径已改变。"):error);return;}
        for(const auto &existing:candidate.assets){if(existing.hash==asset.hash){success(existing);return;}}
        bool created=false;
        if(!exists){if(!QFile::rename(stagedPath(),destination)){fail(QStringLiteral("无法创建视频素材副本；目标已存在或目录不可写。"));return;}created=true;}
        candidate.assets.push_back(asset);
        if(!ProjectStore::save(candidate,&error)){if(created)QFile::remove(destination);fail(error);return;}success(asset);
    }
};
VideoAssetImporter::VideoAssetImporter(QObject *parent):QObject(parent),m(new State(this)){
    qRegisterMetaType<domain::Project>();qRegisterMetaType<domain::Asset>();
    connect(&m->probe,&infra::JsonProcess::commandFinished,this,&VideoAssetImporter::commandFinished);
    connect(&m->probe,&infra::JsonProcess::failed,this,[this](const QString &text){m->fail(text);});
    connect(&m->probe,&infra::JsonProcess::result,this,[this](const QJsonObject &object,int code){if(!m->active)return;QString error;if(code!=0||!metadata(object,&m->asset,&error)){m->fail(code!=0?QStringLiteral("ffprobe 返回非零退出码。"):error);return;}m->startFrameProbe();});
    connect(&m->deadline,&QTimer::timeout,this,[this]{m->fail(m->checkingFrames?QStringLiteral("视频帧时间戳检查超过 30 秒。"):QStringLiteral("视频全片解码超过 120 秒。"));});
}
VideoAssetImporter::~VideoAssetImporter(){cancel();delete m;}
bool VideoAssetImporter::start(const domain::Project &project,const QString &source,const QString &ffprobe,const QString &ffmpeg){
    if(m->active||m->stopping)return false;++m->generation;m->active=true;m->candidate=project;m->source=source;m->ffprobe=ffprobe;m->ffmpeg=ffmpeg;m->copied=0;m->hash.reset();m->asset={};m->asset.originalName=QFileInfo(source).fileName();m->asset.mime="video/mp4";
    const auto gen=m->generation;emit stateChanged(true);if(gen!=m->generation||!m->active)return false;
    if(project.maxAssetBytes<=0){m->fail(QStringLiteral("素材大小上限无效。"));return false;}m->limit=qMin(project.maxAssetBytes,hardByteLimit);
    const QFileInfo info(source);if(!info.isFile()||info.isSymLink()||info.size()<=0||info.size()>m->limit){m->fail(QStringLiteral("视频必须是可读取的普通文件，非空且不超过 %1 字节。").arg(m->limit));return false;}
    for(const auto &tool:{qMakePair(ffprobe,QStringLiteral("ffprobe")),qMakePair(ffmpeg,QStringLiteral("ffmpeg"))})if(!QFileInfo(tool.first).isFile()||!QFileInfo(tool.first).isExecutable()){m->fail(QStringLiteral("缺少可执行的 %1，请安装媒体工具后重试。").arg(tool.second));return false;}
    QString error;if(!ProjectStore::resolvePath(project,QStringLiteral("assets/staging-check.mp4"),nullptr,&error,false)){m->fail(error);return false;}
    m->stage=std::make_unique<QTemporaryDir>(QDir(QFileInfo(project.rootPath).canonicalFilePath()).filePath(".qvw-video-XXXXXX"));if(!m->stage->isValid()){m->fail(QStringLiteral("无法创建工程内视频暂存目录。"));return false;}
    m->input.setFileName(source);m->output.setFileName(m->stagedPath());
    if(!m->input.open(QIODevice::ReadOnly)||!m->output.open(QIODevice::WriteOnly|QIODevice::NewOnly)){m->fail(QStringLiteral("无法读取视频或创建暂存副本。"));return false;}
    m->enqueue(&State::copyChunk);return true;
}
void VideoAssetImporter::setEnvironment(const QProcessEnvironment &environment){if(!m->active){m->environment=environment;m->probe.setEnvironment(environment);}}
void VideoAssetImporter::cancel(){++m->generation;const auto wasActive=m->active;m->active=false;m->stop();if(wasActive)emit stateChanged(false);}
bool VideoAssetImporter::active() const{return m->active;}
}
