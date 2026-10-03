#include "services/ProjectStore.h"
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <cmath>
#include <limits>

namespace qvw::services {
namespace {
constexpr auto projectFormat = "qt-video-workbench.project@1";
constexpr auto templateFormat = "qt-video-workbench.template@1";
constexpr qint64 maxJsonBytes = 4 * 1024 * 1024;
constexpr double maxSafeInteger = 9007199254740991.0;
bool fail(QString *error, const QString &message) { if (error) *error = message; return false; }
bool readObject(const QString &path, QJsonObject *object, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, QStringLiteral("无法读取 %1：%2").arg(path, file.errorString()));
    if (file.size() > maxJsonBytes) return fail(error, QStringLiteral("元数据文件超过 4 MiB：%1").arg(path));
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return fail(error, QStringLiteral("元数据必须是有效 JSON 对象：%1").arg(path));
    *object = doc.object(); return true;
}
bool stringField(const QJsonObject &object, const char *key, QString *value, QString *error) {
    const auto field = object.value(QLatin1String(key));
    if (!field.isString() || field.toString().trimmed().isEmpty())
        return fail(error, QStringLiteral("缺少有效字符串字段：%1").arg(QLatin1String(key)));
    *value = field.toString(); return true;
}
bool integerField(const QJsonObject &object, const char *key, qint64 min, qint64 max, qint64 *value, QString *error) {
    const auto field = object.value(QLatin1String(key)); const auto number = field.toDouble(-1);
    if (!field.isDouble() || !std::isfinite(number) || std::floor(number) != number || number < min || number > max)
        return fail(error, QStringLiteral("无效整数字段：%1").arg(QLatin1String(key)));
    *value = static_cast<qint64>(number); return true;
}
bool contained(const QString &root, const QString &path) { return path == root || path.startsWith(root + '/'); }
bool validate(const domain::Project &project, QString *error, QVector<domain::Asset> *missing = nullptr) {
    if (project.name.trimmed().isEmpty() || project.templateId.trimmed().isEmpty() || project.templateVersion.trimmed().isEmpty())
        return fail(error, QStringLiteral("工程名称、模板标识和模板版本不能为空"));
    if (project.maxAssetBytes <= 0 || project.maxAssetBytes > maxSafeInteger)
        return fail(error, QStringLiteral("素材大小上限无效"));
    for (const auto &path : {project.sourcePath, project.runPath, project.runtimePath})
        if (!ProjectStore::resolvePath(project, path, nullptr, error)) return false;
    QSet<QString> hashes, paths;
    static const QRegularExpression hashPattern(QStringLiteral("^[0-9a-f]{64}$"));
    for (const auto &asset : project.assets) {
        if (!hashPattern.match(asset.hash).hasMatch() || asset.originalName.trimmed().isEmpty() || asset.mime.trimmed().isEmpty()
            || asset.width <= 0 || asset.height <= 0 || asset.size < 0 || asset.size > maxSafeInteger
            || hashes.contains(asset.hash) || paths.contains(asset.path))
            return fail(error, QStringLiteral("素材元数据无效或重复：%1").arg(asset.originalName));
        QString absolute;
        if (!ProjectStore::resolvePath(project, asset.path, &absolute, error, missing == nullptr)) return false;
        if (missing) {
            const QFileInfo info(absolute);
            if (!info.exists() && !info.isSymLink()) missing->push_back(asset);
            else if (!info.isFile()) return fail(error, QStringLiteral("素材必须是普通文件：%1").arg(asset.path));
        }
        hashes.insert(asset.hash); paths.insert(asset.path);
    }
    return true;
}
QJsonObject serialize(const domain::Project &project) {
    QJsonArray assets;
    for (const auto &asset : project.assets)
        assets.append(QJsonObject{{"hash", asset.hash}, {"path", asset.path}, {"originalName", asset.originalName},
                                  {"mime", asset.mime}, {"width", asset.width}, {"height", asset.height}, {"size", asset.size}});
    return {{"format", projectFormat}, {"name", project.name}, {"templateId", project.templateId},
            {"templateVersion", project.templateVersion}, {"source", project.sourcePath}, {"run", project.runPath},
            {"runtime", project.runtimePath}, {"assets", assets}, {"settings", QJsonObject{{"maxAssetBytes", project.maxAssetBytes}}}};
}
}

