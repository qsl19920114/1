#include "controllers/ProposalController.h"
#include <QFileInfo>
namespace qvw::controllers {
ProposalController::ProposalController(EditorController &editor,QObject *parent):QObject(parent),m_editor(editor),m_provider(std::make_shared<services::DemoProposalProvider>()) {
    connect(&editor,&EditorController::snapshotReady,this,[this](const domain::Snapshot &){invalidate();});
    connect(&editor,&EditorController::stateChanged,this,[this](bool ready,bool,bool,bool){if(!ready)invalidate();});
}
void ProposalController::setProject(const domain::Project &project) {m_project=project;m_hasProject=true;invalidate();}
void ProposalController::clearProject() {m_hasProject=false;m_project={};invalidate();}
void ProposalController::setProvider(std::shared_ptr<services::IProposalProvider> provider) {
    m_provider=provider?std::move(provider):std::make_shared<services::DemoProposalProvider>();invalidate();
}
bool ProposalController::available(QString *error) const {
    if(!m_hasProject||!QDir::isAbsolutePath(m_project.rootPath)||!QFileInfo(m_project.rootPath).isDir()) {
        if(error)*error=QStringLiteral("请先打开有效工程。");return false;
    }
    const auto projectRoot=QFileInfo(m_project.rootPath).canonicalFilePath();
    const auto editorRoot=QFileInfo(m_editor.workspace()).canonicalFilePath();
    if(projectRoot.isEmpty()||editorRoot!=projectRoot) {
        if(error)*error=QStringLiteral("编辑器会话与当前工程根目录不匹配，请重新打开工程。");return false;
    }
    if(m_editor.isBusy()){if(error)*error=QStringLiteral("编辑器正在编辑或刷新，请等待完成后重新生成或确认。");return false;}
    const auto &snapshot=m_editor.snapshot();
    if(!m_editor.isReady()||!snapshot.isLoaded()||snapshot.sourceFiles.isEmpty()||snapshot.sourceFingerprint.size()!=32) {
        if(error)*error=QStringLiteral("当前会话尚未就绪，请先刷新。");return false;
    }
    if(!m_project.sourcePath.isEmpty()&&snapshot.sourcePath!=m_project.sourcePath) {
        if(error)*error=QStringLiteral("当前会话源码与工程不匹配，请重新打开工程。");return false;
    }
    if(error)error->clear();return true;
}
void ProposalController::invalidate() {
    ++m_generation;const bool notify=m_pending;m_pending=false;m_summary.clear();m_proposal={};
    if(notify)emit proposalChanged({},false);
}
void ProposalController::generateDemo(const QString &input) {
    const auto generation=m_generation+1;invalidate();if(generation!=m_generation)return;
    QString error;if(!available(&error)){emit failed(error);return;}
    const auto snapshot=m_editor.snapshot();const auto project=m_project;const auto provider=m_provider;domain::EditProposal proposal;
    const bool generated=provider->generate(input,snapshot,project,&proposal,&error);
    if(generation!=m_generation)return;
    if(!generated){emit failed(error.isEmpty()?QStringLiteral("模拟未能生成有效提案。"):error);return;}
    proposal.origin=domain::ProposalOrigin::Demo;publish(proposal,generation);
}
void ProposalController::importJson(const QByteArray &json) {
    const auto generation=m_generation+1;invalidate();if(generation!=m_generation)return;
    QString error;if(!available(&error)){emit failed(error);return;}
    domain::EditProposal proposal;
    if(!services::ProposalService::parseJson(json,domain::ProposalOrigin::ExternalUnverified,&proposal,&error)){emit failed(error);return;}
    publish(proposal,generation);
}
void ProposalController::publish(const domain::EditProposal &proposal,quint64 generation) {
    if(generation!=m_generation)return;
    QString error;
    if(!available(&error)||!services::ProposalService::validate(proposal,m_editor.snapshot(),m_project,&error)){emit failed(error);return;}
    m_proposal=proposal;m_summary=services::ProposalService::summary(proposal,m_editor.snapshot());m_pending=true;
    emit proposalChanged(m_summary,true);
}
void ProposalController::confirm() {
    if(!m_pending){emit failed(QStringLiteral("没有待确认提案，请重新生成或导入。"));return;}
    const auto proposal=m_proposal;QString error;
    if(!available(&error)||!services::ProposalService::validate(proposal,m_editor.snapshot(),m_project,&error)) {
        invalidate();emit failed(error);return;
    }
    // Consume first so a nested confirm cannot submit twice. Any reentrant
    // project/snapshot/clear action advances the generation before we edit.
    const auto generation=m_generation+1;invalidate();if(generation!=m_generation)return;
    if(!available(&error)||!services::ProposalService::validate(proposal,m_editor.snapshot(),m_project,&error)){emit failed(error);return;}
    m_editor.edit(proposal.entityId,proposal.fieldId,proposal.value);
    if(generation==m_generation&&m_editor.isBusy())emit message(QStringLiteral("提案已确认，正在通过属性编辑器验证与应用。"));
}
void ProposalController::discard() {invalidate();}
}
