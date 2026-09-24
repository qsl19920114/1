#include "backend/hypit/SnapshotMapper.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace qvw::backend::hypit {
namespace {

using domain::Clip;
using domain::ControlKind;
using domain::InspectorField;
using domain::Track;

QString stringifyValue(const QJsonValue &value) {
    switch (value.type()) {
    case QJsonValue::String: return value.toString();
    case QJsonValue::Double: return QString::number(value.toDouble());
    case QJsonValue::Bool: return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    case QJsonValue::Null:
    case QJsonValue::Undefined: return QString();
    default: return QString::fromUtf8(QJsonDocument::fromVariant(value.toVariant()).toJson(QJsonDocument::Compact));
    }
}

InspectorField mapField(const QJsonObject &raw) {
    InspectorField field;
    field.id = raw.value(QStringLiteral("id")).toString();
    field.label = raw.value(QStringLiteral("label")).toString();
    if (field.label.isEmpty()) field.label = raw.value(QStringLiteral("binding")).toString();
    field.control = domain::controlKindFromKey(raw.value(QStringLiteral("control")).toString());
    field.value = stringifyValue(raw.value(QStringLiteral("value")));
    field.writable = raw.contains(QStringLiteral("edit"));
    field.disabledReason = raw.value(QStringLiteral("disabledReason")).toString();
    return field;
}

Clip mapClip(const QJsonObject &raw) {
    Clip clip;
    clip.id = raw.value(QStringLiteral("id")).toString();
    clip.label = raw.value(QStringLiteral("label")).toString();
    clip.startFrame = raw.value(QStringLiteral("startFrame")).toInt();
    clip.endFrameExclusive = raw.value(QStringLiteral("endFrameExclusive")).toInt();
    for (const QJsonValue &entry : raw.value(QStringLiteral("inspector")).toArray()) {
        clip.inspector.append(mapField(entry.toObject()));
    }
    return clip;
}

domain::CanvasSpace mapSpace(const QJsonObject &raw) {
    domain::CanvasSpace space;
    space.width = raw.value(QStringLiteral("canvasWidth")).toInt();
    space.height = raw.value(QStringLiteral("canvasHeight")).toInt();
    space.frameCount = raw.value(QStringLiteral("frameCount")).toInt();
    space.durationSec = raw.value(QStringLiteral("durationSec")).toDouble();

    // frameRate arrives as an exact rational, not a float, so 30000/1001 stays
    // representable upstream; collapse it only for display here.
    const QJsonObject rate = raw.value(QStringLiteral("frameRate")).toObject();
    const double denominator = rate.value(QStringLiteral("denominator")).toDouble();
    if (denominator > 0.0) {
        space.frameRate = rate.value(QStringLiteral("numerator")).toDouble() / denominator;
    }
    return space;
}

}

MapResult mapSessionPayload(const QByteArray &payload) {
    MapResult result;

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        result.error = QStringLiteral("会话响应不是合法 JSON：%1").arg(parseError.errorString());
        return result;
    }
    if (!document.isObject()) {
        result.error = QStringLiteral("会话响应不是 JSON 对象。");
        return result;
    }

    const QJsonObject root = document.object();

    // A compile failure returns 500 with {revision, error, range?}; surface the
    // server's own message instead of reporting an empty project.
    if (root.contains(QStringLiteral("error"))) {
        result.error = root.value(QStringLiteral("error")).toString();
        return result;
    }
    if (!root.contains(QStringLiteral("revision"))) {
        result.error = QStringLiteral("会话响应缺少 revision 字段。");
        return result;
    }

    domain::Snapshot snapshot;
    snapshot.revision = root.value(QStringLiteral("revision")).toInt();
    snapshot.sourcePath = root.value(QStringLiteral("source")).toObject()
                              .value(QStringLiteral("path")).toString();
    snapshot.space = mapSpace(root.value(QStringLiteral("space")).toObject());

    for (const QJsonValue &trackValue : root.value(QStringLiteral("tracks")).toArray()) {
        const QJsonObject rawTrack = trackValue.toObject();
        Track track;
        track.id = rawTrack.value(QStringLiteral("id")).toString();
        track.label = rawTrack.value(QStringLiteral("label")).toString();
        for (const QJsonValue &clipValue : rawTrack.value(QStringLiteral("clips")).toArray()) {
            track.clips.append(mapClip(clipValue.toObject()));
        }
        snapshot.tracks.append(track);
    }

    result.snapshot = snapshot;
    return result;
}

}
