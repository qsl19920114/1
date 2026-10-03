#include "ui/MainWindow.h"
#include "ui/AgentPanel.h"
#include <QTest>
#include <QAction>
#include <QPushButton>
#include <QTreeWidget>
#include <QSignalSpy>
#include <QLineEdit>
#include <QTemporaryFile>
class ProductWindowTest : public QObject {
    Q_OBJECT
    QAction *action(qvw::ui::MainWindow &w,const QString &text) {
        for(auto *a:w.findChildren<QAction*>()) if(a->text()==text)return a;
        return nullptr;
    }
    QPushButton *apply(qvw::ui::MainWindow &w) {
        for(auto *b:w.findChildren<QPushButton*>())if(b->text().startsWith("应用到"))return b;
        return nullptr;
    }
private slots:
    void selectingTrackClearsAgentComponentContext() {
        qvw::ui::MainWindow w;qvw::domain::Snapshot s;s.revision=1;
        qvw::domain::Track track;track.id="track";qvw::domain::Clip clip;clip.id="clip";track.clips.append(clip);s.tracks.append(track);
        QSignalSpy selected(w.agentPanel(),&qvw::ui::AgentPanel::selectionChanged);w.showSnapshot(s);
        QCOMPARE(selected.last().first().toString(),QString("clip"));
        auto *tree=w.findChild<QTreeWidget*>("components");QVERIFY(tree);tree->setCurrentItem(tree->topLevelItem(0));
        QCOMPARE(selected.last().first().toString(),QString());
        tree->setCurrentItem(tree->topLevelItem(0)->child(0));QCOMPARE(selected.last().first().toString(),QString("clip"));
        tree->setCurrentItem(nullptr);QCOMPARE(selected.last().first().toString(),QString());
    }
    void agentIsPrimaryCreationEntry() {
        qvw::ui::MainWindow w;
        QVERIFY(w.findChild<QWidget*>("agentPanel"));
    }
    void closeRequiresSession() {
        qvw::ui::MainWindow w;w.setBackendAvailable(true);
        auto *close=action(w,"关闭工程");QVERIFY(close);QVERIFY(!close->isEnabled());
    }
    void stoppedObservationAllowsSwitchWithoutCallingCancel() {
        qvw::ui::MainWindow w;w.setBackendAvailable(true);qvw::domain::Project p;p.name="test";w.showDocument(p);
        qvw::domain::ExportTask task;task.phase="working";task.active=true;task.buildId="bld_active";w.showExportTask(task);
        auto *close=action(w,"关闭工程");QVERIFY(close);QVERIFY(!close->isEnabled());
        QSignalSpy cancel(&w,&qvw::ui::MainWindow::cancelExportRequested);
        task.phase="stopped";w.showExportTask(task);QVERIFY(close->isEnabled());QVERIFY(action(w,"新建工程…")->isEnabled());QCOMPARE(cancel.size(),0);
    }
    void writingPreventsProjectReplacement() {
        qvw::ui::MainWindow w;w.setBackendAvailable(true);
        w.setEditorState(true,true,false,false);
        auto *create=action(w,"新建工程…");QVERIFY(create);QVERIFY(!create->isEnabled());
        auto *open=action(w,"打开工程…");QVERIFY(open);QVERIFY(!open->isEnabled());
    }
    void applyingRequiresCompatibleSelection() {
        qvw::ui::MainWindow w;w.setBackendAvailable(true);w.setEditorState(true,false,false,false);
        QVERIFY(apply(w));QVERIFY(!apply(w)->isEnabled());
    }
    void matchingVideoAssetEmitsActualBinding() {
        qvw::ui::MainWindow w;w.setBackendAvailable(true);
        qvw::domain::Snapshot s;s.revision=1;qvw::domain::Track t;qvw::domain::Clip c;c.id="clip";
        qvw::domain::InspectorField f;f.id="video-slot";f.binding="video";f.writable=true;f.control=qvw::domain::ControlKind::Text;f.rawValue="./assets/default.png";f.value=f.rawValue.toString();c.inspector.append(f);t.clips.append(c);s.tracks.append(t);w.showSnapshot(s);
        qvw::domain::Project p;p.name="test";qvw::domain::Asset asset;asset.path="assets/movie.mp4";asset.mime="video/mp4";p.assets.append(asset);w.showDocument(p);w.setEditorState(true,false,false,false);
        QSignalSpy writes(&w,&qvw::ui::MainWindow::editRequested);QVERIFY(apply(w)->isEnabled());apply(w)->click();QCOMPARE(writes.size(),1);QCOMPARE(writes.front()[1].toString(),QString("video-slot"));QCOMPARE(writes.front()[2].toString(),QString("./assets/movie.mp4"));
        p.assets.front().mime="image/png";w.showDocument(p);QVERIFY(!apply(w)->isEnabled());
    }
    void queuedEditCannotWriteAfterClose() {
        qvw::ui::MainWindow w;qvw::domain::Snapshot s;s.revision=1;qvw::domain::Track t;qvw::domain::Clip c;c.id="clip";qvw::domain::InspectorField f;f.id="title";f.writable=true;f.control=qvw::domain::ControlKind::Text;f.rawValue="before";f.value="before";c.inspector.append(f);t.clips.append(c);s.tracks.append(t);w.showSnapshot(s);w.setEditorState(true,false,false,false);
        QSignalSpy writes(&w,&qvw::ui::MainWindow::editRequested);QLineEdit *control=nullptr;for(auto *line:w.findChildren<QLineEdit*>())if(line->text()=="before")control=line;QVERIFY(control);control->setText("after");QMetaObject::invokeMethod(control,"editingFinished");w.clearProject();QTest::qWait(10);QCOMPARE(writes.size(),0);
    }
    void playbackRequiresCompletedExistingFilm() {
        qvw::ui::MainWindow w;auto *play=w.findChild<QPushButton*>("playFilm");QVERIFY(play);
        QTemporaryFile file;QVERIFY(file.open());file.write("test film placeholder");file.flush();
        qvw::domain::ExportTask task;task.destination=file.fileName();task.phase="validating";w.showExportTask(task);QVERIFY(!play->isEnabled());task.phase="failed";w.showExportTask(task);QVERIFY(!play->isEnabled());task.phase="complete";w.showExportTask(task);QVERIFY(play->isEnabled());file.remove();w.showExportTask(task);QVERIFY(!play->isEnabled());
    }
    void errorPreservesCompiledDocument() {
        qvw::ui::MainWindow w;qvw::domain::Snapshot s;s.revision=1;
        qvw::domain::Track track;track.id="t";qvw::domain::Clip clip;clip.id="clip";track.clips.append(clip);s.tracks.append(track);
        w.showSnapshot(s);w.showError("Failed to open another file");
        QTreeWidget *components=nullptr;for(auto *tree:w.findChildren<QTreeWidget*>())if(tree->headerItem()->text(0)=="组件")components=tree;
        QVERIFY(components);QCOMPARE(components->topLevelItemCount(),1);
    }
};
QTEST_MAIN(ProductWindowTest)
#include "ProductWindowTest.moc"
