#include "controllers/DocumentController.h"
#include "services/ProjectStore.h"
#include "services/AssetService.h"
namespace qvw::controllers {
bool DocumentController::create(const QString &templateDirectory,const QString &destination,const QString &name) {
    domain::Project project; QString error;
    if (!services::ProjectStore::create(templateDirectory,destination,name,&project,&error)) { emit failed(error); return false; }
    m_project=project; emit projectChanged(project); emit projectLoaded(project);
    emit message(QStringLiteral("已创建工程：%1").arg(project.manifestPath())); return true;
}
bool DocumentController::open(const QString &manifest) {
    domain::Project project; QString error;
    if (!services::ProjectStore::load(manifest,&project,&error)) { emit failed(error); return false; }
    m_project=project; emit projectChanged(project); emit projectLoaded(project);
    emit message(QStringLiteral("已打开工程：%1").arg(project.manifestPath())); return true;
}
bool DocumentController::save() {
    if (!m_project) { emit failed(QStringLiteral("请先新建或打开工程。")); return false; }
    QString error;
    if (!services::ProjectStore::save(*m_project,&error)) { emit failed(error); return false; }
    emit message(QStringLiteral("工程元数据已保存；源文件由编辑操作保存，不触发导出。")); return true;
}
bool DocumentController::importImage(const QString &path) {
    if (!m_project) { emit failed(QStringLiteral("请先新建或打开工程。")); return false; }
    domain::Asset asset; QString error;
    if (!services::AssetService::importImage(*m_project,path,&asset,&error)) { emit failed(error); return false; }
    emit projectChanged(*m_project);
    emit message(QStringLiteral("图片已导入并保存：%1；设置 Studio 的图片路径后才会应用到画面。").arg(asset.path)); return true;
}
void DocumentController::close() { m_project.reset(); emit documentClosed(); }
}
