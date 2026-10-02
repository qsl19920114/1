#include "controllers/DocumentController.h"
#include "controllers/EditorController.h"
#include "controllers/ExportController.h"
#include "controllers/ProjectController.h"
#include "controllers/ProposalController.h"
#include "infrastructure/AppConfig.h"
#include "infrastructure/LogWriter.h"
#include "ui/MainWindow.h"
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QDateTime>
#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QLinearGradient>
#include <QProcess>
#include <QTimer>
#include <QStatusBar>
#include <QWebEnginePage>
#include <QWebEngineView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QElapsedTimer>
#include <memory>

// Scripted real application actions, not simulated backend responses. Frames
// are continuous grabs of this MainWindow only; no desktop capture permission.
class WalkthroughDemo : public QObject {
    Q_OBJECT
private slots:
    void recordCreationToReopen() {
        const QString repo=QStringLiteral(QVW_SOURCE_DIR), output=repo+"/.workbench/deliverables";
        QVERIFY(QDir().mkpath(output));
        QTemporaryDir frames(output+"/recording-XXXXXX");QVERIFY(frames.isValid());
        const QString projectRoot=output+"/演示工程 "+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz");
        const auto loaded=qvw::infra::loadAppConfig(repo+"/config/version-lock.json");QVERIFY2(loaded.ok(),qPrintable(loaded.error));
        qvw::infra::LogWriter log(repo+"/docs/evidence/m6/walkthrough.jsonl");
        qvw::controllers::DocumentController document;
        qvw::controllers::ProjectController backend(loaded.config,log);
        qvw::controllers::EditorController editor;
        qvw::controllers::ProposalController proposals(editor);
        qvw::controllers::ExportController exporter(loaded.config,log);
        qvw::ui::MainWindow window;window.resize(1480,900);
        struct RuntimeCleanup {
            qvw::infra::AppConfig config;QString relativeRuntime;QSet<QString> roots;
            ~RuntimeCleanup(){for(const auto &root:roots){if(!QFileInfo::exists(root))continue;QProcess p;p.setProcessEnvironment(config.processEnvironment);p.start(config.launcherPath,{"runtime","down","--workspace",root,"--runtime",QDir(root).filePath(relativeRuntime),"--json"});p.waitForFinished(30000);}}
        } cleanup{loaded.config,"hypit.runtime.json",{}};
        connect(&document,&qvw::controllers::DocumentController::projectChanged,&window,&qvw::ui::MainWindow::showDocument);
        connect(&document,&qvw::controllers::DocumentController::projectChanged,&proposals,&qvw::controllers::ProposalController::setProject);
        connect(&document,&qvw::controllers::DocumentController::projectLoaded,&exporter,&qvw::controllers::ExportController::setProject);
        connect(&document,&qvw::controllers::DocumentController::projectLoaded,&backend,[&](const qvw::domain::Project &p){cleanup.relativeRuntime=p.runtimePath;backend.openDocument(p,5780);});
        connect(&document,&qvw::controllers::DocumentController::documentClosed,&window,&qvw::ui::MainWindow::clearDocument);
        connect(&document,&qvw::controllers::DocumentController::documentClosed,&proposals,&qvw::controllers::ProposalController::clearProject);
        connect(&document,&qvw::controllers::DocumentController::documentClosed,&exporter,&qvw::controllers::ExportController::clearProject);
        connect(&backend,&qvw::controllers::ProjectController::availableChanged,&window,&qvw::ui::MainWindow::setBackendAvailable);
        connect(&backend,&qvw::controllers::ProjectController::projectClosed,&window,&qvw::ui::MainWindow::clearProject);
        connect(&backend,&qvw::controllers::ProjectController::projectClosed,&editor,&qvw::controllers::EditorController::clear);
        connect(&backend,&qvw::controllers::ProjectController::previewRequested,&window,&qvw::ui::MainWindow::showPreview);
        connect(&backend,&qvw::controllers::ProjectController::previewRequested,&editor,[&](const QUrl &u){editor.attach(u,backend.workspace());});
        connect(&backend,&qvw::controllers::ProjectController::snapshotReady,&editor,&qvw::controllers::EditorController::acceptSnapshot);
        connect(&editor,&qvw::controllers::EditorController::snapshotReady,&window,&qvw::ui::MainWindow::showSnapshot);
        connect(&editor,&qvw::controllers::EditorController::stateChanged,&window,&qvw::ui::MainWindow::setEditorState);
        connect(&proposals,&qvw::controllers::ProposalController::proposalChanged,&window,&qvw::ui::MainWindow::showProposal);
        connect(&exporter,&qvw::controllers::ExportController::taskChanged,&window,&qvw::ui::MainWindow::showExportTask);
        connect(&exporter,&qvw::controllers::ExportController::taskChanged,this,[&](const qvw::domain::ExportTask &t){if(!t.workspace.isEmpty())cleanup.roots.insert(t.workspace);});
        auto message=[&](const QString &s){window.appendLog(s);qInfo().noquote()<<s;};
        connect(&document,&qvw::controllers::DocumentController::message,&window,message);
        connect(&backend,&qvw::controllers::ProjectController::message,&window,message);
        connect(&editor,&qvw::controllers::EditorController::message,&window,message);
        connect(&proposals,&qvw::controllers::ProposalController::message,&window,message);
        connect(&exporter,&qvw::controllers::ExportController::message,&window,message);
        connect(&window,&qvw::ui::MainWindow::newDocumentRequested,&document,[&](const QString &dir,const QString &name){document.create(repo+"/templates/title-card",dir,name);});
        connect(&window,&qvw::ui::MainWindow::openDocumentRequested,&document,&qvw::controllers::DocumentController::open);
        connect(&window,&qvw::ui::MainWindow::saveDocumentRequested,&document,&qvw::controllers::DocumentController::save);
        connect(&window,&qvw::ui::MainWindow::importImageRequested,&document,&qvw::controllers::DocumentController::importImage);
        connect(&window,&qvw::ui::MainWindow::editRequested,&editor,&qvw::controllers::EditorController::edit);
        connect(&window,&qvw::ui::MainWindow::undoRequested,&editor,&qvw::controllers::EditorController::undo);
        connect(&window,&qvw::ui::MainWindow::redoRequested,&editor,&qvw::controllers::EditorController::redo);
        connect(&window,&qvw::ui::MainWindow::demoProposalRequested,&proposals,&qvw::controllers::ProposalController::generateDemo);
        connect(&window,&qvw::ui::MainWindow::confirmProposalRequested,&proposals,&qvw::controllers::ProposalController::confirm);
        connect(&window,&qvw::ui::MainWindow::exportRequested,&exporter,[&](const QString &p){exporter.startExport(editor.snapshot(),p);});
        connect(&window,&qvw::ui::MainWindow::clearExportCacheRequested,&exporter,&qvw::controllers::ExportController::clearFinishedCache);
        QSignalSpy initialized(&backend,&qvw::controllers::ProjectController::initialized),backendFailed(&backend,&qvw::controllers::ProjectController::failed);
        QSignalSpy docFailed(&document,&qvw::controllers::DocumentController::failed),editFailed(&editor,&qvw::controllers::EditorController::failed);
        QSignalSpy edits(&editor,&qvw::controllers::EditorController::operationSucceeded),done(&exporter,&qvw::controllers::ExportController::completed);
        QSignalSpy exportFailed(&exporter,&qvw::controllers::ExportController::failed),cleared(&exporter,&qvw::controllers::ExportController::cacheCleared);
        int count=0;bool captureFailed=false;QTimer recording;recording.setInterval(200);
        connect(&recording,&QTimer::timeout,&window,[&]{if(count>=1500){captureFailed=true;recording.stop();return;}const auto pixmap=window.grab();if(pixmap.isNull()||!pixmap.save(frames.filePath(QString("frame-%1.png").arg(count,6,10,QChar('0')))))captureFailed=true;++count;});
        QJsonArray stages;QElapsedTimer elapsed;elapsed.start();
        auto stage=[&](const QString &s){window.statusBar()->showMessage(s);window.appendLog("演示步骤："+s);stages.append(QJsonObject{{"atMs",elapsed.elapsed()},{"action",s}});QTest::qWait(1200);};
        auto field=[&](const QString &binding){for(const auto &track:editor.snapshot().tracks)for(const auto &clip:track.clips)for(const auto &f:clip.inspector)if(f.binding==binding)return f;return qvw::domain::InspectorField{};};
        auto previewReady=[&](){auto result=std::make_shared<QPair<bool,bool>>(false,false);window.previewView()->page()->runJavaScript("(() => {const d=document.querySelector('iframe')?.contentDocument;const imgs=d?Array.from(d.querySelectorAll('img')):[];return !!d?.querySelector('[data-composition-id]')&&imgs.length>0&&imgs.every(i=>i.complete&&i.naturalWidth>0)})()",[result](const QVariant &v){result->second=v.toBool();result->first=true;});QElapsedTimer deadline;deadline.start();while(!result->first&&deadline.elapsed()<2000)QTest::qWait(25);return result->first&&result->second;};
        // Click Studio's real transport (pinned ui/stage.ts data-play). Advance
        // beyond the entrance animation so the recorded picture is readable.
        auto playPreview=[&](){window.previewView()->page()->runJavaScript("document.querySelector('[data-play]')?.click()");QTest::qWait(1600);window.previewView()->page()->runJavaScript("document.querySelector('[data-play]')?.click()");QTest::qWait(400);};
        window.show();recording.start();stage("1 / 环境诊断与原创模板新建");
        backend.initialize();QTRY_VERIFY_WITH_TIMEOUT(!initialized.isEmpty()||!backendFailed.isEmpty(),25000);QVERIFY(backendFailed.isEmpty());
        window.newDocumentRequested(projectRoot,"校园光影 · Qt 视频工作台");QVERIFY2(docFailed.isEmpty(),qPrintable(docFailed.isEmpty()?QString():docFailed.last().first().toString()));
        QTRY_VERIFY_WITH_TIMEOUT(editor.isReady()||!backendFailed.isEmpty(),60000);QVERIFY(backendFailed.isEmpty());QTRY_VERIFY_WITH_TIMEOUT(previewReady(),30000);playPreview();
        stage("2 / 原创工程就绪，导入本地 PNG 素材");
        QImage picture(640,680,QImage::Format_RGB32);QPainter painter(&picture);QLinearGradient gradient(0,0,640,680);gradient.setColorAt(0,QColor("#1b3854"));gradient.setColorAt(1,QColor("#efb76a"));painter.fillRect(picture.rect(),gradient);painter.setPen(Qt::white);painter.setFont(QFont("PingFang SC",42,QFont::Bold));painter.drawText(picture.rect(),Qt::AlignCenter,"校园光影\n摄影社招新");painter.end();
        const auto input=frames.filePath("original.png");QVERIFY(picture.save(input));window.importImageRequested(input);QVERIFY(docFailed.isEmpty());QCOMPARE(document.project().assets.size(),1);
        const auto asset=document.project().assets.front();stage("3 / 原生属性：修改标题、主题色并绑定已导入图片");
        const auto clip=editor.snapshot().tracks.front().clips.front().id;
        window.editRequested(clip,field("title").id,QStringLiteral("校园光影 · 摄影社招新"));QTRY_VERIFY_WITH_TIMEOUT(edits.size()==1||!editFailed.isEmpty(),30000);QVERIFY(editFailed.isEmpty());
        window.editRequested(clip,field("color").id,QStringLiteral("#e47735"));QTRY_VERIFY_WITH_TIMEOUT(edits.size()==2||!editFailed.isEmpty(),30000);QVERIFY(editFailed.isEmpty());
        window.editRequested(clip,field("image").id,"./"+asset.path);QTRY_VERIFY_WITH_TIMEOUT(edits.size()==3||!editFailed.isEmpty(),30000);QVERIFY(editFailed.isEmpty());QTRY_VERIFY_WITH_TIMEOUT(previewReady(),30000);
        stage("4 / 撤销图片绑定，随后重做；后端确认后才推进历史");window.undoRequested();QTRY_COMPARE_WITH_TIMEOUT(edits.size(),4,30000);QCOMPARE(field("image").rawValue.toString(),QString("./assets/default.png"));QTest::qWait(1200);
        window.redoRequested();QTRY_COMPARE_WITH_TIMEOUT(edits.size(),5,30000);QCOMPARE(field("image").rawValue.toString(),"./"+asset.path);
        stage("5 / 本地模拟提案：确认前源码与画面保持原值");const auto before=editor.snapshot().sourceFingerprint;window.demoProposalRequested("标题改为校园光影 · 一起发现美");QVERIFY(proposals.pending());QCOMPARE(editor.snapshot().sourceFingerprint,before);QCOMPARE(edits.size(),5);QTest::qWait(2000);
        stage("6 / 显式确认提案，通过共同编辑链写回真实工程");window.confirmProposalRequested();QTRY_VERIFY_WITH_TIMEOUT(edits.size()==6||!editFailed.isEmpty(),30000);QVERIFY(editFailed.isEmpty());QCOMPARE(field("title").rawValue.toString(),QString("校园光影 · 一起发现美"));window.saveDocumentRequested();QVERIFY(docFailed.isEmpty());QTRY_VERIFY_WITH_TIMEOUT(previewReady(),30000);
        stage("7 / 导出：冻结输入、plan、build、精确 Output 与媒体校验");
        const auto film=output+"/校园光影-演示成片.mp4";window.exportRequested(film);QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty()||!exportFailed.isEmpty(),240000);QVERIFY2(exportFailed.isEmpty(),qPrintable(exportFailed.isEmpty()?QString():exportFailed.last().first().toString()));QCOMPARE(done.size(),1);QVERIFY(QFileInfo(film).size()>1000);
        const auto workspace=exporter.task().workspace,buildId=exporter.task().buildId;const auto fingerprint=editor.snapshot().sourceFingerprint;
        stage("8 / 清理自有终态缓存，保留工程输入与已交付成片");window.clearExportCacheRequested();QTRY_VERIFY_WITH_TIMEOUT(!cleared.isEmpty()||!exportFailed.isEmpty(),60000);QVERIFY2(exportFailed.isEmpty(),qPrintable(exportFailed.isEmpty()?QString():exportFailed.last().first().toString()));QCOMPARE(cleared.size(),1);QVERIFY(!QFileInfo::exists(workspace));QVERIFY(QFileInfo::exists(film));QVERIFY(QFileInfo::exists(QDir(projectRoot).filePath(asset.path)));
        stage("9 / 关闭 Studio，再打开同一工程验证持久化");const auto manifest=document.project().manifestPath();backend.closeProject();QVERIFY(!backend.isRunning());document.close();window.openDocumentRequested(manifest);
        QTRY_VERIFY_WITH_TIMEOUT(editor.isReady()||!backendFailed.isEmpty(),60000);QVERIFY(backendFailed.isEmpty());QCOMPARE(editor.snapshot().sourceFingerprint,fingerprint);QCOMPARE(field("title").rawValue.toString(),QString("校园光影 · 一起发现美"));QTRY_VERIFY_WITH_TIMEOUT(previewReady(),30000);playPreview();
        stage("10 / 已完成：真实 MP4、工程可重开；模型生成明确为模拟");backend.closeProject();QVERIFY(!backend.isRunning());QTest::qWait(1000);recording.stop();QVERIFY(!captureFailed);QVERIFY(count>30);
        const auto video=output+"/完整操作演示.mp4";QProcess encode;encode.setProcessEnvironment(loaded.config.processEnvironment);encode.start(loaded.config.ffmpegPath,{"-y","-v","error","-framerate","5","-i",frames.filePath("frame-%06d.png"),"-vf","scale=1280:-2","-c:v","libx264","-pix_fmt","yuv420p","-movflags","+faststart",video});QVERIFY(encode.waitForFinished(180000));QVERIFY2(encode.exitCode()==0,encode.readAllStandardError().constData());
        QProcess decode;decode.setProcessEnvironment(loaded.config.processEnvironment);decode.start(loaded.config.ffmpegPath,{"-v","error","-i",video,"-f","null","-"});QVERIFY(decode.waitForFinished(60000));QVERIFY2(decode.exitCode()==0,decode.readAllStandardError().constData());
        QSaveFile evidence(repo+"/docs/evidence/m6/walkthrough.json");QVERIFY(evidence.open(QIODevice::WriteOnly));const auto json=QJsonDocument(QJsonObject{{"verdict","PASS"},{"recording","continuous own Qt MainWindow; scripted production UI signals"},{"video",video},{"film",film},{"project",manifest},{"buildId",buildId},{"frames",count},{"nominalFps",5},{"elapsedMs",elapsed.elapsed()},{"stages",stages},{"reopenedFingerprint",QString::fromLatin1(fingerprint.toHex())},{"terminalCacheDeleted",true},{"inputsAndFilmKept",true},{"model","local simulation; real Studio mutation"},{"ownedStudioStopped",true},{"videoFullDecode",true},{"screenshotsForAcceptance",false}}).toJson();QCOMPARE(evidence.write(json),json.size());QVERIFY(evidence.commit());
    }
};
QTEST_MAIN(WalkthroughDemo)
#include "WalkthroughDemo.moc"
