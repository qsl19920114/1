#include "backend/hypit/StudioClient.h"
#include "backend/hypit/SnapshotMapper.h"
#include <QTest>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class HttpFixture : public QObject {
public:
    QTcpServer server;
    QByteArray body = R"({"revision":2,"source":{"path":"scene.svml"},"space":{},"tracks":[]})";
    QByteArray request;
    int status = 200;
    bool stall = false;
    HttpFixture() {
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (auto *socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    request += socket->readAll();
                    if (stall || !request.contains("\r\n\r\n")) return;
                    socket->write("HTTP/1.1 " + QByteArray::number(status) + " Result\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
            }
        });
    }
    bool listen() { return server.listen(QHostAddress::LocalHost, 0); }
    QUrl url() const { return QUrl(QString("http://127.0.0.1:%1/").arg(server.serverPort())); }
};
class StudioClientTest : public QObject {
    Q_OBJECT
private slots:
    void missingSourceFilesDoesNotGrantWriteWhitelist() {
        const auto mapped=qvw::backend::hypit::mapSessionPayload(R"({"revision":1,"source":{"path":"main.svml","text":"hello"},"space":{},"tracks":[]})");
        QVERIFY(mapped.ok());QVERIFY(mapped.snapshot.sourceFiles.isEmpty());QVERIFY(mapped.snapshot.sourceFingerprint.isEmpty());
    }
    void sourceProjectionTracksContentAndBindings() {
        auto root=QJsonDocument::fromJson(R"({"revision":1,"source":{"path":"main.svml","text":"initial","files":[{"path":"main.svml","text":"initial"}]},"space":{},"tracks":[{"clips":[{"id":"clip","inspector":[{"id":"field","binding":"title","control":"text","value":"hello","edit":{}}]}]}]})").object();
        auto mapped=qvw::backend::hypit::mapSessionPayload(QJsonDocument(root).toJson());
        QVERIFY(mapped.ok()); QCOMPARE(mapped.snapshot.sourceFiles.value("main.svml"),QString("initial"));
        QVERIFY(!mapped.snapshot.sourceFingerprint.isEmpty());
        QCOMPARE(mapped.snapshot.tracks[0].clips[0].inspector[0].binding,QString("title"));
        root["revision"]=2;
        auto same=qvw::backend::hypit::mapSessionPayload(QJsonDocument(root).toJson());
        QCOMPARE(same.snapshot.sourceFingerprint,mapped.snapshot.sourceFingerprint);
        auto source=root["source"].toObject(); source["text"]="changed";
        source["files"]=QJsonArray{QJsonObject{{"path","main.svml"},{"text","changed"}}};root["source"]=source;
        auto changed=qvw::backend::hypit::mapSessionPayload(QJsonDocument(root).toJson());
        QVERIFY(changed.snapshot.sourceFingerprint!=mapped.snapshot.sourceFingerprint);
    }
    void bareSnapshotAndNoOriginHeader() {
        HttpFixture http; QVERIFY(http.listen());
        qvw::backend::hypit::StudioClient client;
        QSignalSpy success(&client, &qvw::backend::hypit::StudioClient::snapshotReady);
        QSignalSpy failure(&client, &qvw::backend::hypit::StudioClient::failed);
        client.fetchSession(http.url());
        QTRY_COMPARE(success.size(), 1);
        QCOMPARE(failure.size(), 0);
        QCOMPARE(success[0][0].value<qvw::domain::Snapshot>().revision, 2);
        QVERIFY(http.request.startsWith("GET /__studio/session "));
        QVERIFY(!http.request.toLower().contains("\r\norigin:"));
    }
    void compileFailureCarriesRevisionButNetworkFailureDoesNot() {
        HttpFixture http;http.status=500;http.body=R"({"revision":9,"error":"bad source"})";QVERIFY(http.listen());
        qvw::backend::hypit::StudioClient client;QSignalSpy details(&client,&qvw::backend::hypit::StudioClient::requestFailed);
        client.fetchSession(http.url());QTRY_COMPARE(details.size(),1);QCOMPARE(details[0][0].toInt(),500);QCOMPARE(details[0][1].toInt(),9);
        HttpFixture stalled;stalled.stall=true;QVERIFY(stalled.listen());client.setTimeoutMs(50);client.fetchSession(stalled.url());
        QTRY_COMPARE(details.size(),2);QCOMPARE(details[1][0].toInt(),0);QCOMPARE(details[1][1].toInt(),-1);
    }
    void serverErrorIsNotSuccess() {
        HttpFixture http; http.status = 500; http.body = R"({"error":"source compile failed"})";
        QVERIFY(http.listen());
        qvw::backend::hypit::StudioClient client;
        QSignalSpy success(&client, &qvw::backend::hypit::StudioClient::snapshotReady);
        QSignalSpy failure(&client, &qvw::backend::hypit::StudioClient::failed);
        client.fetchSession(http.url()); QTRY_COMPARE(failure.size(), 1);
        QVERIFY(failure[0][0].toString().contains("source compile failed")); QCOMPARE(success.size(), 0);
    }
    void invalidPayload_data() {
        QTest::addColumn<QByteArray>("body");
        QTest::newRow("not-json") << QByteArray("<html>error</html>");
        QTest::newRow("wrapped") << QByteArray(R"({"data":{"revision":1}})");
        QTest::newRow("empty-error") << QByteArray(R"({"error":null})");
        QTest::newRow("invalid-revision") << QByteArray(R"({"revision":"oops","tracks":[],"source":{},"space":{}})");
        QTest::newRow("incomplete") << QByteArray(R"({"revision":1})");
        QTest::newRow("fractional-revision") << QByteArray(R"({"revision":1.2,"tracks":[],"source":{},"space":{}})");
    }
    void invalidPayload() {
        QFETCH(QByteArray, body);
        HttpFixture http; http.body = body; QVERIFY(http.listen());
        qvw::backend::hypit::StudioClient client;
        QSignalSpy failure(&client, &qvw::backend::hypit::StudioClient::failed);
        client.fetchSession(http.url()); QTRY_COMPARE(failure.size(), 1);
    }
    void replacingRequestDoesNotPublishStaleFailure() {
        HttpFixture first, second; first.stall = true;
        QVERIFY(first.listen()); QVERIFY(second.listen());
        qvw::backend::hypit::StudioClient client;
        QSignalSpy success(&client, &qvw::backend::hypit::StudioClient::snapshotReady);
        QSignalSpy failure(&client, &qvw::backend::hypit::StudioClient::failed);
        client.fetchSession(first.url()); QTRY_VERIFY(!first.request.isEmpty());
        client.fetchSession(second.url()); QTRY_COMPARE(success.size(), 1);
        QCOMPARE(failure.size(), 0);
        client.fetchSession(first.url()); client.cancel();
        QTest::qWait(40); QCOMPARE(failure.size(), 0); QCOMPARE(success.size(), 1);
    }
    void timeoutIsReadable() {
        HttpFixture http; http.stall = true; QVERIFY(http.listen());
        qvw::backend::hypit::StudioClient client; client.setTimeoutMs(100);
        QSignalSpy failure(&client, &qvw::backend::hypit::StudioClient::failed);
        client.fetchSession(http.url()); QTRY_COMPARE(failure.size(), 1);
        QVERIFY(!failure[0][0].toString().isEmpty());
    }
    void cancelFromPayloadCallbackSuppressesSnapshot() {
        HttpFixture http; QVERIFY(http.listen());
        qvw::backend::hypit::StudioClient client;
        QSignalSpy payload(&client, &qvw::backend::hypit::StudioClient::payloadReceived);
        QSignalSpy success(&client, &qvw::backend::hypit::StudioClient::snapshotReady);
        connect(&client, &qvw::backend::hypit::StudioClient::payloadReceived, &client, &qvw::backend::hypit::StudioClient::cancel);
        client.fetchSession(http.url()); QTRY_COMPARE(payload.size(), 1);
        QCOMPARE(success.size(), 0);
    }
    void rejectsNonlocalUrl() {
        qvw::backend::hypit::StudioClient client;
        QSignalSpy failure(&client, &qvw::backend::hypit::StudioClient::failed);
        client.fetchSession(QUrl("https://example.com/")); QCOMPARE(failure.size(), 1);
    }
    void mapperSeparatesControlAndWriteAuthority() {
        auto root = QJsonDocument::fromJson(R"({"revision":3,"source":{},"space":{},"tracks":[{"id":"t","clips":[{"id":"c","inspector":[{"id":"text","control":"text","value":"test","disabledReason":"引用绑定"},{"id":"select","control":"select","value":"a","options":[{"value":"a","label":"Alpha"}],"edit":{}}]}]}]})").object();
        const auto mapped = qvw::backend::hypit::mapSessionPayload(QJsonDocument(root).toJson());
        QVERIFY(mapped.ok());
        const auto fields = mapped.snapshot.tracks[0].clips[0].inspector;
        QVERIFY(!fields[0].isEditable()); QCOMPARE(fields[0].disabledReason, QString("引用绑定"));
        QVERIFY(fields[1].isEditable()); QCOMPARE(fields[1].options[0].label, QString("Alpha"));
    }
};
QTEST_GUILESS_MAIN(StudioClientTest)
#include "StudioClientTest.moc"
