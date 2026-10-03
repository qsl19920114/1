#pragma once

#include <QString>
#include "infrastructure/RuntimePaths.h"

namespace qvw::infra {

/// Resolved application settings.
///
/// distributionPath is stored as an absolute path: the value in version-lock.json
/// is relative to the repository root ("../hypit"), but the app may be launched
/// from any working directory, so resolving it late would silently look in the
/// wrong place.
struct AppConfig {
    QString configFilePath;
    QString distributionPath;
    QString launcherPath;
    QString expectedHypitVersion;
    QString logFilePath;
    QString nodePath;
    bool nodeExplicit = false;
    QString ffmpegPath;
    QString ffprobePath;
    QString codexPath;
    QString sampleCatalogPath;
    QProcessEnvironment processEnvironment = RuntimePaths::processEnvironment();
};

struct ConfigLoadResult {
    AppConfig config;
    QString error;

    bool ok() const { return error.isEmpty(); }
};

/// Reads a regular JSON file up to 64 KiB. An empty path first uses executable
/// resources/config/version-lock.json, then the developer upward search.
ConfigLoadResult loadAppConfig(const QString &configPath = QString());

}
