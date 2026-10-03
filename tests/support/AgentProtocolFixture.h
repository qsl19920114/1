#pragma once

// Unit protocol simulation. This is deliberately not a real Hypit server or
// real model request; production controllers, JSON mapping and HTTP are real.
#include "agent/AgentController.h"
#include "agent/AgentTaskStore.h"
#include "backend/hypit/SnapshotMapper.h"
#include <QFile>
#include <QJsonDocument>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <functional>

namespace agent_test {

inline bool writeFile(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

inline QByteArray readFile(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

inline QJsonObject operation(const QString &field, const QString &value,
                             const QString &entity = "scene-1") {
    return {{"entityId", entity}, {"fieldId", field}, {"value", value}};
}

inline QJsonObject editPlan(const QJsonArray &operations) {
    return {{"format", "qvw.agent-plan@1"}, {"kind", "edit"},
            {"summary", "协议模拟：审阅真实 Controller 的修改"},
            {"question", ""}, {"projectName", ""},
            {"scenes", QJsonArray{}}, {"operations", operations}};
}

class AgentHttp : public QObject {
public:
    QTcpServer server;
    QString root, sourceSalt;
    int revision = 1, writes = 0, gets = 0;
    QMap<int, int> writeStatuses;
    QStringList writtenFields;
    QList<QJsonObject> wireOperations;
    bool lateRepublish = false, externalOnRepublish = false;
    std::function<void()> onWrite;
    QJsonObject values{{"scene-1", QJsonObject{{"title", "old title"}, {"subtitle", "old subtitle"}}},
                       {"scene-2", QJsonObject{{"title", "other title"}, {"subtitle", "other subtitle"}}}};

    AgentHttp() {
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (auto *socket = server.nextPendingConnection()) {
                auto request = std::make_shared<QByteArray>();
                auto complete = std::make_shared<bool>(false);
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket, request, complete] {
                    *request += socket->readAll();
                    const auto divider = request->indexOf("\r\n\r\n");
                    if (*complete || divider < 0) return;
                    int length = 0;
                    for (const auto &line : request->left(divider).split('\n'))
                        if (line.toLower().startsWith("content-length:"))
                            length = line.mid(15).trimmed().toInt();
                    if (request->size() < divider + 4 + length) return;
                    *complete = true;
                    if (request->startsWith("GET ")) {
                        ++gets;
                        const bool schedule = m_pendingRepublish;
                        m_pendingRepublish = false;
                        respond(socket, 200, payload());
                        // Republish after the confirmation GET. The next
                        // operation must observe this revision before writing.
                        if (schedule) QTimer::singleShot(60, this, [this] {
                            ++revision;
                            if (externalOnRepublish) {
                                sourceSalt = "external source change";
                                saveSource();
                            }
                        });
                        return;
                    }
                    ++writes;
                    const auto body = QJsonDocument::fromJson(request->mid(divider + 4)).object();
                    writtenFields.append(body["parameterId"].toString());
                    wireOperations.append(body);
                    const auto status = writeStatuses.value(writes, 200);
                    if (body["revision"].toInt(-1) != revision) {
                        respond(socket, 409, R"({"error":"protocol simulation: stale revision"})");
                        return;
                    }
                    if (status == 422) {
                        revision += 2; // Compiled rejection, source rolled back.
                        respond(socket, 422, R"({"error":"protocol simulation: rejected and rolled back"})");
                        return;
                    }
                    if (status != 200) {
                        respond(socket, status, R"({"error":"protocol simulation: injected failure"})");
                        return;
                    }
                    const auto entity = body["entityId"].toString();
                    auto fields = values[entity].toObject();
                    fields[body["parameterId"].toString()] = body["value"];
                    values[entity] = fields;
                    ++revision;
                    saveSource();
                    m_pendingRepublish = lateRepublish;
                    if (onWrite) onWrite();
                    respond(socket, 200, QJsonDocument(QJsonObject{{"revision", revision}}).toJson());
                });
            }
        });
    }

    bool listen() { return server.listen(QHostAddress::LocalHost, 0); }
    QUrl url() const { return QUrl(QString("http://127.0.0.1:%1").arg(server.serverPort())); }
    QString value(const QString &field, const QString &entity = "scene-1") const {
        return values[entity].toObject()[field].toString();
    }
    bool saveSource() const { return writeFile(QDir(root).filePath("main.svml"), source()); }
    QByteArray source() const {
        return QJsonDocument(values).toJson(QJsonDocument::Compact) + "\n" + sourceSalt.toUtf8();
    }
    QByteArray payload() const {
        QJsonArray clips;
        for (const auto &entity : {QString("scene-1"), QString("scene-2")}) {
            QJsonArray fields;
            const auto current = values[entity].toObject();
            for (const auto &name : {QString("title"), QString("subtitle")})
                fields.append(QJsonObject{{"id", name}, {"binding", name}, {"control", "text"},
                                          {"value", current[name]}, {"edit", QJsonObject{}}});
            clips.append(QJsonObject{{"id", entity}, {"authoredId", entity}, {"inspector", fields}});
        }
        const auto text = QString::fromUtf8(source());
        return QJsonDocument(QJsonObject{{"revision", revision},
            {"source", QJsonObject{{"path", "main.svml"}, {"text", text},
                                   {"files", QJsonArray{QJsonObject{{"path", "main.svml"}, {"text", text}}}}}},
            {"space", QJsonObject{}},
            {"tracks", QJsonArray{QJsonObject{{"id", "track"}, {"clips", clips}}}}}).toJson();
    }
    qvw::domain::Snapshot snapshot() const {
        return qvw::backend::hypit::mapSessionPayload(payload()).snapshot;
    }
    static void respond(QTcpSocket *socket, int status, const QByteArray &body) {
        socket->write("HTTP/1.1 " + QByteArray::number(status) + " Result\r\nConnection: close\r\nContent-Length: "
                      + QByteArray::number(body.size()) + "\r\n\r\n" + body);
        socket->disconnectFromHost();
    }
