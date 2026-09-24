#include "infrastructure/AppConfig.h"
#include "infrastructure/LogWriter.h"
#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class InfrastructureTest : public QObject {
    Q_OBJECT
private slots:
    void pathIsRelativeToRepository() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QDir(tmp.path()).mkpath("repo/config");
        const QString path = tmp.path() + "/repo/config/version-lock.json";
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"hypit":{"distributionPath":"../hypit","launcher":"bin/hypit.mjs","version":"0.2.10"}})");
        file.close();
        const auto result = qvw::infra::loadAppConfig(path);
        QVERIFY2(result.ok(), qPrintable(result.error));
        QCOMPARE(result.config.distributionPath, tmp.path() + "/hypit");
        QCOMPARE(result.config.launcherPath, tmp.path() + "/hypit/bin/hypit.mjs");
    }
    void invalidConfigIsReadableError() {
        QTemporaryDir tmp;
        QFile file(tmp.filePath("bad.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[]"); file.close();
        const auto result = qvw::infra::loadAppConfig(file.fileName());
        QVERIFY(!result.ok());
        QVERIFY(!result.error.isEmpty());
    }
    void logsHaveOneJsonRecordPerEvent() {
        QTemporaryDir tmp;
        const QString path = tmp.filePath("logs/run.jsonl");
        qvw::infra::LogWriter log(path);
        QVERIFY(log.isReady());
        log.info("first\nsecond");
        log.command("/path with spaces/tool", {"--run", "中文 路径/run.svrun"}, 7);
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly));
        const auto lines = file.readAll().trimmed().split('\n');
        QCOMPARE(lines.size(), 2);
        const auto first = QJsonDocument::fromJson(lines[0]).object();
        QCOMPARE(first["message"].toString(), QString("first\nsecond"));
        const auto command = QJsonDocument::fromJson(lines[1]).object();
        QCOMPARE(command["exitCode"].toInt(), 7);
        QCOMPARE(command["arguments"].toArray().at(1).toString(), QString("中文 路径/run.svrun"));
    }
};
QTEST_GUILESS_MAIN(InfrastructureTest)
#include "InfrastructureTest.moc"
