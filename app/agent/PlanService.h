#pragma once
#include "domain/AgentPlan.h"
namespace qvw::agent {
class PlanService {
public:
    static QJsonObject schema();
    static bool parse(const QJsonObject &content, const domain::AgentBaseline &base,
                      domain::AgentPlan *out, QString *error);
    static bool validate(const domain::AgentPlan &, const domain::Project &,
                         const domain::Snapshot &, const domain::AgentAssets &, QString *error);
    static QByteArray identity(const domain::AgentPlan &);
};
}
