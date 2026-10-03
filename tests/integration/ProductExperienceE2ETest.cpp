#include "controllers/DocumentController.h"
#include "controllers/ProjectController.h"
#include "controllers/EditorController.h"
#include "controllers/ExportController.h"
#include "infrastructure/WorkspaceStore.h"
#include "infrastructure/AppConfig.h"
#include "infrastructure/LogWriter.h"
#include "services/ProjectStore.h"
#include "services/AssetService.h"
#include "services/SampleCatalog.h"
#include "ui/MainWindow.h"
#include <QtTest>
#include <QDateTime>
#include <QDirIterator>
#include <QListWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QSlider>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QJsonDocument>
#include <QSaveFile>
#include <QImage>
using namespace qvw;
class ProductExperienceE2ETest:public QObject {
 Q_OBJECT
private slots:
 void linkedScenesAndRememberedWork(){
  const QString repo=QVW_SOURCE_DIR;const auto evidence=repo+"/docs/evidence/m11";QVERIFY(QDir().mkpath(evidence));
  const auto source=qEnvironmentVariable("QVW_EXPERIENCE_PROJECT",repo+"/.workbench/agent-creation-20261002-234036-336/校园故事工程");
  const auto root=repo+"/.workbench/deliverables-m11/创作体验 "+QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");QVERIFY(QDir().mkpath(root));
  QDirIterator files(source,QDir::Files,QDirIterator::Subdirectories);while(files.hasNext()){const auto file=files.next();const auto relative=QDir(source).relativeFilePath(file);if(relative.startsWith(".workbench/")||relative.startsWith(".hypit/"))continue;QVERIFY(!files.fileInfo().isSymLink());const auto dest=QDir(root).filePath(relative);QVERIFY(QDir().mkpath(QFileInfo(dest).absolutePath()));QVERIFY(QFile::copy(file,dest));}
  auto loaded=infra::loadAppConfig(repo+"/.workbench/local-agent-config.json");QVERIFY2(loaded.ok(),qPrintable(loaded.error));infra::LogWriter log(evidence+"/experience-run.jsonl");
  controllers::DocumentController document;document.setMediaTools(loaded.config.ffprobePath,loaded.config.ffmpegPath,loaded.config.processEnvironment);controllers::ProjectController backend(loaded.config,log);controllers::EditorController editor;controllers::ExportController exporter(loaded.config,log);ui::MainWindow window;
  infra::WorkspaceStore workspace(root+"/.workbench/experience-workspace.json");
  connect(&window,&ui::MainWindow::historyRecorded,&window,[&](const QJsonObject &record){workspace.record(record);window.showHistory(workspace.history());});
  connect(&window,&ui::MainWindow::playheadChanged,&window,[&](const QString &path,int frame){workspace.setFrame(path,frame);});
  connect(&document,&controllers::DocumentController::projectChanged,&window,&ui::MainWindow::showDocument);
  connect(&document,&controllers::DocumentController::projectLoaded,&backend,[&](const auto &p){backend.openDocument(p,5794);workspace.visit(p.manifestPath(),p.name);});
  connect(&document,&controllers::DocumentController::projectLoaded,&exporter,&controllers::ExportController::setProject);
  connect(&backend,&controllers::ProjectController::projectClosed,&window,&ui::MainWindow::clearProject);connect(&backend,&controllers::ProjectController::projectClosed,&editor,&controllers::EditorController::clear);
  connect(&backend,&controllers::ProjectController::availableChanged,&window,&ui::MainWindow::setBackendAvailable);
  connect(&backend,&controllers::ProjectController::previewRequested,&window,&ui::MainWindow::showPreview);connect(&backend,&controllers::ProjectController::previewRequested,&editor,[&](const QUrl &url){editor.attach(url,backend.workspace());});
  connect(&backend,&controllers::ProjectController::snapshotReady,&editor,&controllers::EditorController::acceptSnapshot);connect(&editor,&controllers::EditorController::snapshotReady,&window,&ui::MainWindow::showSnapshot);connect(&editor,&controllers::EditorController::stateChanged,&window,&ui::MainWindow::setEditorState);connect(&window,&ui::MainWindow::editRequested,&editor,&controllers::EditorController::edit);
  connect(&exporter,&controllers::ExportController::taskChanged,&window,&ui::MainWindow::showExportTask);
  QSignalSpy initialized(&backend,&controllers::ProjectController::initialized),errors(&backend,&controllers::ProjectController::failed),writes(&editor,&controllers::EditorController::operationSucceeded),editErrors(&editor,&controllers::EditorController::failed),done(&exporter,&controllers::ExportController::completed),exportErrors(&exporter,&controllers::ExportController::failed);
  struct Cleanup{controllers::ProjectController &b;~Cleanup(){b.closeProject();}}cleanup{backend};
  window.show();backend.initialize();QTRY_VERIFY_WITH_TIMEOUT(initialized.count()||errors.count(),30000);QVERIFY(errors.isEmpty());QVERIFY(document.open(root+"/workbench.qvw.json"));
  auto *play=window.findChild<QPushButton*>("nativePlay");QTRY_VERIFY_WITH_TIMEOUT(play->isEnabled()||errors.count(),30000);QVERIFY2(errors.isEmpty(),qPrintable(errors.isEmpty()?QString():errors.last()[0].toString()));QTRY_VERIFY_WITH_TIMEOUT(window.previewVersion().isConfirmed(),30000);
  auto *scenes=window.findChild<QListWidget*>("scenes");QVERIFY(scenes->count()>=3);auto *target=scenes->item(scenes->count()-1);const auto entity=target->data(Qt::UserRole).toString();const auto frame=target->data(Qt::UserRole+1).toInt();scenes->itemClicked(target);
  auto *tree=window.findChild<QTreeWidget*>("components");QCOMPARE(tree->currentItem()->toolTip(0),entity);auto *slider=window.findChild<QSlider*>("nativeSeek");QTRY_VERIFY_WITH_TIMEOUT(qAbs(slider->value()-frame)<=1,10000);
  const auto remembered=qMax(0,frame+10);slider->setValue(remembered);QMetaObject::invokeMethod(slider,"sliderReleased");QTRY_COMPARE_WITH_TIMEOUT(workspace.frame(document.project().manifestPath()),remembered,10000);
  QImage image(200,300,QImage::Format_RGB32);image.fill(QColor("#1d6c89"));const auto imported=root+"/新素材.png";QVERIFY(image.save(imported));const auto samples=services::SampleCatalog::discover(loaded.config.distributionPath);QVERIFY(!samples.isEmpty()&&samples.first().available);QSignalSpy batch(&document,&controllers::DocumentController::importBatchFinished);QVERIFY(document.importFiles({imported,samples.first().path,root+"/不存在.mp4"}));QTRY_COMPARE_WITH_TIMEOUT(batch.count(),1,30000);QCOMPARE(batch[0][0].toInt(),2);QCOMPARE(batch[0][1].toInt(),3);QCOMPARE(batch[0][2].toStringList().size(),1);domain::Asset newImage;for(const auto &asset:document.project().assets)if(asset.originalName=="新素材.png")newImage=asset;QVERIFY(!newImage.hash.isEmpty());window.selectImportedAsset(newImage);auto *apply=window.findChild<QPushButton*>("applyAsset");QVERIFY(apply->isEnabled());apply->click();QTRY_VERIFY_WITH_TIMEOUT(writes.count()||editErrors.count(),15000);QVERIFY2(editErrors.isEmpty(),qPrintable(editErrors.isEmpty()?QString():editErrors.last()[0].toString()));QTRY_VERIFY_WITH_TIMEOUT(window.previewVersion().sourceFingerprint==editor.snapshot().sourceFingerprint,20000);QVERIFY(!workspace.history().isEmpty());
  workspace.setLayout(window.workspaceLayout());QString error;QVERIFY2(workspace.save(&error),qPrintable(error));infra::WorkspaceStore reopened(root+"/.workbench/experience-workspace.json");QVERIFY(reopened.load(&error));QCOMPARE(reopened.frame(document.project().manifestPath()),remembered);
  const auto output=root+"/FrameLab-创作体验.mp4";exporter.startExport(editor.snapshot(),output);QTRY_VERIFY_WITH_TIMEOUT(done.count()||exportErrors.count(),120000);QVERIFY2(exportErrors.isEmpty(),qPrintable(exportErrors.isEmpty()?QString():exportErrors.last()[0].toString()));QCOMPARE(done.count(),1);QVERIFY(QFileInfo(output).size()>1000);
  const auto history=workspace.history();bool film=false,edit=false;for(const auto &v:history){const auto r=v.toObject();film|=r["artifact"]==output;edit|=r["id"].toString().startsWith("edit/");}QVERIFY(film&&edit);
  const auto manifest=document.project().manifestPath();backend.closeProject();document.close();window.clearDocument();const auto lost=QDir(root).filePath(newImage.path);QVERIFY(QFile::remove(lost));domain::Project repair;QVector<domain::Asset> missing;QVERIFY(services::ProjectStore::inspectMissingAssets(manifest,&repair,&missing,&error));QCOMPARE(missing.size(),1);QVERIFY(services::AssetService::restoreMissing(repair,missing.first(),imported,&error));QVERIFY(document.open(manifest));window.restorePlayhead(remembered);QTRY_VERIFY_WITH_TIMEOUT(play->isEnabled(),30000);QTRY_VERIFY_WITH_TIMEOUT(qAbs(slider->value()-remembered)<=1,10000);QTRY_VERIFY_WITH_TIMEOUT(window.previewVersion().isConfirmed(),30000);
  QJsonObject report{{"verdict","PASS"},{"project",manifest},{"artifact",output},{"sceneCount",scenes->count()},{"selectedEntity",entity},{"sceneFrame",frame},{"restoredFrame",slider->value()},{"historyCount",history.size()},{"previewFingerprint",QString::fromLatin1(window.previewVersion().sourceFingerprint.toHex())},{"realAssetWrite",writes.count()},{"batchSuccess",batch[0][0].toInt()},{"batchTotal",batch[0][1].toInt()},{"missingAssetRestored",true}};QSaveFile file(evidence+"/experience-e2e.json");QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(report).toJson());QVERIFY(file.commit());backend.closeProject();QVERIFY(!backend.isRunning());
 }
};
QTEST_MAIN(ProductExperienceE2ETest)
#include "ProductExperienceE2ETest.moc"
