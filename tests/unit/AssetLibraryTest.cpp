#include "controllers/DocumentController.h"
#include "services/ProjectStore.h"
#include "services/AssetService.h"
#include <QtTest>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QImage>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif
using namespace qvw;
namespace {
bool writeBytes(const QString &path, const QByteArray &bytes) { QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size(); }
QByteArray readBytes(const QString &path) { QFile file(path); return file.open(QIODevice::ReadOnly)?file.readAll():QByteArray(); }
QString python(const QString &path, const QString &body) {
    if(!writeBytes(path,("#!/usr/bin/python3\n"+body+"\n").toUtf8()))return {};
    QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);return path;
}
QString fakeProbe(const QString &path) {
    // These unit fixtures exercise queue orchestration; real video acceptance is tested separately.
    return python(path,QString::fromUtf8(R"PY(import json,sys
if '-count_frames' in sys.argv:
 print(json.dumps({'streams':[{'codec_type':'video','codec_name':'h264','width':720,'height':1280,'avg_frame_rate':'30/1','r_frame_rate':'30/1','duration':'8.0','nb_read_frames':'240'}],'format':{'format_name':'mov,mp4,m4a,3gp,3g2,mj2','duration':'8.0','tags':{'major_brand':'isom'}}}))
else:
 print(json.dumps({'frames':[{'best_effort_timestamp_time':format(i/30,'.6f')} for i in range(240)]}))
)PY"));
}
}
class AssetLibraryTest:public QObject {
 Q_OBJECT
private slots:
 void recoveryResetCannotPublishClosedProject(){
  QTemporaryDir dir;controllers::DocumentController doc;QSignalSpy loaded(&doc,&controllers::DocumentController::projectLoaded),changed(&doc,&controllers::DocumentController::projectChanged);
  bool closed=false;connect(&doc,&controllers::DocumentController::importReportReady,&doc,[&](const QJsonObject &r){if(r.isEmpty()&&!closed){closed=true;doc.close();}});
  doc.create(QString(QVW_SOURCE_DIR)+"/templates/title-card",dir.filePath("project"),"closed");QVERIFY(closed);QCOMPARE(loaded.size(),0);QCOMPARE(changed.size(),0);
 }
 void missingOriginalCanBeRestoredButDifferentContentCannot(){
  QTemporaryDir dir;controllers::DocumentController doc;QVERIFY(doc.create(QString(QVW_SOURCE_DIR)+"/templates/title-card",dir.filePath("project"),"repair"));QImage image(10,10,QImage::Format_RGB32);image.fill(Qt::red);const auto original=dir.filePath("original.png");QVERIFY(image.save(original));QVERIFY(doc.importImage(original));const auto asset=doc.project().assets.last();const auto path=QDir(doc.project().rootPath).filePath(asset.path);QVERIFY(QFile::remove(path));domain::Project inspected;QVector<domain::Asset> missing;QString error;QVERIFY(!services::ProjectStore::load(doc.project().manifestPath(),&inspected,&error));
  QVERIFY(services::ProjectStore::inspectMissingAssets(doc.project().manifestPath(),&inspected,&missing,&error));QCOMPARE(missing.size(),1);image.fill(Qt::blue);const auto changed=dir.filePath("changed.png");QVERIFY(image.save(changed));QVERIFY(!services::AssetService::restoreMissing(inspected,asset,changed,&error));QVERIFY(!QFileInfo::exists(path));QVERIFY2(services::AssetService::restoreMissing(inspected,asset,original,&error),qPrintable(error));QVERIFY(services::ProjectStore::load(inspected.manifestPath(),&inspected,&error));QVERIFY(!services::AssetService::restoreMissing(inspected,asset,changed,&error));
 }
 void mixedBatchPreservesSuccessfulPrefixAndContinues(){
  QTemporaryDir dir;controllers::DocumentController doc;QVERIFY(doc.create(QString(QVW_SOURCE_DIR)+"/templates/title-card",dir.filePath("project"),"batch"));QImage image(10,10,QImage::Format_RGB32);image.fill(Qt::red);const auto first=dir.filePath("first.png"),last=dir.filePath("last.png");QVERIFY(image.save(first));image.fill(Qt::blue);QVERIFY(image.save(last));
  QSignalSpy finished(&doc,&controllers::DocumentController::importBatchFinished);QVERIFY(doc.importFiles({first,dir.filePath("missing.mp4"),last}));QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,10000);QCOMPARE(finished[0][0].toInt(),2);QCOMPARE(finished[0][1].toInt(),3);QCOMPARE(finished[0][2].toStringList().size(),1);QCOMPARE(doc.project().assets.size(),2);QVERIFY(!doc.importingVideo());
 }
 void cancelStopsQueueWithoutUndoingImports(){
  QTemporaryDir dir;controllers::DocumentController doc;QVERIFY(doc.create(QString(QVW_SOURCE_DIR)+"/templates/title-card",dir.filePath("project"),"cancel"));QImage image(10,10,QImage::Format_RGB32);image.fill(Qt::red);const auto first=dir.filePath("first.png"),last=dir.filePath("last.png");QVERIFY(image.save(first));image.fill(Qt::blue);QVERIFY(image.save(last));connect(&doc,&controllers::DocumentController::assetImported,&doc,[&]{doc.cancelVideoImport();});QVERIFY(doc.importFiles({first,last}));QTRY_VERIFY_WITH_TIMEOUT(!doc.importingVideo(),10000);QCOMPARE(doc.project().assets.size(),1);
 }
 void repairRejectsForgedAssetAndSymlink(){
  QTemporaryDir dir;controllers::DocumentController doc;QVERIFY(doc.create(QString(QVW_SOURCE_DIR)+"/templates/title-card",dir.filePath("project"),"repair"));QImage image(10,10,QImage::Format_RGB32);image.fill(Qt::red);const auto source=dir.filePath("source.png");QVERIFY(image.save(source));QVERIFY(doc.importImage(source));auto asset=doc.project().assets.last();QString error;auto forged=asset;forged.path="../escape.png";QVERIFY(!services::AssetService::restoreMissing(doc.project(),forged,source,&error));const auto path=QDir(doc.project().rootPath).filePath(asset.path);QVERIFY(QFile::remove(path));QVERIFY(QFile::link(source,path));QVERIFY(!services::AssetService::restoreMissing(doc.project(),asset,source,&error));
 }
 void inspectionRemainsStrictAndDoesNotMutateOutputsOnFailure(){
  QTemporaryDir dir;controllers::DocumentController doc;QVERIFY(doc.create(QString(QVW_SOURCE_DIR)+"/templates/title-card",dir.filePath("project"),"strict"));
  auto original=doc.project();domain::Project output;output.name="sentinel";QVector<domain::Asset> missing(1);missing[0].path="sentinel";QString error;
  QVERIFY(QFile::remove(QDir(original.rootPath).filePath(original.sourcePath)));
  QVERIFY(!services::ProjectStore::inspectMissingAssets(original.manifestPath(),&output,&missing,&error));QCOMPARE(output.name,QString("sentinel"));QCOMPARE(missing[0].path,QString("sentinel"));
 }
 void restorationRejectsLinkedParentsAndDoesNotRewriteManifest(){
  QTemporaryDir dir;controllers::DocumentController doc;QVERIFY(doc.create(QString(QVW_SOURCE_DIR)+"/templates/title-card",dir.filePath("project"),"repair"));
  QImage image(10,10,QImage::Format_RGB32);image.fill(Qt::red);QVERIFY(QDir().mkpath(dir.filePath("sources")));const auto source=dir.filePath("sources/source.png");QVERIFY(image.save(source));QVERIFY(doc.importImage(source));
  const auto project=doc.project();const auto asset=project.assets.last();const auto destination=QDir(project.rootPath).filePath(asset.path);const auto manifest=readBytes(project.manifestPath());QString error;QVERIFY(QFile::remove(destination));
  QVERIFY(QFile::link(dir.filePath("sources"),dir.filePath("linked")));QVERIFY(!services::AssetService::restoreMissing(project,asset,dir.filePath("linked/source.png"),&error));
  const auto assets=QDir(project.rootPath).filePath("assets");QVERIFY(QDir().rename(assets,assets+"-original"));QVERIFY(QFile::link(assets+"-original",assets));QVERIFY(!services::AssetService::restoreMissing(project,asset,source,&error));QVERIFY(QFile::remove(assets));QVERIFY(QDir().rename(assets+"-original",assets));
#ifdef Q_OS_UNIX
  const auto fifo=dir.filePath("fifo");QVERIFY(::mkfifo(QFile::encodeName(fifo).constData(),0600)==0);QVERIFY(!services::AssetService::restoreMissing(project,asset,fifo,&error));
#endif
  QVERIFY2(services::AssetService::restoreMissing(project,asset,source,&error),qPrintable(error));QCOMPARE(readBytes(destination),readBytes(source));QCOMPARE(readBytes(project.manifestPath()),manifest);
 }
 void batchBusyProgressAndLimit(){
  QTemporaryDir dir;controllers::DocumentController doc;QVERIFY(doc.create(QString(QVW_SOURCE_DIR)+"/templates/title-card",dir.filePath("project"),"batch"));
  QImage image(10,10,QImage::Format_RGB32);image.fill(Qt::red);const auto first=dir.filePath("first.png");QVERIFY(image.save(first));
  QStringList excessive;for(int i=0;i<65;++i)excessive.append(first);QVERIFY(!doc.importFiles(excessive));QVERIFY(!doc.importFiles({}));
  QSignalSpy state(&doc,&controllers::DocumentController::videoImportStateChanged),progress(&doc,&controllers::DocumentController::importProgress),finished(&doc,&controllers::DocumentController::importBatchFinished);
  QVERIFY(doc.importFiles({first,dir.filePath("unsupported.txt"),first}));QVERIFY(doc.importingVideo());QCOMPARE(state.size(),1);QVERIFY(state[0][0].toBool());
  QVERIFY(!doc.create(QString(QVW_SOURCE_DIR)+"/templates/title-card",dir.filePath("other"),"blocked"));QVERIFY(!doc.save());QVERIFY(!doc.importImage(first));QVERIFY(!doc.importVideo("missing.mp4"));QVERIFY(!doc.importFiles({first}));
  QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,5000);QCOMPARE(progress.size(),3);for(int i=0;i<3;++i){QCOMPARE(progress[i][0].toInt(),i+1);QCOMPARE(progress[i][1].toInt(),3);}QCOMPARE(progress[0][2].toString(),first);QCOMPARE(state.size(),2);QVERIFY(!state[1][0].toBool());QCOMPARE(finished[0][0].toInt(),2);QCOMPARE(doc.project().assets.size(),1);
 }
 void videoSuccessAndFailureAdvanceSequentiallyWithoutBusyFlicker(){
  QTemporaryDir dir;controllers::DocumentController doc;QVERIFY(doc.create(QString(QVW_SOURCE_DIR)+"/templates/title-card",dir.filePath("project"),"mixed"));
  QImage image(10,10,QImage::Format_RGB32);image.fill(Qt::green);const auto still=dir.filePath("still.jpg"),video=dir.filePath("movie.mp4");QVERIFY(image.save(still));QVERIFY(writeBytes(video,"fake video bytes for queue test"));
  doc.setMediaTools(fakeProbe(dir.filePath("probe")),python(dir.filePath("decode"),"pass"),QProcessEnvironment::systemEnvironment());
  QSignalSpy finished(&doc,&controllers::DocumentController::importBatchFinished),state(&doc,&controllers::DocumentController::videoImportStateChanged),progress(&doc,&controllers::DocumentController::importProgress),imported(&doc,&controllers::DocumentController::assetImported);
  QVERIFY(doc.importFiles({video,dir.filePath("missing.mp4"),still}));QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,10000);QCOMPARE(finished[0][0].toInt(),2);QCOMPARE(finished[0][2].toStringList().size(),1);QCOMPARE(imported.size(),2);QCOMPARE(doc.project().assets[0].mime,QString("video/mp4"));QCOMPARE(doc.project().assets[1].mime,QString("image/jpeg"));QCOMPARE(state.size(),2);QCOMPARE(progress.size(),3);
 }
 void closeFromImportedCannotLeakOldQueueIntoNewDocument_data(){QTest::addColumn<bool>("video");QTest::newRow("image")<<false;QTest::newRow("video-success-before-state-false")<<true;}
 void closeFromImportedCannotLeakOldQueueIntoNewDocument(){
  QFETCH(bool,video);
  QTemporaryDir dir;controllers::DocumentController doc;QVERIFY(doc.create(QString(QVW_SOURCE_DIR)+"/templates/title-card",dir.filePath("old"),"old"));QImage image(10,10,QImage::Format_RGB32);image.fill(Qt::green);const auto first=dir.filePath(video?"first.mp4":"first.png"),second=dir.filePath("second.png");
  if(video){QVERIFY(writeBytes(first,"video"));doc.setMediaTools(fakeProbe(dir.filePath("probe")),python(dir.filePath("decode"),"pass"),QProcessEnvironment::systemEnvironment());}else{QVERIFY(image.save(first));}image.fill(Qt::blue);QVERIFY(image.save(second));
  bool switched=false;connect(&doc,&controllers::DocumentController::assetImported,&doc,[&]{if(switched)return;switched=true;doc.close();QVERIFY(doc.create(QString(QVW_SOURCE_DIR)+"/templates/title-card",dir.filePath("new"),"new"));QVERIFY(doc.importFiles({second}));});
  QSignalSpy finished(&doc,&controllers::DocumentController::importBatchFinished);QVERIFY(doc.importFiles({first,first}));QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,5000);QCOMPARE(finished[0][0].toInt(),1);QCOMPARE(finished[0][1].toInt(),2);QCOMPARE(finished[1][0].toInt(),1);QCOMPARE(doc.project().name,QString("new"));QCOMPARE(doc.project().assets.size(),1);QCOMPARE(doc.project().assets[0].originalName,QString("second.png"));
 }
 void cancelDuringVideoKeepsCompletedImage(){
  QTemporaryDir dir;controllers::DocumentController doc;QVERIFY(doc.create(QString(QVW_SOURCE_DIR)+"/templates/title-card",dir.filePath("project"),"cancel"));QImage image(10,10,QImage::Format_RGB32);image.fill(Qt::green);const auto still=dir.filePath("still.png"),video=dir.filePath("movie.mp4"),marker=dir.filePath("decoding");QVERIFY(image.save(still));QVERIFY(writeBytes(video,"fake video"));
  doc.setMediaTools(fakeProbe(dir.filePath("probe")),python(dir.filePath("decode"),"import pathlib,time\npathlib.Path('"+marker+"').write_text('started')\ntime.sleep(10)"),QProcessEnvironment::systemEnvironment());
  QSignalSpy finished(&doc,&controllers::DocumentController::importBatchFinished),progress(&doc,&controllers::DocumentController::importProgress);QVERIFY(doc.importFiles({still,video,still}));QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(marker),5000);doc.cancelVideoImport();QVERIFY(!doc.importingVideo());QCOMPARE(finished.size(),1);QCOMPARE(finished[0][0].toInt(),1);QCOMPARE(finished[0][1].toInt(),3);QCOMPARE(progress.size(),1);QCOMPARE(doc.project().assets.size(),1);QTest::qWait(50);QCOMPARE(doc.project().assets.size(),1);
 }

 void cancelFromProgressStillNotifiesCommittedProject(){
  QTemporaryDir dir;controllers::DocumentController doc;QVERIFY(doc.create(QString(QVW_SOURCE_DIR)+"/templates/title-card",dir.filePath("project"),"cancel"));QImage image(10,10,QImage::Format_RGB32);image.fill(Qt::green);const auto still=dir.filePath("still.png");QVERIFY(image.save(still));
  QSignalSpy changed(&doc,&controllers::DocumentController::projectChanged),finished(&doc,&controllers::DocumentController::importBatchFinished);
  connect(&doc,&controllers::DocumentController::importProgress,&doc,[&]{doc.cancelVideoImport();});
  QVERIFY(doc.importFiles({still,still}));QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,5000);QCOMPARE(doc.project().assets.size(),1);QCOMPARE(changed.size(),1);QCOMPARE(qvariant_cast<domain::Project>(changed[0][0]).assets.size(),1);
 }
 void inspectionRejectsMissingAssetBehindRegularFile(){
  QTemporaryDir dir;controllers::DocumentController doc;QVERIFY(doc.create(QString(QVW_SOURCE_DIR)+"/templates/title-card",dir.filePath("project"),"strict"));QImage image(10,10,QImage::Format_RGB32);image.fill(Qt::green);const auto still=dir.filePath("still.png");QVERIFY(image.save(still));QVERIFY(doc.importImage(still));
  const auto project=doc.project();auto manifest=QJsonDocument::fromJson(readBytes(project.manifestPath())).object();auto assets=manifest["assets"].toArray();auto asset=assets[0].toObject();asset["path"]="assets/blocked/missing.png";assets[0]=asset;manifest["assets"]=assets;QVERIFY(writeBytes(project.manifestPath(),QJsonDocument(manifest).toJson()));QVERIFY(writeBytes(QDir(project.rootPath).filePath("assets/blocked"),"ordinary file"));domain::Project inspected;QVector<domain::Asset> missing;QString error;
  QVERIFY(!services::ProjectStore::inspectMissingAssets(project.manifestPath(),&inspected,&missing,&error));
 }

 void asynchronousVideoFailureContinuesToNextImage(){
  QTemporaryDir dir;controllers::DocumentController doc;QVERIFY(doc.create(QString(QVW_SOURCE_DIR)+"/templates/title-card",dir.filePath("project"),"failure"));QImage image(10,10,QImage::Format_RGB32);image.fill(Qt::green);const auto still=dir.filePath("still.png"),video=dir.filePath("movie.mp4");QVERIFY(image.save(still));QVERIFY(writeBytes(video,"fake video"));
  doc.setMediaTools(fakeProbe(dir.filePath("probe")),python(dir.filePath("decode"),"import sys\nsys.exit(1)"),QProcessEnvironment::systemEnvironment());
  QSignalSpy finished(&doc,&controllers::DocumentController::importBatchFinished),state(&doc,&controllers::DocumentController::videoImportStateChanged);QVERIFY(doc.importFiles({video,still}));QTRY_COMPARE_WITH_TIMEOUT(finished.size(),1,5000);QCOMPARE(finished[0][0].toInt(),1);QCOMPARE(finished[0][2].toStringList().size(),1);QCOMPARE(doc.project().assets.size(),1);QCOMPARE(state.size(),2);
 }

 void retryOnlyIncompleteFilesAfterRepairingSource(){
  QTemporaryDir dir;controllers::DocumentController doc;QVERIFY(doc.create(QString(QVW_SOURCE_DIR)+"/templates/title-card",dir.filePath("project"),"retry"));QImage image(10,10,QImage::Format_RGB32);image.fill(Qt::red);const auto good=dir.filePath("good.png"),missing=dir.filePath("missing.png");QVERIFY(image.save(good));QSignalSpy reports(&doc,&controllers::DocumentController::importReportReady),finished(&doc,&controllers::DocumentController::importBatchFinished);QVERIFY(doc.importFiles({good,missing}));QTRY_COMPARE(finished.count(),1);const auto report=reports.last()[0].toJsonObject();QCOMPARE(report["entries"].toArray()[0].toObject()["state"].toString(),QString("imported"));QCOMPARE(report["entries"].toArray()[1].toObject()["state"].toString(),QString("failed"));
  image.fill(Qt::blue);QVERIFY(image.save(missing));QSignalSpy imported(&doc,&controllers::DocumentController::assetImported);QVERIFY(doc.retryIncompleteImports());QTRY_COMPARE(finished.count(),2);QCOMPARE(imported.count(),1);QCOMPARE(finished.last()[1].toInt(),1);QCOMPARE(doc.project().assets.size(),2);QVERIFY(!doc.retryIncompleteImports());
 }
 void cancelledQueueCanContinueAndProjectSwitchClearsRecovery(){
  QTemporaryDir dir;controllers::DocumentController doc;QVERIFY(doc.create(QString(QVW_SOURCE_DIR)+"/templates/title-card",dir.filePath("project"),"retry"));QImage image(10,10,QImage::Format_RGB32);image.fill(Qt::red);const auto first=dir.filePath("first.png"),second=dir.filePath("second.png");QVERIFY(image.save(first));image.fill(Qt::blue);QVERIFY(image.save(second));const auto connection=connect(&doc,&controllers::DocumentController::importProgress,&doc,[&]{doc.cancelVideoImport();});QSignalSpy finished(&doc,&controllers::DocumentController::importBatchFinished);QVERIFY(doc.importFiles({first,second}));QVERIFY(!doc.retryIncompleteImports());QTRY_COMPARE(finished.count(),1);disconnect(connection);QVERIFY(doc.retryIncompleteImports());QTRY_COMPARE(finished.count(),2);QCOMPARE(finished.last()[1].toInt(),1);QCOMPARE(doc.project().assets.size(),2);
  QVERIFY(doc.importFiles({dir.filePath("absent.png")}));QTRY_COMPARE(finished.count(),3);doc.close();QVERIFY(doc.create(QString(QVW_SOURCE_DIR)+"/templates/title-card",dir.filePath("other"),"other"));QVERIFY(!doc.retryIncompleteImports());QCOMPARE(doc.project().assets.size(),0);
 }

};
QTEST_GUILESS_MAIN(AssetLibraryTest)
#include "AssetLibraryTest.moc"
