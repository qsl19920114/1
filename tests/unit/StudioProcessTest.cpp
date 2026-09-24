#include "backend/hypit/StudioProcess.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#ifdef Q_OS_UNIX
#include <signal.h>
#endif

using qvw::backend::hypit::StudioProcess;

class StudioProcessTest : public QObject {
    Q_OBJECT
private:
    QTemporaryDir workspace;
    QString launcher;
    QString run;
    QString runtime;

    bool writeMode(const QByteArray &mode) {
        QFile file(run);
        return file.open(QIODevice::WriteOnly) && file.write(mode) == mode.size();
    }
    bool start(StudioProcess &studio) {
        return studio.start(launcher, workspace.path(), run, runtime);
    }

private slots:
    void initTestCase() {
        QVERIFY(workspace.isValid());
        const QString source = QFINDTESTDATA("../fixtures/studio-process/launcher.py");
        QVERIFY(!source.isEmpty());
        launcher = workspace.filePath("launcher with spaces.py");
        QVERIFY(QFile::copy(source, launcher));
        QVERIFY(QFile::setPermissions(launcher, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        run = workspace.filePath("project run.svrun");
        runtime = workspace.filePath("hypit.runtime.json");
        QFile file(runtime);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{}");
    }

    void fragmentedAnsiAndActualPort() {
        QVERIFY(writeMode("normal"));
        StudioProcess studio;
        QSignalSpy ready(&studio, &StudioProcess::ready);
        QSignalSpy failure(&studio, &StudioProcess::failed);
        QSignalSpy command(&studio, &StudioProcess::commandFinished);
        QVERIFY(start(studio));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 5000);
        QCOMPARE(qvariant_cast<QUrl>(ready.at(0).at(0)), QUrl("http://localhost:5600/"));
        QVERIFY(studio.isRunning());
        QVERIFY(studio.processId() > 0);
        QFile arguments(workspace.filePath("arguments.json"));
        QVERIFY(arguments.open(QIODevice::ReadOnly));
        const auto args = QJsonDocument::fromJson(arguments.readAll()).array();
        QCOMPARE(args, QJsonArray({"studio", "--run", run, "--workspace", workspace.path(),
                                  "--runtime", runtime, "--port", "5599"}));
        studio.stop();
        QVERIFY(!studio.isRunning());
        QCOMPARE(studio.processId(), 0);
        QCOMPARE(ready.size(), 1);
        QCOMPARE(failure.size(), 0);
        QCOMPARE(command.size(), 1);
        QCOMPARE(command.at(0).at(0).toString(), launcher);
    }

    void localAddressVariants_data() {
        QTest::addColumn<QByteArray>("mode");
        QTest::addColumn<QString>("expected");
        QTest::newRow("ipv4-final-line") << QByteArray("tail") << QString("http://127.0.0.1:5612/");
        QTest::newRow("ipv6") << QByteArray("ipv6") << QString("http://[::1]:5613/");
    }
    void localAddressVariants() {
        QFETCH(QByteArray, mode);
        QFETCH(QString, expected);
        QVERIFY(writeMode(mode));
        StudioProcess studio;
        QSignalSpy ready(&studio, &StudioProcess::ready);
        QVERIFY(start(studio));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 5000);
        QCOMPARE(qvariant_cast<QUrl>(ready.at(0).at(0)), QUrl(expected));
        studio.stop();
    }

    void failsBeforeReady_data() {
        QTest::addColumn<QByteArray>("mode");
        QTest::newRow("early-exit") << QByteArray("early");
        QTest::newRow("reject-network-url") << QByteArray("invalid");
        QTest::newRow("ignore-stderr-url") << QByteArray("stderr");
        QTest::newRow("crash") << QByteArray("crash");
    }
    void failsBeforeReady() {
        QFETCH(QByteArray, mode);
        QVERIFY(writeMode(mode));
        StudioProcess studio;
        QSignalSpy ready(&studio, &StudioProcess::ready);
        QSignalSpy failure(&studio, &StudioProcess::failed);
        QSignalSpy finished(&studio, &StudioProcess::finished);
        QSignalSpy output(&studio, &StudioProcess::output);
        QVERIFY(start(studio));
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QCOMPARE(ready.size(), 0);
        QCOMPARE(failure.size(), 1);
        QVERIFY(!failure.at(0).at(0).toString().isEmpty());
        QVERIFY(!studio.isRunning());
        if (mode == "early") {
            QCOMPARE(finished.at(0).at(0).toInt(), 23);
            bool found = false;
            for (const auto &line : output)
                found |= line.at(0).toString().contains("fixture startup rejected");
            QVERIFY(found);
        }
    }

