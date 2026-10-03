#pragma once
#include "agent/PlanService.h"
namespace qvw::agent {
class ApprovalManager {
public:
    void approve(const domain::AgentPlan &p){m_identity=PlanService::identity(p);}
    bool allows(const domain::AgentPlan &p) const{return !m_identity.isEmpty()&&m_identity==PlanService::identity(p);}
    void clear(){m_identity.clear();}
    QByteArray identity() const{return m_identity;}
private: QByteArray m_identity;
};
}
