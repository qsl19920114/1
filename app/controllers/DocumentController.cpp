#include "controllers/DocumentController.h"
#include "services/ProjectStore.h"
#include "services/AssetService.h"
namespace qvw::controllers {
DocumentController::DocumentController(QObject *parent):QObject(parent),m_videoImporter(this){
    connect(&m_videoImporter,&services::VideoAssetImporter::stateChanged,this,&DocumentController::videoImportStateChanged);
    connect(&m_videoImporter,&services::VideoAssetImporter::failed,this,&DocumentController::failed);
    connect(&m_videoImporter,&services::VideoAssetImporter::imported,this,[this](const domain::Project &project,const domain::Asset &asset){
        if(!m_project||m_importGeneration!=m_documentGeneration)return;
        m_project=project;const auto gen=m_documentGeneration;emit projectChanged(project);if(gen!=m_documentGeneration)return;
        emit assetImported(asset);if(gen!=m_documentGeneration)return;
        emit message(QStringLiteral("视频已导入并保存：%1；模板使用前 8 秒，静音播放。").arg(asset.path));
    });
}
bool DocumentController::create(const QString &templateDirectory,const QString &destination,const QString &name) {
    cancelVideoImport();
    domain::Project project; QString error;
    if (!services::ProjectStore::create(templateDirectory,destination,name,&project,&error)) { emit failed(error); return false; }
    ++m_documentGeneration;m_project=project; emit projectChanged(project); emit projectLoaded(project);
    emit message(QStringLiteral("已创建工程：%1").arg(project.manifestPath())); return true;
}
bool DocumentController::open(const QString &manifest) {
    cancelVideoImport();
    domain::Project project; QString error;
    if (!services::ProjectStore::load(manifest,&project,&error)) { emit failed(error); return false; }
    ++m_documentGeneration;m_project=project; emit projectChanged(project); emit projectLoaded(project);
    emit message(QStringLiteral("已打开工程：%1").arg(project.manifestPath())); return true;
}
bool DocumentController::save() {
    if(importingVideo()){emit failed(QStringLiteral("视频正在导入，请等待完成或取消后保存工程。"));return false;}
    if (!m_project) { emit failed(QStringLiteral("请先新建或打开工程。")); return false; }
    QString error;
    if (!services::ProjectStore::save(*m_project,&error)) { emit failed(error); return false; }
    emit message(QStringLiteral("工程元数据已保存；源文件由编辑操作保存，不触发导出。")); return true;
}
bool DocumentController::importImage(const QString &path) {
    if(importingVideo()){emit failed(QStringLiteral("视频正在导入，请等待完成或取消后导入图片。"));return false;}
    if (!m_project) { emit failed(QStringLiteral("请先新建或打开工程。")); return false; }
    domain::Asset asset; QString error;
    if (!services::AssetService::importImage(*m_project,path,&asset,&error)) { emit failed(error); return false; }
    emit projectChanged(*m_project);emit assetImported(asset);
    emit message(QStringLiteral("图片已导入并保存：%1；选中素材和组件后，点击“应用到选中组件的图片”。").arg(asset.path)); return true;
}
void DocumentController::setMediaTools(const QString &ffprobe,const QString &ffmpeg,const QProcessEnvironment &environment){
    if(importingVideo()){emit failed(QStringLiteral("视频正在导入，请等待完成或取消后设置媒体工具。"));return;}
    m_ffprobe=ffprobe;m_ffmpeg=ffmpeg;m_videoImporter.setEnvironment(environment);
}
bool DocumentController::importVideo(const QString &path){
    if(importingVideo()){emit failed(QStringLiteral("已有视频正在导入，请等待完成或取消后重试。"));return false;}
    if(!m_project){emit failed(QStringLiteral("请先新建或打开工程。"));return false;}
    m_importGeneration=m_documentGeneration;return m_videoImporter.start(*m_project,path,m_ffprobe,m_ffmpeg);
}
bool DocumentController::importingVideo() const{return m_videoImporter.active();}
void DocumentController::cancelVideoImport(){m_videoImporter.cancel();}
void DocumentController::close() {cancelVideoImport();++m_documentGeneration;m_project.reset();emit documentClosed();}
}