bool ProjectStore::resolvePath(const domain::Project &project, const QString &relative,
                               QString *absolute, QString *error, bool mustExist) {
    const auto rootInfo = QFileInfo(project.rootPath);
    const auto root = rootInfo.canonicalFilePath();
    if (!rootInfo.isDir() || root.isEmpty() || !QDir::isAbsolutePath(project.rootPath))
        return fail(error, QStringLiteral("工程根目录无效"));
    if (relative.isEmpty() || QDir::isAbsolutePath(relative) || relative.contains('\\') || relative.contains(':') || relative.contains(QChar::Null))
        return fail(error, QStringLiteral("工程路径必须是安全的相对路径：%1").arg(relative));
    const auto components = relative.split('/');
    auto current = root;
    for (qsizetype i = 0; i < components.size(); ++i) {
        const auto &component = components[i];
        if (component.isEmpty() || component == "." || component == "..")
            return fail(error, QStringLiteral("工程路径不能包含空段、. 或 ..：%1").arg(relative));
        current = QDir(current).filePath(component);
        const QFileInfo info(current);
        if (info.exists() || info.isSymLink()) {
            if (i + 1 < components.size() && !info.isDir())
                return fail(error, QStringLiteral("工程路径父级必须是目录：%1").arg(relative));
            const auto canonical = info.canonicalFilePath();
            if (canonical.isEmpty() || !contained(root, canonical))
                return fail(error, QStringLiteral("工程路径越过根目录或符号链接无效：%1").arg(relative));
        }
    }
    if (mustExist && !QFileInfo(current).isFile()) return fail(error, QStringLiteral("工程文件不存在：%1").arg(relative));
    if (absolute) *absolute = current;
    if (error) error->clear();
    return true;
}

namespace {
bool readProject(const QString &manifestPath, domain::Project *project, QVector<domain::Asset> *missing, QString *error) {
    if (!project) return fail(error, QStringLiteral("工程输出参数为空"));
    const QFileInfo manifest(manifestPath);
    if (manifest.fileName() != "workbench.qvw.json") return fail(error, QStringLiteral("工程清单必须命名为 workbench.qvw.json"));
    if (manifest.isSymLink()) return fail(error, QStringLiteral("工程清单不能是符号链接"));
    QJsonObject object;
    if (!readObject(manifestPath, &object, error)) return false;
    if (object.value("format") != projectFormat) return fail(error, QStringLiteral("不支持的工程格式或版本"));
    domain::Project parsed; parsed.rootPath = manifest.absoluteDir().canonicalPath();
    if (!stringField(object, "name", &parsed.name, error)
        || !stringField(object, "templateId", &parsed.templateId, error)
        || !stringField(object, "templateVersion", &parsed.templateVersion, error)
        || !stringField(object, "source", &parsed.sourcePath, error)
        || !stringField(object, "run", &parsed.runPath, error)
        || !stringField(object, "runtime", &parsed.runtimePath, error)) return false;
    if (!object.value("settings").isObject() || !object.value("assets").isArray())
        return fail(error, QStringLiteral("工程缺少 settings 对象或 assets 数组"));
    if (!integerField(object.value("settings").toObject(), "maxAssetBytes", 1, static_cast<qint64>(maxSafeInteger), &parsed.maxAssetBytes, error)) return false;
    for (const auto &value : object.value("assets").toArray()) {
        if (!value.isObject()) return fail(error, QStringLiteral("素材条目必须是对象"));
        const auto item = value.toObject(); domain::Asset asset; qint64 width = 0, height = 0;
        if (!stringField(item, "hash", &asset.hash, error) || !stringField(item, "path", &asset.path, error)
            || !stringField(item, "originalName", &asset.originalName, error) || !stringField(item, "mime", &asset.mime, error)
            || !integerField(item, "width", 1, std::numeric_limits<int>::max(), &width, error)
            || !integerField(item, "height", 1, std::numeric_limits<int>::max(), &height, error)
            || !integerField(item, "size", 0, static_cast<qint64>(maxSafeInteger), &asset.size, error)) return false;
        asset.width = static_cast<int>(width); asset.height = static_cast<int>(height); parsed.assets.push_back(asset);
    }
    QVector<domain::Asset> absent;
    if (!validate(parsed, error, missing ? &absent : nullptr)) return false;
    *project = parsed;
    if (missing) *missing = absent;
    if (error) error->clear(); return true;
}
}

bool ProjectStore::load(const QString &manifestPath, domain::Project *project, QString *error) {
    return readProject(manifestPath, project, nullptr, error);
}
bool ProjectStore::inspectMissingAssets(const QString &manifestPath, domain::Project *project,
                                      QVector<domain::Asset> *missing, QString *error) {
    if (!missing) return fail(error, QStringLiteral("缺失素材输出参数为空"));
    return readProject(manifestPath, project, missing, error);
}

bool ProjectStore::save(domain::Project &project, QString *error) {
    if (!validate(project, error)) return false;
    const auto path = project.manifestPath(); const QFileInfo info(path);
    if (info.isSymLink()) return fail(error, QStringLiteral("工程清单不能是符号链接"));
    if (info.exists()) {
        QJsonObject previous;
        if (!readObject(path, &previous, error)) return false;
        if (previous.value("format") != projectFormat) return fail(error, QStringLiteral("不能覆盖未知版本的工程清单"));
    }
    const auto bytes = QJsonDocument(serialize(project)).toJson(QJsonDocument::Indented);
    if (bytes.size() > maxJsonBytes) return fail(error, QStringLiteral("工程元数据超过 4 MiB"));
    QSaveFile output(path); output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit())
        return fail(error, QStringLiteral("原子保存工程失败：%1").arg(output.errorString()));
    if (error) error->clear(); return true;
}

