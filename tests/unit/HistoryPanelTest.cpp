#include "ui/HistoryPanel.h"
#include <QTest>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QFont>

using namespace qvw;
class HistoryPanelTest : public QObject {
 Q_OBJECT
private slots:
 void initTestCase() { QApplication::setFont(QFont("PingFang SC")); }
 void currentProjectFilterAndLiveUpdatesPreserveVisibleSelection() {
  ui::HistoryPanel panel; panel.setCurrentProject("/projects/a/workbench.json");
  const QJsonObject first{{"id","agent/1"},{"title","开场"},{"project","/projects/a/workbench.json"}};
  auto second=first; second["id"]="agent/2"; second["title"]="结尾";
  auto other=first; other["id"]="edit/3"; other["project"]="/projects/b/workbench.json";
  panel.showRecords({first,second,other}); auto *tree=panel.findChild<QTreeWidget *>("historyRecords");
  auto *current=panel.findChild<QCheckBox *>("historyCurrentProject"); QVERIFY(current); current->setChecked(true); QCOMPARE(tree->topLevelItemCount(),2);
  tree->setCurrentItem(tree->topLevelItem(1)); second["detail"]="实时更新的结果"; panel.showRecords({other,first,second});
  QCOMPARE(tree->currentItem()->data(0,Qt::UserRole).toJsonObject()["id"].toString(),QString("agent/2"));
  QVERIFY(panel.findChild<QPlainTextEdit *>("historyDetail")->toPlainText().contains("实时更新"));
  panel.findChild<QLineEdit *>("historySearch")->setText("开场"); QCOMPARE(tree->topLevelItemCount(),1);
  QCOMPARE(tree->currentItem()->data(0,Qt::UserRole).toJsonObject()["id"].toString(),QString("agent/1"));
  panel.setCurrentProject("/projects/missing/workbench.json"); QCOMPARE(tree->topLevelItemCount(),0);
  panel.setCurrentProject({}); QVERIFY(!current->isEnabled()); QVERIFY(!current->isChecked()); QCOMPARE(tree->topLevelItemCount(),2);
 }
 void reusesOnlyTheFullStructuredGoalAndHonorsAvailability() {
  QTemporaryDir dir; QFile manifest(dir.filePath("project.json")); QVERIFY(manifest.open(QIODevice::WriteOnly)); manifest.write("{}"); manifest.close();
  ui::HistoryPanel panel; panel.setAvailable(true);
  const QString goal="完整历史目标\n"+QString(100,QChar(u'长'));
  panel.showRecords({QJsonObject{{"id","agent/1"},{"title","被截断的目标…"},{"goal",goal},{"project",manifest.fileName()},{"artifact",manifest.fileName()}}});
  auto *reuse=panel.findChild<QPushButton *>("historyReuseGoal"), *open=panel.findChild<QPushButton *>("historyOpenProject"); QVERIFY(reuse); QVERIFY(open);
  QCOMPARE(reuse->text(),QString("复用目标到当前工程")); QVERIFY(reuse->isEnabled()); QVERIFY(open->isEnabled());
  QSignalSpy reused(&panel,&ui::HistoryPanel::reuseGoalRequested), opened(&panel,&ui::HistoryPanel::openProjectRequested);
  reuse->click(); QCOMPARE(reused.count(),1); QCOMPARE(reused.first().first().toString(),goal); QCOMPARE(opened.count(),0);
  panel.setAvailable(false); QVERIFY(panel.findChild<QPushButton *>("historyPlay")->isEnabled()); QVERIFY(!reuse->isEnabled()); QVERIFY(!open->isEnabled()); reuse->click(); open->click(); QCOMPARE(reused.count(),1); QCOMPARE(opened.count(),0);
  panel.setAvailable(true); panel.findChild<QLineEdit *>("historySearch")->setText("没有匹配的记录");
  QVERIFY(!reuse->isEnabled()); QVERIFY(!open->isEnabled()); reuse->click(); open->click(); QCOMPARE(reused.count(),1); QCOMPARE(opened.count(),0);
 }
 void reusableGoalsUseTheControllersUtf8ByteLimit_data() {
  QTest::addColumn<QString>("goal");
  QTest::newRow("ascii-5000-bytes")<<QString(5000,QChar('x'));
  QTest::newRow("ascii-8192-bytes")<<QString(8192,QChar('x'));
  QTest::newRow("chinese-8190-bytes")<<QString(2730,QChar(u'长'));
  QTest::newRow("exact-whitespace-and-newlines")<<QString("  保留原始目标\n与换行  ");
 }
 void reusableGoalsUseTheControllersUtf8ByteLimit() {
  QFETCH(QString,goal); ui::HistoryPanel panel; panel.setAvailable(true);
  panel.showRecords({QJsonObject{{"id","agent/1"},{"title","截断标题…"},{"goal",goal}}});
  auto *reuse=panel.findChild<QPushButton *>("historyReuseGoal"); QVERIFY(reuse); QVERIFY(reuse->isEnabled());
  QSignalSpy reused(&panel,&ui::HistoryPanel::reuseGoalRequested); reuse->click(); QCOMPARE(reused.count(),1); QCOMPARE(reused.first().first().toString(),goal);
 }
 void legacyOrInvalidGoalsCannotBeReused_data() {
  QTest::addColumn<QJsonValue>("goal");
  QTest::newRow("legacy-missing")<<QJsonValue(QJsonValue::Undefined); QTest::newRow("empty")<<QJsonValue("  ");
  QTest::newRow("wrong-type")<<QJsonValue(42); QTest::newRow("ascii-8193-bytes")<<QJsonValue(QString(8193,QChar('x')));
  QTest::newRow("chinese-8193-bytes")<<QJsonValue(QString(2731,QChar(u'长')));
 }
 void legacyOrInvalidGoalsCannotBeReused() {
  QFETCH(QJsonValue,goal); ui::HistoryPanel panel; panel.setAvailable(true);
  panel.showRecords({QJsonObject{{"id","agent/legacy"},{"title","Agent: 不得从标题拼接…"},{"detail","目标：不得从详情提取"},{"goal",goal}}});
  auto *reuse=panel.findChild<QPushButton *>("historyReuseGoal"); QVERIFY(reuse); QVERIFY(!reuse->isEnabled()); QVERIFY(!reuse->toolTip().isEmpty());
  if(goal.isString()&&goal.toString().toUtf8().size()>8192) { QVERIFY(reuse->toolTip().contains("8192")); QVERIFY(reuse->toolTip().contains("UTF-8")); }
  QSignalSpy reused(&panel,&ui::HistoryPanel::reuseGoalRequested); reuse->click(); QCOMPARE(reused.count(),0);
 }
 void searchesAllUserFacingFieldsCaseInsensitively() {
  ui::HistoryPanel panel;
  panel.showRecords({QJsonObject{{"id","agent/1"},{"title","Travel"},{"projectName","上海"},{"detail","SUNSET"},{"goal","保留完整结尾"}},QJsonObject{{"id","edit/1"},{"title","其它"}}});
  auto *search=panel.findChild<QLineEdit *>("historySearch"); QVERIFY(search);
  auto *tree=panel.findChild<QTreeWidget *>("historyRecords"); QVERIFY(tree);
  for(const auto &term:QStringList{"travel","上海","sunset","完整结尾"}) { search->setText(term); QCOMPARE(tree->topLevelItemCount(),1); QCOMPARE(tree->topLevelItem(0)->data(0,Qt::UserRole).toJsonObject()["id"].toString(),QString("agent/1")); }
  search->setText("不存在"); QCOMPARE(tree->topLevelItemCount(),0);
  auto *count=panel.findChild<QLabel *>("historyCount"); QVERIFY(count); QVERIFY(count->text().contains("0")); QVERIFY(count->text().contains("2"));
  QVERIFY(panel.findChild<QPlainTextEdit *>("historyDetail")->toPlainText().isEmpty());
  search->clear(); QCOMPARE(tree->topLevelItemCount(),2);
 }
 void filtersExplicitAndLegacyKindsWithoutGuessingFromTitles() {
  ui::HistoryPanel panel;
  panel.showRecords({QJsonObject{{"id","agent/1"}},QJsonObject{{"id","edit/1"}},QJsonObject{{"id","export/1"}},QJsonObject{{"id","unknown/1"},{"title","Agent 导出"}},QJsonObject{{"id","edit/2"},{"kind","agent"}}});
  auto *kind=panel.findChild<QComboBox *>("historyKind"); QVERIFY(kind);
  auto *tree=panel.findChild<QTreeWidget *>("historyRecords"); QVERIFY(tree);
  for(const auto &value:QStringList{"agent","edit","export"}) { const int index=kind->findData(value); QVERIFY(index>=0); kind->setCurrentIndex(index); QCOMPARE(tree->topLevelItemCount(),value=="agent"?2:1); }
  kind->setCurrentIndex(kind->findData("all")); QCOMPARE(tree->topLevelItemCount(),5);
 }
};
QTEST_MAIN(HistoryPanelTest)
#include "HistoryPanelTest.moc"