private:
    bool m_pendingRepublish = false;
};

class AgentProtocolFixture {
public:
    QTemporaryDir temp;
    qvw::infra::LogWriter log{temp.filePath("log.jsonl")};
    qvw::controllers::DocumentController document;
    qvw::controllers::EditorController editor;
    qvw::controllers::ExportController exporter{{}, log};
    qvw::agent::ModelClient model;
    AgentHttp http;
    std::unique_ptr<qvw::agent::AgentController> agent;

    bool initialize() {
        if (!temp.isValid() || !http.listen()) return false;
        if (!document.create(QStringLiteral(QVW_SOURCE_DIR) + "/templates/title-card",
                             temp.filePath("project"), "协议模拟工程")) return false;
        attachDocument();
        agent = std::make_unique<qvw::agent::AgentController>(document, editor, exporter, model,
                                            QStringLiteral(QVW_SOURCE_DIR) + "/templates");
        return true;
    }
    void attachDocument() {
        http.root = document.project().rootPath;
        http.saveSource();
        editor.attach(http.url(), http.root);
        editor.acceptSnapshot(http.snapshot());
    }
    QString statePath() const { return QDir(document.project().rootPath).filePath(".workbench/agent/current.json"); }
    QJsonObject savedState() const { return QJsonDocument::fromJson(readFile(statePath())).object(); }
    QJsonObject baseline(const QString &scope = {}) const {
        return {{"root", document.project().rootPath}, {"revision", editor.snapshot().revision},
                {"fingerprint", QString::fromLatin1(editor.snapshot().sourceFingerprint.toHex())}, {"scope", scope}};
    }
    QJsonObject storedTask(const QJsonArray &operations, int next = 0, const QString &scope = {}) const {
        return {{"format", "qvw.agent-task@1"}, {"projectId", document.project().rootPath},
                {"taskId", "saved-task"}, {"goal", "只修改当前组件"}, {"scope", scope}, {"phase", "paused"},
                {"plan", editPlan(operations)}, {"planBase", baseline(scope)}, {"executionBase", baseline(scope)},
                {"operations", operations}, {"next", next}, {"retries", 0}, {"events", QJsonArray{}},
                {"approvalIdentity", "saved approval must never authorize execution"}};
    }
    bool saveTask(const QJsonObject &state) {
        QString error;
        return qvw::agent::AgentTaskStore::save(document.project(), state, &error);
    }
    bool useFakeModel(const QJsonObject &plan, bool held = false) {
        const auto response = temp.filePath("response.json");
        if (!writeFile(response, QJsonDocument(plan).toJson())) return false;
        const auto literal = [](const QString &path) {
            const auto quoted = QJsonDocument(QJsonArray{path}).toJson(QJsonDocument::Compact);
            return quoted.mid(1, quoted.size() - 2);
        };
        QByteArray script = "#!/usr/bin/python3\nimport sys,pathlib,time\na=sys.argv\n";
        script += "pathlib.Path(" + literal(temp.filePath("model-prompt.txt")) + ").write_text(sys.stdin.read())\n";
        script += "pathlib.Path(" + literal(temp.filePath("model-started")) + ").touch()\n";
        if (held) {
            script += "gate=pathlib.Path(" + literal(temp.filePath("model-release")) + ")\n";
            script += "while not gate.exists(): time.sleep(0.01)\n";
        }
        script += "pathlib.Path(a[a.index('--output-last-message')+1]).write_bytes(pathlib.Path(" + literal(response) + ").read_bytes())\n";
        const auto path = temp.filePath("fake-codex");
        if (!writeFile(path, script)) return false;
        QFile file(path);
        if (!file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) return false;
        model.setProgram(path);
        model.setTimeoutMs(5000);
        return true;
    }
};
}
