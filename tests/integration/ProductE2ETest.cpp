#include "controllers/DocumentController.h"
#include "controllers/EditorController.h"
#include "controllers/ExportController.h"
#include "controllers/ProjectController.h"
#include "controllers/SampleCreationController.h"
#include "infrastructure/AppConfig.h"
#include "infrastructure/LogWriter.h"
#include "services/SampleCatalog.h"
#include "ui/MainWindow.h"
#include <QTest>
#include <QSignalSpy>
#include <QDateTime>
#include <QSaveFile>
#include <QDir>
#include <QFileInfo>
#include <QPushButton>
#include <QDialog>
#include <QWheelEvent>
#include <QSlider>
#include <QProcess>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>
#include <memory>
class ProductE2ETest:public QObject {
    Q_OBJECT
private slots:
    void sampleToValidatedFilm() {
        const QString repo=QStringLiteral(QVW_SOURCE_DIR),out=repo+"/.workbench/deliverables-m7";QVERIFY(QDir().mkpath(out));
        const auto loaded=qvw::infra::loadAppConfig(repo+"/config/version-lock.json");QVERIFY2(loaded.ok(),qPrintable(loaded.error));
        const auto samples=qvw::services::SampleCatalog::discover(loaded.config.distributionPath);QCOMPARE(samples.size(),1);QCOMPARE(samples.front().sources.size(),3);
        qvw::infra::LogWriter log(repo+"/docs/evidence/m7/product-run.jsonl");
        qvw::controllers::DocumentController document;document.setMediaTools(loaded.config.ffprobePath,loaded.config.ffmpegPath,loaded.config.processEnvironment);
        qvw::controllers::ProjectController backend(loaded.config,log);qvw::controllers::EditorController editor;qvw::controllers::SampleCreationController creation(document,editor);qvw::controllers::ExportController exporter(loaded.config,log);qvw::ui::MainWindow window;window.setSamples(samples);
        QString frozenWorkspace;
        struct Cleanup {qvw::controllers::ProjectController &backend;qvw::controllers::DocumentController &document;qvw::infra::AppConfig config;QString &workspace;~Cleanup(){document.cancelVideoImport();backend.closeProject();if(!workspace.isEmpty()){QProcess p;p.setProcessEnvironment(config.processEnvironment);p.start(config.launcherPath,{"runtime","down","--workspace",workspace,"--runtime",workspace+"/hypit.runtime.json","--json"});p.waitForFinished(30000);}}} cleanup{backend,document,loaded.config,frozenWorkspace};
        connect(&document,&qvw::controllers::DocumentController::projectChanged,&window,&qvw::ui::MainWindow::showDocument);
        connect(&document,&qvw::controllers::DocumentController::projectLoaded,&exporter,&qvw::controllers::ExportController::setProject);
        connect(&document,&qvw::controllers::DocumentController::projectLoaded,&backend,[&](const auto &p){backend.openDocument(p,5791);});
        connect(&document,&qvw::controllers::DocumentController::documentClosed,&window,&qvw::ui::MainWindow::clearDocument);
        connect(&document,&qvw::controllers::DocumentController::videoImportStateChanged,&window,&qvw::ui::MainWindow::setImportBusy);
        connect(&backend,&qvw::controllers::ProjectController::availableChanged,&window,&qvw::ui::MainWindow::setBackendAvailable);
        connect(&backend,&qvw::controllers::ProjectController::projectClosed,&window,&qvw::ui::MainWindow::clearProject);
        connect(&backend,&qvw::controllers::ProjectController::projectClosed,&editor,&qvw::controllers::EditorController::clear);
        connect(&backend,&qvw::controllers::ProjectController::previewRequested,&window,&qvw::ui::MainWindow::showPreview);
        connect(&backend,&qvw::controllers::ProjectController::previewRequested,&editor,[&](const QUrl &u){editor.attach(u,backend.workspace());});
        connect(&backend,&qvw::controllers::ProjectController::snapshotReady,&editor,&qvw::controllers::EditorController::acceptSnapshot);
        connect(&editor,&qvw::controllers::EditorController::snapshotReady,&window,&qvw::ui::MainWindow::showSnapshot);
        connect(&editor,&qvw::controllers::EditorController::stateChanged,&window,&qvw::ui::MainWindow::setEditorState);
        connect(&exporter,&qvw::controllers::ExportController::taskChanged,&window,&qvw::ui::MainWindow::showExportTask);
        connect(&exporter,&qvw::controllers::ExportController::taskChanged,this,[&](const auto &t){if(!t.workspace.isEmpty())frozenWorkspace=t.workspace;});
        connect(&window,&qvw::ui::MainWindow::sampleDocumentRequested,&creation,[&](const QString &video,const QString &dir,const QString &name){creation.create(video,repo+"/templates/video-story",dir,name);});
        connect(&window,&qvw::ui::MainWindow::editRequested,&editor,&qvw::controllers::EditorController::edit);
        connect(&window,&qvw::ui::MainWindow::exportRequested,&exporter,[&](const QString &p){exporter.startExport(editor.snapshot(),p);});
        auto message=[&](const QString &s){window.appendLog(s);qInfo().noquote()<<s;};
        connect(&document,&qvw::controllers::DocumentController::message,&window,message);connect(&creation,&qvw::controllers::SampleCreationController::message,&window,message);connect(&backend,&qvw::controllers::ProjectController::message,&window,message);connect(&exporter,&qvw::controllers::ExportController::message,&window,message);
        QSignalSpy initialized(&backend,&qvw::controllers::ProjectController::initialized),backendFailed(&backend,&qvw::controllers::ProjectController::failed),docFailed(&document,&qvw::controllers::DocumentController::failed),creationFailed(&creation,&qvw::controllers::SampleCreationController::failed),bound(&creation,&qvw::controllers::SampleCreationController::bound),edits(&editor,&qvw::controllers::EditorController::operationSucceeded),editFailed(&editor,&qvw::controllers::EditorController::failed),done(&exporter,&qvw::controllers::ExportController::completed),exportFailed(&exporter,&qvw::controllers::ExportController::failed);
        auto jsView=[&](QWebEngineView *view,const QString &script){auto state=std::make_shared<QPair<bool,QVariant>>(false,QVariant{});if(!view)return QVariant{};view->page()->runJavaScript(script,[state](const QVariant &v){state->second=v;state->first=true;});QElapsedTimer t;t.start();while(!state->first&&t.elapsed()<2500)QTest::qWait(25);return state->second;};
        auto js=[&](const QString &script){return jsView(window.previewView(),script);};
        auto field=[&](const QString &binding){for(const auto &track:editor.snapshot().tracks)for(const auto &clip:track.clips)for(const auto &f:clip.inspector)if(f.binding==binding)return f;return qvw::domain::InspectorField{};};
        window.show();backend.initialize();QTRY_VERIFY_WITH_TIMEOUT(!initialized.isEmpty()||!backendFailed.isEmpty(),25000);QVERIFY(backendFailed.isEmpty());
        const QString projectRoot=out+"/对话之外 "+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz");
        window.sampleDocumentRequested(samples.front().path,projectRoot,"对话之外 · 光影故事");
        QTRY_VERIFY_WITH_TIMEOUT(!bound.isEmpty()||!creationFailed.isEmpty()||!docFailed.isEmpty()||!backendFailed.isEmpty(),60000);
        QVERIFY2(backendFailed.isEmpty(),qPrintable(backendFailed.isEmpty()?QString():backendFailed.last().first().toString()));QVERIFY2(docFailed.isEmpty(),qPrintable(docFailed.isEmpty()?QString():docFailed.last().first().toString()));QVERIFY2(creationFailed.isEmpty(),qPrintable(creationFailed.isEmpty()?QString():creationFailed.last().first().toString()));QCOMPARE(bound.size(),1);QCOMPARE(document.project().assets.size(),1);QCOMPARE(field("video").rawValue.toString(),"./"+document.project().assets.front().path);
        auto *play=window.findChild<QPushButton*>("nativePlay");auto *slider=window.findChild<QSlider*>("nativeSeek");QVERIFY(play);QVERIFY(slider);QTRY_VERIFY_WITH_TIMEOUT(play->isEnabled(),30000);
        QTRY_VERIFY_WITH_TIMEOUT(js("(()=>{const v=document.querySelector('.stage iframe')?.contentDocument?.querySelector('video');return !!v&&v.readyState>=2&&v.videoWidth>0;})()").toBool(),30000);
        slider->setValue(90);QMetaObject::invokeMethod(slider,"sliderReleased");QTRY_VERIFY_WITH_TIMEOUT(js("Number(document.querySelector('.stage-scrubber')?.value)").toDouble()>=2.9,10000);
        QTest::keyClick(slider,Qt::Key_Right);
        QTRY_VERIFY_WITH_TIMEOUT(js("Number(document.querySelector('.stage-scrubber')?.value)").toDouble()>3.01,3000);
        const auto keyboardPosition=js("Number(document.querySelector('.stage-scrubber')?.value)").toDouble();
        QWheelEvent wheel(QPointF(slider->rect().center()),QPointF(slider->mapToGlobal(slider->rect().center())),QPoint{},QPoint{0,120},Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(slider,&wheel);
        QTRY_VERIFY_WITH_TIMEOUT(js("Number(document.querySelector('.stage-scrubber')?.value)").toDouble()>keyboardPosition,3000);
        const auto before=js("Number(document.querySelector('.stage-scrubber')?.value)").toDouble();play->click();QTest::qWait(700);play->click();QVERIFY(js("Number(document.querySelector('.stage-scrubber')?.value)").toDouble()>before);
        QVERIFY(js("getComputedStyle(document.querySelector('.source-panel')).display==='none'").toBool());
        // Observable condition used by pinned Studio when showing a timed artifact.
        js("document.querySelector('.stage-scaler').hidden=true");QTest::qWait(750);QVERIFY2(!play->isEnabled(),"native composition controls must disable in artifact mode");
        js("document.querySelector('.stage-scaler').hidden=false");QTRY_VERIFY_WITH_TIMEOUT(play->isEnabled(),10000);
        const auto clip=editor.snapshot().tracks.front().clips.front().id;
        window.editRequested(clip,field("title").id,QStringLiteral("对话之外"));QTRY_VERIFY_WITH_TIMEOUT(edits.size()==2||!editFailed.isEmpty(),30000);QVERIFY(editFailed.isEmpty());
        window.editRequested(clip,field("subtitle").id,QStringLiteral("把一段对话，变成一场光影故事。"));QTRY_VERIFY_WITH_TIMEOUT(edits.size()==3||!editFailed.isEmpty(),30000);QVERIFY(editFailed.isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(play->isEnabled(),30000);slider->setValue(90);QMetaObject::invokeMethod(slider,"sliderReleased");QTest::qWait(700);QVERIFY(window.grab().save(out+"/product-preview.png")); // Private QA frame, not acceptance material.
        const auto film=out+"/对话之外-FrameLab成片.mp4";window.exportRequested(film);QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty()||!exportFailed.isEmpty(),240000);QVERIFY2(exportFailed.isEmpty(),qPrintable(exportFailed.isEmpty()?QString():exportFailed.last().first().toString()));QCOMPARE(done.size(),1);QVERIFY(QFileInfo(film).size()>1000);QVERIFY(window.findChild<QPushButton*>("playFilm")->isEnabled());
        window.findChild<QPushButton*>("playFilm")->click();auto *player=window.findChild<QDialog*>();QVERIFY(player);auto *playerView=player->findChild<QWebEngineView*>();QVERIFY(playerView);
        QTRY_VERIFY_WITH_TIMEOUT(jsView(playerView,"(()=>{const v=document.querySelector('video');return !!v&&v.readyState>=2&&v.videoWidth===1280&&v.videoHeight===720;})()").toBool(),20000);player->close();QTest::qWait(100);
        const auto fingerprint=editor.snapshot().sourceFingerprint,buildId=exporter.task().buildId.toUtf8();const auto manifest=document.project().manifestPath();backend.closeProject();document.close();QVERIFY(!backend.isRunning());QVERIFY(document.open(manifest));QTRY_VERIFY_WITH_TIMEOUT(editor.isReady()||!backendFailed.isEmpty(),60000);QVERIFY(backendFailed.isEmpty());QCOMPARE(editor.snapshot().sourceFingerprint,fingerprint);QCOMPARE(field("title").rawValue.toString(),QString("对话之外"));QCOMPARE(document.project().assets.size(),1);
        QSaveFile evidence(repo+"/docs/evidence/m7/product-run.json");QVERIFY(evidence.open(QIODevice::WriteOnly));const auto bytes=QJsonDocument(QJsonObject{{"verdict","PASS"},{"ui","production MainWindow signals plus actual native playback/seek buttons"},{"sampleSha256",samples.front().sha256},{"duplicateSources",samples.front().sources.size()},{"project",manifest},{"film",film},{"buildId",QString::fromUtf8(buildId)},{"nativeSeekFrame",90},{"keyboardAndWheelSeek",true},{"artifactModeGuard",true},{"realPlaybackAdvanced",true},{"previewVideoDecoded",true},{"filmPlayerDecoded",true},{"exportPhase","complete: production full media validation"},{"sourceFingerprint",QString::fromLatin1(fingerprint.toHex())},{"reopened",true},{"audio","muted in original video-story template"},{"screenshotsForAcceptance",false}}).toJson();QCOMPARE(evidence.write(bytes),bytes.size());QVERIFY(evidence.commit());
    }
};
QTEST_MAIN(ProductE2ETest)
#include "ProductE2ETest.moc"
