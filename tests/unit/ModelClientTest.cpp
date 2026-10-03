#include "agent/ModelClient.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QProcess>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <memory>
#include <QtTest>

using qvw::agent::ModelClient;

namespace {
int fakeCodex(const QStringList &arguments)
{
    QFile input;
    if (!input.open(stdin, QIODevice::ReadOnly)) return 92;
    const QByteArray prompt = input.readAll();
    const auto request = QJsonDocument::fromJson(prompt).object();
    const QString mode = request.value("mode").toString("good");
    auto argumentValue = [&](const QString &name) {
        const int index = arguments.indexOf(name);
        return index >= 0 && index + 1 < arguments.size() ? arguments.at(index + 1) : QString();
    };
    QFile schemaFile(argumentValue("--output-schema"));
    if (!schemaFile.open(QIODevice::ReadOnly)) return 93;
    const auto schema = QJsonDocument::fromJson(schemaFile.readAll()).object();
    QString instructionsPath;
    for (int i = 0; i + 1 < arguments.size(); ++i) {
        if (arguments.at(i) == "-c" && arguments.at(i + 1).startsWith("model_instructions_file=")) {
            const QByteArray literal = arguments.at(i + 1).mid(QString("model_instructions_file=").size()).toUtf8();
            instructionsPath = QJsonDocument::fromJson("[" + literal + "]").array().at(0).toString();
        }
    }
    QFile instructionsFile(instructionsPath);
    if (!instructionsFile.open(QIODevice::ReadOnly)) return 95;
    const QString instructions = QString::fromUtf8(instructionsFile.readAll());
    QFile output;
    QFile diagnostics;
    if (!output.open(stdout, QIODevice::WriteOnly)) return 96;
    if (!diagnostics.open(stderr, QIODevice::WriteOnly)) return 97;
    if (mode == "slow") QThread::msleep(500);
    if (mode == "public-output" || mode == "public-malformed" || mode == "public-held") {
        output.write("{\"type\":\"item.completed\",\"item\":{\"id\":\"r\",\"type\":\"reasoning\",\"text\":\"private reasoning\"}}\n");
        output.write("{\"type\":\"item.updated\",\"item\":{\"id\":\"m\",\"type\":\"agent_message\",\"text\":\"received\"}}\n"); output.flush();
        QThread::msleep(200);
        output.write("{\"type\":\"item.completed\",\"item\":{\"id\":\"m\",\"type\":\"agent_message\",\"text\":\"received public message\"}}\n"); output.flush();
        QThread::msleep(200);
        if (mode == "public-held") QThread::msleep(1000);
        if (mode == "public-malformed") { output.write("{bad event}\n"); output.flush(); return 0; }
    }

    if (mode == "nonzero") {
        diagnostics.write("Authorization: Bearer private-test-token\n");
        diagnostics.flush();
    }
    if (mode == "stdout-limit") output.write(QByteArray(256 * 1024 + 1, 'x'));
    if (mode == "stderr-limit") diagnostics.write(QByteArray(128 * 1024 + 1, 'x'));
    if (mode == "tool" || mode == "unknown-item") {
        const auto itemType = mode == "tool" ? "command_execution" : "unrecognized_tool";
        output.write(QJsonDocument(QJsonObject{{"type", "item.started"}, {"item", QJsonObject{{"type", itemType}, {"command", "touch forbidden"}}}}).toJson(QJsonDocument::Compact) + "\n");
        output.flush();
        QThread::msleep(100);
    } else if (mode == "malformed-event") {
        output.write("{bad event}\n");
    } else if (mode == "fragmented-tool") {
        output.write("{\"type\":\"item.started\",\"item\":{\"type\":\"mcp_");
        output.flush();
        QThread::msleep(30);
        output.write("tool_call\"}}\n");
    } else if (mode != "stdout-limit") {
        output.write("{\"type\":\"thread.started\",\"thread_id\":\"test\"}\n");
        output.write("{\"type\":\"item.completed\",\"item\":{\"id\":\"item_0\",\"type\":\"agent_message\",\"text\":\"{}\"}}\n");
        output.write("{\"type\":\"turn.completed\",\"usage\":{\"input_tokens\":1,\"output_tokens\":1}}\n");
    }
    output.flush();
    diagnostics.flush();
    if (mode == "missing") return 0;
    QFile result(argumentValue("--output-last-message"));
    if (!result.open(QIODevice::WriteOnly)) return 94;
    if (mode == "malformed") result.write("{broken");
    else if (mode == "empty") result.write("");
    else if (mode == "empty-object") result.write("{}");
    else if (mode == "array") result.write("[]");
    else if (mode == "result-limit") result.write(QByteArray(64 * 1024 + 1, 'x'));
    else {
        result.write(QJsonDocument(QJsonObject{
            {"title", request.value("title").toString("clip")},
            {"seenPrompt", QString::fromUtf8(prompt)},
            {"arguments", QJsonArray::fromStringList(arguments)},
            {"schema", schema},
            {"instructions", instructions},
            {"workingDirectory", QDir::currentPath()},
            {"schemaDirectory", QFileInfo(schemaFile).canonicalPath()},
            {"resultDirectory", QFileInfo(result).canonicalPath()},
            {"environmentMarker",qEnvironmentVariable("QVW_MODEL_ENV_TEST")},
            {"launchPath",qEnvironmentVariable("PATH")}
        }).toJson(QJsonDocument::Compact));
    }
    return mode == "nonzero" ? 23 : 0;
}

QString requestPrompt(const QString &mode = "good", const QString &title = "clip")
{
    return QString::fromUtf8(QJsonDocument(QJsonObject{{"mode", mode}, {"title", title}}).toJson(QJsonDocument::Compact));
}

QJsonObject exampleSchema()
{
    return {{"type", "object"}, {"properties", QJsonObject{{"title", QJsonObject{{"type", "string"}}}}}, {"required", QJsonArray{"title"}}, {"additionalProperties", false}};
}
}

