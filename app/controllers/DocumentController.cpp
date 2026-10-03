#include "controllers/DocumentController.h"
#include "services/ProjectStore.h"
#include "services/AssetService.h"
#include <QFileInfo>
#include <QTimer>
namespace qvw::controllers {
DocumentController::DocumentController(QObject *parent):QObject(parent),m_videoImporter(this){
    connect(&m_videoImporter,&services::VideoAssetImporter::stateChanged,this,[this]{publishImportState();});
    connect(&m_videoImporter,&services::VideoAssetImporter::failed,this,[this](const QString &error){
        if(m_batchActive && m_batchVideo && m_batchPending) rejectBatchEntry(error);
        else emit failed(error);
    });
    connect(&m_videoImporter,&services::VideoAssetImporter::imported,this,[this](const domain::Project &project,const domain::Asset &asset){
        if(!m_project || m_importGeneration!=m_documentGeneration)return;
        acceptImport(project,asset,true);
    });
}
bool DocumentController::create(const QString &templateDirectory,const QString &destination,const QString &name) {
    if(importingVideo()){emit failed(QStringLiteral("素材正在导入，请等待完成或取消后新建工程。"));return false;}
    domain::Project project; QString error;
    if (!services::ProjectStore::create(templateDirectory,destination,name,&project,&error)) { emit failed(error); return false; }
    ++m_documentGeneration;m_project=project; emit projectChanged(project); emit projectLoaded(project);
    emit message(QStringLiteral("已创建工程：%1").arg(project.manifestPath())); return true;
}
bool DocumentController::open(const QString &manifest) {
    if(importingVideo()){emit failed(QStringLiteral("素材正在导入，请等待完成或取消后打开工程。"));return false;}
    domain::Project project; QString error;
    if (!services::ProjectStore::load(manifest,&project,&error)) { emit failed(error); return false; }
    ++m_documentGeneration;m_project=project; emit projectChanged(project); emit projectLoaded(project);
    emit message(QStringLiteral("已打开工程：%1").arg(project.manifestPath())); return true;
}
bool DocumentController::save() {
    if(importingVideo()){emit failed(QStringLiteral("素材正在导入，请等待完成或取消后保存工程。"));return false;}
    if (!m_project) { emit failed(QStringLiteral("请先新建或打开工程。")); return false; }
    QString error;
    if (!services::ProjectStore::save(*m_project,&error)) { emit failed(error); return false; }
    emit message(QStringLiteral("工程元数据已保存；源文件由编辑操作保存，不触发导出。")); return true;
}
bool DocumentController::importImage(const QString &path) {
    if(importingVideo()){emit failed(QStringLiteral("素材正在导入，请等待完成或取消后导入图片。"));return false;}
    if (!m_project) { emit failed(QStringLiteral("请先新建或打开工程。")); return false; }
    auto candidate=*m_project;domain::Asset asset; QString error;
    if (!services::AssetService::importImage(candidate,path,&asset,&error)) { emit failed(error); return false; }
    acceptImport(candidate,asset,false);return true;
}
void DocumentController::setMediaTools(const QString &ffprobe,const QString &ffmpeg,const QProcessEnvironment &environment){
    if(importingVideo()){emit failed(QStringLiteral("素材正在导入，请等待完成或取消后设置媒体工具。"));return;}
    m_ffprobe=ffprobe;m_ffmpeg=ffmpeg;m_videoImporter.setEnvironment(environment);
}
bool DocumentController::importVideo(const QString &path){
    if(importingVideo()){emit failed(QStringLiteral("已有素材正在导入，请等待完成或取消后重试。"));return false;}
    if(!m_project){emit failed(QStringLiteral("请先新建或打开工程。"));return false;}
    m_importGeneration=m_documentGeneration;return m_videoImporter.start(*m_project,path,m_ffprobe,m_ffmpeg);
}
bool DocumentController::importFiles(const QStringList &paths){
    if(importingVideo()){emit failed(QStringLiteral("已有素材正在导入，请等待完成或取消后重试。"));return false;}
    if(!m_project){emit failed(QStringLiteral("请先新建或打开工程。"));return false;}
    if(paths.isEmpty() || paths.size()>64){emit failed(QStringLiteral("每批请选择 1 到 64 个 PNG、JPEG 或 MP4 文件。"));return false;}
    ++m_batchGeneration;m_batchActive=true;m_batchPending=false;m_batchVideo=false;
    m_batchPaths=paths;m_batchErrors.clear();m_batchNext=0;m_batchCompleted=0;m_batchSuccess=0;
    m_importGeneration=m_documentGeneration;
    const auto generation=m_batchGeneration;
    publishImportState();
    if(m_batchActive && generation==m_batchGeneration)scheduleNextImport();
    return true;
}
void DocumentController::publishImportState(){
    const auto busy=importingVideo();
    if(busy==m_announcedBusy)return;
    m_announcedBusy=busy;emit videoImportStateChanged(busy);
}
void DocumentController::scheduleNextImport(){
    const auto generation=m_batchGeneration;
    QTimer::singleShot(0,this,[this,generation]{processNextImport(generation);});
}
void DocumentController::processNextImport(quint64 generation){
    if(!m_batchActive || generation!=m_batchGeneration || m_batchPending
       || !m_project || m_importGeneration!=m_documentGeneration)return;
    if(m_batchNext==m_batchPaths.size()){finishBatch(false);return;}
    const auto path=m_batchPaths[m_batchNext++];m_batchPending=true;
    const auto extension=QFileInfo(path).suffix().toLower();m_batchVideo=(extension=="mp4");
    if(m_batchVideo){
        // The importer reports synchronous start failures through failed(), too.
        if(!m_videoImporter.start(*m_project,path,m_ffprobe,m_ffmpeg)
           && m_batchActive && generation==m_batchGeneration && m_batchPending)
            rejectBatchEntry(QStringLiteral("视频导入未能启动。"));
        return;
    }
    if(extension!="png" && extension!="jpg" && extension!="jpeg"){
        rejectBatchEntry(QStringLiteral("仅支持 PNG、JPEG 或 MP4 文件。"));return;
    }
    auto candidate=*m_project;domain::Asset asset;QString error;
    if(!services::AssetService::importImage(candidate,path,&asset,&error)){rejectBatchEntry(error);return;}
    acceptImport(candidate,asset,false);
}
void DocumentController::acceptImport(const domain::Project &project,const domain::Asset &asset,bool video){
    const auto documentGeneration=m_documentGeneration,batchGeneration=m_batchGeneration;
    const bool batch=m_batchActive && m_batchPending;
    m_project=project;
    if(batch){m_batchPending=false;++m_batchCompleted;++m_batchSuccess;}
    // Publish the committed project before a progress handler can cancel the remaining queue.
    emit projectChanged(project);
    if(documentGeneration!=m_documentGeneration || (batch && (!m_batchActive || batchGeneration!=m_batchGeneration)))return;
    if(batch){
        emit importProgress(m_batchCompleted,m_batchPaths.size(),m_batchPaths[m_batchNext-1]);
        if(!m_batchActive || batchGeneration!=m_batchGeneration || documentGeneration!=m_documentGeneration)return;
    }
    emit assetImported(asset);
    if(documentGeneration!=m_documentGeneration || (batch && (!m_batchActive || batchGeneration!=m_batchGeneration)))return;
    emit message(video ? QStringLiteral("视频已导入并保存：%1；模板使用前 8 秒，静音播放。").arg(asset.path)
                       : QStringLiteral("图片已导入并保存：%1；选中素材和组件后，点击“替换选中组件素材”。").arg(asset.path));
    if(batch && m_batchActive && batchGeneration==m_batchGeneration)scheduleNextImport();
}
void DocumentController::rejectBatchEntry(const QString &error){
    const auto generation=m_batchGeneration;
    const auto path=m_batchPaths[m_batchNext-1];
    m_batchPending=false;++m_batchCompleted;
    m_batchErrors.append(QStringLiteral("%1：%2").arg(QFileInfo(path).fileName(),error));
    emit importProgress(m_batchCompleted,m_batchPaths.size(),path);
    if(m_batchActive && generation==m_batchGeneration)scheduleNextImport();
}
void DocumentController::finishBatch(bool canceled){
    if(!m_batchActive)return;
    const auto success=m_batchSuccess,total=int(m_batchPaths.size());
    auto errors=m_batchErrors;
    if(canceled)errors.append(QStringLiteral("导入已取消；已完成 %1/%2，成功 %3 个。").arg(m_batchCompleted).arg(total).arg(success));
    ++m_batchGeneration;m_batchActive=false;m_batchPending=false;m_batchVideo=false;
    emit importBatchFinished(success,total,errors);
    publishImportState();
}
bool DocumentController::importingVideo() const{return m_batchActive || m_videoImporter.active();}
void DocumentController::cancelVideoImport(){
    // Keep aggregate busy true until the process is stopped and the queue is discarded.
    m_videoImporter.cancel();finishBatch(true);publishImportState();
}
void DocumentController::close() {
    const auto generation=++m_documentGeneration;m_project.reset();cancelVideoImport();
    if(generation==m_documentGeneration)emit documentClosed();
}
}
