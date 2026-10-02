#include "MediaValidation.h"
#include "infrastructure/JsonProcess.h"
#include "infrastructure/RuntimePaths.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <cmath>

namespace qvw::services {
namespace {
double rate(const QString &text) {
    const auto terms=text.split('/');bool ok=false;const double a=terms.value(0).toDouble(&ok);if(!ok||!std::isfinite(a))return -1;
    if(terms.size()==1)return a;if(terms.size()!=2)return -1;const double b=terms[1].toDouble(&ok);return ok&&std::isfinite(b)&&b>0?a/b:-1;
}
bool metadata(const QJsonObject &object,const domain::CanvasSpace &expected,QString *error) {
    const auto format=object["format"].toObject();const auto duration=rate(format["duration"].toString());
    if(!object["streams"].isArray()||!object["format"].isObject()||!format["format_name"].toString().split(',').contains("mp4")) {*error=QStringLiteral("成片容器不是 MP4 或 ffprobe 数据缺失。");return false;}
    if(!std::isfinite(duration)||duration<=0||std::abs(duration-expected.durationSec)>1.0/expected.frameRate+0.1){*error=QStringLiteral("成片时长与冻结快照不符。");return false;}
    int videoCount=0;
    for(const auto &item:object["streams"].toArray()) {
        if(!item.isObject())continue;const auto stream=item.toObject();if(stream["codec_type"]!="video")continue;++videoCount;
        const auto fps=rate(stream["avg_frame_rate"].toString());
        if(stream["codec_name"]!="h264"||stream["width"].toInt()!=expected.width||stream["height"].toInt()!=expected.height||!std::isfinite(fps)||fps<=0||std::abs(fps-expected.frameRate)>0.01){*error=QStringLiteral("成片编码、画幅或帧率与冻结快照不符（需要 H.264）。");return false;}
    }
    if(videoCount!=1){*error=QStringLiteral("成片必须含且仅含一个视频流。");return false;}return true;
}
}
class MediaValidation::State {
public:
    MediaValidation *owner;infra::JsonProcess probe;QPointer<QProcess> decode;QTimer deadline;
    QString path,ffmpeg;domain::CanvasSpace expected;QByteArray output,diagnostics;bool active=false;quint64 generation=0;
    QProcessEnvironment environment=infra::RuntimePaths::processEnvironment();
    explicit State(MediaValidation *o):owner(o),probe(o){deadline.setSingleShot(true);}
    void dispose(QProcess *p){deadline.stop();decode=nullptr;p->disconnect(owner);if(p->state()!=QProcess::NotRunning){p->kill();p->waitForFinished(500);}p->deleteLater();}
    void fail(const QString &text){if(!active)return;active=false;probe.cancel();if(decode){auto *p=decode.data();const auto program=p->program();const auto args=p->arguments();dispose(p);const auto gen=generation;emit owner->commandFinished(program,args,-1);if(gen!=generation)return;}emit owner->failed(text);}
    bool consume(QProcess *p){p->setReadChannel(QProcess::StandardOutput);output+=p->read(64*1024-output.size()+1);p->setReadChannel(QProcess::StandardError);diagnostics+=p->read(64*1024-diagnostics.size()+1);if(output.size()>64*1024||diagnostics.size()>64*1024){fail(QStringLiteral("全片解码输出超过 64 KiB 上限。"));return false;}return true;}
    void startDecode() {
        if(!active)return;auto *p=new QProcess(owner);decode=p;output.clear();diagnostics.clear();p->setProcessEnvironment(environment);p->setProgram(ffmpeg);p->setArguments({"-v","error","-xerror","-i",path,"-f","null","-"});p->setWorkingDirectory(QFileInfo(path).absolutePath());
        QObject::connect(p,&QProcess::readyReadStandardOutput,owner,[this,p]{if(decode==p)consume(p);});
        QObject::connect(p,&QProcess::readyReadStandardError,owner,[this,p]{if(decode==p)consume(p);});
        QObject::connect(p,&QProcess::errorOccurred,owner,[this,p](QProcess::ProcessError error){if(decode==p&&error==QProcess::FailedToStart)fail(QStringLiteral("ffmpeg 无法启动：%1").arg(p->errorString()));});
        QObject::connect(p,&QProcess::finished,owner,[this,p](int code,QProcess::ExitStatus status){
            if(decode!=p||!active||!consume(p))return;
            const auto program=p->program();const auto args=p->arguments();const auto error=QString::fromUtf8(diagnostics).trimmed();const auto gen=generation;dispose(p);emit owner->commandFinished(program,args,status==QProcess::CrashExit?-1:code);if(gen!=generation||!active)return;
            if(status!=QProcess::NormalExit||code!=0||!error.isEmpty()){fail(QStringLiteral("ffmpeg 全片解码失败：%1").arg(error));return;}
            active=false;emit owner->validated();
        });
        deadline.start(120000);p->start();
    }
};
MediaValidation::MediaValidation(QObject *parent):QObject(parent),m(new State(this)) {
    connect(&m->probe,&infra::JsonProcess::commandFinished,this,&MediaValidation::commandFinished);
    connect(&m->probe,&infra::JsonProcess::failed,this,[this](const QString &error){m->fail(error);});
    connect(&m->probe,&infra::JsonProcess::result,this,[this](const QJsonObject &object,int code){if(!m->active)return;QString error;if(code!=0||!metadata(object,m->expected,&error)){m->fail(code!=0?QStringLiteral("ffprobe 返回非零退出码。") : error);return;}m->startDecode();});
    connect(&m->deadline,&QTimer::timeout,this,[this]{m->fail(QStringLiteral("ffmpeg 全片解码超过 120 秒。"));});
}
MediaValidation::~MediaValidation(){cancel();delete m;}
bool MediaValidation::start(const QString &path,const domain::CanvasSpace &expected,const QString &ffprobe,const QString &ffmpeg) {
    if(m->active)return false;++m->generation;m->active=true;
    if(!QFileInfo(path).isFile()||QFileInfo(path).isSymLink()||QFileInfo(path).size()<=0){m->fail(QStringLiteral("待验证成片不存在或文件无效。"));return false;}
    for(const auto &tool:{qMakePair(ffprobe,QStringLiteral("ffprobe")),qMakePair(ffmpeg,QStringLiteral("ffmpeg"))})if(!QFileInfo(tool.first).isFile()||!QFileInfo(tool.first).isExecutable()){m->fail(QStringLiteral("缺少可执行的 %1，请安装媒体工具后重试。").arg(tool.second));return false;}
    if(expected.width<=0||expected.height<=0||!std::isfinite(expected.frameRate)||expected.frameRate<=0||!std::isfinite(expected.durationSec)||expected.durationSec<=0){m->fail(QStringLiteral("冻结快照的画幅、帧率或时长无效。"));return false;}
    m->path=path;m->ffmpeg=ffmpeg;m->expected=expected;
    return m->probe.start(ffprobe,{"-v","error","-show_streams","-show_format","-of","json",path},QFileInfo(path).absolutePath(),30000);
}
void MediaValidation::cancel(){++m->generation;m->active=false;m->probe.cancel();if(m->decode){auto *p=m->decode.data();const auto program=p->program();const auto args=p->arguments();m->dispose(p);emit commandFinished(program,args,-1);}m->deadline.stop();}
void MediaValidation::setEnvironment(const QProcessEnvironment &environment){if(!m->active){m->environment=environment;m->probe.setEnvironment(environment);}}
}