bool ProjectStore::create(const QString &templateDir, const QString &destination, const QString &name,
                          domain::Project *project, QString *error) {
    if (!project) return fail(error, QStringLiteral("工程输出参数为空"));
    const QFileInfo templateInfo(templateDir); const auto templateRoot = templateInfo.canonicalFilePath();
    if (!templateInfo.isDir() || templateInfo.isSymLink()) return fail(error, QStringLiteral("模板目录无效"));
    QJsonObject metadata;
    if (!readObject(QDir(templateRoot).filePath("template.json"), &metadata, error)) return false;
    if (metadata.value("format") != templateFormat) return fail(error, QStringLiteral("不支持的模板格式或版本"));
    domain::Project candidate; candidate.name = name; candidate.rootPath = templateRoot;
    if (!stringField(metadata, "id", &candidate.templateId, error)
        || !stringField(metadata, "version", &candidate.templateVersion, error)
        || !stringField(metadata, "source", &candidate.sourcePath, error)
        || !stringField(metadata, "run", &candidate.runPath, error)
        || !stringField(metadata, "runtime", &candidate.runtimePath, error)) return false;
    QString title; if (!stringField(metadata, "title", &title, error) || !validate(candidate, error)) return false;
    const auto target = QFileInfo(destination).absoluteFilePath(); const QFileInfo targetInfo(target);
    if (targetInfo.isSymLink() || (targetInfo.exists() && (!targetInfo.isDir()
        || !QDir(target).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot).isEmpty())))
        return fail(error, QStringLiteral("目标必须是新目录或空目录"));
    // Inventory before creating anything, and refuse links even if they point inside the template.
    QStringList files, directories;
    QDirIterator iterator(templateRoot, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next(); const auto info = iterator.fileInfo(); const auto relative = QDir(templateRoot).relativeFilePath(info.filePath());
        if (info.isSymLink() || (!info.isFile() && !info.isDir())) return fail(error, QStringLiteral("模板不能包含符号链接或特殊文件：%1").arg(relative));
        if (relative == "workbench.qvw.json") return fail(error, QStringLiteral("模板不能携带现有工程清单"));
        if (info.isDir()) directories.push_back(relative);
        else if (relative != "template.json") files.push_back(relative);
    }
    // A descendant destination would mutate the source template itself.
    auto ancestorPath = QFileInfo(target).absolutePath();
    while (!QFileInfo::exists(ancestorPath)) {
        const auto parent = QFileInfo(ancestorPath).absolutePath();
        if (parent == ancestorPath) break;
        ancestorPath = parent;
    }
    const QDir ancestor(ancestorPath);
    const auto actualTarget = QDir(ancestor.canonicalPath()).filePath(QDir(ancestor.absolutePath()).relativeFilePath(target));
    if (contained(templateRoot, QDir::cleanPath(actualTarget))) return fail(error, QStringLiteral("工程目录不能位于模板目录内"));
    QStringList createdFiles, createdDirs;
    const auto rollback = [&] { for (auto it = createdFiles.crbegin(); it != createdFiles.crend(); ++it) QFile::remove(*it);
                              for (auto it = createdDirs.crbegin(); it != createdDirs.crend(); ++it) QDir().rmdir(*it); };
    const auto makeDirectories = [&](const QString &path) {
        QStringList missing; auto cursor = QDir::cleanPath(path);
        while (!QFileInfo::exists(cursor)) { missing.prepend(cursor); const auto parent = QFileInfo(cursor).absolutePath(); if (parent == cursor) break; cursor = parent; }
        for (const auto &directory : missing) { if (!QDir().mkdir(directory)) return false; createdDirs.push_back(directory); }
        return QFileInfo(path).isDir();
    };
    if (!makeDirectories(target)) { rollback(); return fail(error, QStringLiteral("无法创建工程目录")); }
    for (const auto &relative : directories) {
        if (!makeDirectories(QDir(target).filePath(relative))) { rollback(); return fail(error, QStringLiteral("无法创建模板子目录")); }
    }
    for (const auto &relative : files) {
        const auto destinationFile = QDir(target).filePath(relative);
        if (!QFile::copy(QDir(templateRoot).filePath(relative), destinationFile)) { rollback(); return fail(error, QStringLiteral("无法复制模板文件：%1").arg(relative)); }
        createdFiles.push_back(destinationFile);
    }
    candidate.rootPath = QFileInfo(target).canonicalFilePath();
    if (!save(candidate, error)) { rollback(); return false; }
    *project = candidate; if (error) error->clear(); return true;
}
}
