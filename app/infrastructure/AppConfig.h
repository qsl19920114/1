#pragma once

#include <QString>

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
};

struct ConfigLoadResult {
    AppConfig config;
    QString error;

    bool ok() const { return error.isEmpty(); }
};

/// Reads config/version-lock.json. `configPath` empty means "search upward from
/// the executable and the current directory" so the app works both from the
/// build tree and from an installed location.
ConfigLoadResult loadAppConfig(const QString &configPath = QString());

}
