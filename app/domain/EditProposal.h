#pragma once
#include <QByteArray>
#include <QString>
#include <QVariant>

namespace qvw::domain {
enum class ProposalOrigin { Demo, ExternalUnverified };
struct EditProposal {
    int revision = -1;
    QByteArray sourceFingerprint; // Raw SHA256; JSON uses 64 lowercase hex characters.
    QString entityId;
    QString fieldId;
    QVariant value;
    ProposalOrigin origin = ProposalOrigin::ExternalUnverified;
    QByteArray toJson() const;
};
QString proposalOriginLabel(ProposalOrigin origin);
}
