#pragma once
#include "domain/AgentPlan.h"
#include <QWidget>
class QLabel;
class QPushButton;
class QScrollArea;

namespace qvw::ui {
class PlanReviewPanel : public QWidget {
    Q_OBJECT
public:
    explicit PlanReviewPanel(QWidget *parent = nullptr);
    void showPlan(const domain::AgentPlan &, const domain::Snapshot &);
    void clearPlan();
    void showAssets(const domain::AgentAssets &);
    void setEditingEnabled(bool);
    void setApprovalEnabled(bool);
    QJsonObject editedPlan() const;
signals:
    void approveRequested(const QJsonObject &editedPlan, const QString &destination);
private:
    void rebuild();
    void updateScene(int index, const QString &key, const QJsonValue &value);
    void updateOperation(int index, const QJsonValue &value);
    void moveScene(int index, int offset);
    void updateValidation();
    void updateOverview();
    void approve();
    QString validationError() const;
    const domain::InspectorField *fieldFor(const QJsonObject &operation) const;
    QJsonObject m_content;
    domain::Snapshot m_snapshot;
    domain::AgentAssets m_assets;
    QLabel *m_summary = nullptr;
    QLabel *m_changes = nullptr;
    QScrollArea *m_overview = nullptr;
    QPushButton *m_modify = nullptr;
    bool m_modifying = false;
    QLabel *m_question = nullptr;
    QLabel *m_totalDuration = nullptr;
    QLabel *m_validation = nullptr;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_body = nullptr;
    QPushButton *m_approve = nullptr;
    bool m_editingEnabled = true;
    bool m_approvalEnabled = false;
};
}
