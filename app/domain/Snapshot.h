#pragma once

#include "domain/InspectorField.h"

#include <QString>
#include <QVector>
#include <QMetaType>
#include <QMap>

namespace qvw::domain {

struct Clip {
    QString id;
    QString label;
    int startFrame = 0;
    int endFrameExclusive = 0;
    InspectorFields inspector;
    QString authoredId; // Exact upstream authoredId; mutation still uses the full compiled id.
};

struct Track {
    QString id;
    QString label;
    QVector<Clip> clips;
};

struct CanvasSpace {
    int width = 0;
    int height = 0;
    int frameCount = 0;
    double durationSec = 0.0;
    double frameRate = 0.0;

    QString describe() const;
};

/// A compiled projection of the authored source, never an independent model.
///
/// `revision` must be echoed back on every write and re-read afterwards: the
/// server rejects a stale value with 409, so it is carried here rather than
/// cached elsewhere (docs/API_CONTRACT.md §5).
struct Snapshot {
    int revision = -1;
    QString sourcePath;
    QMap<QString, QString> sourceFiles;
    QByteArray sourceFingerprint;
    CanvasSpace space;
    QVector<Track> tracks;
    /// Compiled, local Studio HTML used only to verify the visible preview.
    /// Never include it in the model context or authored-source fingerprint.
    QString previewSrcdoc;

    bool isLoaded() const { return revision >= 0; }
    int writableFieldCount() const;
};

struct PreviewVersion {
    int revision = -1;
    QByteArray sourceFingerprint;
    bool isConfirmed() const { return revision >= 0 && !sourceFingerprint.isEmpty(); }
};

}
Q_DECLARE_METATYPE(qvw::domain::Snapshot)
Q_DECLARE_METATYPE(qvw::domain::PreviewVersion)
