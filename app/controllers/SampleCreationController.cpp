#include "controllers/SampleCreationController.h"
#include <QFileInfo>
namespace qvw::controllers {
SampleCreationController::SampleCreationController(DocumentController &document,EditorController &editor,QObject *parent)
    :QObject(parent),m_document(document),m_editor(editor) {
    connect(&document,&DocumentController::documentClosed,this,&SampleCreationController::cancel);
    connect(&document,&DocumentController::projectLoaded,this,[this](const domain::Project &p){if(!m_root.isEmpty()&&p.rootPath!=m_root)cancel();});
    connect(&document,&DocumentController::failed,this,&SampleCreationController::cancel);
    connect(&document,&DocumentController::assetImported,this,[this](const domain::Asset &asset){
        if(m_root.isEmpty()||m_document.project().rootPath!=m_root||asset.mime!="video/mp4")return;
        m_assetPath="./"+asset.path;attemptBinding();
    });
    connect(&document,&DocumentController::videoImportStateChanged,this,[this](bool busy){if(!busy)attemptBinding();});
    connect(&editor,&EditorController::stateChanged,this,[this](bool ready,bool busy,bool,bool){if(ready&&!busy)attemptBinding();});
    connect(&editor,&EditorController::operationSucceeded,this,[this]{if(!m_waitingForEdit)return;const auto path=m_assetPath;cancel();emit bound(path);emit message(QStringLiteral("示例视频已应用，可以调整文案并导出作品。"));});
    connect(&editor,&EditorController::failed,this,[this](const QString &text){if(m_root.isEmpty())return;cancel();emit failed(QStringLiteral("示例视频已保留为素材，但自动应用未完成：%1").arg(text));});
}
bool SampleCreationController::create(const QString &sample,const QString &templateDirectory,const QString &destination,const QString &name) {
    if(m_editor.isBusy()||m_document.importingVideo()){emit failed(QStringLiteral("请等待当前编辑或导入完成。"));return false;}
    cancel();if(!QFileInfo(sample).isFile()){emit failed(QStringLiteral("示例文件不可读，请检查本地 Hypit。"));return false;}
    if(!m_document.create(templateDirectory,destination,name))return false;
    m_root=m_document.project().rootPath;
    if(!m_document.importVideo(sample)){cancel();return false;}
    emit message(QStringLiteral("已创建视频故事工程，正在校验并导入示例。"));return true;
}
void SampleCreationController::cancel(){m_root.clear();m_assetPath.clear();m_waitingForEdit=false;}
void SampleCreationController::attemptBinding() {
    if(m_root.isEmpty()||m_assetPath.isEmpty()||m_waitingForEdit||m_document.importingVideo()||!m_editor.isReady()||m_editor.isBusy())return;
    if(m_document.project().rootPath!=m_root||QFileInfo(m_editor.workspace()).canonicalFilePath()!=QFileInfo(m_root).canonicalFilePath()){cancel();return;}
    for(const auto &track:m_editor.snapshot().tracks)for(const auto &clip:track.clips)for(const auto &field:clip.inspector)if(field.binding=="video"&&field.isEditable()){
        m_waitingForEdit=true;m_editor.edit(clip.id,field.id,m_assetPath);return;
    }
    cancel();emit failed(QStringLiteral("工程没有可写视频组件；已保留导入的素材。"));
}
}
