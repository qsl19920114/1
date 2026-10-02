#include "services/SourceEditGuard.h"
#include "services/ProjectStore.h"
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
namespace qvw::services {
namespace {
bool fail(QString *error,const QString &text){if(error)*error=text;return false;}
bool contents(const QString &path,QByteArray *bytes,QString *error) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)||file.size()>2*1024*1024) return fail(error,QStringLiteral("源码不可读或超过2MiB上限。"));
    *bytes=file.readAll();
    if(file.error()!=QFileDevice::NoError)return fail(error,file.errorString());
    return true;
}
}
bool SourceEditGuard::arm(const QString &root,const domain::Snapshot &snapshot,const QString &relative,const QString &text,QString *error) {
    clear();
    if(!snapshot.sourceFiles.contains(relative))return fail(error,QStringLiteral("只能编辑当前会话声明的源码文件。"));
    domain::Project project;project.rootPath=root;QString path;
    if(!ProjectStore::resolvePath(project,relative,&path,error))return false;
    if(QFileInfo(path).isSymLink())return fail(error,QStringLiteral("源码文件不能是符号链接。"));
    QByteArray before;if(!contents(path,&before,error))return false;
    if(before!=snapshot.sourceFiles.value(relative).toUtf8())return fail(error,QStringLiteral("源码已被外部修改，请先刷新会话。"));
    const auto after=text.toUtf8();
    if(after.size()>2*1024*1024)return fail(error,QStringLiteral("源码超过2MiB上限。"));
    m_root=QFileInfo(root).canonicalFilePath();m_relative=relative;m_before=before;m_after=after;m_armed=true;
    if(error)error->clear();return true;
}
bool SourceEditGuard::restore(QString *error) {
    if(!m_armed)return fail(error,QStringLiteral("没有可恢复的源码预像。"));
    // Consume the guard: no blind second attempt after any failure.
    m_armed=false;
    domain::Project project;project.rootPath=m_root;QString path;
    if(!ProjectStore::resolvePath(project,m_relative,&path,error))return false;
    if(QFileInfo(path).isSymLink())return fail(error,QStringLiteral("源码已被替换为符号链接，未恢复。"));
    QByteArray current;if(!contents(path,&current,error))return false;
    if(current!=m_after)return fail(error,QStringLiteral("源码出现外部修改，保留当前文件，不覆盖恢复。"));
    QSaveFile output(path);output.setDirectWriteFallback(false);
    if(!output.open(QIODevice::WriteOnly)||output.write(m_before)!=m_before.size())return fail(error,output.errorString());
    // Check again immediately before atomic replacement; external editors do
    // not share a lock, so the supported guarantee is content-checked recovery.
    if(!contents(path,&current,error)||current!=m_after){output.cancelWriting();return fail(error,QStringLiteral("恢复前检测到外部修改，已停止。"));}
    if(!output.commit())return fail(error,output.errorString());
    if(error)error->clear();return true;
}
void SourceEditGuard::clear(){m_armed=false;m_root.clear();m_relative.clear();m_before.clear();m_after.clear();}
}
