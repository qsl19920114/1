#include "infrastructure/WorkspaceStore.h"
#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QtTest>
using qvw::infra::WorkspaceStore;
class WorkspaceStoreTest:public QObject {
 Q_OBJECT
private slots:
 void roundTripBoundedHistory(){QTemporaryDir dir;const auto path=dir.filePath("workspace.json");WorkspaceStore store(path);store.setLayout({{"geometry","abc"}});store.visit("/tmp/a/workbench.qvw.json","A");store.setFrame("/tmp/a/workbench.qvw.json",145);QString error;
  for(int i=0;i<205;++i)store.record({{"id",QString::number(i)},{"title","修改"},{"detail","old → new"}});
  QVERIFY2(store.save(&error),qPrintable(error));WorkspaceStore reopened(path);QVERIFY(reopened.load(&error));QCOMPARE(reopened.layout()["geometry"].toString(),QString("abc"));QCOMPARE(reopened.frame("/tmp/a/workbench.qvw.json"),145);QCOMPARE(reopened.history().size(),200);QCOMPARE(reopened.recent().size(),1);
  reopened.record({{"id","204"},{"title","完成"}});QCOMPARE(reopened.history().size(),200);QCOMPARE(reopened.history()[0].toObject()["title"].toString(),QString("完成"));
 }
 void longStructuredGoalSurvivesSave(){
  QTemporaryDir dir;WorkspaceStore store(dir.filePath("workspace.json"));const QString goal(4096,u'长');
  store.record({{"id","agent/long"},{"kind","agent"},{"goal",goal},{"detail",QString(8000,u'文')}});
  QCOMPARE(store.history().size(),1);QString error;QVERIFY2(store.save(&error),qPrintable(error));WorkspaceStore reopened(dir.filePath("workspace.json"));QVERIFY(reopened.load(&error));QCOMPARE(reopened.history().first().toObject()["goal"].toString(),goal);
 }
 void escapedGoalsPreservedWithinTotalBudget(){
  QTemporaryDir dir;const auto path=dir.filePath("workspace.json");WorkspaceStore store(path);const QString quotes(8192,u'"');
  store.record({{"id","agent/quotes"},{"goal",quotes}});QCOMPARE(store.history().size(),1);QString error;QVERIFY2(store.save(&error),qPrintable(error));WorkspaceStore reopened(path);QVERIFY(reopened.load(&error));QCOMPARE(reopened.history().first().toObject()["goal"].toString(),quotes);
  const QString escaped=QStringLiteral("x")+QString(8191,QChar(1));for(int i=0;i<205;++i)store.record({{"id",QString("agent/%1").arg(i)},{"goal",escaped}});
  QVERIFY(store.history().size()>1);QVERIFY(store.history().size()<=200);QCOMPARE(store.history().first().toObject()["goal"].toString(),escaped);QVERIFY2(store.save(&error),qPrintable(error));QVERIFY(QFileInfo(path).size()<4*1024*1024);WorkspaceStore bounded(path);QVERIFY(bounded.load(&error));QCOMPARE(bounded.history(),store.history());
 }
 void corruptInputAndSymlink(){QTemporaryDir dir;const auto path=dir.filePath("bad.json");QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("not json");file.close();WorkspaceStore store(path);QString error;QVERIFY(!store.load(&error));QVERIFY(store.history().isEmpty());
  const auto link=dir.filePath("link.json");QVERIFY(QFile::link(path,link));WorkspaceStore linked(link);QVERIFY(!linked.load(&error));QVERIFY(!linked.save(&error));QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),QByteArray("not json"));
 }
};
QTEST_GUILESS_MAIN(WorkspaceStoreTest)
#include "WorkspaceStoreTest.moc"
