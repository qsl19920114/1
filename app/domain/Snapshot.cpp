#include "domain/Snapshot.h"

namespace qvw::domain {

QString CanvasSpace::describe() const {
    if (width <= 0 || height <= 0) return QStringLiteral("未加载");
    return QStringLiteral("%1x%2 · %3fps · %4s")
        .arg(width)
        .arg(height)
        .arg(frameRate, 0, 'g', 4)
        .arg(durationSec, 0, 'f', 2);
}

int Snapshot::writableFieldCount() const {
    int total = 0;
    for (const Track &track : tracks) {
        for (const Clip &clip : track.clips) {
            for (const InspectorField &field : clip.inspector) {
                if (field.isEditable()) ++total;
            }
        }
    }
    return total;
}

}
