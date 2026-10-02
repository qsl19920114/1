#include "ExportController.h"
#include "infrastructure/JsonProcess.h"
#include "services/ExportWorkspace.h"
#include "services/MediaValidation.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <cmath>

namespace qvw::controllers {
namespace {
bool buildId(const QString &value){static const QRegularExpression pattern(QStringLiteral("^bld_[A-Za-z0-9_-]{1,156}$"));return pattern.match(value).hasMatch();}
bool localProvider(const QString &use){return use=="@hypit/provider-media-local"||use=="@hypit/provider-hyperframes-local";}
bool zero(const QJsonObject &object,const char *key){const auto value=object.value(QLatin1String(key));return value.isDouble()&&value.toDouble()==0;}
bool planAccepted(const QJsonObject &object,int code) {
    if(code!=0||object["format"]!="hypit.cli-plan@1"||object["ok"]!=true||!zero(object,"providerRequestCount")||!zero(object,"requestIssueCount")||!zero(object,"unresolvedRequestCount")||!zero(object,"unsupportedRequestCount")||!object["preflight"].isObject()||object["preflight"].toObject()["ok"]!=true)return false;
    const auto count=object["localRequestCount"];if(!count.isDouble()||!std::isfinite(count.toDouble())||count.toDouble()<0||std::floor(count.toDouble())!=count.toDouble())return false;
    {
        if(!object["providers"].isArray())return false;
        for(const auto &value:object["providers"].toArray()){if(!value.isObject())return false;const auto provider=value.toObject();if(provider["status"]!="resolved"||provider["pricing"].toObject()["kind"]!="local"||!localProvider(provider["use"].toString()))return false;}
    }
    return true;
}
bool destinationPath(const domain::Project &project,const QString &requested,QString *out,QString *error) {
    if(!QDir::isAbsolutePath(requested)||requested.contains(QChar::Null)||QFileInfo(requested).suffix().compare("mp4",Qt::CaseInsensitive)!=0||QFileInfo(requested).isSymLink()){*error=QStringLiteral("导出目标必须是明确选择的 MP4 文件路径，且不能是符号链接。");return false;}
    const auto parent=QFileInfo(requested).absoluteDir().canonicalPath();if(parent.isEmpty()){*error=QStringLiteral("导出目标目录不存在。");return false;}
    const auto path=QDir(parent).filePath(QFileInfo(requested).fileName());const auto root=QFileInfo(project.rootPath).canonicalFilePath();
    if(path.startsWith(root+"/.workbench/")||path==root+"/.workbench"){*error=QStringLiteral("不能把导出目标写入任务工作区。");return false;}
    if(path.startsWith(root+'/')) {
        const auto relative=QDir(root).relativeFilePath(path);
        if(relative==project.sourcePath||relative==project.runPath||relative==project.runtimePath||relative=="workbench.qvw.json"||relative.startsWith("assets/")||relative.startsWith("packages/")){*error=QStringLiteral("导出目标不能覆盖工程输入文件。");return false;}
        for(const auto &asset:project.assets)if(relative==asset.path){*error=QStringLiteral("导出目标不能覆盖素材文件。");return false;}
    }
    if(QFileInfo(path).exists()&&!QFileInfo(path).isFile()){*error=QStringLiteral("导出目标不是文件。");return false;}*out=path;return true;
}
}
class ExportController::State {
public:
    ExportController *owner;infra::AppConfig config;infra::LogWriter &log;
    domain::Project project;domain::ExportTask task;services::FrozenExport frozen;
    infra::JsonProcess process;services::MediaValidation validation;QTimer poll;
    QElapsedTimer resultWait;
    QString command,ffprobe=QStandardPaths::findExecutable("ffprobe"),ffmpeg=QStandardPaths::findExecutable("ffmpeg");
    bool busy=false,lastPublishedBusy=false,pendingCancel=false,cancelIssued=false;quint64 generation=0;
    State(ExportController *o,infra::AppConfig c,infra::LogWriter &l):owner(o),config(std::move(c)),log(l),process(o),validation(o){poll.setSingleShot(true);poll.setInterval(1000);}
    bool publish(bool persist=true) {
        const auto gen=generation;
        bool persisted=true;
        if(persist&&!project.rootPath.isEmpty()&&!task.workspace.isEmpty()) {
            QString error;if(!services::ExportWorkspace::saveTask(project,task,&error)){log.error(error);task.error=error;busy=false;task.phase="stopped";poll.stop();process.cancel();validation.cancel();command.clear();persisted=false;}
        }
        const auto published=task;emit owner->taskChanged(published);if(gen!=generation)return false;
        if(lastPublishedBusy!=busy){lastPublishedBusy=busy;emit owner->busyChanged(busy);}
        if(gen==generation&&!persisted)emit owner->failed(task.error);
        return gen==generation&&persisted;
    }
    void fail(const QString &error,bool unknown=false) {
        poll.stop();process.cancel();validation.cancel();command.clear();pendingCancel=false;busy=false;
        task.error=unknown?QStringLiteral("观察结果未知；%1 可以恢复同一 Build 的状态查询。").arg(error):error;
        task.phase=unknown?"stopped":"failed";if(!unknown)task.active=false;log.error(task.error);
        if(publish())emit owner->failed(task.error);
    }
    bool send(const QString &operation) {
        if(!busy||project.rootPath.isEmpty())return false;
        QString error;
        if(!services::ExportWorkspace::validateLocalRuntime(frozen.runtime,&error)){fail(error,task.active);return false;}
        if(!QFileInfo(config.launcherPath).isFile()||!QFileInfo(config.launcherPath).isExecutable()||!QFileInfo(config.distributionPath).isDir()){fail(QStringLiteral("Hypit 启动器或发行目录无效。"),task.active);return false;}
        command=operation;QStringList arguments{operation};
        if(operation=="plan"||operation=="build")arguments<<frozen.run;
        else if(operation!="activity"&&operation!="builds")arguments<<task.buildId;
        if(operation=="get") {
            if(QFileInfo(frozen.stagePath).isSymLink()){fail(QStringLiteral("成片暂存路径不能是符号链接。"));return false;}
            if(QFileInfo::exists(frozen.stagePath)&&(!QFileInfo(frozen.stagePath).isFile()||!QFile::remove(frozen.stagePath))){fail(QStringLiteral("无法清理同一任务的旧暂存成片。"));return false;}
            arguments<<"--output"<<task.output<<"--to"<<frozen.stagePath;
        }
        arguments<<"--workspace"<<frozen.workspace;
        if(operation!="get"&&operation!="builds")arguments<<"--runtime"<<frozen.runtime;
        arguments<<"--json";
        if(!process.start(config.launcherPath,arguments,config.distributionPath,operation=="plan"||operation=="build"?120000:30000)){fail(QStringLiteral("已有导出命令正在运行。"),task.active);return false;}
        return true;
    }
    void requestCancel() {
        if(!busy||task.buildId.isEmpty()||cancelIssued)return;
        poll.stop();process.cancel();command.clear();pendingCancel=false;cancelIssued=true;task.phase="cancelling";
        if(publish())send("cancel");
    }
    bool acceptBuild(const QJsonObject &build,int code) {
        const auto id=build["id"].toString();
        if(!buildId(id)||(!task.buildId.isEmpty()&&id!=task.buildId)||!build["work"].isObject()||!build["result"].isObject()){fail(QStringLiteral("Build 状态的 ID 或结构与当前任务不符。"),task.active);return false;}
        task.buildId=id;const auto work=build["work"].toObject();const auto result=build["result"].toObject();const auto state=work["state"].toString();const auto outcome=work["outcome"].toString();const auto resultState=result["state"].toString();
        if(!QStringList{"unknown","submitting","working","done"}.contains(state)||!QStringList{"missing","open","complete","failed","cancelled","unavailable"}.contains(resultState)||(!outcome.isEmpty()&&!QStringList{"complete","failed","cancelled"}.contains(outcome))){fail(QStringLiteral("Build 状态包含未知字段值。"),true);return false;}
        if(build.contains("attention")){const auto attention=build["attention"].toObject()["message"].toString();fail(attention.isEmpty()?QStringLiteral("Build 需要处理后才能继续。"):attention,true);return false;}
        if(resultState=="failed"||(state=="done"&&outcome=="failed")){fail(build["failure"].toString(QStringLiteral("Build 执行失败。")));return false;}
        if(resultState=="cancelled"||(state=="done"&&outcome=="cancelled")) {
            task.phase="cancelled";task.active=false;task.error.clear();busy=false;poll.stop();if(publish())emit owner->message(QStringLiteral("Build 已进入取消终态。"));return false;
        }
        if(resultState=="complete") {
            if(code!=0||state!="done"||outcome!="complete"){fail(QStringLiteral("Build 完成状态不一致。"),true);return false;}
            task.active=false;task.phase="getting";task.error.clear();if(publish())send("get");return false;
        }
        if(state=="done"&&outcome=="complete"&&(resultState=="missing"||resultState=="open")&&code==0) {
            if(!resultWait.isValid())resultWait.start();
            if(resultWait.elapsed()>60000){fail(QStringLiteral("工作已完成，但 Result 在 60 秒内仍未保存。"),true);return false;}
        }else if(state=="done"||state=="unknown"||resultState=="unavailable"||code!=0){fail(QStringLiteral("Build 尚无可交付的完成 Result，观察已停止。"),true);return false;}
        task.active=true;task.phase=(cancelIssued||work["cancellationRequested"].toBool())?"cancelling":state=="done"?QStringLiteral("working"):state;task.error.clear();
        if(!publish())return false;
        // A direct taskChanged callback can request cancellation and start its
        // command. Its own reply will schedule the next status observation.
        if(process.isRunning())return true;
        if(pendingCancel)requestCancel();else poll.start();return true;
    }
    void onResult(const QJsonObject &object,int code) {
        if(!busy)return;const auto operation=command;command.clear();
        // Pinned output.ts:1047-1056 uses one error object for exceptions,
        // including author package syntax errors before a Plan can be built.
        if(object["format"]=="hypit.cli-error@1") {
            const auto error=object["error"].toObject();const auto text=error["message"].toString();
            const auto detail=text.isEmpty()?QStringLiteral("Hypit 返回错误，但缺少有效的错误说明。"):(error["code"].toString()+QStringLiteral("：")+text).left(16384);
            const auto gen=generation;log.error(detail);emit owner->message(detail);if(gen==generation&&busy)fail(detail,task.active);return;
        }
        if(operation=="plan") {
            const auto gen=generation;QStringList diagnostics;const auto preflight=object["preflight"].toObject();
            for(const auto &value:preflight["diagnostics"].toArray()){const auto diagnostic=value.isString()?value.toString():value.toObject()["message"].toString(QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact)));diagnostics<<diagnostic.left(4096);}
            QStringList targets;for(const auto &target:object["targets"].toArray())targets<<target.toString();
            const auto summary=QStringLiteral("导出计划：目标 %1；%2 项本地请求，%3 项付费请求；预检%4。%5").arg(targets.isEmpty()?task.output:targets.join(", ")).arg(object["localRequestCount"].toInt()).arg(object["providerRequestCount"].toInt()).arg(preflight["ok"]==true?QStringLiteral("通过"):QStringLiteral("未通过"),diagnostics.isEmpty()?QString():QStringLiteral("\n")+diagnostics.join('\n').left(16384));
            log.info(summary);emit owner->message(summary);if(gen!=generation||!busy)return;
            if(!planAccepted(object,code)){fail(QStringLiteral("导出计划、预检或本地 Provider 检查未通过。%1").arg(diagnostics.isEmpty()?QString():QStringLiteral("\n")+diagnostics.join('\n').left(16384)));return;}
            task.phase="submitting";task.active=true;if(publish())send("build");return;
        }
        if(operation=="activity"||operation=="builds") {
            const auto format=operation=="activity"?QStringLiteral("hypit.cli-activity@1"):QStringLiteral("hypit.cli-builds@1");
            if(code!=0||object["format"]!=format||!object["builds"].isArray()||object["omittedBuilds"].toInt()>0||object.contains("next")){fail(QStringLiteral("无法核对提交结果未知的 Build，未重新提交。"),true);return;}
            QSet<QString> ids;
            for(const auto &value:object["builds"].toArray()) {
                if(!value.isObject()||!buildId(value.toObject()["id"].toString())){fail(QStringLiteral("恢复列表包含无效 Build ID。"),true);return;}
                if(operation=="builds"&&value.toObject()["run"].toString()!=project.runPath){fail(QStringLiteral("恢复结果列表的 Run 与冻结任务不符。"),true);return;}
                ids.insert(value.toObject()["id"].toString());
            }
            if(ids.isEmpty()&&operation=="activity"){send("builds");return;}
            if(ids.size()!=1){fail(QStringLiteral("独立 Runtime 中未找到唯一 Build，保留未知提交记录，未重新提交。"),true);return;}
            task.buildId=*ids.cbegin();task.phase="working";task.error.clear();if(publish()){if(process.isRunning())return;if(pendingCancel)requestCancel();else send("status");}return;
        }
        if(operation=="build"||operation=="status") {
            const auto format=operation=="build"?QStringLiteral("hypit.cli-build@1"):QStringLiteral("hypit.cli-status@1");
            if(object["format"]!=format||!object["build"].isObject()){fail(QStringLiteral("命令未返回当前 Build 的有效状态。"),true);return;}
            acceptBuild(object["build"].toObject(),code);return;
        }
        if(operation=="cancel") {
            if(object["format"]!="hypit.cli-cancel@1"||!object["requested"].isBool()||!object["build"].isObject()){fail(QStringLiteral("取消请求结果无效，需恢复观察确认终态。"),true);return;}
            if(object["requested"]!=true){const auto gen=generation;emit owner->message(QStringLiteral("未确认取消请求；继续查询 Build 的真实状态。"));if(gen!=generation||!busy)return;}
            acceptBuild(object["build"].toObject(),code);return;
        }
        if(operation=="get") {
            if(code!=0||object["format"]!="hypit.cli-get@1"||object["build"]!=task.buildId||object["output"]!=task.output||object["kind"]!="resource"||object["path"]!=frozen.stagePath||!QFileInfo(frozen.stagePath).isFile()||QFileInfo(frozen.stagePath).isSymLink()||QFileInfo(frozen.stagePath).canonicalFilePath()!=frozen.stagePath){fail(QStringLiteral("Output 获取未返回当前任务的实际暂存成片。"));return;}
            task.phase="validating";if(publish())validation.start(frozen.stagePath,task.space,ffprobe,ffmpeg);
        }
    }
    void deliver() {
        if(!busy||task.phase!="validating")return;QString destination,error;
        if(!destinationPath(project,task.destination,&destination,&error)){fail(error);return;}
        QFile input(frozen.stagePath);QSaveFile output(destination);output.setDirectWriteFallback(false);
        if(!input.open(QIODevice::ReadOnly)||!output.open(QIODevice::WriteOnly)){fail(QStringLiteral("无法打开验证成片或目标文件：%1").arg(output.errorString()));return;}
        while(!input.atEnd()){const auto bytes=input.read(1024*1024);if(bytes.isEmpty()&&input.error()!=QFileDevice::NoError){fail(QStringLiteral("复制验证成片时读取失败。"));return;}if(output.write(bytes)!=bytes.size()){fail(QStringLiteral("原子交付成片失败：%1").arg(output.errorString()));return;}}
        if(!output.commit()){fail(QStringLiteral("原子交付成片失败：%1").arg(output.errorString()));return;}
        task.destination=destination;task.phase="complete";task.active=false;task.error.clear();busy=false;log.info(QStringLiteral("导出完整验证通过：%1 / %2").arg(task.buildId,destination));
        if(publish())emit owner->completed(destination);
    }
};
ExportController::ExportController(infra::AppConfig config,infra::LogWriter &log,QObject *parent):QObject(parent),m(new State(this,std::move(config),log)) {
    qRegisterMetaType<domain::ExportTask>();
    connect(&m->process,&infra::JsonProcess::commandFinished,this,[this](const QString &program,const QStringList &args,int code){m->log.command(program,args,code);});
    connect(&m->validation,&services::MediaValidation::commandFinished,this,[this](const QString &program,const QStringList &args,int code){m->log.command(program,args,code);});
    connect(&m->process,&infra::JsonProcess::result,this,[this](const QJsonObject &object,int code){m->onResult(object,code);});
    connect(&m->process,&infra::JsonProcess::failed,this,[this](const QString &error){if(m->busy)m->fail(error,m->task.active);});
    connect(&m->validation,&services::MediaValidation::failed,this,[this](const QString &error){if(m->busy)m->fail(error);});
    connect(&m->validation,&services::MediaValidation::validated,this,[this]{m->deliver();});
    connect(&m->poll,&QTimer::timeout,this,[this]{if(m->busy)m->send("status");});
}
ExportController::~ExportController(){stopObserving();delete m;}
void ExportController::setConfig(infra::AppConfig config){if(!m->busy)m->config=std::move(config);}
void ExportController::setMediaTools(QString ffprobe,QString ffmpeg){if(!m->busy){m->ffprobe=std::move(ffprobe);m->ffmpeg=std::move(ffmpeg);}}
void ExportController::setProject(domain::Project project) {
    clearProject();m->project=std::move(project);QString error;domain::ExportTask loaded;
    if(services::ExportWorkspace::loadTask(m->project,&loaded,&error)) {
        m->task=loaded;
        if((loaded.active||!loaded.buildId.isEmpty())&&loaded.phase!="complete"&&loaded.phase!="cancelled"&&loaded.phase!="failed"){m->task.phase="stopped";m->task.active=true;}
        m->publish();
    }else if(!error.isEmpty()){m->log.warn(error);emit message(error);}
}
void ExportController::clearProject() {
    stopObserving();++m->generation;m->project={};m->task={};m->frozen={};m->pendingCancel=false;m->cancelIssued=false;m->publish(false);
}
void ExportController::startExport(const domain::Snapshot &snapshot,QString destination) {
    if(m->busy||m->task.active){emit failed(QStringLiteral("已有导出任务；请先恢复观察或等待其终态。"));return;}
    if(m->project.rootPath.isEmpty()){emit failed(QStringLiteral("请先打开工程。"));return;}
    QString normalized,error;if(!destinationPath(m->project,destination,&normalized,&error)){emit failed(error);return;}
    ++m->generation;m->resultWait.invalidate();m->task={};m->task.hypitVersion=m->config.expectedHypitVersion;m->task.revision=snapshot.revision;m->task.sourceFingerprint=snapshot.sourceFingerprint;m->task.space=snapshot.space;m->task.destination=normalized;m->task.phase="planning";m->busy=true;m->pendingCancel=false;m->cancelIssued=false;
    if(!m->publish(false))return;
    if(!services::ExportWorkspace::freeze(m->project,snapshot,&m->frozen,&error)){m->fail(error);return;}
    m->task.workspace=m->frozen.workspace;if(m->publish())m->send("plan");
}
void ExportController::stopObserving() {
    const bool planning=m->busy&&m->task.phase=="planning"&&m->task.buildId.isEmpty();
    ++m->generation;m->poll.stop();m->process.cancel();m->validation.cancel();m->command.clear();
    if(!m->busy)return;m->busy=false;
    if(m->task.phase!="complete"&&m->task.phase!="failed"&&m->task.phase!="cancelled")m->task.phase="stopped";
    if(m->publish()&&planning)emit message(QStringLiteral("导出计划观察已停止，尚未提交 Build；可以开始新的导出。"));
}
void ExportController::resume() {
    if(m->busy){emit failed(QStringLiteral("导出任务正在观察中。"));return;}
    if(m->task.buildId.isEmpty()&&!m->task.active){emit failed(QStringLiteral("尚未提交 Build，可以开始新的导出。"));return;}
    QString error;
    if(m->project.rootPath.isEmpty()||!services::ExportWorkspace::validateRecovered(m->project,m->task,&m->frozen,&error)){emit failed(error.isEmpty()?QStringLiteral("没有可以恢复的 Build。"):error);return;}
    if(m->task.hypitVersion!=m->config.expectedHypitVersion){emit failed(QStringLiteral("恢复任务需要其固定的 Hypit 版本 %1，当前配置是 %2。").arg(m->task.hypitVersion,m->config.expectedHypitVersion));return;}
    QString destination;if(!destinationPath(m->project,m->task.destination,&destination,&error)){emit failed(error);return;}
    ++m->generation;m->resultWait.invalidate();m->busy=true;m->pendingCancel=false;m->cancelIssued=false;m->task.phase=m->task.buildId.isEmpty()?"stopped":"working";m->task.active=true;m->task.error.clear();if(m->publish())m->send(m->task.buildId.isEmpty()?QStringLiteral("activity"):QStringLiteral("status"));
}
void ExportController::cancelBuild() {
    if(!m->busy){if(m->task.active){resume();if(m->busy){m->pendingCancel=true;m->requestCancel();}}return;}
    if(m->task.phase=="planning"||m->task.phase=="getting"||m->task.phase=="validating") {
        ++m->generation;m->process.cancel();m->validation.cancel();m->poll.stop();m->command.clear();m->task.phase="cancelled";m->task.active=false;m->task.error.clear();m->busy=false;m->publish();return;
    }
    if(m->task.buildId.isEmpty()){m->pendingCancel=true;return;}
    m->requestCancel();
}
bool ExportController::isBusy() const{return m->busy;}
const domain::ExportTask &ExportController::task() const{return m->task;}
}
