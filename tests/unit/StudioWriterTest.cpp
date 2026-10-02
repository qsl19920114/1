#include "backend/hypit/StudioWriter.h"
#include <QTest>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <limits>
#include <memory>
using qvw::backend::hypit::StudioWriter;

class WriteHttpFixture : public QObject {
public:
    QTcpServer server;
    QByteArray body = R"({"revision":8})";
    QByteArray extraHeaders;
    QList<QByteArray> requests;
    QList<QPointer<QTcpSocket>> pending;
    int status = 200;
    bool stall = false;
    WriteHttpFixture() {
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (auto *socket = server.nextPendingConnection()) {
                auto data = std::make_shared<QByteArray>();
                auto handled = std::make_shared<bool>(false);
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket, data, handled] {
                    *data += socket->readAll();
                    const auto boundary = data->indexOf("\r\n\r\n");
                    if (*handled || boundary < 0) return;
                    int length = 0;
                    for (const auto &line : data->left(boundary).split('\n'))
                        if (line.toLower().startsWith("content-length:")) length = line.mid(15).trimmed().toInt();
                    if (data->size() < boundary + 4 + length) return;
                    *handled = true;
                    requests.append(*data);
                    pending.append(socket);
                    if (!stall) respond();
                });
            }
        });
    }
    bool listen() { return server.listen(QHostAddress::LocalHost, 0); }
    QUrl url() const { return QUrl(QString("http://127.0.0.1:%1/").arg(server.serverPort())); }
    void respond() {
        const auto sockets = pending; pending.clear();
        for (const auto &socket : sockets) if (socket && socket->state() == QAbstractSocket::ConnectedState) {
            socket->write("HTTP/1.1 " + QByteArray::number(status) + " Result\r\nContent-Type: application/json\r\nConnection: close\r\n" + extraHeaders + "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
            socket->disconnectFromHost();
        }
    }
    QJsonObject payload() const { const auto &r = requests.last(); return QJsonDocument::fromJson(r.mid(r.indexOf("\r\n\r\n") + 4)).object(); }
};

