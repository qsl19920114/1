// T004 Qt-native HTTP contract probe.
//
// Exercises the Studio endpoints from QNetworkAccessManager, which is how the
// Qt app will talk to Studio. The point is to record what Qt's own Host/Origin
// headers do to the mutation gate in packages/studio/src/mutation-origin.ts,
// instead of assuming curl behaviour transfers.
//
// With --write it also exercises the success path: it finds the first inspector
// field carrying an `edit` endpoint and adjusts it for real, which rewrites the
// SVML source on disk. That requires a Run whose component ships a Studio
// Companion declaring `writable: true`; see tests/fixtures/writable-probe.
//
// Usage: http_probe <base-url> [--write]

#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QDebug>

namespace {

struct Outcome {
    int httpStatus = 0;
    QByteArray body;
    QString transportError;
};

Outcome sendBlocking(QNetworkAccessManager &net, QNetworkRequest request,
                     const QByteArray &verb, const QByteArray &payload) {
    Outcome outcome;
    QNetworkReply *reply = payload.isEmpty() && verb == "GET"
        ? net.get(request)
        : net.sendCustomRequest(request, verb, payload);
    QEventLoop loop;
    QTimer guard;
    guard.setSingleShot(true);
    QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    guard.start(20000);
    loop.exec();
    if (!reply->isFinished()) {
        outcome.transportError = QStringLiteral("timeout");
        reply->abort();
    } else {
        outcome.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        outcome.body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError && outcome.httpStatus == 0) {
            outcome.transportError = reply->errorString();
        }
    }
    reply->deleteLater();
    return outcome;
}

struct WritableField {
    QString entityId;
    QString parameterId;
    QString control;
    QJsonValue value;
    bool found = false;
};

// Mirrors the only supported discovery route: walk tracks -> clips -> inspector
// and accept a field solely because it carries `edit`. `control` never implies
// writability (studio/src/parameters.ts:495).
WritableField firstWritableField(const QJsonObject &snapshot) {
    WritableField field;
    for (const QJsonValue &track : snapshot.value(QStringLiteral("tracks")).toArray()) {
        for (const QJsonValue &clipValue : track.toObject().value(QStringLiteral("clips")).toArray()) {
            const QJsonObject clip = clipValue.toObject();
            for (const QJsonValue &fieldValue : clip.value(QStringLiteral("inspector")).toArray()) {
                const QJsonObject candidate = fieldValue.toObject();
                if (!candidate.contains(QStringLiteral("edit"))) continue;
                field.entityId = clip.value(QStringLiteral("id")).toString();
                field.parameterId = candidate.value(QStringLiteral("id")).toString();
                field.control = candidate.value(QStringLiteral("control")).toString();
                field.value = candidate.value(QStringLiteral("value"));
                field.found = true;
                return field;
            }
        }
    }
    return field;
}

