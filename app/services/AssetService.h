#pragma once
#include "domain/Project.h"

namespace qvw::services {
class AssetService {
public:
    // Restores only the exact registered bytes; never changes the manifest or replaces a file.
    static bool restoreMissing(const domain::Project &project, const domain::Asset &asset,
                               const QString &source, QString *error);
    // Copies validated image bytes into the project and commits its manifest.
    // On failure neither project nor the optional output asset is modified.
    static bool importImage(domain::Project &project, const QString &source,
                            domain::Asset *asset, QString *error);
};
}
