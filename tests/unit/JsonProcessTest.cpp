#include "infrastructure/JsonProcess.h"
#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
using qvw::infra::JsonProcess;
class JsonProcessTest:public QObject {
    Q_OBJECT
private slots:
    void nonzeroJsonIsAvailable() {
        QTemporaryDir dir;JsonProcess p;QSignalSpy result(&p,&JsonProcess::result),failed(&p,&JsonProcess::failed);
        QVERIFY(p.start("/usr/bin/python3",{"-c","import sys; print('{\"state\":\"failed\"}'); sys.exit(1)"},dir.path()));
        QTRY_COMPARE_WITH_TIMEOUT(result.size(),1,1000);QCOMPARE(failed.size(),0);QCOMPARE(result[0][1].toInt(),1);
        QCOMPARE(qvariant_cast<QJsonObject>(result[0][0])["state"].toString(),QString("failed"));
    }
    void rejectsNonObject_data() {
        QTest::addColumn<QString>("output");QTest::newRow("array")<<QString("[]");QTest::newRow("noise")<<QString("noise\n{}");QTest::newRow("truncated")<<QString("{");
    }
    void rejectsNonObject() {
        QFETCH(QString,output);QTemporaryDir dir;JsonProcess p;QSignalSpy result(&p,&JsonProcess::result),failed(&p,&JsonProcess::failed);
        const auto literal=QString::fromUtf8(QJsonDocument(QJsonArray{output}).toJson(QJsonDocument::Compact));
        QVERIFY(p.start("/usr/bin/python3",{"-c","import json; print(json.loads('"+literal+"')[0])"},dir.path()));
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(),1,1000);QCOMPARE(result.size(),0);
    }
    void boundsOutput() {
        QTemporaryDir dir;JsonProcess p;QSignalSpy failed(&p,&JsonProcess::failed),result(&p,&JsonProcess::result);
        QVERIFY(p.start("/usr/bin/python3",{"-c","print('x'*5000000)"},dir.path()));
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(),1,2000);QCOMPARE(result.size(),0);
    }
    void timeout() {
        QTemporaryDir dir;JsonProcess p;QSignalSpy failed(&p,&JsonProcess::failed);
        QVERIFY(p.start("/usr/bin/python3",{"-c","import time; time.sleep(2)"},dir.path(),30));
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(),1,1000);
    }
    void cancellationIsolatesNextInvocation() {
        QTemporaryDir dir;JsonProcess p;QSignalSpy failed(&p,&JsonProcess::failed),result(&p,&JsonProcess::result);
        QVERIFY(p.start("/usr/bin/python3",{"-c","import time; time.sleep(1); print('{\"old\":true}')"},dir.path()));p.cancel();
        QVERIFY(p.start("/usr/bin/python3",{"-c","print('{\"new\":true}')"},dir.path()));
        QTRY_COMPARE_WITH_TIMEOUT(result.size(),1,1500);QTest::qWait(80);QCOMPARE(failed.size(),0);QVERIFY(qvariant_cast<QJsonObject>(result[0][0])["new"].toBool());
    }
    void reentrantCommandCallbackIsolatesOldResult() {
        QTemporaryDir dir;JsonProcess p;QSignalSpy result(&p,&JsonProcess::result);bool switched=false;
        connect(&p,&JsonProcess::commandFinished,&p,[&](const QString &,const QStringList &,int){if(switched)return;switched=true;p.cancel();p.start("/usr/bin/python3",{"-c","print('{\"new\":true}')"},dir.path());});
        QVERIFY(p.start("/usr/bin/python3",{"-c","print('{\"old\":true}')"},dir.path()));
        QTRY_COMPARE_WITH_TIMEOUT(result.size(),1,1000);QTest::qWait(80);QCOMPARE(result.size(),1);QVERIFY(qvariant_cast<QJsonObject>(result[0][0])["new"].toBool());
    }
};
QTEST_GUILESS_MAIN(JsonProcessTest)
#include "JsonProcessTest.moc"