    void validationAndConcurrentStart() {
        QVERIFY(writeMode("normal"));
        StudioProcess studio;
        QSignalSpy failure(&studio, &StudioProcess::failed);
        QVERIFY(!studio.start(launcher, workspace.path(), run, runtime, 0));
        QCOMPARE(failure.size(), 1);
        QVERIFY(!studio.start(launcher, workspace.path(), run, runtime, 65536));
        QVERIFY(!studio.start(launcher + ".missing", workspace.path(), run, runtime));
        QVERIFY(!studio.start(launcher, workspace.path(), run + ".missing", runtime));
        QVERIFY(start(studio));
        QVERIFY(!start(studio));
        studio.stop();
    }

    void missingInterpreterFailsOnceAndCanRestart() {
        const QString broken = workspace.filePath("missing-interpreter");
        QFile file(broken);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("#!/nonexistent/qvw-test-interpreter\n");
        file.close();
        QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(writeMode("tail"));
        StudioProcess studio;
        QSignalSpy failure(&studio, &StudioProcess::failed);
        QSignalSpy finished(&studio, &StudioProcess::finished);
        QSignalSpy command(&studio, &StudioProcess::commandFinished);
        QVERIFY(studio.start(broken, workspace.path(), run, runtime));
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 5000);
        QCOMPARE(failure.size(), 1);
        QCOMPARE(command.size(), 1);
        QCOMPARE(finished.at(0).at(0).toInt(), -1);
        QVERIFY(start(studio));
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, 5000);
        QCOMPARE(failure.size(), 1);
    }

    void startupTimeoutStopsProcess() {
        QVERIFY(writeMode("timeout"));
        StudioProcess studio;
        QSignalSpy failure(&studio, &StudioProcess::failed);
        QSignalSpy finished(&studio, &StudioProcess::finished);
        QVERIFY(start(studio));
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 35000);
        QCOMPARE(failure.size(), 1);
        QVERIFY(failure.at(0).at(0).toString().contains("30 seconds"));
        QVERIFY(!studio.isRunning());
    }

    void stopOwnProcessOnly_data() {
        QTest::addColumn<QByteArray>("mode");
        QTest::newRow("graceful") << QByteArray("normal");
        QTest::newRow("forced") << QByteArray("stubborn");
    }
    void stopOwnProcessOnly() {
#ifdef Q_OS_UNIX
        QFETCH(QByteArray, mode);
        QVERIFY(writeMode(mode));
        QProcess unrelated;
        unrelated.start("/bin/sleep", {"60"});
        QVERIFY(unrelated.waitForStarted());
        qint64 childPid = 0;
        {
            StudioProcess studio;
            QSignalSpy ready(&studio, &StudioProcess::ready);
            QVERIFY(start(studio));
            QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 5000);
            QFile child(workspace.filePath("child.pid"));
            QVERIFY(child.open(QIODevice::ReadOnly));
            childPid = child.readAll().trimmed().toLongLong();
            QVERIFY(childPid > 0);
            QCOMPARE(::kill(pid_t(childPid), 0), 0);
        }
        QTRY_VERIFY_WITH_TIMEOUT(::kill(pid_t(childPid), 0) == -1, 3000);
        QCOMPARE(unrelated.state(), QProcess::Running);
        QCOMPARE(::kill(pid_t(unrelated.processId()), 0), 0);
        unrelated.terminate();
        QVERIFY(unrelated.waitForFinished());
#else
        QSKIP("Unix ownership test");
#endif
    }
};

QTEST_GUILESS_MAIN(StudioProcessTest)
#include "StudioProcessTest.moc"
