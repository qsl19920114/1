#pragma once
#include "domain/Project.h"
#include "domain/ExportTask.h"
namespace qvw::services {
struct FrozenExport { QString workspace, run, runtime, stagePath; };
class ExportWorkspace {
public:
    static bool freeze(const domain::Project &,const domain::Snapshot &,FrozenExport *,QString *error);
    static bool validateLocalRuntime(const QString &path,QString *error);
    static bool saveTask(const domain::Project &,const domain::ExportTask &,QString *error);
    static bool loadTask(const domain::Project &,domain::ExportTask *,QString *error);
    static bool validateRecovered(const domain::Project &,const domain::ExportTask &,FrozenExport *,QString *error);
};
}
