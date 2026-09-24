#pragma once

#include <QString>
#include <QVector>

namespace qvw::domain {

/// How the Inspector should render a field. Mirrors Studio's control vocabulary
/// but is re-declared here so the UI never includes an adapter header.
enum class ControlKind {
    Text,
    Number,
    Boolean,
    Select,
    Color,
    Unsupported,
};

ControlKind controlKindFromKey(const QString &key);
QString controlKindLabel(ControlKind kind);

/// A normalised, UI-facing parameter.
///
/// `writable` is authoritative and comes from whether the upstream snapshot
/// carried an `edit` endpoint for the field. It is NOT derived from `control`:
/// per docs/API_CONTRACT.md §4 a field can be rendered as a text box and still
/// be read-only, so the two concepts are stored separately on purpose.
struct InspectorField {
    QString id;
    QString label;
    ControlKind control = ControlKind::Unsupported;
    QString value;
    bool writable = false;
    /// Verbatim server-side explanation; shown as-is when !writable so the UI
    /// never invents its own wording.
    QString disabledReason;

    bool isEditable() const;
};

using InspectorFields = QVector<InspectorField>;

}
