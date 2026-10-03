#pragma once
#include "domain/Project.h"
#include "domain/Snapshot.h"
#include <QJsonObject>
#include <QMetaType>

namespace qvw::domain {
struct AgentAsset {
    Asset asset;
    QString localPath; // Qt-only; never put an absolute path or pixels in the model prompt.
};
using AgentAssets = QVector<AgentAsset>;
struct AgentBaseline {
    QString root;
    int revision = -1;
    QByteArray fingerprint;
    QString scope;
};
struct AgentPlan {
    QJsonObject content;
    AgentBaseline base;
    QString kind() const { return content.value("kind").toString(); }
};
struct AgentStatus {
    QString taskId;
    QString phase = "idle";
    QString message;
    QString goal;
    QString scope;
    int completed = 0;
    int total = 0;
    int revision = -1;
    QStringList events;
    bool canApprove = false;
    bool canRetry = false;
    bool canUndo = false;
};
}
Q_DECLARE_METATYPE(qvw::domain::AgentPlan)
Q_DECLARE_METATYPE(qvw::domain::AgentStatus)
Q_DECLARE_METATYPE(qvw::domain::AgentAssets)
