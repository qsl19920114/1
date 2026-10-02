#pragma once
#include <QDir>
#include <QString>
#include <QVector>

namespace qvw::domain {
struct Asset {
    QString hash;
    QString path;
    QString originalName;
    QString mime;
    int width = 0;
    int height = 0;
    qint64 size = 0;
};
// Native metadata only; authored source remains the timeline authority.
struct Project {
    QString rootPath;
    QString name;
    QString templateId;
    QString templateVersion;
    QString sourcePath;
    QString runPath;
    QString runtimePath;
    QVector<Asset> assets;
    qint64 maxAssetBytes = 64 * 1024 * 1024;
    QString manifestPath() const { return QDir(rootPath).filePath(QStringLiteral("workbench.qvw.json")); }
};
}
