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
    const QString resourceConfig = QDir(RuntimePaths::resourceRoot()).filePath(QString::fromLatin1(kConfigRelativePath));
    if (QFileInfo(resourceConfig).exists()) return resourceConfig;
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

    const QFileInfo info(path);
    if (!info.isFile() || info.isSymLink() || info.size() > 64 * 1024) {
        result.error = QStringLiteral("配置必须是普通文件、不能是符号链接，且不能超过 64 KiB：%1").arg(path);
        return result;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("无法读取配置 %1：%2").arg(path, file.errorString());
        return result;
    }

    QJsonParseError parseError{};
    const auto data = file.read(64 * 1024 + 1);
    if (file.error() != QFileDevice::NoError || data.size() > 64 * 1024) {
        result.error = QStringLiteral("配置读取失败或超过 64 KiB：%1").arg(path);
        return result;
    }
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.error = QStringLiteral("配置 %1 不是合法 JSON：%2").arg(path, parseError.errorString());
        return result;
    }

    const auto root = document.object();
    const QJsonObject hypit = root.value(QStringLiteral("hypit")).toObject();
    const QString declaredPath = hypit.value(QStringLiteral("distributionPath")).toString();
    const QString launcher = hypit.value(QStringLiteral("launcher")).toString();
    const QString version = hypit.value(QStringLiteral("version")).toString();

    const auto validString = [](const QJsonValue &value, int limit) {
        return value.isString() && !value.toString().trimmed().isEmpty()
            && value.toString().size() <= limit && !value.toString().contains(QChar::Null);
    };
    if (!root.value("hypit").isObject() || !validString(hypit["distributionPath"], 4096)
        || !validString(hypit["launcher"], 4096) || !validString(hypit["version"], 128)) {
        result.error = QStringLiteral("配置 %1 缺少 hypit.distributionPath / launcher / version。").arg(path);
        return result;
    }

    if (root.contains("tools") && !root["tools"].isObject()) {
        result.error = QStringLiteral("配置 tools 必须是 JSON 对象。");
        return result;
    }
    const auto tools = root["tools"].toObject();
    QStringList explicitPaths;
    for (const auto &name : {QStringLiteral("node"), QStringLiteral("ffmpeg"), QStringLiteral("ffprobe"), QStringLiteral("codex")}) {
        if (!tools.contains(name)) continue;
        const auto value = tools[name];
        const QFileInfo tool(value.toString());
        if (!validString(value, 4096) || !QDir::isAbsolutePath(value.toString()) || !tool.isFile() || !tool.isExecutable()) {
            result.error = QStringLiteral("tools.%1 必须是现有可执行文件的绝对路径。备选工具可省略此字段。").arg(name);
            return result;
        }
        explicitPaths << tool.absoluteFilePath();
    }
    result.config.processEnvironment = RuntimePaths::processEnvironment(explicitPaths);
    const auto resolve = [&](const QString &name) {
        return tools.contains(name) ? QFileInfo(tools[name].toString()).absoluteFilePath() : RuntimePaths::resolveTool(name, result.config.processEnvironment);
    };
    result.config.nodePath = resolve(QStringLiteral("node"));
    result.config.nodeExplicit = tools.contains(QStringLiteral("node"));
    result.config.ffmpegPath = resolve(QStringLiteral("ffmpeg"));
    result.config.ffprobePath = resolve(QStringLiteral("ffprobe"));
    result.config.codexPath = resolve(QStringLiteral("codex"));

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
