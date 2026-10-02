#pragma once
#include "controllers/EditorController.h"
#include "services/ProposalProvider.h"
#include <memory>

namespace qvw::controllers {
class ProposalController : public QObject {
    Q_OBJECT
public:
    explicit ProposalController(EditorController &, QObject *parent = nullptr);
    void setProject(const domain::Project &);
    void clearProject();
    void setProvider(std::shared_ptr<services::IProposalProvider>);
    void generateDemo(const QString &);
    void importJson(const QByteArray &);
    void confirm();
    void discard();
    bool pending() const { return m_pending; }
    QString summary() const { return m_summary; }
signals:
    void proposalChanged(const QString &summary, bool pending);
    void message(const QString &);
    void failed(const QString &);
private:
    EditorController &m_editor;
    domain::Project m_project;
    domain::EditProposal m_proposal;
    std::shared_ptr<services::IProposalProvider> m_provider;
    QString m_summary;
    bool m_hasProject = false, m_pending = false;
    quint64 m_generation = 0;
    bool available(QString *error) const;
    void publish(const domain::EditProposal &, quint64 generation);
    void invalidate();
};
}
