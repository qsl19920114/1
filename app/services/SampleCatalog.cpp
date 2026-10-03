#include "services/SampleCatalog.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QUrl>
#include <cmath>

namespace qvw::services {
namespace {
constexpr qint64 maxVideoBytes = 64 * 1024 * 1024;
constexpr qint64 maxCatalogBytes = 256 * 1024;
const QStringList known{
    "output/chat-demo.mp4",
    "examples/semantic-composition/.hypit/results/2026-09-20/bld_20260920T071529859Z_F3FD835600/files/file-0001.mp4",
    "examples/semantic-composition/.hypit/results/2026-09-23/bld_20260923T100423557Z_A07A70298A/files/file-0001.mp4"};

QString safeVideoPath(const QString &root, const QString &relative) {
    if (root.isEmpty() || relative.isEmpty() || relative.size() > 4096
        || QDir::isAbsolutePath(relative) || relative.contains('\\') || relative.contains(QChar::Null)) return {};
    auto current = root;
    for (const auto &part : relative.split('/')) {
        if (part.isEmpty() || part == "." || part == "..") return {};
        current = QDir(current).filePath(part);
        if (QFileInfo(current).isSymLink()) return {};
    }
    const QFileInfo info(current);
    const auto canonical = info.canonicalFilePath();
    if (!canonical.startsWith(root + "/") || !info.isFile() || info.size() <= 0 || info.size() > maxVideoBytes) return {};
    return canonical;
}
QString digest(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    return hash.addData(&file) ? QString::fromLatin1(hash.result().toHex()) : QString();
}
void appendUnique(domain::VideoSamples &samples, domain::VideoSample sample) {
    for (auto &existing : samples) {
        if (existing.sha256 != sample.sha256) continue;
        for (const auto &source : sample.sources) if (!existing.sources.contains(source)) existing.sources.append(source);
        return;
    }
    samples.append(std::move(sample));
}
bool boundedString(const QJsonValue &value, int limit) {
    return value.isString() && !value.toString().trimmed().isEmpty()
        && value.toString().size() <= limit && !value.toString().contains(QChar::Null);
}
bool positiveNumber(const QJsonValue &value, double max, bool integer = false) {
    const double n = value.toDouble(-1);
    return value.isDouble() && std::isfinite(n) && n > 0 && n <= max && (!integer || n == std::floor(n));
}
}

domain::VideoSamples SampleCatalog::discover(const QString &distribution, const QString &catalogPath) {
    domain::VideoSamples result;
    QStringList problems;
    if (!catalogPath.isEmpty()) {
        const QFileInfo info(catalogPath);
        QFile file(catalogPath);
        if (!info.isFile() || info.isSymLink() || info.size() > maxCatalogBytes || !file.open(QIODevice::ReadOnly)) {
            problems.append(QStringLiteral("样例目录无法读取、路径不安全或超过256KiB：%1").arg(catalogPath));
        } else {
            const auto bytes = file.read(maxCatalogBytes + 1);
            const auto document = QJsonDocument::fromJson(bytes);
            const auto object = document.object();
            const auto entries = object["samples"].toArray();
            if (file.error() != QFileDevice::NoError || bytes.size() > maxCatalogBytes || !document.isObject()
                || object["schemaVersion"].toDouble() != 1 || !object["samples"].isArray() || entries.size() > 32) {
                problems.append(QStringLiteral("样例目录格式无效（需要schemaVersion 1，最多32条样例）：%1").arg(catalogPath));
            } else {
                const auto root = info.absoluteDir().canonicalPath();
                qint64 inspectedBytes = 0;
                for (const auto &value : entries) {
                    const auto entry = value.toObject();
                    const QString relative = entry["path"].toString();
                    const auto path = safeVideoPath(root, relative);
                    const QUrl url(entry["sourceUrl"].toString());
                    const QString expected = entry["sha256"].toString();
                    const bool valid = value.isObject() && boundedString(entry["name"], 256)
                        && !path.isEmpty() && QRegularExpression("^[a-f0-9]{64}$").match(expected).hasMatch()
                        && positiveNumber(entry["sizeBytes"], maxVideoBytes, true)
                        && QFileInfo(path).size() == entry["sizeBytes"].toDouble()
                        && positiveNumber(entry["durationSeconds"], 86400)
                        && positiveNumber(entry["width"], 16384, true) && positiveNumber(entry["height"], 16384, true)
                        && boundedString(entry["sourceUrl"], 4096) && url.isValid() && url.scheme() == "https" && !url.host().isEmpty()
                        && (!entry.contains("archiveMember") || boundedString(entry["archiveMember"], 4096));
                    if (!valid) {
                        problems.append(QStringLiteral("样例路径或元数据无效：%1").arg(relative));
                        continue;
                    }
                    inspectedBytes += QFileInfo(path).size();
                    if (inspectedBytes > 512 * 1024 * 1024) {
                        problems.append(QStringLiteral("样例总校验数据超过512MiB"));
                        break;
                    }
                    if (digest(path) != expected) {
                        problems.append(QStringLiteral("样例SHA-256不匹配：%1").arg(relative));
                        continue;
                    }
                    QStringList sources{entry["sourceUrl"].toString(), relative};
                    if (entry.contains("archiveMember")) sources.append(entry["archiveMember"].toString());
                    domain::VideoSample sample{entry["name"].toString(),path,expected,sources};
                    sample.durationSeconds = entry["durationSeconds"].toDouble();
                    sample.width = entry["width"].toInt();
                    sample.height = entry["height"].toInt();
                    appendUnique(result, std::move(sample));
                }
            }
        }
    }
    const auto root = QFileInfo(distribution).canonicalFilePath();
    const bool validDistribution = !root.isEmpty() && QFileInfo(root).isDir();
    if (validDistribution) {
        for (const auto &relative : known) {
            const auto path = safeVideoPath(root, relative);
            if (path.isEmpty()) {
                problems.append(QStringLiteral("样例不存在、路径不安全或超过64MiB：%1").arg(relative));
                continue;
            }
            const auto hash = digest(path);
            if (hash.isEmpty()) { problems.append(QStringLiteral("无法校验样例：%1").arg(relative)); continue; }
            appendUnique(result, {QStringLiteral("对话故事 · 本地测试视频"),path,hash,{relative}});
        }
    }
    if (result.isEmpty() && (validDistribution || !catalogPath.isEmpty()))
        result.append({QStringLiteral("本地测试视频尚未就绪"),{}, {},known,false,problems.join("\n")});
    else for (auto &sample : result) sample.error = problems.join("\n");
    return result;
}
}