void report(const QString &name, int expected, const Outcome &outcome, bool &allOk) {
    const bool ok = outcome.transportError.isEmpty() && outcome.httpStatus == expected;
    if (!ok) allOk = false;
    qInfo().noquote() << (ok ? "PASS" : "FAIL") << name
                      << "expected=" << expected
                      << "got=" << outcome.httpStatus
                      << (outcome.transportError.isEmpty() ? QString() : QStringLiteral("transport=") + outcome.transportError)
                      << "body=" << QString::fromUtf8(outcome.body.left(160));
}

}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QString base = QStringLiteral("http://localhost:5599");
    bool exerciseWrite = false;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--write")) exerciseWrite = true;
        else base = arg;
    }
    QNetworkAccessManager net;
    bool allOk = true;

    QNetworkRequest sessionRequest(QUrl(base + QStringLiteral("/__studio/session")));
    const Outcome session = sendBlocking(net, sessionRequest, "GET", {});
    report(QStringLiteral("GET /__studio/session"), 200, session, allOk);

    int revision = -1;
    const QJsonObject snapshot = QJsonDocument::fromJson(session.body).object();
    if (snapshot.contains(QStringLiteral("revision"))) {
        revision = snapshot.value(QStringLiteral("revision")).toInt();
        // The plan's §8 warning: the snapshot is the body, not {data:...}.
        qInfo().noquote() << "INFO snapshot is top-level (no data envelope):"
                          << "revision=" << revision
                          << "hasTracks=" << snapshot.contains(QStringLiteral("tracks"))
                          << "hasPreview=" << snapshot.contains(QStringLiteral("preview"))
                          << "wrappedInData=" << snapshot.contains(QStringLiteral("data"));
    } else {
        allOk = false;
        qInfo().noquote() << "FAIL snapshot has no revision field";
    }

    const auto postMutation = [&](const QJsonObject &body, const QByteArray &origin) {
        QNetworkRequest request(QUrl(base + QStringLiteral("/__studio/mutation")));
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        if (!origin.isEmpty()) request.setRawHeader("Origin", origin);
        return sendBlocking(net, request, "POST", QJsonDocument(body).toJson(QJsonDocument::Compact));
    };

    report(QStringLiteral("POST mutation, Qt default headers, malformed body -> gate passed, validation rejects"),
           400, postMutation(QJsonObject{{"type", "bogus"}}, {}), allOk);

    report(QStringLiteral("POST mutation, cross-origin header -> gate rejects"),
           403, postMutation(QJsonObject{
               {"type", "parameter.adjust"}, {"revision", revision},
               {"entityId", "x"}, {"parameterId", "y"}, {"value", "z"}},
               "http://evil.example"), allOk);

    if (revision >= 0) {
        report(QStringLiteral("POST mutation, stale revision -> 409 conflict"),
               409, postMutation(QJsonObject{
                   {"type", "parameter.adjust"}, {"revision", revision + 5000},
                   {"entityId", "missing"}, {"parameterId", "missing"}, {"value", "z"}}, {}), allOk);

        report(QStringLiteral("POST mutation, current revision but unknown entity -> 500 not-found"),
               500, postMutation(QJsonObject{
                   {"type", "parameter.adjust"}, {"revision", revision},
                   {"entityId", "missing"}, {"parameterId", "missing"}, {"value", "z"}}, {}), allOk);
    }

    const WritableField writable = firstWritableField(snapshot);
    qInfo().noquote() << "INFO writable field discovered:" << (writable.found ? "yes" : "no")
                      << (writable.found ? QStringLiteral("control=%1 parameterId=%2")
                                               .arg(writable.control, writable.parameterId)
                                         : QStringLiteral("(snapshot exposes no inspector field with an edit endpoint)"));

    if (exerciseWrite) {
        if (!writable.found) {
            allOk = false;
            qInfo().noquote() << "FAIL --write requested but no writable field exists in this Run";
        } else {
            // Re-read the revision: the failure assertions above may have advanced it.
            const Outcome current = sendBlocking(net, sessionRequest, "GET", {});
            const QJsonObject fresh = QJsonDocument::fromJson(current.body).object();
            const int liveRevision = fresh.value(QStringLiteral("revision")).toInt();

            // A no-op value can still return 200 without advancing the revision;
            // choose a fresh value so success is observable.
            const QJsonValue probeValue = writable.control == QStringLiteral("number")
                ? QJsonValue(writable.value.toString().toInt() + 1)
                : QJsonValue(QStringLiteral("Qt 写入验证 %1").arg(QDateTime::currentMSecsSinceEpoch()));

            const Outcome write = postMutation(QJsonObject{
                {"type", "parameter.adjust"}, {"revision", liveRevision},
                {"entityId", writable.entityId}, {"parameterId", writable.parameterId},
                {"value", probeValue}}, {});
            report(QStringLiteral("POST mutation, real writable field -> 200 accepted"), 200, write, allOk);

            const int newRevision = QJsonDocument::fromJson(write.body).object()
                                        .value(QStringLiteral("revision")).toInt();
            const bool advanced = newRevision > liveRevision;
            if (!advanced) allOk = false;
            qInfo().noquote() << (advanced ? "PASS" : "FAIL") << "revision advanced"
                              << liveRevision << "->" << newRevision;

            // The write is only real if the recompiled snapshot reports the new value.
            const Outcome after = sendBlocking(net, sessionRequest, "GET", {});
            const WritableField reread = firstWritableField(QJsonDocument::fromJson(after.body).object());
            const bool persisted = reread.found && reread.value != writable.value;
            if (!persisted) allOk = false;
            qInfo().noquote() << (persisted ? "PASS" : "FAIL") << "value persisted in recompiled snapshot:"
                              << QJsonDocument(QJsonObject{{"before", writable.value}, {"after", reread.value}})
                                     .toJson(QJsonDocument::Compact);

            report(QStringLiteral("POST mutation, replaying the consumed revision -> 409 conflict"),
                   409, postMutation(QJsonObject{
                       {"type", "parameter.adjust"}, {"revision", liveRevision},
                       {"entityId", writable.entityId}, {"parameterId", writable.parameterId},
                       {"value", probeValue}}, {}), allOk);
        }
    }

    qInfo().noquote() << (allOk ? "ALL PASS" : "SOME FAILED");
    return allOk ? 0 : 1;
}
