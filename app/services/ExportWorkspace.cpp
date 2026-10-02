#include "ExportWorkspace.h"
#include "services/ProjectStore.h"
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <cmath>

namespace qvw::services {
namespace {
constexpr qint64 maxFileBytes=64*1024*1024, maxCopyBytes=512*1024*1024;
bool fail(QString *error,const QString &text){if(error)*error=text;return false;}
bool noLinks(const domain::Project &project,const QString &relative,QString *absolute,QString *error,bool mustExist=true) {
    QString path;if(!ProjectStore::resolvePath(project,relative,&path,error,mustExist))return false;
    auto current=QFileInfo(project.rootPath).canonicalFilePath();
    for(const auto &part:relative.split('/')){current=QDir(current).filePath(part);if(QFileInfo(current).isSymLink())return fail(error,QStringLiteral("工程路径不能包含符号链接：%1").arg(relative));}
    if(absolute)*absolute=path;return true;
}
bool bytes(const QString &path,QByteArray *out,QString *error,qint64 limit=maxFileBytes) {
    QFile file(path);if(QFileInfo(path).isSymLink()||!file.open(QIODevice::ReadOnly)||file.size()>limit)
        return fail(error,QStringLiteral("文件不可读、是符号链接或超过大小上限：%1").arg(path));
    *out=file.read(limit+1);if(file.error()!=QFileDevice::NoError||out->size()>limit)return fail(error,QStringLiteral("读取文件失败或超过大小上限：%1").arg(path));return true;
}
bool object(const QString &path,QJsonObject *out,QString *error) {
    QByteArray data;if(!bytes(path,&data,error,4*1024*1024))return false;QJsonParseError parse;const auto doc=QJsonDocument::fromJson(data,&parse);
    if(parse.error!=QJsonParseError::NoError||!doc.isObject())return fail(error,QStringLiteral("文件必须是 JSON 对象：%1").arg(path));*out=doc.object();return true;
}
bool atomic(const QString &path,const QByteArray &data,QString *error) {
    if(QFileInfo(path).isSymLink())return fail(error,QStringLiteral("保存路径不能是符号链接。"));
    QSaveFile file(path);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly)||file.write(data)!=data.size()||!file.commit())return fail(error,QStringLiteral("原子保存失败：%1").arg(file.errorString()));return true;
}
bool metadata(const domain::Project &project,QString *error) {
    QString path;if(!ProjectStore::resolvePath(project,".workbench/builds",&path,error,false))return false;
    for(const auto &name:{QStringLiteral(".workbench"),QStringLiteral(".workbench/builds")}) {
        if(!ProjectStore::resolvePath(project,name,&path,error,false)||QFileInfo(path).isSymLink())return fail(error,QStringLiteral("导出元数据目录不安全。"));
        if(!QDir().mkpath(path)||!QFileInfo(path).isDir())return fail(error,QStringLiteral("无法创建导出元数据目录。"));
    }
    return true;
}
bool safeWorkspace(const domain::Project &project,const QString &workspace,QString *error) {
    const auto root=QFileInfo(project.rootPath).canonicalFilePath();const auto prefix=root+"/.workbench/builds/";
    static const QRegularExpression uuid(QStringLiteral("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"));
    if(root.isEmpty()||!QDir::isAbsolutePath(workspace)||!workspace.startsWith(prefix)||!uuid.match(workspace.mid(prefix.size())).hasMatch())return fail(error,QStringLiteral("恢复工作区不属于当前工程。"));
    QString checked;if(!ProjectStore::resolvePath(project,QDir(root).relativeFilePath(workspace),&checked,error,false))return false;
    auto cursor=workspace;while(cursor!=root){if(QFileInfo(cursor).isSymLink())return fail(error,QStringLiteral("恢复工作区不能包含符号链接。"));cursor=QFileInfo(cursor).absolutePath();}
    if(!QFileInfo(workspace).isDir()||QFileInfo(workspace).canonicalFilePath()!=workspace)return fail(error,QStringLiteral("恢复工作区不存在或路径不安全。"));return true;
}
bool sourcesMatch(const domain::Project &project,const domain::Snapshot &snapshot,QString *error) {
    if(!snapshot.isLoaded()||snapshot.sourceFiles.isEmpty()||snapshot.sourceFingerprint.size()!=32)return fail(error,QStringLiteral("需要已加载且含源码指纹的会话。"));
    QJsonObject source;
    for(auto it=snapshot.sourceFiles.cbegin();it!=snapshot.sourceFiles.cend();++it) {
        QString path;QByteArray data;if(!noLinks(project,it.key(),&path,error)||!bytes(path,&data,error,2*1024*1024))return false;
        if(data!=it.value().toUtf8())return fail(error,QStringLiteral("源码已被修改，请先刷新会话：%1").arg(it.key()));source.insert(it.key(),it.value());
    }
    if(QCryptographicHash::hash(QJsonDocument(source).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256)!=snapshot.sourceFingerprint)return fail(error,QStringLiteral("会话源码指纹不一致。"));return true;
}
bool collect(const QString &root,const QString &relative,QStringList *files,QString *error,qint64 *total) {
    const QDir dir(relative.isEmpty()?root:QDir(root).filePath(relative));
    for(const auto &info:dir.entryInfoList(QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System,QDir::Name)) {
        const auto name=info.fileName();const auto path=relative.isEmpty()?name:relative+'/'+name;
        if(info.isSymLink())return fail(error,QStringLiteral("工程不能含符号链接：%1").arg(path));
        if(name.startsWith('.')||name=="build"||name.startsWith("cmake-build-"))continue;
        if(info.isDir()){if(!collect(root,path,files,error,total))return false;}
        else if(!info.isFile())return fail(error,QStringLiteral("工程不能含特殊文件：%1").arg(path));
        else {
            *total+=info.size();if(info.size()>maxFileBytes||*total>maxCopyBytes||files->size()>=10000)return fail(error,QStringLiteral("工程超过导出冻结复制限制（单文件 64 MiB / 总计 512 MiB / 10000 文件）。"));
            files->append(path);
        }
    }
    return true;
}
bool removableTree(const QString &root,QString *error,int depth=0,int *count=nullptr) {
    int localCount=0;if(!count)count=&localCount;
    if(depth>64)return fail(error,QStringLiteral("缓存目录层级超过安全上限。"));
    for(const auto &entry:QDir(root).entryInfoList(QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System)) {
        if(++*count>100000||entry.isSymLink()||(!entry.isDir()&&!entry.isFile())||entry.canonicalFilePath()!=entry.absoluteFilePath())
            return fail(error,QStringLiteral("缓存含不安全路径、特殊文件或超过安全数量限制：%1").arg(entry.absoluteFilePath()));
        if(entry.isDir()&&!removableTree(entry.absoluteFilePath(),error,depth+1,count))return false;
    }
    return true;
}
}
bool ExportWorkspace::validateLocalRuntime(const QString &path,QString *error) {
    QJsonObject runtime;if(!object(path,&runtime,error))return false;
    const auto data=runtime.value("dataRoot");const auto endpoints=runtime.value("endpoints");
    if(runtime.value("format")!="hypit.runtime-local@1"||!data.isString()||data.toString().isEmpty()||QDir::isAbsolutePath(data.toString())||data.toString().contains('\\')||data.toString().contains(':'))return fail(error,QStringLiteral("Runtime 必须使用隔离工作区内的相对 dataRoot。"));
    for(const auto &component:data.toString().split('/'))if(component.isEmpty()||component=="."||component=="..")return fail(error,QStringLiteral("Runtime dataRoot 包含不安全路径。"));
    domain::Project runtimeProject;runtimeProject.rootPath=QFileInfo(path).absoluteDir().canonicalPath();
    if(!noLinks(runtimeProject,data.toString(),nullptr,error,false))return false;
    if(!endpoints.isObject()||endpoints.toObject().isEmpty())return fail(error,QStringLiteral("Runtime 缺少本地 Endpoint。"));
    for(const auto &value:endpoints.toObject()) {
        if(!value.isObject())return fail(error,QStringLiteral("Runtime Endpoint 结构无效。"));
        const auto use=value.toObject().value("use").toString();
        if(use!="@hypit/provider-media-local"&&use!="@hypit/provider-hyperframes-local")return fail(error,QStringLiteral("导出只允许已核对的本地 Provider：%1").arg(use));
    }
    return true;
}
bool ExportWorkspace::freeze(const domain::Project &project,const domain::Snapshot &snapshot,FrozenExport *out,QString *error) {
    if(!out)return fail(error,QStringLiteral("冻结输出为空。"));
    const auto root=QFileInfo(project.rootPath).canonicalFilePath();
    if(root.isEmpty()||QFileInfo(project.rootPath).isSymLink())return fail(error,QStringLiteral("工程根目录不安全。"));
    QString runtime;if(!noLinks(project,project.runtimePath,&runtime,error)||!validateLocalRuntime(runtime,error)||!sourcesMatch(project,snapshot,error))return false;
    for(const auto &relative:{project.runPath,project.sourcePath})if(!noLinks(project,relative,nullptr,error))return false;
    QStringList files;qint64 total=0;if(!collect(root,{},&files,error,&total)||!metadata(project,error))return false;
    const auto workspace=QDir(root).filePath(".workbench/builds/"+QUuid::createUuid().toString(QUuid::WithoutBraces));
    if(!QDir().mkdir(workspace))return fail(error,QStringLiteral("无法创建冻结工作区。"));
    const auto rollback=[&]{QDir(workspace).removeRecursively();};
    QMap<QString,QByteArray> hashes;
    for(const auto &relative:files) {
        QString input;QByteArray data;if(!noLinks(project,relative,&input,error)||!bytes(input,&data,error)){rollback();return false;}
        const auto destination=QDir(workspace).filePath(relative);
        if(!QDir().mkpath(QFileInfo(destination).absolutePath())||!atomic(destination,data,error)){rollback();return false;}
        hashes.insert(relative,QCryptographicHash::hash(data,QCryptographicHash::Sha256));
    }
    if(!sourcesMatch(project,snapshot,error)){rollback();return false;}
    domain::Project frozen=project;frozen.rootPath=workspace;
    if(!sourcesMatch(frozen,snapshot,error)){rollback();return false;}
    for(auto it=hashes.cbegin();it!=hashes.cend();++it) {
        QString input;QByteArray data;if(!noLinks(project,it.key(),&input,error)||!bytes(input,&data,error)||QCryptographicHash::hash(data,QCryptographicHash::Sha256)!=it.value()){rollback();return fail(error,QStringLiteral("冻结过程中工程文件发生变化：%1").arg(it.key()));}
    }
    QStringList afterFiles;qint64 afterTotal=0;if(!collect(root,{},&afterFiles,error,&afterTotal)||afterFiles!=files){rollback();return fail(error,QStringLiteral("冻结过程中工程文件列表发生变化。"));}
    QJsonObject inputHashes;for(auto it=hashes.cbegin();it!=hashes.cend();++it)inputHashes.insert(it.key(),QString::fromLatin1(it.value().toHex()));
    const auto manifest=QJsonDocument(QJsonObject{{"format","qt-video-workbench.frozen-inputs@1"},{"sourceFingerprint",QString::fromLatin1(snapshot.sourceFingerprint.toHex())},{"hashes",inputHashes}}).toJson(QJsonDocument::Compact);
    if(manifest.size()>4*1024*1024||!atomic(QDir(workspace).filePath(".frozen-inputs.json"),manifest,error)){rollback();return fail(error,QStringLiteral("冻结清单无法安全保存。"));}
    out->workspace=workspace;
    if(!noLinks(frozen,project.runPath,&out->run,error)||!noLinks(frozen,project.runtimePath,&out->runtime,error)||!validateLocalRuntime(out->runtime,error)){rollback();return false;}
    out->stagePath=QDir(workspace).filePath(".export-stage.mp4");if(error)error->clear();return true;
}
bool ExportWorkspace::saveTask(const domain::Project &project,const domain::ExportTask &task,QString *error) {
    if(!safeWorkspace(project,task.workspace,error)||!metadata(project,error))return false;
    const auto root=QFileInfo(project.rootPath).canonicalFilePath();
    QJsonObject record{{"format","qt-video-workbench.export-task@1"},{"projectRoot",root},{"phase",task.phase},{"buildId",task.buildId},{"output",task.output},{"destination",task.destination},{"workspace",QDir(root).relativeFilePath(task.workspace)},{"error",task.error},{"hypitVersion",task.hypitVersion},{"active",task.active},{"revision",task.revision},{"sourceFingerprint",QString::fromLatin1(task.sourceFingerprint.toHex())},{"space",QJsonObject{{"width",task.space.width},{"height",task.space.height},{"frameCount",task.space.frameCount},{"durationSec",task.space.durationSec},{"frameRate",task.space.frameRate}}}};
    return atomic(QDir(root).filePath(".workbench/export-task.json"),QJsonDocument(record).toJson(QJsonDocument::Indented),error);
}
bool ExportWorkspace::loadTask(const domain::Project &project,domain::ExportTask *out,QString *error) {
    if(error)error->clear();if(!out)return fail(error,QStringLiteral("任务输出为空。"));
    QString path;if(!ProjectStore::resolvePath(project,".workbench/export-task.json",&path,error,false))return false;if(!QFileInfo::exists(path))return false;
    QJsonObject record;if(!object(path,&record,error))return false;const auto root=QFileInfo(project.rootPath).canonicalFilePath();
    if(record.value("format")!="qt-video-workbench.export-task@1"||record.value("projectRoot")!=root)return fail(error,QStringLiteral("导出记录格式或所属工程无效。"));
    domain::ExportTask task;
    for(const auto &key:{"phase","buildId","output","destination","workspace","sourceFingerprint","error"})if(!record.value(QLatin1String(key)).isString())return fail(error,QStringLiteral("导出记录字段无效。"));
    task.phase=record["phase"].toString();task.buildId=record["buildId"].toString();task.output=record["output"].toString();task.destination=record["destination"].toString();task.error=record["error"].toString();
    if(!QStringList{"planning","submitting","working","cancelling","getting","validating","stopped","complete","failed","cancelled"}.contains(task.phase))return fail(error,QStringLiteral("导出记录任务阶段未知。"));
    if(!record["active"].isBool()||!record["hypitVersion"].isString())return fail(error,QStringLiteral("导出记录缺少执行状态或 Hypit 版本。"));task.active=record["active"].toBool();task.hypitVersion=record["hypitVersion"].toString();
    const auto revision=record["revision"].toDouble(-1);if(!std::isfinite(revision)||revision<0||revision>2147483647||std::floor(revision)!=revision)return fail(error,QStringLiteral("导出记录修订号无效。"));task.revision=int(revision);
    static const QRegularExpression hex(QStringLiteral("^[0-9a-f]{64}$"));if(!hex.match(record["sourceFingerprint"].toString()).hasMatch())return fail(error,QStringLiteral("导出记录指纹无效。"));task.sourceFingerprint=QByteArray::fromHex(record["sourceFingerprint"].toString().toLatin1());
    if(!ProjectStore::resolvePath(project,record["workspace"].toString(),&task.workspace,error,false))return false;
    if(!record["space"].isObject())return fail(error,QStringLiteral("导出记录画幅无效。"));const auto space=record["space"].toObject();
    task.space={space["width"].toInt(),space["height"].toInt(),space["frameCount"].toInt(),space["durationSec"].toDouble(),space["frameRate"].toDouble()};
    FrozenExport frozen;if(!validateRecovered(project,task,&frozen,error))return false;*out=task;return true;
}
bool ExportWorkspace::validateRecovered(const domain::Project &project,const domain::ExportTask &task,FrozenExport *out,QString *error) {
    static const QRegularExpression id(QStringLiteral("^[A-Za-z0-9_-]{1,160}$"));
    if(!out||!safeWorkspace(project,task.workspace,error))return false;
    const bool awaitingId=task.buildId.isEmpty()&&task.active&&(task.phase=="stopped"||task.phase=="submitting");
    const bool unsubmitted=task.buildId.isEmpty()&&!task.active&&task.phase=="stopped";
    if((!awaitingId&&!unsubmitted&&!id.match(task.buildId).hasMatch())||task.output!="final.video"||!QDir::isAbsolutePath(task.destination)||task.destination.contains(QChar::Null)||QFileInfo(task.destination).isSymLink())return fail(error,QStringLiteral("恢复任务的 Build、Output 或目标路径无效。"));
    if(task.space.width<=0||task.space.height<=0||task.space.frameCount<=0||!std::isfinite(task.space.durationSec)||task.space.durationSec<=0||!std::isfinite(task.space.frameRate)||task.space.frameRate<=0)return fail(error,QStringLiteral("恢复任务的画幅或时长无效。"));
    domain::Project frozen=project;frozen.rootPath=task.workspace;out->workspace=task.workspace;
    if(!noLinks(frozen,project.runPath,&out->run,error)||!noLinks(frozen,project.runtimePath,&out->runtime,error)||!validateLocalRuntime(out->runtime,error))return false;
    QJsonObject manifest;if(!object(QDir(task.workspace).filePath(".frozen-inputs.json"),&manifest,error))return false;
    if(manifest["format"]!="qt-video-workbench.frozen-inputs@1"||manifest["sourceFingerprint"]!=QString::fromLatin1(task.sourceFingerprint.toHex())||!manifest["hashes"].isObject()||manifest["hashes"].toObject().isEmpty()||manifest["hashes"].toObject().size()>10000)return fail(error,QStringLiteral("冻结输入清单或源码指纹不一致。"));
    const auto hashes=manifest["hashes"].toObject();qint64 total=0;
    for(auto it=hashes.begin();it!=hashes.end();++it) {
        QString path;QByteArray data;if(!it.value().isString()||!noLinks(frozen,it.key(),&path,error)||!bytes(path,&data,error))return false;
        total+=data.size();if(total>maxCopyBytes||QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex())!=it.value().toString())return fail(error,QStringLiteral("冻结输入已变化，不能恢复此 Build：%1").arg(it.key()));
    }
    out->stagePath=QDir(task.workspace).filePath(".export-stage.mp4");if(QFileInfo(out->stagePath).isSymLink())return fail(error,QStringLiteral("恢复暂存成片路径不安全。"));return true;
}
bool ExportWorkspace::validateFinishedCache(const domain::Project &project,const domain::ExportTask &task,FrozenExport *out,QString *error) {
    if(task.active||!QStringList{"complete","failed","cancelled"}.contains(task.phase)||task.buildId.isEmpty())
        return fail(error,QStringLiteral("只可清理已有 Build ID、已确认终态且没有活动任务的缓存。"));
    domain::ExportTask persisted;
    if(!loadTask(project,&persisted,error))return fail(error,QStringLiteral("缓存任务记录无法安全读取：%1").arg(error?*error:QString()));
    if(persisted.active||persisted.phase!=task.phase||persisted.buildId!=task.buildId||persisted.workspace!=task.workspace
        ||persisted.sourceFingerprint!=task.sourceFingerprint||persisted.revision!=task.revision||persisted.output!=task.output
        ||persisted.destination!=task.destination||persisted.hypitVersion!=task.hypitVersion
        ||persisted.space.width!=task.space.width||persisted.space.height!=task.space.height||persisted.space.frameCount!=task.space.frameCount
        ||persisted.space.durationSec!=task.space.durationSec||persisted.space.frameRate!=task.space.frameRate)
        return fail(error,QStringLiteral("磁盘缓存任务记录已变化，未清理。"));
    return validateRecovered(project,task,out,error)&&removableTree(task.workspace,error);
}
bool ExportWorkspace::removeFinishedCache(const domain::Project &project,const domain::ExportTask &task,QString *error) {
    FrozenExport frozen;if(!validateFinishedCache(project,task,&frozen,error))return false;
    QString record;if(!noLinks(project,".workbench/export-task.json",&record,error)||!QFileInfo(record).isFile())return false;
    // The UUID and manifest checks above prohibit project inputs and sibling
    // Builds. Generated files are included in the no-links tree check.
    if(!QDir(frozen.workspace).removeRecursively())return fail(error,QStringLiteral("无法删除当前终态任务的冻结缓存。"));
    if(!QFile::remove(record))return fail(error,QStringLiteral("缓存已删除，但无法删除当前任务记录。"));
    if(error)error->clear();return true;
}
}
