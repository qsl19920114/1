#pragma once
#include "agent/ModelClient.h"
#include "agent/ApprovalManager.h"
#include "controllers/DocumentController.h"
#include "controllers/EditorController.h"
#include "controllers/ExportController.h"
#include "domain/AgentPlan.h"
#include <QTemporaryDir>
#include <QJsonArray>
#include <memory>
#include <optional>
namespace qvw::agent {
class AgentController : public QObject {
    Q_OBJECT
public:
    AgentController(controllers::DocumentController &,controllers::EditorController &,
                    controllers::ExportController &,ModelClient &,QString templates,QObject *parent=nullptr);
    ~AgentController() override;
    bool setImages(const QStringList &paths);
    void setScope(const QString &entityId);
    void setSelection(const QString &entityId);
    void generate(const QString &goal);
    void updatePlan(const QJsonObject &content);
    void approve(const QString &destination={});
    void stop();
    void retry();
    void repair();
    void undoLast();
    void restore();
    void startExport(const QString &destination);
    void backendFailed(const QString &error);
    const domain::AgentStatus &status() const {return m_status;}
    domain::AgentAssets assets() const {return m_assets;}
    std::optional<domain::AgentPlan> plan() const {return m_plan;}
signals:
    void statusChanged(const qvw::domain::AgentStatus &status);
    void planReady(const qvw::domain::AgentPlan &plan);
    void assetsChanged(const qvw::domain::AgentAssets &assets);
    void failed(const QString &message);
    void message(const QString &message);
private:
    void publish(const QString &phase,const QString &message);
    void persist();
    void record(const QString &tool,const QString &outcome);
    void executeNext();
    void projectLoaded(const domain::Project &);
    void snapshotChanged(const domain::Snapshot &);
    void operationDone();
    void operationFailed(const QString &);
    void settleSnapshot();
    void clearExecution();
    void materializeCreation();
    domain::AgentBaseline currentBase() const;
    bool sameBase(const domain::AgentBaseline &) const;
    controllers::DocumentController &m_document;
    controllers::EditorController &m_editor;
    controllers::ExportController &m_exporter;
    ModelClient &m_model;
    QString m_templates,m_scope,m_selectedEntity,m_expectedRoot,m_lastResult;
    domain::AgentStatus m_status;
    domain::AgentAssets m_assets;
    std::unique_ptr<QTemporaryDir> m_staging;
    domain::Project m_stagingProject;
    std::optional<domain::AgentPlan> m_plan;
    domain::AgentBaseline m_requestBase,m_executionBase;
    ApprovalManager m_approval;
    QJsonArray m_operations;
    int m_next=0,m_retries=0,m_effectiveDone=0;
    bool m_inflight=false,m_creating=false,m_ownProjectSignal=false,m_stopRequested=false,m_undoing=false,m_canUndo=false,m_settling=false;
    int m_settleReads=0;
    QVariant m_before;
    QTimer m_creationDeadline;
    QTimer m_settlePoll,m_settleDeadline;
};
}
