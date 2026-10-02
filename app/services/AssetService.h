#pragma once
#include "domain/Project.h"

namespace qvw::services {
class AssetService {
public:
    // Copies validated image bytes into the project and commits its manifest.
    // On failure neither project nor the optional output asset is modified.
    static bool importImage(domain::Project &project, const QString &source,
                            domain::Asset *asset, QString *error);
};
}
