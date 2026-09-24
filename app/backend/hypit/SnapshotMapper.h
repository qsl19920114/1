#pragma once

#include "domain/Snapshot.h"

#include <QByteArray>
#include <QString>

namespace qvw::backend::hypit {

struct MapResult {
    domain::Snapshot snapshot;
    QString error;

    bool ok() const { return error.isEmpty(); }
};

/// Translates a raw Studio session payload into the project's own DTOs.
///
/// This is the only place allowed to know Hypit's field names. Two traps it
/// deliberately guards against, both verified in docs/API_CONTRACT.md:
///   - the response body IS the snapshot, there is no {data:...} envelope (§3);
///   - a field is writable only if it carries an `edit` endpoint (§4).
MapResult mapSessionPayload(const QByteArray &payload);

}
