#include "services/AssetService.h"
#include "services/ProjectStore.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QtEndian>
#include <QTemporaryFile>
#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace qvw::services {
namespace {
constexpr qint64 maxPixels = 40'000'000;
constexpr qint64 hardByteLimit = 64 * 1024 * 1024;
bool fail(QString *error, const QString &message) { if (error) *error = message; return false; }
bool completePng(const QByteArray &bytes) {
    // Some decoders tolerate a missing IEND. Reject incomplete containers before decoding.
    qsizetype offset = 8;
    while (offset <= bytes.size() - 12) {
        const auto length = qFromBigEndian<quint32>(bytes.constData() + offset);
        if (length > static_cast<quint64>(bytes.size() - offset - 12)) return false;
        if (bytes.mid(offset + 4, 4) == "IEND") return length == 0;
        offset += static_cast<qsizetype>(length) + 12;
    }
    return false;
}
bool matches(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() != bytes.size()) return false;
    // Exact comparison also rules out a hypothetical hash collision and bounds memory use.
    const auto stored = file.read(bytes.size() + 1);
    return file.error() == QFileDevice::NoError && stored == bytes;
}
}

namespace {
bool sameAsset(const domain::Asset &a, const domain::Asset &b) {
    return a.hash == b.hash && a.path == b.path && a.originalName == b.originalName
        && a.mime == b.mime && a.width == b.width && a.height == b.height && a.size == b.size;
}
// Reject links at every user-controlled component, including dangling links and linked parents.
bool plainPath(const QString &path, bool allowMissing, QString *error) {
    if (path.contains(QChar::Null) || path.split('/').contains(".."))
        return fail(error, QStringLiteral("素材路径不能包含上级目录跳转"));
    const auto absolute = QFileInfo(path).absoluteFilePath();
    QString cursor = QDir::rootPath();
    const auto parts = absolute.split('/', Qt::SkipEmptyParts);
    for (qsizetype i = 0; i < parts.size(); ++i) {
        cursor = QDir(cursor).filePath(parts[i]);
        const QFileInfo info(cursor);
        bool systemAlias = false;
#ifdef Q_OS_MACOS
        systemAlias = (cursor == "/var" && info.canonicalFilePath() == "/private/var")
                   || (cursor == "/tmp" && info.canonicalFilePath() == "/private/tmp");
#endif
        if (info.isSymLink() && !systemAlias)
            return fail(error, QStringLiteral("素材路径不能含符号链接：%1").arg(cursor));
        if (!info.exists()) {
            if (allowMissing) continue;
            return fail(error, QStringLiteral("素材文件不存在：%1").arg(cursor));
        }
        if ((i + 1 < parts.size() && !info.isDir())
            || (i + 1 == parts.size() && !info.isFile() && !info.isDir()))
            return fail(error, QStringLiteral("素材路径包含特殊文件或无效目录：%1").arg(cursor));
    }
    return true;
}
}

bool AssetService::restoreMissing(const domain::Project &project, const domain::Asset &asset,
                                 const QString &source, QString *error) {
    bool registered = false;
    for (const auto &entry : project.assets) if (sameAsset(entry, asset)) { registered = true; break; }
    if (!registered) return fail(error, QStringLiteral("只能恢复工程已登记的原始素材"));
    if (asset.size <= 0 || asset.size > qMin(project.maxAssetBytes, hardByteLimit))
        return fail(error, QStringLiteral("素材大小无效或超过大小上限"));
    QString destination;
    if (!plainPath(project.rootPath, false, error)
        || !ProjectStore::resolvePath(project, asset.path, &destination, error, false)
        || !plainPath(destination, true, error)) return false;
    if (QFileInfo::exists(destination) || QFileInfo(destination).isSymLink())
        return fail(error, QStringLiteral("素材目标已存在，不能覆盖"));
    if (!plainPath(source, false, error) || !QFileInfo(source).isFile())
        return fail(error, QStringLiteral("恢复来源必须是不含符号链接的普通文件"));
    QFile input(source);
    if (!input.open(QIODevice::ReadOnly) || input.size() != asset.size)
        return fail(error, QStringLiteral("恢复文件大小与原素材不一致"));
    const auto bytes = input.read(asset.size + 1);
    if (input.error() != QFileDevice::NoError || bytes.size() != asset.size
        || QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) != asset.hash)
        return fail(error, QStringLiteral("恢复文件内容与原素材哈希不一致"));
    const auto directory = QFileInfo(destination).absolutePath();
    if (!QDir().mkpath(directory) || !plainPath(destination, true, error))
        return fail(error, QStringLiteral("无法安全创建素材目录"));
    QTemporaryFile temporary(QDir(directory).filePath(".qvw-restore-XXXXXX"));
    if (!temporary.open() || temporary.write(bytes) != bytes.size() || !temporary.flush())
        return fail(error, QStringLiteral("无法写入恢复素材暂存文件"));
    const auto staging = temporary.fileName();
    temporary.close();
    if (!matches(staging, bytes) || !plainPath(destination, true, error))
        return fail(error, QStringLiteral("恢复素材校验失败"));
