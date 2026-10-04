#include "ui/ComponentBrowser.h"
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QtTest>
using namespace qvw;
class ComponentBrowserTest : public QObject {
 Q_OBJECT
 static domain::Snapshot snapshot() {
  domain::Snapshot s;s.revision=1;
  domain::InspectorField edit;edit.writable=true;edit.control=domain::ControlKind::Text;
  domain::Track first;first.id="track/alpha";first.label="主轨道";
  first.clips={{"compiled/a","标题",0,150,{edit},"a"},{"compiled/b","图片",150,300,{},"b"}};
  domain::Track second;second.id="track/beta";second.label="结尾轨道";
  second.clips={{"compiled/c","标题",300,450,{edit},"c"}};s.tracks={first,second};return s;
 }
private slots:
 void searchAndEditableMatchActualFields() {
  ui::ComponentBrowser browser;browser.setSnapshot(snapshot());
  auto *search=browser.findChild<QLineEdit*>("componentSearch");auto *filter=browser.findChild<QComboBox*>("componentFilter");auto *count=browser.findChild<QLabel*>("componentCount");
  QVERIFY(search);QVERIFY(filter);QVERIFY(count);QVERIFY(!search->accessibleName().isEmpty());
  search->setText("  COMPILED/B  ");QVERIFY(count->text().contains("1 / 3"));QVERIFY(!browser.tree()->topLevelItem(0)->child(1)->isHidden());QVERIFY(browser.tree()->topLevelItem(1)->isHidden());
  search->setText("主轨道");QVERIFY(count->text().contains("2 / 3"));filter->setCurrentIndex(filter->findData("editable"));QVERIFY(count->text().contains("1 / 3"));
  search->setText("track/beta");QVERIFY(count->text().contains("1 / 3"));
  search->setText("absent");QVERIFY(count->text().contains("0 / 3"));QVERIFY(!browser.findChild<QLabel*>("componentEmpty")->isHidden());
  browser.findChild<QPushButton*>("clearComponentFilters")->click();QVERIFY(count->text().contains("3 / 3"));QCOMPARE(filter->currentData().toString(),QString("all"));
 }
 void refreshPreservesIdentityAndHiddenSelectionStaysCleared() {
  ui::ComponentBrowser browser;auto s=snapshot();browser.setSnapshot(s);QVERIFY(browser.selectEntity("compiled/b"));
  QSignalSpy activation(&browser,&ui::ComponentBrowser::componentActivated);
  auto *search=browser.findChild<QLineEdit*>("componentSearch");search->setText("主轨道");
  std::swap(s.tracks[0].clips[0],s.tracks[0].clips[1]);browser.setSnapshot(s);QCOMPARE(browser.selectedEntityId(),QString("compiled/b"));QCOMPARE(browser.tree()->currentItem()->data(0,Qt::UserRole+1).toInt(),0);
  search->setText("compiled/a");QVERIFY(browser.selectedEntityId().isEmpty());QVERIFY(browser.tree()->selectedItems().isEmpty());browser.setSnapshot(s);QVERIFY(browser.selectedEntityId().isEmpty());QCOMPARE(search->text(),QString("compiled/a"));
  QVERIFY(browser.selectEntity("compiled/b"));QCOMPARE(search->text(),QString());QCOMPARE(browser.selectedEntityId(),QString("compiled/b"));QCOMPARE(activation.count(),0);
  s.tracks[0].clips.removeFirst();browser.setSnapshot(s);QVERIFY(browser.selectedEntityId().isEmpty());QCOMPARE(activation.count(),0);
 }
 void enterClickAndArrowsUseExactVisibleIdentity() {
  ui::ComponentBrowser browser;browser.resize(500,500);browser.show();browser.setSnapshot(snapshot());QSignalSpy activation(&browser,&ui::ComponentBrowser::componentActivated);
  auto *search=browser.findChild<QLineEdit*>("componentSearch");search->setText("标题");search->setFocus();QTest::keyClick(search,Qt::Key_Return);QCOMPARE(activation.count(),1);QCOMPARE(activation[0][0].toString(),QString("compiled/a"));QCOMPARE(activation[0][1].toInt(),0);
  auto *tree=browser.tree();tree->setFocus();QTest::keyClick(tree,Qt::Key_Down);QCOMPARE(browser.selectedEntityId(),QString("compiled/c"));QCOMPARE(activation.count(),1);QTest::keyClick(tree,Qt::Key_Return);QCOMPARE(activation.count(),2);QCOMPARE(activation[1][0].toString(),QString("compiled/c"));QCOMPARE(activation[1][1].toInt(),300);
  auto *item=tree->topLevelItem(0)->child(0);QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(item).center());QCOMPARE(activation.count(),3);QCOMPARE(activation[2][0].toString(),QString("compiled/a"));
  search->setText("missing");QTest::keyClick(tree,Qt::Key_Return);QCOMPARE(activation.count(),3);
 }
};
QTEST_MAIN(ComponentBrowserTest)
#include "ComponentBrowserTest.moc"
