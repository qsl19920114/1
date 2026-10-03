#pragma once
#include "domain/AgentPlan.h"
#include "domain/ExportTask.h"
#include <QWidget>
class QCheckBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

namespace qvw::ui {
class PlanReviewPanel;
class AgentPanel : public QWidget {
    Q_OBJECT
public:
    explicit AgentPanel(QWidget *parent = nullptr);
    void showPlan(const domain::AgentPlan &, const domain::Snapshot &);
    void showStatus(const domain::AgentStatus &);
    void showAssets(const domain::AgentAssets &);
    void setSelection(const QString &entityId, const QString &label);
    void setAvailable(bool);
    void setVersions(const domain::Snapshot &, const domain::ExportTask &);
    void setPreviewVersion(const domain::PreviewVersion &);
signals:
    void generateRequested(const QString &goal);
    void imagesRequested(const QStringList &paths);
    void selectionChanged(const QString &entityId);
    void scopeChanged(const QString &entityId);
    void approveRequested(const QJsonObject &editedPlan, const QString &destination);
    void stopRequested();
    void retryRequested();
    void repairRequested();
    void undoRequested();
    void restoreRequested();
private:
    void updateActions();
    void updateVersions();
    domain::AgentStatus m_status;
    domain::Snapshot m_snapshot;
    domain::PreviewVersion m_previewVersion;
    domain::ExportTask m_export;
    QString m_selectedEntity;
    bool m_available = false;
    QPlainTextEdit *m_goal = nullptr;
    QLabel *m_selection = nullptr;
    QLabel *m_taskScope = nullptr;
    QCheckBox *m_scope = nullptr;
    QPushButton *m_images = nullptr;
    QListWidget *m_assets = nullptr;
    QLabel *m_assetSummary = nullptr;
    QPushButton *m_generate = nullptr;
    QLabel *m_phase = nullptr;
    QLabel *m_progress = nullptr;
    QLabel *m_message = nullptr;
    QLabel *m_versions = nullptr;
    QPlainTextEdit *m_events = nullptr;
    PlanReviewPanel *m_review = nullptr;
    QPushButton *m_stop = nullptr;
    QPushButton *m_retry = nullptr;
    QPushButton *m_repair = nullptr;
    QPushButton *m_undo = nullptr;
    QPushButton *m_restore = nullptr;
};
}
