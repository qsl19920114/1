#include "infrastructure/HypitProbe.h"
#include "infrastructure/LogWriter.h"
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
class HypitProbeTest : public QObject {
    Q_OBJECT
private slots:
    void detectsFailures_data() {
        QTest::addColumn<QByteArray>("script"); QTest::addColumn<int>("expected");
        QTest::newRow("success") << QByteArray("printf '%s' '{\"version\":\"0.2.10\"}'") << int(qvw::infra::ProbeStatus::Ok);
        QTest::newRow("mismatch") << QByteArray("printf '%s' '{\"version\":\"9.0\"}'") << int(qvw::infra::ProbeStatus::VersionMismatch);
        QTest::newRow("malformed") << QByteArray("printf '%s' broken") << int(qvw::infra::ProbeStatus::UnreadableOutput);
        QTest::newRow("exit7") << QByteArray("exit 7") << int(qvw::infra::ProbeStatus::InvocationFailed);
        QTest::newRow("timeout") << QByteArray("exec /bin/sleep 3") << int(qvw::infra::ProbeStatus::InvocationFailed);
    }
    void detectsFailures() {
        QFETCH(QByteArray, script); QFETCH(int, expected);
        QTemporaryDir tmp;
        QFile file(tmp.filePath("fake hypit")); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("#!/bin/sh\n" + script + "\n"); file.close();
        QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        qvw::infra::AppConfig config; config.launcherPath = file.fileName(); config.distributionPath = tmp.path(); config.expectedHypitVersion = "0.2.10";
        qvw::infra::LogWriter log(tmp.filePath("log.jsonl"));
        qvw::infra::HypitProbe probe; probe.setTimeoutMs(300);
        QSignalSpy done(&probe, &qvw::infra::HypitProbe::completed);
        probe.start(config, log);
        QTRY_COMPARE(done.size(), 1);
        const auto result = done[0][0].value<qvw::infra::ProbeResult>();
        QCOMPARE(int(result.status), expected); QVERIFY(!result.message.isEmpty());
    }
    void missingLauncher() {
        QTemporaryDir tmp; qvw::infra::LogWriter log(tmp.filePath("log.jsonl"));
        qvw::infra::AppConfig config; config.launcherPath = tmp.filePath("missing");
        qvw::infra::HypitProbe probe;
        QSignalSpy done(&probe, &qvw::infra::HypitProbe::completed);
        probe.start(config, log); QCOMPARE(done.size(), 1);
        QCOMPARE(done[0][0].value<qvw::infra::ProbeResult>().status, qvw::infra::ProbeStatus::LauncherMissing);
    }
};
QTEST_GUILESS_MAIN(HypitProbeTest)
#include "HypitProbeTest.moc"
