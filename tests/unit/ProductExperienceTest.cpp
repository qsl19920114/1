#include "ui/SceneStrip.h"
#include "ui/MainWindow.h"
#include "ui/HistoryPanel.h"
#include "ui/AssetTree.h"
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTemporaryDir>
#include <QFile>
#include <QListWidget>
#include <QTreeWidget>
#include <QtTest>
using namespace qvw;
class ProductExperienceTest: public QObject {
 Q_OBJECT
private slots:
 void groupsAndActivatesActualComponent() {
  domain::Snapshot snapshot;snapshot.revision=1;snapshot.space.frameRate=30;snapshot.space.frameCount=300;
  domain::Track track;track.clips={{"compiled/a","标题",0,150,{},"a"},{"compiled/image","图片",0,150,{},"image"},{"compiled/b","结尾",150,300,{},"b"}};snapshot.tracks={track};
  ui::SceneStrip strip;strip.setSnapshot(snapshot,{});auto *list=strip.findChild<QListWidget*>("scenes");QVERIFY(list);QCOMPARE(list->count(),2);
  QSignalSpy activated(&strip,&ui::SceneStrip::sceneActivated);list->itemClicked(list->item(1));QCOMPARE(activated.count(),1);QCOMPARE(activated[0][0].toString(),QString("compiled/b"));QCOMPARE(activated[0][1].toInt(),150);
  strip.setPosition(170);QCOMPARE(list->currentRow(),1);strip.setPosition(10);QCOMPARE(list->currentRow(),0);QCOMPARE(activated.count(),1);
 }
 void sceneSelectsComponent() {
  ui::MainWindow window;domain::Snapshot snapshot;snapshot.revision=1;snapshot.space.frameRate=30;snapshot.space.frameCount=300;
  domain::Track track;track.clips={{"real/a","开场",0,150,{},"a"},{"real/b","结尾",150,300,{},"b"}};snapshot.tracks={track};window.showSnapshot(snapshot);
  auto *list=window.findChild<QListWidget*>("scenes");auto *tree=window.findChild<QTreeWidget*>("components");QVERIFY(list);list->itemClicked(list->item(1));QCOMPARE(tree->currentItem()->toolTip(0),QString("real/b"));
 }
 void fileDropUsesBatchSignalAndRejectsNetworkUrls(){
  QTemporaryDir dir;const auto path=dir.filePath("asset.png");QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("bytes are checked by service");file.close();ui::AssetTree tree;tree.show();QSignalSpy dropped(&tree,&ui::AssetTree::filesDropped);QMimeData mime;mime.setUrls({QUrl::fromLocalFile(path)});QDragEnterEvent enter(QPoint(5,5),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(tree.viewport(),&enter);QVERIFY(enter.isAccepted());QDropEvent drop(QPointF(5,5),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(tree.viewport(),&drop);QCOMPARE(dropped.count(),1);QCOMPARE(dropped[0][0].toStringList(),QStringList{path});
  QMimeData remote;remote.setUrls({QUrl("https://example.invalid/video.mp4")});QDragEnterEvent rejected(QPoint(5,5),Qt::CopyAction,&remote,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(tree.viewport(),&rejected);QVERIFY(!rejected.isAccepted());
 }

 void layoutAndHistoryRoundTrip(){
  ui::MainWindow first;first.resize(1500,950);first.findChild<QSplitter*>("workspaceColumns")->setSizes({300,750,450});const auto layout=first.workspaceLayout();ui::MainWindow second;second.restoreWorkspaceLayout(layout);QCOMPARE(second.workspaceLayout()["columns"],layout["columns"]);
  QJsonObject record{{"id","task1"},{"title","任务 · 测试"},{"project","/missing/workbench.qvw.json"},{"detail","旧文案 → 新文案\n完成 2 / 2"},{"artifact","/missing/film.mp4"}};second.showHistory({record});auto *tree=second.findChild<QTreeWidget*>("historyRecords");QCOMPARE(tree->topLevelItemCount(),1);auto *detail=second.findChild<QPlainTextEdit*>("historyDetail");QVERIFY(detail->toPlainText().contains("旧文案 → 新文案"));QVERIFY(detail->toPlainText().contains("文件已移动"));
 }
 void historicalEventsAreActualChanges(){
  ui::MainWindow window;domain::Project project;project.rootPath="/tmp/history-test";window.showDocument(project);QSignalSpy history(&window,&ui::MainWindow::historyRecorded);
  domain::Snapshot snapshot;snapshot.revision=1;snapshot.sourceFingerprint="aaa";domain::Track track;domain::Clip clip;clip.id="exact/id";clip.label="开场";domain::InspectorField field;field.id="text";field.label="标题";field.rawValue="旧标题";clip.inspector={field};track.clips={clip};snapshot.tracks={track};window.showSnapshot(snapshot);QCOMPARE(history.count(),0);
  snapshot.revision=2;snapshot.sourceFingerprint="bbb";snapshot.tracks[0].clips[0].inspector[0].rawValue="新标题";window.showSnapshot(snapshot);QCOMPARE(history.count(),1);QVERIFY(history[0][0].toJsonObject()["detail"].toString().contains("旧标题 → 新标题"));window.showSnapshot(snapshot);QCOMPARE(history.count(),1);
 }
 void replacementUsesSelectedAssetInsteadOfDeselectedCurrentItem(){
  ui::MainWindow window;domain::Project project;project.rootPath="/tmp/library";project.assets={{"a","assets/a.png","a","image/png",1,1,1},{"b","assets/b.png","b","image/png",1,1,1}};window.showDocument(project);window.setBackendAvailable(true);domain::Snapshot snapshot;snapshot.revision=1;domain::Clip clip;clip.id="exact";domain::InspectorField field;field.id="src";field.binding="image";field.writable=true;field.control=domain::ControlKind::Text;clip.inspector={field};domain::Track track;track.clips={clip};snapshot.tracks={track};window.showSnapshot(snapshot);window.setEditorState(true,false,false,false);
  auto *tree=window.findChild<QTreeWidget*>("assets");auto *apply=window.findChild<QPushButton*>("applyAsset");auto *first=tree->topLevelItem(0),*second=tree->topLevelItem(1);tree->setCurrentItem(second);first->setSelected(true);second->setSelected(false);QCOMPARE(tree->selectedItems().size(),1);QCOMPARE(tree->currentItem(),second);QVERIFY(apply->isEnabled());QSignalSpy writes(&window,&ui::MainWindow::editRequested);apply->click();QCOMPARE(writes.size(),1);QCOMPARE(writes[0][2].toString(),QString("./assets/a.png"));
 }

 void batchSelectionCannotApplyAmbiguousAsset(){
  ui::MainWindow window;domain::Project project;project.rootPath="/tmp/library";project.assets={{"a","assets/a.png","a","image/png",1,1,1},{"b","assets/b.png","b","image/png",1,1,1}};window.showDocument(project);window.setBackendAvailable(true);domain::Snapshot snapshot;snapshot.revision=1;domain::Clip clip;clip.id="exact";domain::InspectorField field;field.id="src";field.binding="image";field.writable=true;field.control=domain::ControlKind::Text;clip.inspector={field};domain::Track track;track.clips={clip};snapshot.tracks={track};window.showSnapshot(snapshot);window.setEditorState(true,false,false,false);auto *tree=window.findChild<QTreeWidget*>("assets");auto *apply=window.findChild<QPushButton*>("applyAsset");QVERIFY(apply->isEnabled());tree->topLevelItem(1)->setSelected(true);QVERIFY(!apply->isEnabled());
 }

};
QTEST_MAIN(ProductExperienceTest)
#include "ProductExperienceTest.moc"
