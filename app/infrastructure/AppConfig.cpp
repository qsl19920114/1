#include "infrastructure/AppConfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>

namespace qvw::infra {
namespace {

constexpr auto kConfigRelativePath = "config/version-lock.json";

// Walks up from `start` looking for config/version-lock.json. The app is run
// both from the build tree (build/app/) and from the repository root, so a fixed
// relative path would only work in one of them.
QString findConfigUpwards(const QString &start) {
    QDir dir(start);
    for (int depth = 0; depth < 6; ++depth) {
        const QString candidate = dir.filePath(QString::fromLatin1(kConfigRelativePath));
        if (QFile::exists(candidate)) return candidate;
        if (!dir.cdUp()) break;
    }
    return QString();
}

QString locateConfig() {
    const QString fromExecutable = findConfigUpwards(QCoreApplication::applicationDirPath());
    if (!fromExecutable.isEmpty()) return fromExecutable;
    return findConfigUpwards(QDir::currentPath());
}

QString defaultLogPath() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(base).filePath(QStringLiteral("workbench.jsonl"));
}

}

ConfigLoadResult loadAppConfig(const QString &configPath) {
    ConfigLoadResult result;

    const QString path = configPath.isEmpty() ? locateConfig() : configPath;
    if (path.isEmpty()) {
        result.error = QStringLiteral("找不到 %1，请从仓库根目录运行，或用 --config 指定。")
                           .arg(QString::fromLatin1(kConfigRelativePath));
        return result;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("无法读取配置 %1：%2").arg(path, file.errorString());
        return result;
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.error = QStringLiteral("配置 %1 不是合法 JSON：%2").arg(path, parseError.errorString());
        return result;
    }

    const QJsonObject hypit = document.object().value(QStringLiteral("hypit")).toObject();
    const QString declaredPath = hypit.value(QStringLiteral("distributionPath")).toString();
    const QString launcher = hypit.value(QStringLiteral("launcher")).toString();
    const QString version = hypit.value(QStringLiteral("version")).toString();

    if (declaredPath.isEmpty() || launcher.isEmpty() || version.isEmpty()) {
        result.error = QStringLiteral("配置 %1 缺少 hypit.distributionPath / launcher / version。").arg(path);
        return result;
    }

    // The lock lives in <repository>/config; its ../hypit points to a sibling
    // checkout of that repository, independent of the caller's working directory.
    QDir repository = QFileInfo(path).absoluteDir();
    repository.cdUp();
    result.config.configFilePath = QFileInfo(path).absoluteFilePath();
    result.config.distributionPath = QDir::cleanPath(repository.absoluteFilePath(declaredPath));
    result.config.launcherPath = QDir::cleanPath(QDir(result.config.distributionPath).absoluteFilePath(launcher));
    result.config.expectedHypitVersion = version;
    result.config.logFilePath = defaultLogPath();
    return result;
}

}