#ifdef Q_OS_UNIX
    // link publishes the complete file atomically and fails if any destination entry exists.
    if (::link(QFile::encodeName(staging).constData(), QFile::encodeName(destination).constData()) != 0)
#else
    if (!QFile::rename(staging, destination))
#endif
        return fail(error, QStringLiteral("无法恢复素材；目标已存在或目录不可写"));
    if (error) error->clear();
    return true;
}

bool AssetService::importImage(domain::Project &project, const QString &source, domain::Asset *asset, QString *error) {
    if (project.maxAssetBytes <= 0) return fail(error, QStringLiteral("素材大小上限无效"));
    const auto limit = qMin(project.maxAssetBytes, hardByteLimit);
    if (!QFileInfo(source).isFile()) return fail(error, QStringLiteral("素材必须是可读取的普通文件"));
    QFile input(source);
    if (!input.open(QIODevice::ReadOnly)) return fail(error, QStringLiteral("无法读取素材：%1").arg(input.errorString()));
    if (input.size() <= 0 || input.size() > limit) return fail(error, QStringLiteral("素材为空或超过大小上限（%1 字节）").arg(limit));
    const auto bytes = input.read(limit + 1);
    if (input.error() != QFileDevice::NoError) return fail(error, QStringLiteral("读取素材失败：%1").arg(input.errorString()));
    if (bytes.isEmpty() || bytes.size() > limit) return fail(error, QStringLiteral("素材为空或超过大小上限（%1 字节）").arg(limit));

    QBuffer buffer; buffer.setData(bytes); buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer); reader.setDecideFormatFromContent(true);
    const auto format = reader.format().toLower();
    if (format != "png" && format != "jpeg" && format != "jpg")
        return fail(error, QStringLiteral("仅支持内容有效的 PNG 或 JPEG 图片"));
    if ((format == "png" && !completePng(bytes))
        || (format != "png" && bytes.lastIndexOf(QByteArray::fromHex("ffd9")) < 2))
        return fail(error, QStringLiteral("图片数据不完整"));
    const auto size = reader.size();
    if (!size.isValid() || size.isEmpty() || qint64(size.width()) * size.height() > maxPixels)
        return fail(error, QStringLiteral("图片尺寸无效或超过 4000 万像素上限"));
    const auto decoded = reader.read();
    if (decoded.isNull() || decoded.size() != size)
        return fail(error, QStringLiteral("图片解码失败：%1").arg(reader.errorString()));

    domain::Asset imported;
    imported.hash = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    imported.path = QStringLiteral("assets/%1.%2").arg(imported.hash, format == "png" ? "png" : "jpg");
    imported.originalName = QFileInfo(source).fileName();
    imported.mime = format == "png" ? QStringLiteral("image/png") : QStringLiteral("image/jpeg");
    imported.width = size.width(); imported.height = size.height(); imported.size = bytes.size();

    QString destination;
    if (!ProjectStore::resolvePath(project, imported.path, &destination, error, false)) return false;
    for (const auto &existing : project.assets) {
        if (existing.hash != imported.hash) continue;
        QString existingPath;
        if (!ProjectStore::resolvePath(project, existing.path, &existingPath, error)) return false;
        if (!matches(existingPath, bytes)) return fail(error, QStringLiteral("已登记素材内容与哈希不符，已保留原文件"));
        if (existing.mime != imported.mime || existing.width != imported.width || existing.height != imported.height || existing.size != imported.size)
            return fail(error, QStringLiteral("已登记素材元数据与实际图片不符"));
        if (asset) *asset = existing;
        if (error) error->clear();
        return true;
    }

    const auto assetsDirectory = QFileInfo(destination).absolutePath();
    if (!QDir().mkpath(assetsDirectory)) return fail(error, QStringLiteral("无法创建工程素材目录"));
    // Recheck after directory creation, including existing/broken symlinks.
    if (!ProjectStore::resolvePath(project, imported.path, &destination, error, false)) return false;
    bool created = false;
    if (QFileInfo::exists(destination)) {
        if (!matches(destination, bytes)) return fail(error, QStringLiteral("目标素材已存在且内容不符，已保留原文件"));
    } else {
        QFile output(destination);
        // NewOnly prevents replacing another file, including one created since our existence check.
        if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly))
            return fail(error, QStringLiteral("无法创建素材副本：%1").arg(output.errorString()));
        created = true;
        if (output.write(bytes) != bytes.size() || !output.flush()) {
            const auto message = output.errorString(); output.close(); QFile::remove(destination);
            return fail(error, QStringLiteral("写入素材副本失败：%1").arg(message));
        }
        output.close();
    }
    auto candidate = project; candidate.assets.push_back(imported);
    if (!ProjectStore::save(candidate, error)) {
        if (created) QFile::remove(destination);
        return false;
    }
    project = candidate;
    if (asset) *asset = imported;
    if (error) error->clear();
    return true;
}
}
