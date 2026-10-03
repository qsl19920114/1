#pragma once
#include "domain/Project.h"
#include <QJsonObject>
namespace qvw::agent {
class AgentTaskStore {
public:
    static bool save(const domain::Project &,const QJsonObject &state,QString *error);
    static bool load(const domain::Project &,QJsonObject *state,QString *error);
    static bool append(const domain::Project &,const QJsonObject &event,QString *error);
};
}
