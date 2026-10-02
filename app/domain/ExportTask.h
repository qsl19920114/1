#pragma once
#include "domain/Snapshot.h"
#include <QString>
#include <QMetaType>
namespace qvw::domain {
struct ExportTask {
    QString phase = QStringLiteral("idle");
    QString buildId;
    QString output = QStringLiteral("final.video");
    QString destination;
    QString workspace;
    QString error;
    QString hypitVersion;
    int revision = -1;
    QByteArray sourceFingerprint;
    CanvasSpace space;
    bool active = false;
};
}
Q_DECLARE_METATYPE(qvw::domain::ExportTask)
