#include "domain/EditProposal.h"
#include <QJsonDocument>
#include <QJsonObject>
namespace qvw::domain {
QByteArray EditProposal::toJson() const {
    return QJsonDocument(QJsonObject{{"format","qvw.edit-proposal@1"},{"revision",revision},
        {"sourceFingerprint",QString::fromLatin1(sourceFingerprint.toHex())},{"entityId",entityId},
        {"fieldId",fieldId},{"value",QJsonValue::fromVariant(value)},
        {"origin",origin==ProposalOrigin::Demo?"demo":"external-unverified"}}).toJson(QJsonDocument::Compact);
}
QString proposalOriginLabel(ProposalOrigin origin) {
    return origin==ProposalOrigin::Demo?QStringLiteral("本地模拟（未接入真实模型）"):
        QStringLiteral("外部 JSON，来源未核验");
}
}
