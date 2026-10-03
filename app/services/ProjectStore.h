#pragma once
#include "domain/Project.h"

namespace qvw::services {
class ProjectStore {
public:
    static bool create(const QString &templateDir, const QString &destination, const QString &name,
                       domain::Project *project, QString *error);
    static bool load(const QString &manifestPath, domain::Project *project, QString *error);
    // Validates a manifest without opening it; only absent registered media are tolerated.
    static bool inspectMissingAssets(const QString &manifestPath, domain::Project *project,
                                     QVector<domain::Asset> *missing, QString *error);
    static bool save(domain::Project &project, QString *error);
    static bool resolvePath(const domain::Project &project, const QString &relative,
                            QString *absolute, QString *error, bool mustExist = true);
};
}
