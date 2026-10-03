#include "workflow/AgentWorkbenchBridge.h"
#include "agent/AgentController.h"
#include "controllers/ProjectController.h"
#include "controllers/SampleCreationController.h"
#include "infrastructure/AppConfig.h"
#include "infrastructure/WorkspaceStore.h"
#include "services/SampleCatalog.h"
#include "ui/MainWindow.h"
#include "ui/AgentPanel.h"
#include <QtTest>
#include <QDateTime>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QPainter>
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QSlider>
#include <QStatusBar>
#include <QProcess>

using namespace qvw;
// Captures only the actual application's rendered widget. The ffconcat file
// preserves observed frame intervals; model wait time is not edited away.
class WindowRecording {
public:
    WindowRecording(ui::MainWindow &window,QString folder):window(window),folder(std::move(folder)){
        QDir().mkpath(this->folder+"/frames");clock.start();timer.setInterval(200);
        QObject::connect(&timer,&QTimer::timeout,&window,[this]{capture();});timer.start();capture();
    }
    ~WindowRecording(){timer.stop();}
    void capture(){
        if(times.size()>=3000){failed=true;timer.stop();return;}
        const auto pixels=window.grab().toImage();if(pixels.isNull()){failed=true;return;}
        QImage canvas(1600,1000,QImage::Format_RGB32);canvas.fill(QColor("#10151d"));QPainter painter(&canvas);
        auto fitted=pixels.scaled(canvas.size(),Qt::KeepAspectRatio,Qt::SmoothTransformation);fitted.setDevicePixelRatio(1);
        painter.drawImage(QPoint((canvas.width()-fitted.width())/2,(canvas.height()-fitted.height())/2),fitted);painter.end();
        if(!canvas.save(folder+QString("/frames/frame-%1.jpg").arg(times.size(),6,10,QChar('0')),"JPG",88))failed=true;
        times.append(clock.elapsed());
    }
    bool finish(const infra::AppConfig &config,QString *error){
        capture();timer.stop();elapsed=clock.elapsed();if(failed||times.size()<10){*error="窗口采集失败或帧数不足";return false;}
        QFile list(folder+"/recording.ffconcat");if(!list.open(QIODevice::WriteOnly)){*error=list.errorString();return false;}
        list.write("ffconcat version 1.0\n");for(int i=0;i<times.size();++i){list.write(QString("file 'frames/frame-%1.jpg'\n").arg(i,6,10,QChar('0')).toUtf8());const auto ms=i+1<times.size()?times[i+1]-times[i]:200;list.write(QString("duration %1\n").arg(qMax<qint64>(1,ms)/1000.,0,'f',3).toUtf8());}list.close();
        QProcess encode;encode.setProcessEnvironment(config.processEnvironment);encode.setWorkingDirectory(folder);encode.start(config.ffmpegPath,{"-y","-v","error","-f","concat","-safe","1","-i","recording.ffconcat","-r","30","-c:v","libx264","-preset","fast","-crf","21","-pix_fmt","yuv420p","-movflags","+faststart",folder+"/Qt-Agent真实操作演示.mp4"});
        if(!encode.waitForFinished(120000)||encode.exitCode()!=0){*error=QString::fromUtf8(encode.readAllStandardError());return false;}
        QProcess decode;decode.setProcessEnvironment(config.processEnvironment);decode.start(config.ffmpegPath,{"-v","error","-i",folder+"/Qt-Agent真实操作演示.mp4","-f","null","-"});if(!decode.waitForFinished(60000)||decode.exitCode()!=0){*error=QString::fromUtf8(decode.readAllStandardError());return false;}return true;
    }
    ui::MainWindow &window;QString folder;QTimer timer;QElapsedTimer clock;QVector<qint64> times;qint64 elapsed=0;bool failed=false;
};
class AgentWorkbenchDemo:public QObject {
    Q_OBJECT
private slots:
    void realQtAgentMaterialReviewAndExport(){
        const QString repo=QVW_SOURCE_DIR;const auto output=repo+"/.workbench/deliverables-m13/runs/"+QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");QVERIFY(QDir().mkpath(output));QVERIFY(QDir().mkpath(repo+"/docs/evidence/m13"));
        const auto loaded=infra::loadAppConfig(repo+"/.workbench/local-agent-config.json");QVERIFY2(loaded.ok(),qPrintable(loaded.error));const auto config=loaded.config;infra::LogWriter log(output+"/application.jsonl");
        controllers::DocumentController document;document.setMediaTools(config.ffprobePath,config.ffmpegPath,config.processEnvironment);
        controllers::ProjectController backend(config,log);controllers::EditorController editor;controllers::SampleCreationController creation(document,editor);controllers::ExportController exporter(config,log);
        agent::ModelClient model;model.setProgram(config.codexPath);model.setEnvironment(config.processEnvironment);agent::AgentController creator(document,editor,exporter,model,repo+"/templates");ui::MainWindow window;window.resize(1580,960);workflow::bindAgentWorkbench(window,creator);
        infra::WorkspaceStore history(output+"/workspace.json");connect(&window,&ui::MainWindow::historyRecorded,&window,[&](const auto &r){history.record(r);window.showHistory(history.history());});
        connect(&document,&controllers::DocumentController::projectChanged,&window,&ui::MainWindow::showDocument);
        connect(&document,&controllers::DocumentController::projectLoaded,&backend,[&](const auto &p){exporter.setProject(p);backend.openDocument(p,5796);});
        connect(&document,&controllers::DocumentController::documentClosed,&backend,[&]{backend.closeProject();exporter.clearProject();window.clearDocument();});
        connect(&backend,&controllers::ProjectController::projectClosed,&window,&ui::MainWindow::clearProject);connect(&backend,&controllers::ProjectController::projectClosed,&editor,&controllers::EditorController::clear);
        connect(&backend,&controllers::ProjectController::availableChanged,&window,&ui::MainWindow::setBackendAvailable);
        connect(&backend,&controllers::ProjectController::previewRequested,&window,&ui::MainWindow::showPreview);connect(&backend,&controllers::ProjectController::previewRequested,&editor,[&](const QUrl &u){editor.attach(u,backend.workspace());});
        connect(&backend,&controllers::ProjectController::snapshotReady,&editor,&controllers::EditorController::acceptSnapshot);connect(&backend,&controllers::ProjectController::failed,&creator,&agent::AgentController::backendFailed);
        connect(&editor,&controllers::EditorController::snapshotReady,&window,&ui::MainWindow::showSnapshot);connect(&editor,&controllers::EditorController::stateChanged,&window,&ui::MainWindow::setEditorState);
        connect(&window,&ui::MainWindow::previewVersionChanged,window.agentPanel(),&ui::AgentPanel::setPreviewVersion);
        connect(&document,&controllers::DocumentController::videoImportStateChanged,&window,&ui::MainWindow::setImportBusy);connect(&document,&controllers::DocumentController::assetImported,&window,&ui::MainWindow::selectImportedAsset);
        connect(&document,&controllers::DocumentController::importReportReady,&window,&ui::MainWindow::showImportReport);connect(&exporter,&controllers::ExportController::taskChanged,&window,&ui::MainWindow::showExportTask);
        connect(&window,&ui::MainWindow::exportRequested,&creator,&agent::AgentController::startExport);connect(&window,&ui::MainWindow::undoRequested,&editor,&controllers::EditorController::undo);connect(&window,&ui::MainWindow::redoRequested,&editor,&controllers::EditorController::redo);
        connect(&window,&ui::MainWindow::sampleDocumentRequested,&creation,[&](const QString &sample,const QString &dir,const QString &name){creation.create(sample,repo+"/templates/video-story",dir,name);});
        connect(&creator,&agent::AgentController::message,&window,[&](const QString &s){log.info(s);qInfo().noquote()<<s;});
        QSignalSpy initialized(&backend,&controllers::ProjectController::initialized),backendErrors(&backend,&controllers::ProjectController::failed),bound(&creation,&controllers::SampleCreationController::bound),documentErrors(&document,&controllers::DocumentController::failed),creationErrors(&creation,&controllers::SampleCreationController::failed),edits(&editor,&controllers::EditorController::operationSucceeded),editErrors(&editor,&controllers::EditorController::failed),agentErrors(&creator,&agent::AgentController::failed),done(&exporter,&controllers::ExportController::completed),exportErrors(&exporter,&controllers::ExportController::failed),publicMessages(&creator,&agent::AgentController::publicMessageReceived);
        struct Cleanup{controllers::ProjectController &backend;~Cleanup(){backend.closeProject();}}cleanup{backend};
        const auto samples=services::SampleCatalog::discover(config.distributionPath,config.sampleCatalogPath);int available=0;for(const auto &s:samples)available+=s.available;QVERIFY(available>=4);window.setSamples(samples);window.show();WindowRecording recording(window,output);QJsonArray stages;
        auto stage=[&](const QString &text){window.statusBar()->showMessage(text);window.appendLog("演示："+text);stages.append(QJsonObject{{"atMs",recording.clock.elapsed()},{"title",text},{"frameIndex",recording.times.size()}});qInfo().noquote()<<text;QTest::qWait(1500);};
        stage("1 / Hypit 示例库：官方来源、时长和去重结果");auto *sampleTree=window.findChild<QTreeWidget*>("samples");for(auto *tab:window.findChildren<QTabWidget*>())if(tab->count()&&tab->tabText(0)=="素材")tab->setCurrentIndex(2);backend.initialize();QTRY_VERIFY_WITH_TIMEOUT(initialized.count()||backendErrors.count(),30000);QVERIFY(backendErrors.isEmpty());sampleTree->setCurrentItem(sampleTree->topLevelItem(0));QTest::qWait(1800);
        stage("2 / 在 Qt 中创建可编辑工程，使用本地 Hypit 对话示例");window.sampleDocumentRequested(config.distributionPath+"/output/chat-demo.mp4",output+"/QtAgent集成工程","Qt × Agent · 从素材到成片");QTRY_VERIFY_WITH_TIMEOUT(bound.count()||creationErrors.count()||documentErrors.count()||backendErrors.count(),60000);QVERIFY2(documentErrors.isEmpty(),qPrintable(documentErrors.isEmpty()?QString():documentErrors.last()[0].toString()));QVERIFY(creationErrors.isEmpty());QVERIFY(backendErrors.isEmpty());QCOMPARE(bound.count(),1);QTRY_VERIFY_WITH_TIMEOUT(window.previewVersion().isConfirmed(),30000);
        auto *play=window.findChild<QPushButton*>("nativePlay");QVERIFY(play->isEnabled());play->click();QTest::qWait(2200);play->click();
        stage("3 / 导入另一个官方视频，选择素材并交给 Agent");const auto ranking=repo+"/.workbench/showcase/videos/ranking-hypit-final.mp4";QSignalSpy imported(&document,&controllers::DocumentController::assetImported);QVERIFY(document.importVideo(ranking));QTRY_VERIFY_WITH_TIMEOUT(imported.count()||documentErrors.count(),30000);QVERIFY(documentErrors.isEmpty());QCOMPARE(imported.count(),1);const auto replacement=imported[0][0].value<domain::Asset>();
        for(auto *tab:window.findChildren<QTabWidget*>())if(tab->count()&&tab->tabText(0)=="素材")tab->setCurrentIndex(0);window.selectImportedAsset(replacement);auto *handoff=window.findChild<QPushButton*>("handoffAssetToAgent");QVERIFY(handoff->isEnabled());const auto before=editor.snapshot().sourceFingerprint;const auto sourceBefore=editor.snapshot().sourceFiles;handoff->click();QTest::qWait(1500);QCOMPARE(editor.snapshot().sourceFingerprint,before);
        auto *goal=window.findChild<QPlainTextEdit*>("agentGoal");QVERIFY(goal->toPlainText().contains(replacement.path));QVERIFY(goal->isVisible());const auto requestedGoal=goal->toPlainText();
        stage("4 / 使用当前 Codex 登录生成方案，等待真实模型返回");auto *generate=window.findChild<QPushButton*>("agentGenerate");QVERIFY(generate->isEnabled());generate->click();QTRY_VERIFY_WITH_TIMEOUT(creator.status().phase=="review"||creator.status().phase=="failed"||creator.status().phase=="clarify",135000);QVERIFY2(creator.status().phase=="review",qPrintable(creator.status().message));QVERIFY(agentErrors.isEmpty());QCOMPARE(editor.snapshot().sourceFiles,sourceBefore);QVERIFY(creator.plan().has_value());const auto plan=creator.plan()->content;const auto ops=plan["operations"].toArray();QCOMPARE(ops.size(),1);QCOMPARE(ops.first().toObject()["value"].toString(),"./"+replacement.path);
        stage("5 / 审阅修改前后：确认前工程源码保持不变");QCOMPARE(editor.snapshot().sourceFingerprint,before);QTest::qWait(2200);auto *approve=window.findChild<QPushButton*>("planApprove");QVERIFY(approve->isVisible());QVERIFY(approve->isEnabled());approve->click();QTRY_VERIFY_WITH_TIMEOUT(creator.status().phase=="complete"||creator.status().phase=="failed"||creator.status().phase=="stale",30000);QVERIFY2(creator.status().phase=="complete",qPrintable(creator.status().message));QVERIFY(editErrors.isEmpty());QVERIFY(editor.snapshot().sourceFingerprint!=before);QCOMPARE(creator.status().completed,1);QTRY_VERIFY_WITH_TIMEOUT(window.previewVersion().isConfirmed()&&window.previewVersion().sourceFingerprint==editor.snapshot().sourceFingerprint,30000);
        stage("6 / 真实修改已写回 Hypit，Qt 预览同步到新版本");play->click();QTest::qWait(3000);play->click();const auto afterAsset=editor.snapshot().sourceFingerprint;
        stage("7 / 再用自然语言修改标题，复用同一审阅和执行链");QVERIFY(window.agentPanel()->composeGoal("只把当前视频组件标题改为‘Qt 与 Agent，让创作连起来’，其它文字、视频素材、时长和颜色都保持不变。"));generate->click();QTRY_VERIFY_WITH_TIMEOUT(creator.status().phase=="review"||creator.status().phase=="failed"||creator.status().phase=="clarify",135000);QVERIFY2(creator.status().phase=="review",qPrintable(creator.status().message));QCOMPARE(editor.snapshot().sourceFingerprint,afterAsset);const auto titlePlan=creator.plan()->content;QCOMPARE(titlePlan["operations"].toArray().size(),1);QTest::qWait(1800);QVERIFY(approve->isEnabled());approve->click();QTRY_VERIFY_WITH_TIMEOUT(creator.status().phase=="complete"||creator.status().phase=="failed",30000);QVERIFY2(creator.status().phase=="complete",qPrintable(creator.status().message));bool titleFound=false;for(const auto &t:editor.snapshot().tracks)for(const auto &c:t.clips)for(const auto &f:c.inspector)if(f.binding=="title")titleFound=f.rawValue.toString()=="Qt 与 Agent，让创作连起来";QVERIFY(titleFound);QTRY_VERIFY_WITH_TIMEOUT(window.previewVersion().sourceFingerprint==editor.snapshot().sourceFingerprint,30000);
        stage("8 / 导出成片：计划、构建、获取输出和全片解码");const auto film=output+"/Qt-Agent联动成片.mp4";window.exportRequested(film);QTRY_VERIFY_WITH_TIMEOUT(done.count()||exportErrors.count(),180000);QVERIFY2(exportErrors.isEmpty(),qPrintable(exporter.task().error));QCOMPARE(done.count(),1);QCOMPARE(exporter.task().phase,QString("complete"));QCOMPARE(exporter.task().sourceFingerprint,editor.snapshot().sourceFingerprint);
        stage("9 / 成片已完成，可查看任务与修改历史");for(auto *tab:window.findChildren<QTabWidget*>())for(int i=0;i<tab->count();++i)if(tab->tabText(i)=="工作历史")tab->setCurrentIndex(i);QTest::qWait(2500);QString error;QVERIFY2(history.save(&error),qPrintable(error));const auto fingerprint=editor.snapshot().sourceFingerprint;const auto project=document.project().manifestPath();QVERIFY2(recording.finish(config,&error),qPrintable(error));backend.closeProject();QVERIFY(!backend.isRunning());
        const QJsonObject report{{"verdict","PASS"},{"recording",output+"/Qt-Agent真实操作演示.mp4"},{"artifact",film},{"project",project},{"provider","current Codex login"},{"realModelRequests",2},{"publicMessages",publicMessages.count()},{"driver","scripted production widgets/signals; production AgentWorkbenchBridge; actual Codex/Hypit"},{"capture","continuous own MainWindow, actual frame intervals, no accelerated model waits"},{"recordingFrames",recording.times.size()},{"recordingElapsedMs",recording.elapsed},{"stages",stages},{"availableSamples",available},{"handoffGoal",requestedGoal},{"materialPlan",plan},{"titlePlan",titlePlan},{"sourceUnchangedBeforeApproval",true},{"explicitDriverApprovals",2},{"pixelsUploaded",false},{"artifactFullDecode",true},{"recordingFullDecode",true},{"buildId",exporter.task().buildId},{"previewFingerprint",QString::fromLatin1(fingerprint.toHex())},{"ownedStudioStopped",true},{"audio","original video-story template is muted"}};
        for(const auto &path:{output+"/demo.json",repo+"/docs/evidence/m13/agent-workbench-demo.json"}){QSaveFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(report).toJson());QVERIFY(file.commit());}
    }
};
QTEST_MAIN(AgentWorkbenchDemo)
#include "AgentWorkbenchDemo.moc"