class ModelClientTest : public QObject {
    Q_OBJECT
private slots:
    void onlyReceivedPublicMessagesAreProgressivelyPublished() {
        ModelClient client; client.setProgram(QCoreApplication::applicationFilePath());
        QSignalSpy output(&client, SIGNAL(publicMessageReceived(QString)));
        QVERIFY(output.isValid()); QSignalSpy completed(&client, &ModelClient::completed);
        client.request(requestPrompt("public-output"), exampleSchema());
        QTRY_COMPARE_WITH_TIMEOUT(output.size(), 1, 1000); QVERIFY(client.isBusy()); QCOMPARE(completed.size(), 0);
        QCOMPARE(output[0][0].toString(), QString("received"));
        QTRY_VERIFY_WITH_TIMEOUT(output.size() >= 2, 1000); QCOMPARE(output[1][0].toString(), QString("received public message"));
        QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 2000);
        for (const auto &message : output) QVERIFY(!message[0].toString().contains("private reasoning"));
    }
    void publicOutputDoesNotMakeMalformedOrCancelledResponseComplete() {
        ModelClient client; client.setProgram(QCoreApplication::applicationFilePath());
        QSignalSpy output(&client, SIGNAL(publicMessageReceived(QString))); QVERIFY(output.isValid());
        QSignalSpy completed(&client, &ModelClient::completed), failed(&client, &ModelClient::failed);
        client.request(requestPrompt("public-malformed"), exampleSchema());
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 2000); QVERIFY(output.size() >= 2); QCOMPARE(completed.size(), 0);
        output.clear(); client.request(requestPrompt("public-held"), exampleSchema());
        QTRY_COMPARE_WITH_TIMEOUT(output.size(), 1, 1000); client.cancel();
        QTest::qWait(300); QCOMPARE(output.size(), 1); QCOMPARE(completed.size(), 0); QCOMPARE(failed.size(), 1);
    }
    void envLauncherUsesConfiguredEnvironmentWithoutChangingParentPath() {
#ifdef Q_OS_UNIX
        QTemporaryDir dir;const auto tools=dir.filePath("tools");QVERIFY(QDir().mkpath(tools));
        QVERIFY(QFile::link("/bin/sh",QDir(tools).filePath("qvw-codex-test-runtime")));
        auto executable=QCoreApplication::applicationFilePath();executable.replace("'","'\"'\"'");
        const auto launcher=dir.filePath("codex launcher");QFile script(launcher);QVERIFY(script.open(QIODevice::WriteOnly));
        script.write(("#!/usr/bin/env qvw-codex-test-runtime\nexec '"+executable+"' \"$@\"\n").toUtf8());script.close();
        QVERIFY(script.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        const auto parentPath=qgetenv("PATH");auto env=QProcessEnvironment::systemEnvironment();
        env.insert("PATH",tools+":/usr/bin:/bin");env.insert("QVW_MODEL_ENV_TEST","application-config");
        ModelClient client;client.setProgram(launcher);client.setEnvironment(env);
        QSignalSpy completed(&client,&ModelClient::completed),failed(&client,&ModelClient::failed);client.request(requestPrompt(),exampleSchema());
        QTRY_VERIFY_WITH_TIMEOUT(completed.size()+failed.size()>0,3000);QCOMPARE(completed.size(),1);QCOMPARE(failed.size(),0);
        const auto result=qvariant_cast<QJsonObject>(completed.front().front());
        QCOMPARE(result["environmentMarker"].toString(),QString("application-config"));QCOMPARE(result["launchPath"].toString(),env.value("PATH"));
        QCOMPARE(qgetenv("PATH"),parentPath);
#else
        QSKIP("POSIX env launcher test");
#endif
    }
    void runsAsynchronouslyWithIsolatedArgumentsAndStdin()
    {
        QTemporaryDir directory;
        const QString program = directory.filePath("fake codex ; literal");
        QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(), program));
        ModelClient client;
        client.setProgram(program);
        QSignalSpy completed(&client, &ModelClient::completed);
        QSignalSpy failed(&client, &ModelClient::failed);
        QSignalSpy busy(&client, &ModelClient::busyChanged);
        const QString prompt = requestPrompt("good", "中文 ; $(never run)");
        QVERIFY(!client.isBusy());
        client.request(prompt, exampleSchema());
        QVERIFY(client.isBusy());
        QCOMPARE(completed.size(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 3000);
        QCOMPARE(failed.size(), 0);
        QVERIFY(!client.isBusy());
        QCOMPARE(busy.size(), 2);
        QCOMPARE(busy.at(0).at(0).toBool(), true);
        QCOMPARE(busy.at(1).at(0).toBool(), false);
        const auto result = qvariant_cast<QJsonObject>(completed.at(0).at(0));
        QCOMPARE(result.value("seenPrompt").toString(), prompt);
        QCOMPARE(result.value("schema").toObject(), exampleSchema());
        QVERIFY(result.value("instructions").toString().contains("JSON"));
        const QStringList arguments = result.value("arguments").toVariant().toStringList();
        QCOMPARE(arguments.first(), QString("exec"));
        QCOMPARE(arguments.last(), QString("-"));
        for (const auto &flag : {"--ephemeral", "--ignore-user-config", "--ignore-rules", "--skip-git-repo-check", "--json"}) QVERIFY(arguments.contains(flag));
        const int sandboxIndex = arguments.indexOf("--sandbox");
        QVERIFY(sandboxIndex >= 0);
        QCOMPARE(arguments.at(sandboxIndex + 1), QString("read-only"));
        for (const auto &setting : {"features.shell_tool=false", "features.apps=false", "features.hooks=false", "features.multi_agent=false", "features.remote_plugin=false", "features.unified_exec=false", "features.shell_snapshot=false", "features.memories=false", "memories.use_memories=false", "memories.generate_memories=false", "web_search=\"disabled\"", "project_doc_max_bytes=0", "history.persistence=\"none\""}) QVERIFY2(arguments.contains(setting), setting);
        QVERIFY(!arguments.contains("--dangerously-bypass-approvals-and-sandbox"));
        const QString workingDirectory = result.value("workingDirectory").toString();
        QVERIFY(!workingDirectory.isEmpty());
        QVERIFY(workingDirectory != QDir::currentPath());
        const QString schemaPath = arguments.at(arguments.indexOf("--output-schema") + 1);
        const QString resultPath = arguments.at(arguments.indexOf("--output-last-message") + 1);
        QCOMPARE(result.value("schemaDirectory").toString(), workingDirectory);
        QCOMPARE(result.value("resultDirectory").toString(), workingDirectory);
        QCOMPARE(QFileInfo(schemaPath).fileName(), QString("schema.json"));
        QCOMPARE(QFileInfo(resultPath).fileName(), QString("result.json"));
        QVERIFY(!QDir(workingDirectory).exists());
    }

    void rejectsInvalidCliResults_data()
    {
        QTest::addColumn<QString>("mode");
        for (const auto &mode : {"nonzero", "malformed", "empty", "empty-object", "array", "missing", "result-limit", "stdout-limit", "stderr-limit", "tool", "unknown-item", "fragmented-tool", "malformed-event"}) QTest::newRow(mode) << QString(mode);
    }
    void rejectsInvalidCliResults()
    {
        QFETCH(QString, mode);
        ModelClient client;
        client.setProgram(QCoreApplication::applicationFilePath());
        QSignalSpy completed(&client, &ModelClient::completed), failed(&client, &ModelClient::failed);
        client.request(requestPrompt(mode), exampleSchema());
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 3000);
        QCOMPARE(completed.size(), 0);
        QVERIFY(!client.isBusy());
        const auto error = failed.at(0).at(0).toString();
        QVERIFY(!error.isEmpty());
        QVERIFY(!error.contains("private-test-token"));
        QVERIFY(!error.contains("Authorization"));
        if (mode.endsWith("-limit")) QVERIFY(error.contains("限制"));
        if (mode == "nonzero") QVERIFY(error.contains("退出码 23"));
        if (mode == "tool" || mode == "unknown-item" || mode == "fragmented-tool") QVERIFY(error.contains("工具"));
        if (mode == "malformed-event") QVERIFY(error.contains("JSONL"));
        if (mode == "empty-object") QVERIFY(error.contains("空"));
        QTest::qWait(120);
        QCOMPARE(failed.size(), 1);
    }

    void failedStartAndTimeout()
    {
        ModelClient client;
        QSignalSpy failed(&client, &ModelClient::failed), completed(&client, &ModelClient::completed);
        client.setProgram("/nonexistent/model-client-test/codex");
        client.request(requestPrompt(), exampleSchema());
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 1000);
        QVERIFY(!client.isBusy());
        client.setProgram(QCoreApplication::applicationFilePath());
        client.setTimeoutMs(30);
        client.request(requestPrompt("slow"), exampleSchema());
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 2, 1000);
        QVERIFY(failed.at(1).at(0).toString().contains("超时"));
        QVERIFY(!client.isBusy());
        QCOMPARE(completed.size(), 0);
    }

    void reclaimsCancelledStartingFailure_data()
    {
        QTest::addColumn<QString>("action");
        for (const auto &action : {"cancel", "replace", "destroy"}) QTest::newRow(action) << QString(action);
    }
    void reclaimsCancelledStartingFailure()
    {
        QFETCH(QString, action);
        auto client = std::make_unique<ModelClient>();
        client->setProgram("/nonexistent/model-client-test/starting-codex");
        QSignalSpy completed(client.get(), &ModelClient::completed), failed(client.get(), &ModelClient::failed);
        client->request(requestPrompt(), exampleSchema());
        QPointer<QProcess> retiredProcess = client->findChild<QProcess *>();
        QVERIFY(retiredProcess);
        QCOMPARE(retiredProcess->state(), QProcess::Starting);
        QPointer<QTimer> deadlineTimer = retiredProcess->findChild<QTimer *>();
        QVERIFY(deadlineTimer);
        const QString requestDirectory = retiredProcess->workingDirectory();
        QVERIFY(QDir(requestDirectory).exists());
        // Remove leaked test fixtures after a RED assertion; this runs only
        // after the bounded observation and cannot make that assertion pass.
        const auto fixtureCleanup = qScopeGuard([retiredProcess] {
            if (retiredProcess && retiredProcess->state() == QProcess::NotRunning) delete retiredProcess.data();
        });
        if (action == "cancel") {
            client->cancel();
            QVERIFY(!client->isBusy());
        } else if (action == "replace") {
            client->setProgram(QCoreApplication::applicationFilePath());
            client->request(requestPrompt("good", "replacement"), exampleSchema());
        } else {
            client.reset();
        }
        QTRY_VERIFY_WITH_TIMEOUT(!QDir(requestDirectory).exists(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(retiredProcess.isNull(), 1000);
        QVERIFY(deadlineTimer.isNull());
        QCOMPARE(failed.size(), 0);
        if (action == "replace") {
            QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 1000);
            QCOMPARE(qvariant_cast<QJsonObject>(completed.at(0).at(0)).value("title").toString(), QString("replacement"));
        } else {
            QCOMPARE(completed.size(), 0);
        }
    }

    void cancelAndRestartIgnoreOldCallbacks()
    {
        ModelClient client;
        client.setProgram(QCoreApplication::applicationFilePath());
        QSignalSpy completed(&client, &ModelClient::completed), failed(&client, &ModelClient::failed);
        client.request(requestPrompt("slow", "old"), exampleSchema());
        QVERIFY(client.isBusy());
        client.cancel();
        QVERIFY(!client.isBusy());
        client.request(requestPrompt("good", "new"), exampleSchema());
        QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 2000);
        QTest::qWait(600);
        QCOMPARE(completed.size(), 1);
        QCOMPARE(failed.size(), 0);
        QCOMPARE(qvariant_cast<QJsonObject>(completed.at(0).at(0)).value("title").toString(), QString("new"));
        client.cancel();
        QCOMPARE(failed.size(), 0);
    }

    void replacesBusyRequestAndSuppressesReentrantOldCompletion()
    {
        ModelClient client;
        client.setProgram(QCoreApplication::applicationFilePath());
        QSignalSpy completed(&client, &ModelClient::completed), failed(&client, &ModelClient::failed);
        client.request(requestPrompt("slow", "old"), exampleSchema());
        client.request(requestPrompt("good", "replacement"), exampleSchema());
        QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 2000);
        QCOMPARE(qvariant_cast<QJsonObject>(completed.at(0).at(0)).value("title").toString(), QString("replacement"));
        completed.clear();
        bool restarted = false;
        connect(&client, &ModelClient::busyChanged, &client, [&](bool busy) {
            if (!busy && !restarted) {
                restarted = true;
                client.request(requestPrompt("good", "reentrant"), exampleSchema());
            }
        });
        client.request(requestPrompt("good", "superseded"), exampleSchema());
        QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 2000);
        QTest::qWait(100);
        QCOMPARE(completed.size(), 1);
        QCOMPARE(failed.size(), 0);
        QCOMPARE(qvariant_cast<QJsonObject>(completed.at(0).at(0)).value("title").toString(), QString("reentrant"));
    }
};

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (application.arguments().value(1) == "exec") return fakeCodex(application.arguments().mid(1));
    ModelClientTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "ModelClientTest.moc"
