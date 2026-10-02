#include "services/AssetService.h"
#include "services/ProjectStore.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QtEndian>

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
