#pragma once
#include "domain/AgentPlan.h"
namespace qvw::agent {
class ContextBuilder {
public:
    static QJsonObject build(const domain::Project &,const domain::Snapshot &,
                             const domain::AgentAssets &,const QString &scope,const QString &selectedEntity={});
    static QString prompt(const QString &goal,const QJsonObject &context,const QString &lastResult={});
};
}
