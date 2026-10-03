#include "agent/AgentTaskStore.h"
#include "services/ProjectStore.h"
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
namespace qvw::agent {
namespace {
bool fail(QString *error,const QString &e){if(error)*error=e;return false;}
bool path(const domain::Project &p,const QString &name,QString *out,QString *error,bool create){
    const auto relative=".workbench/agent/"+name;
    // Containment alone also permits links into other authored files in this
    // project. Task metadata must never follow any link, even an internal one.
    auto cursor=QFileInfo(p.rootPath).absoluteFilePath();
    if(QFileInfo(cursor).isSymLink())return fail(error,"任务记录路径不能包含符号链接。");
    const auto parts=relative.split('/');
    for(int i=0;i<parts.size();++i){const auto &part=parts[i];
        cursor=QDir(cursor).filePath(part);
        const QFileInfo info(cursor);
        if(info.isSymLink())return fail(error,"任务记录路径不能包含符号链接。");
        if(info.exists()&&(i==parts.size()-1?!info.isFile():!info.isDir()))return fail(error,"任务记录只能使用普通文件和目录，不能使用特殊文件。");
    }
    if(!services::ProjectStore::resolvePath(p,relative,out,error,false))return false;
    if(create&&!QDir().mkpath(QFileInfo(*out).absolutePath()))return fail(error,"无法创建任务记录目录。");
    return services::ProjectStore::resolvePath(p,relative,out,error,false);
}
}
bool AgentTaskStore::save(const domain::Project &p,const QJsonObject &o,QString *error){
    const auto bytes=QJsonDocument(o).toJson();
    if(o["format"]!="qvw.agent-task@1"||bytes.size()>256*1024)return fail(error,"任务记录格式无效或超过256KiB。");
    QString file,lockPath;if(!path(p,"current.json",&file,error,true)||!path(p,"task.lock",&lockPath,error,false))return false;
    QLockFile lock(lockPath);if(!lock.tryLock(0))return fail(error,"其他进程正在更新任务记录。");
    QSaveFile out(file);if(!out.open(QIODevice::WriteOnly)||out.write(bytes)!=bytes.size()||!out.commit())return fail(error,"任务记录保存失败。");
    if(error)error->clear();return true;
}
bool AgentTaskStore::load(const domain::Project &p,QJsonObject *state,QString *error){
    QString file;if(!state||!path(p,"current.json",&file,error,false))return false;
    QFile input(file);if(!input.open(QIODevice::ReadOnly))return fail(error,"没有可恢复的Agent任务。");
    if(input.size()>256*1024)return fail(error,"任务记录超过256KiB。");
    QJsonParseError parse;const auto document=QJsonDocument::fromJson(input.read(256*1024+1),&parse);
    if(parse.error!=QJsonParseError::NoError||!document.isObject()||document.object()["format"]!="qvw.agent-task@1")return fail(error,"任务记录损坏或版本不支持。");
    *state=document.object();if(error)error->clear();return true;
}
bool AgentTaskStore::append(const domain::Project &p,const QJsonObject &event,QString *error){
    const auto bytes=QJsonDocument(event).toJson(QJsonDocument::Compact)+'\n';if(bytes.size()>16384)return fail(error,"执行事件超过16KiB。");
    QString file,lockPath;if(!path(p,"events.jsonl",&file,error,true)||!path(p,"task.lock",&lockPath,error,false))return false;
    QLockFile lock(lockPath);if(!lock.tryLock(0))return fail(error,"其他进程正在更新任务记录。");
    if(QFileInfo(file).size()+bytes.size()>1024*1024){QString old;if(!path(p,"events.previous.jsonl",&old,error,false))return false;if(QFileInfo::exists(old)&&!QFile::remove(old))return fail(error,"事件轮转失败。");if(!QFile::rename(file,old))return fail(error,"事件轮转失败。");}
    QFile out(file);if(!out.open(QIODevice::WriteOnly|QIODevice::Append)||out.write(bytes)!=bytes.size()||!out.flush())return fail(error,"执行事件保存失败。");
    if(error)error->clear();return true;
}
}
