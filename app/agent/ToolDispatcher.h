#pragma once
#include "domain/AgentPlan.h"
#include <QJsonArray>
#include <QMap>
namespace qvw::agent {
class ToolDispatcher {
public:
    static QJsonArray creationEdits(const domain::AgentPlan &,const domain::Snapshot &,const QMap<QString,domain::Asset> &,QString *error);
    static const domain::InspectorField *field(const domain::Snapshot &,const QString &entity,const QString &fieldId);
};
}
