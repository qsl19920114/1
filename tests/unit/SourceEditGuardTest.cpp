#include "services/SourceEditGuard.h"
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
class SourceEditGuardTest : public QObject {
    Q_OBJECT
    void write(const QString &path,const QByteArray &bytes){QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(bytes),bytes.size());}
    QByteArray read(const QString &path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
private slots:
    void restoresOnlyItsOwnFailedWrite() {
        QTemporaryDir tmp;const auto file=tmp.filePath("main.svml");write(file,"before");
        qvw::domain::Snapshot snapshot;snapshot.sourceFiles.insert("main.svml","before");
        qvw::services::SourceEditGuard guard;QString error;
        QVERIFY2(guard.arm(tmp.path(),snapshot,"main.svml","broken",&error),qPrintable(error));
        QCOMPARE(read(file),QByteArray("before"));write(file,"broken");
        QVERIFY2(guard.restore(&error),qPrintable(error));QCOMPARE(read(file),QByteArray("before"));
        QVERIFY(!guard.restore(&error));
    }
    void protectsExternalChangesAndWhitelist() {
        QTemporaryDir tmp;const auto file=tmp.filePath("main.svml");write(file,"before");
        qvw::domain::Snapshot snapshot;snapshot.sourceFiles.insert("main.svml","before");
        qvw::services::SourceEditGuard guard;QString error;
        QVERIFY(!guard.arm(tmp.path(),snapshot,"../main.svml","broken",&error));
        QVERIFY(!guard.arm(tmp.path(),snapshot,"unlisted","broken",&error));
        QVERIFY(guard.arm(tmp.path(),snapshot,"main.svml","broken",&error));
        write(file,"external edit");QVERIFY(!guard.restore(&error));QCOMPARE(read(file),QByteArray("external edit"));
        QVERIFY(!guard.arm(tmp.path(),snapshot,"main.svml","broken",&error));
    }
    void detectsReplacedSymlinkBeforeRestore() {
        QTemporaryDir tmp,other;const auto file=tmp.filePath("main.svml"),outside=other.filePath("outside");
        write(file,"before");write(outside,"broken");
        qvw::domain::Snapshot snapshot;snapshot.sourceFiles.insert("main.svml","before");
        qvw::services::SourceEditGuard guard;QString error;
        QVERIFY(guard.arm(tmp.path(),snapshot,"main.svml","broken",&error));
        QVERIFY(QFile::remove(file));QVERIFY(QFile::link(outside,file));
        QVERIFY(!guard.restore(&error));QCOMPARE(read(outside),QByteArray("broken"));
    }
};
QTEST_GUILESS_MAIN(SourceEditGuardTest)
#include "SourceEditGuardTest.moc"