class StudioWriterTest : public QObject {
    Q_OBJECT
private slots:
    void preservesScalarAndIds_data() {
        QTest::addColumn<QVariant>("value");
        QTest::newRow("unicode-text") << QVariant(QString("校园标题 \"quoted\""));
        QTest::newRow("number") << QVariant(12.5);
        QTest::newRow("large-finite-number") << QVariant(1e100);
        QTest::newRow("boolean") << QVariant(false);
        QTest::newRow("null") << QVariant::fromValue(nullptr);
    }
    void preservesScalarAndIds() {
        QFETCH(QVariant, value);
        WriteHttpFixture http; QVERIFY(http.listen());
        StudioWriter writer; QSignalSpy success(&writer, &StudioWriter::completed), failure(&writer, &StudioWriter::failed);
        writer.adjustParameter(http.url(), 7, "clip:真实", "title", value);
        QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, 1000); QCOMPARE(failure.size(), 0);
        QCOMPARE(success[0][0].toInt(), 8);
        QCOMPARE(http.payload().value("type").toString(), QString("parameter.adjust"));
        QCOMPARE(http.payload().value("revision").toInt(), 7);
        QCOMPARE(http.payload().value("entityId").toString(), QString("clip:真实"));
        QCOMPARE(http.payload().value("parameterId").toString(), QString("title"));
        QCOMPARE(http.payload().value("value"), QJsonValue::fromVariant(value));
        QVERIFY(http.requests[0].startsWith("POST /__studio/mutation "));
        QVERIFY(!http.requests[0].toLower().contains("\r\norigin:"));
    }
    void replaceSourceRequires202() {
        WriteHttpFixture http; http.status = 202; QVERIFY(http.listen());
        StudioWriter writer; QSignalSpy success(&writer, &StudioWriter::completed);
        writer.replaceSource(http.url(), 7, "templates/标题.svml", "<Title>你好</Title>\n");
        QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, 1000);
        QCOMPARE(http.payload().value("path").toString(), QString("templates/标题.svml"));
        QCOMPARE(http.payload().value("text").toString(), QString("<Title>你好</Title>\n"));
        QCOMPARE(http.payload().value("revision").toInt(), 7);
        QVERIFY(http.requests[0].startsWith("PUT /__studio/source "));
        QVERIFY(!http.requests[0].toLower().contains("\r\norigin:"));
    }
    void noOpMayKeepRevision() {
        WriteHttpFixture http; http.body = R"({"revision":7})"; QVERIFY(http.listen());
        StudioWriter writer; QSignalSpy success(&writer, &StudioWriter::completed);
        writer.adjustParameter(http.url(), 7, "c", "p", "unchanged");
        QTRY_COMPARE_WITH_TIMEOUT(success.size(), 1, 1000); QCOMPARE(success[0][0].toInt(), 7);
    }
    void responseClassification_data() {
        QTest::addColumn<int>("status"); QTest::addColumn<QByteArray>("body");
        QTest::addColumn<bool>("source"); QTest::addColumn<int>("kind");
        QTest::newRow("conflict") << 409 << QByteArray(R"({"error":"external edit"})") << false << int(StudioWriter::Conflict);
        QTest::newRow("source-conflict") << 409 << QByteArray("not json") << true << int(StudioWriter::Conflict);
        QTest::newRow("rejected-rollback") << 422 << QByteArray(R"({"error":"compile rollback"})") << false << int(StudioWriter::Rejected);
        QTest::newRow("source-422-unknown") << 422 << QByteArray(R"({"error":"compile failure"})") << true << int(StudioWriter::Indeterminate);
        QTest::newRow("invalid-body") << 400 << QByteArray("bad body") << false << int(StudioWriter::InvalidRequest);
        QTest::newRow("forbidden") << 403 << QByteArray(R"({"error":"forbidden"})") << false << int(StudioWriter::InvalidRequest);
        QTest::newRow("server-failure") << 500 << QByteArray(R"({"error":"write then fail"})") << false << int(StudioWriter::Indeterminate);
        QTest::newRow("malformed-success") << 200 << QByteArray("html") << false << int(StudioWriter::Indeterminate);
        QTest::newRow("missing-revision") << 200 << QByteArray("{}") << false << int(StudioWriter::Indeterminate);
        QTest::newRow("old-revision") << 200 << QByteArray(R"({"revision":6})") << false << int(StudioWriter::Indeterminate);
        QTest::newRow("fractional-revision") << 200 << QByteArray(R"({"revision":8.5})") << false << int(StudioWriter::Indeterminate);
        QTest::newRow("string-revision") << 200 << QByteArray(R"({"revision":"8"})") << false << int(StudioWriter::Indeterminate);
        QTest::newRow("source-200") << 200 << QByteArray(R"({"revision":8})") << true << int(StudioWriter::Indeterminate);
        QTest::newRow("source-unchanged-revision") << 202 << QByteArray(R"({"revision":7})") << true << int(StudioWriter::Indeterminate);
        QTest::newRow("mutation-202") << 202 << QByteArray(R"({"revision":8})") << false << int(StudioWriter::Indeterminate);
    }
    void responseClassification() {
        QFETCH(int, status); QFETCH(QByteArray, body); QFETCH(bool, source); QFETCH(int, kind);
        WriteHttpFixture http; http.status = status; http.body = body; QVERIFY(http.listen());
        StudioWriter writer; QSignalSpy success(&writer, &StudioWriter::completed), failure(&writer, &StudioWriter::failed);
        if (source) writer.replaceSource(http.url(), 7, "scene.svml", "text");
        else writer.adjustParameter(http.url(), 7, "c", "p", 4);
        QTRY_COMPARE_WITH_TIMEOUT(failure.size(), 1, 1000); QCOMPARE(success.size(), 0);
        QCOMPARE(failure[0][0].value<StudioWriter::WriteFailure>(), StudioWriter::WriteFailure(kind));
        QCOMPARE(failure[0][2].toInt(), status); QVERIFY(!failure[0][1].toString().isEmpty());
    }
    void timeoutHasUnknownOutcome() {
        WriteHttpFixture http; http.stall = true; QVERIFY(http.listen());
        StudioWriter writer; writer.setTimeoutMs(60); QSignalSpy failure(&writer, &StudioWriter::failed);
        writer.adjustParameter(http.url(), 7, "c", "p", 4);
        QTRY_COMPARE_WITH_TIMEOUT(failure.size(), 1, 1000);
        QCOMPARE(failure[0][0].value<StudioWriter::WriteFailure>(), StudioWriter::Indeterminate);
        QCOMPARE(http.requests.size(), 1);
    }
    void refusesParallelWriteWithoutCancellingFirst() {
        WriteHttpFixture http; http.stall = true; QVERIFY(http.listen());
        StudioWriter writer; QSignalSpy failure(&writer, &StudioWriter::failed), success(&writer, &StudioWriter::completed);
        writer.adjustParameter(http.url(), 7, "c", "p", 4); QTRY_COMPARE(http.requests.size(), 1);
        writer.replaceSource(http.url(), 7, "scene.svml", "text");
        QCOMPARE(failure.size(), 1); QCOMPARE(failure[0][0].value<StudioWriter::WriteFailure>(), StudioWriter::InvalidRequest);
        http.respond(); QTRY_COMPARE(success.size(), 1); QCOMPARE(http.requests.size(), 1);
    }
    void cancelledReplyCannotSignalIntoNextWrite() {
        WriteHttpFixture first, second; first.stall = true; QVERIFY(first.listen()); QVERIFY(second.listen());
        StudioWriter writer; QSignalSpy failure(&writer, &StudioWriter::failed), success(&writer, &StudioWriter::completed);
        writer.adjustParameter(first.url(), 7, "c", "p", 4); QTRY_COMPARE(first.requests.size(), 1);
        writer.cancel(); writer.adjustParameter(second.url(), 7, "c", "p", 5);
        first.respond(); QTRY_COMPARE(success.size(), 1); QTest::qWait(30); QCOMPARE(failure.size(), 0);
    }
    void completionCanStartNextWriteReentrantly() {
        WriteHttpFixture http; QVERIFY(http.listen()); StudioWriter writer;
        QSignalSpy failure(&writer, &StudioWriter::failed), success(&writer, &StudioWriter::completed);
        connect(&writer, &StudioWriter::completed, &writer, [&](int) {
            if (http.requests.size() == 1) { http.body = R"({"revision":9})"; writer.adjustParameter(http.url(), 8, "c", "p", 5); }
        });
        writer.adjustParameter(http.url(), 7, "c", "p", 4);
        QTRY_COMPARE(success.size(), 2); QCOMPARE(failure.size(), 0); QCOMPARE(http.requests.size(), 2);
    }
    void redirectsAreNeverFollowed() {
        WriteHttpFixture redirect, destination; QVERIFY(redirect.listen()); QVERIFY(destination.listen());
        redirect.status = 307; redirect.extraHeaders = "Location: " + destination.url().toEncoded() + "\r\n";
        StudioWriter writer; QSignalSpy failure(&writer, &StudioWriter::failed);
        writer.adjustParameter(redirect.url(), 7, "c", "p", 4); QTRY_COMPARE(failure.size(), 1);
        QCOMPARE(failure[0][0].value<StudioWriter::WriteFailure>(), StudioWriter::Indeterminate);
        QCOMPARE(destination.requests.size(), 0);
    }
    void oversizedResponseIsUnknownOutcome() {
        WriteHttpFixture http; http.body = QByteArray(1024 * 1024 + 1, 'x'); QVERIFY(http.listen());
        StudioWriter writer; QSignalSpy failure(&writer, &StudioWriter::failed);
        writer.adjustParameter(http.url(), 7, "c", "p", 4); QTRY_COMPARE(failure.size(), 1);
        QCOMPARE(failure[0][0].value<StudioWriter::WriteFailure>(), StudioWriter::Indeterminate);
    }
    void invalidInputNeverMakesRequest() {
        WriteHttpFixture http; QVERIFY(http.listen()); StudioWriter writer; QSignalSpy failure(&writer, &StudioWriter::failed);
        writer.adjustParameter(QUrl("https://example.com"), 7, "c", "p", 1);
        writer.adjustParameter(http.url(), -1, "c", "p", 1);
        writer.adjustParameter(http.url(), 7, "", "p", 1);
        writer.adjustParameter(http.url(), 7, "c", "p", std::numeric_limits<double>::infinity());
        writer.adjustParameter(http.url(), 7, "c", "p", QVariantList{1, 2});
        writer.replaceSource(http.url(), 7, "/absolute.svml", "bad");
        writer.replaceSource(http.url(), 7, "../escape.svml", "bad");
        QCOMPARE(failure.size(), 7);
        for (const auto &args : failure) QCOMPARE(args[0].value<StudioWriter::WriteFailure>(), StudioWriter::InvalidRequest);
        QTest::qWait(30); QCOMPARE(http.requests.size(), 0);
    }
};
QTEST_GUILESS_MAIN(StudioWriterTest)
#include "StudioWriterTest.moc"
