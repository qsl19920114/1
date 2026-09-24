#pragma once

#include "domain/InspectorField.h"

#include <QString>
#include <QVector>
#include <QMetaType>

namespace qvw::domain {

struct Clip {
    QString id;
    QString label;
    int startFrame = 0;
    int endFrameExclusive = 0;
    InspectorFields inspector;
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
    CanvasSpace space;
    QVector<Track> tracks;

    bool isLoaded() const { return revision >= 0; }
    int writableFieldCount() const;
};

}
Q_DECLARE_METATYPE(qvw::domain::Snapshot)
