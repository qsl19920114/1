#include "infrastructure/WorkspaceStore.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QSaveFile>
#include <QDateTime>
namespace qvw::infra {
namespace {
constexpr qint64 maxBytes=4*1024*1024;
bool fail(QString *error,const QString &message){if(error)*error=message;return false;}
bool safePath(const QString &path){auto info=QFileInfo(path);if(info.exists()&& !info.isFile())return false;while(!info.filePath().isEmpty()){if(info.isSymLink()){
#ifdef Q_OS_MAC
 // macOS exposes its system temporary roots through these canonical aliases.
 const auto alias=info.absoluteFilePath();if(!((alias=="/var"&&info.canonicalFilePath()=="/private/var")||(alias=="/tmp"&&info.canonicalFilePath()=="/private/tmp")))return false;
#else
 return false;
#endif
 }const auto parent=info.absolutePath();if(parent==info.absoluteFilePath())break;info=QFileInfo(parent);}return true;}
}
WorkspaceStore::WorkspaceStore(QString path):m_path(std::move(path)){}
bool WorkspaceStore::load(QString *error){
 if(!safePath(m_path))return fail(error,"工作区记录路径含链接或特殊文件。");
 QFile file(m_path);if(!file.exists())return true;
 if(!file.open(QIODevice::ReadOnly)||file.size()>maxBytes)return fail(error,"工作区记录不可读或超过 4 MiB。");
 const auto doc=QJsonDocument::fromJson(file.read(maxBytes+1));const auto obj=doc.object();
 if(!doc.isObject()||obj["format"]!="framelab.workspace@1"||!obj["recent"].isArray()||!obj["history"].isArray()||!obj["layout"].isObject())return fail(error,"工作区记录格式无效，已使用默认布局。");
 QJsonArray recent,history;for(const auto &v:obj["recent"].toArray()){auto r=v.toObject();if(!r["path"].isString()||!QDir::isAbsolutePath(r["path"].toString()))continue;recent.append(r);if(recent.size()==12)break;}
 for(const auto &v:obj["history"].toArray()){if(!v.isObject()||v.toObject()["id"].toString().isEmpty()||QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact).size()>16384)continue;history.append(v);if(history.size()==200)break;}
 m_layout=obj["layout"].toObject();m_recent=recent;m_history=history;if(error)error->clear();return true;
}
bool WorkspaceStore::save(QString *error) const {
 if(!safePath(m_path))return fail(error,"工作区记录路径含链接或特殊文件。");
 if(!QDir().mkpath(QFileInfo(m_path).absolutePath())||!safePath(m_path))return fail(error,"无法创建工作区记录目录。");
 const auto bytes=QJsonDocument(QJsonObject{{"format","framelab.workspace@1"},{"layout",m_layout},{"recent",m_recent},{"history",m_history}}).toJson(QJsonDocument::Compact);
 if(bytes.size()>maxBytes)return fail(error,"工作区记录超过大小限制。");QSaveFile file(m_path);file.setDirectWriteFallback(false);
 if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())return fail(error,"无法保存工作区记录："+file.errorString());if(error)error->clear();return true;
}
void WorkspaceStore::visit(const QString &manifest,const QString &name){if(!QDir::isAbsolutePath(manifest))return;QJsonObject item{{"path",manifest},{"name",name.left(256)},{"frame",frame(manifest)}};for(int i=m_recent.size()-1;i>=0;--i)if(m_recent[i].toObject()["path"]==manifest)m_recent.removeAt(i);m_recent.prepend(item);while(m_recent.size()>12)m_recent.removeLast();}
int WorkspaceStore::frame(const QString &manifest) const {for(const auto &v:m_recent){const auto r=v.toObject();if(r["path"]==manifest)return qMax(0,r["frame"].toInt());}return 0;}
void WorkspaceStore::setFrame(const QString &manifest,int frame){for(int i=0;i<m_recent.size();++i){auto r=m_recent[i].toObject();if(r["path"]==manifest){r["frame"]=qMax(0,frame);m_recent.replace(i,r);return;}}}
void WorkspaceStore::record(QJsonObject record){
 if(record["id"].toString().isEmpty())return;record["time"]=QDateTime::currentDateTime().toString(Qt::ISODate);
 if(QJsonDocument(record).toJson(QJsonDocument::Compact).size()>16384){record["detail"]=record["detail"].toString().left(2000);record.remove("events");}
 if(QJsonDocument(record).toJson(QJsonDocument::Compact).size()>16384)return;
 for(int i=m_history.size()-1;i>=0;--i)if(m_history[i].toObject()["id"]==record["id"])m_history.removeAt(i);
 m_history.prepend(record);while(m_history.size()>200)m_history.removeLast();
}
}
