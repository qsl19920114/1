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
#include "ui/MediaPlayerDialog.h"
#include "ui/MediaPlayerWidget.h"
#include <QPointer>
#include <QComboBox>
#include <QCheckBox>
#include <QCryptographicHash>
class ShowcaseRecording {
public:
    ShowcaseRecording(QWidget &window,QString folder):window(window),folder(std::move(folder)){
        QDir().mkpath(this->folder+"/frames");target=&window;clock.start();timer.setInterval(200);
        QObject::connect(&timer,&QTimer::timeout,&window,[this]{capture();});timer.start();capture();
    }
    ~ShowcaseRecording(){timer.stop();}
    void capture(){
        if(times.size()>=3000){failed=true;timer.stop();return;}
        if(!target){failed=true;return;}const auto pixels=target->grab().toImage();if(pixels.isNull()){failed=true;return;}
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
        QProcess encode;encode.setProcessEnvironment(config.processEnvironment);encode.setWorkingDirectory(folder);encode.start(config.ffmpegPath,{"-y","-v","error","-f","concat","-safe","1","-i","recording.ffconcat","-r","30","-c:v","libx264","-preset","fast","-crf","21","-pix_fmt","yuv420p","-movflags","+faststart",folder+"/Qt人物素材替换与播放器演示.mp4"});
        if(!encode.waitForFinished(120000)||encode.exitCode()!=0){*error=QString::fromUtf8(encode.readAllStandardError());return false;}
        QProcess decode;decode.setProcessEnvironment(config.processEnvironment);decode.start(config.ffmpegPath,{"-v","error","-i",folder+"/Qt人物素材替换与播放器演示.mp4","-f","null","-"});if(!decode.waitForFinished(60000)||decode.exitCode()!=0){*error=QString::fromUtf8(decode.readAllStandardError());return false;}return true;
    }
    QWidget &window;QPointer<QWidget> target;QString folder;QTimer timer;QElapsedTimer clock;QVector<qint64> times;qint64 elapsed=0;bool failed=false;
};
class CharacterShowcaseDemo:public QObject {
 Q_OBJECT
private slots:
 void realCharacterVariantsAndPlayer(){
        const QString repo=QVW_SOURCE_DIR;const auto output=repo+"/.workbench/deliverables-m14/runs/"+QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");QVERIFY(QDir().mkpath(output));QVERIFY(QDir().mkpath(repo+"/docs/evidence/m14"));
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
        window.show();ShowcaseRecording recording(window,output);QJsonArray stages,artifacts;
        auto stage=[&](const QString &text){window.statusBar()->showMessage(text);stages.append(QJsonObject{{"atMs",recording.clock.elapsed()},{"title",text}});qInfo().noquote()<<text;QTest::qWait(1200);};
        auto hash=[](const QString &path){QFile file(path);if(!file.open(QIODevice::ReadOnly))return QString();QCryptographicHash h(QCryptographicHash::Sha256);h.addData(&file);return QString::fromLatin1(h.result().toHex());};
        stage("1 / Qt 内嵌 AI · 人物素材替换演示，保留版式与动画");backend.initialize();QTRY_VERIFY_WITH_TIMEOUT(initialized.count()||backendErrors.count(),30000);QVERIFY(backendErrors.isEmpty());
        QVERIFY(document.create(repo+"/templates/title-card",output+"/人物素材工程","FrameLab · 人物表达实验"));QTRY_VERIFY_WITH_TIMEOUT(editor.isReady()||backendErrors.count(),60000);QVERIFY(backendErrors.isEmpty());
        QSignalSpy imported(&document,&controllers::DocumentController::assetImported);QVector<domain::Asset> portraits;
        for(const auto &name:{"portrait-baseline.png","portrait-variant-1.png","portrait-variant-2.png"}){QVERIFY(document.importImage(repo+"/.workbench/character-showcase/materials/"+name));portraits.append(imported.last()[0].value<domain::Asset>());}
        auto field=[&](const QString &binding){for(const auto &t:editor.snapshot().tracks)for(const auto &c:t.clips)for(const auto &f:c.inspector)if(f.binding==binding)return QPair<QString,domain::InspectorField>{c.id,f};return QPair<QString,domain::InspectorField>{};};
        auto properties=[&]{QJsonObject values;for(const auto &t:editor.snapshot().tracks)for(const auto &c:t.clips)for(const auto &f:c.inspector)if(f.binding!="image")values.insert(c.id+"/"+f.binding,QJsonValue::fromVariant(f.rawValue));return values;};
        auto stable=[&]{QElapsedTimer deadline;deadline.start();int equal=0;int revision=-1;QByteArray fingerprint;while(deadline.elapsed()<10000){QTest::qWait(200);if(editor.isBusy())continue;editor.refresh();while(editor.isBusy()&&deadline.elapsed()<10000)QTest::qWait(25);const auto snapshot=editor.snapshot();if(snapshot.revision==revision&&snapshot.sourceFingerprint==fingerprint)++equal;else equal=0;revision=snapshot.revision;fingerprint=snapshot.sourceFingerprint;if(equal>=3&&window.previewVersion().isConfirmed()&&window.previewVersion().sourceFingerprint==fingerprint)return true;}return false;};
        QVERIFY(stable());
        auto write=[&](const QString &binding,const QJsonValue &value){const auto f=field(binding);editor.edit(f.first,f.second.id,value.toVariant());};
        write("title","同一舞台，不同人物");QTRY_VERIFY_WITH_TIMEOUT(!editor.isBusy(),30000);QVERIFY2(editErrors.isEmpty(),qPrintable(editErrors.isEmpty()?QString():editErrors.last()[0].toString()));
        QVERIFY(stable());write("subtitle","Qt × AI × Hypit\n保留版式、文案与动画，只替换人物素材。");QTRY_VERIFY_WITH_TIMEOUT(!editor.isBusy(),30000);QVERIFY2(editErrors.isEmpty(),qPrintable(editErrors.isEmpty()?QString():editErrors.last()[0].toString()));
        QVERIFY(stable());write("image","./"+portraits[0].path);QTRY_VERIFY_WITH_TIMEOUT(!editor.isBusy(),30000);QVERIFY2(editErrors.isEmpty(),qPrintable(editErrors.isEmpty()?QString():editErrors.last()[0].toString()));QCOMPARE(field("image").second.rawValue.toString(),"./"+portraits[0].path);
        QTRY_VERIFY_WITH_TIMEOUT(window.previewVersion().isConfirmed()&&window.previewVersion().sourceFingerprint==editor.snapshot().sourceFingerprint,30000);
        const auto fixedProperties=properties();const QStringList names{"01-原始人物.mp4","02-替换人物.mp4","03-插画人物.mp4"};QJsonArray plans;
        auto *generate=window.findChild<QPushButton*>("agentGenerate");auto *approve=window.findChild<QPushButton*>("planApprove");
        for(int variant=0;variant<3;++variant){
            if(variant){
                stage(QString("%1 / 选择第 %2 个角色素材，交给内嵌 Agent").arg(variant==1?3:6).arg(variant+1));window.selectImportedAsset(portraits[variant]);
                auto *handoff=window.findChild<QPushButton*>("handoffCharacterToAgent");QVERIFY(handoff);QVERIFY(handoff->isEnabled());const auto before=editor.snapshot();handoff->click();QTest::qWait(1800);QCOMPARE(editor.snapshot().sourceFingerprint,before.sourceFingerprint);
                QVERIFY(generate->isEnabled());generate->click();QTRY_VERIFY_WITH_TIMEOUT(creator.status().phase=="review"||creator.status().phase=="failed"||creator.status().phase=="clarify",135000);QVERIFY2(creator.status().phase=="review",qPrintable(creator.status().message));QVERIFY(agentErrors.isEmpty());QCOMPARE(editor.snapshot().sourceFiles,before.sourceFiles);QCOMPARE(editor.snapshot().sourceFingerprint,before.sourceFingerprint);
                const auto plan=creator.plan()->content;const auto ops=plan["operations"].toArray();QCOMPARE(ops.size(),1);QCOMPARE(ops[0].toObject()["fieldId"].toString(),field("image").second.id);QCOMPARE(ops[0].toObject()["value"].toString(),"./"+portraits[variant].path);plans.append(plan);
                stage(QString("%1 / 审阅人物素材修改，确认后才写入工程").arg(variant==1?4:7));QVERIFY(approve->isEnabled());approve->click();QTRY_VERIFY_WITH_TIMEOUT(creator.status().phase=="complete"||creator.status().phase=="failed"||creator.status().phase=="stale",30000);QVERIFY2(creator.status().phase=="complete",qPrintable(creator.status().message));QVERIFY2(editErrors.isEmpty(),qPrintable(editErrors.isEmpty()?QString():editErrors.last()[0].toString()));QCOMPARE(field("image").second.rawValue.toString(),"./"+portraits[variant].path);QCOMPARE(properties(),fixedProperties);QVERIFY(editor.snapshot().sourceFingerprint!=before.sourceFingerprint);
                QTRY_VERIFY_WITH_TIMEOUT(window.previewVersion().isConfirmed()&&window.previewVersion().sourceFingerprint==editor.snapshot().sourceFingerprint,30000);QTest::qWait(2200);
            }
            stage(QString("%1 / 导出%2：真实本地渲染与成片校验").arg(variant==0?2:variant==1?5:8).arg(variant==0?"基准人物":variant==1?"替换人物":"插画人物"));const auto film=output+"/"+names[variant];const auto previous=done.count();window.exportRequested(film);QTRY_VERIFY_WITH_TIMEOUT(done.count()>previous||exportErrors.count(),180000);QVERIFY2(exportErrors.isEmpty(),qPrintable(exporter.task().error));QCOMPARE(exporter.task().phase,QString("complete"));QCOMPARE(exporter.task().sourceFingerprint,editor.snapshot().sourceFingerprint);
            artifacts.append(QJsonObject{{"path",film},{"sha256",hash(film)},{"sourceFingerprint",QString::fromLatin1(editor.snapshot().sourceFingerprint.toHex())},{"portrait",portraits[variant].originalName},{"binding","./"+portraits[variant].path},{"fullDecode",true},{"buildId",exporter.task().buildId}});
        }
        stage("9 / Qt 原生播放器：播放、定位、倍速、音量、循环与全屏");
        auto *playerDialog=new ui::MediaPlayerDialog(output+"/"+names[2],"FrameLab · 人物成片播放",&window);QPointer<ui::MediaPlayerDialog> guard(playerDialog);playerDialog->resize(1400,900);playerDialog->show();recording.target=playerDialog;auto *player=playerDialog->player();QTRY_VERIFY_WITH_TIMEOUT(player->playbackState().ready,20000);
        player->findChild<QPushButton*>("mediaPlayPause")->click();QTRY_VERIFY_WITH_TIMEOUT(!player->playbackState().paused&&player->playbackState().position>0.3,10000);QTest::qWait(1800);player->findChild<QPushButton*>("mediaPlayPause")->click();QTRY_VERIFY_WITH_TIMEOUT(player->playbackState().paused,5000);
        player->seek(3.0);QTRY_VERIFY_WITH_TIMEOUT(qAbs(player->playbackState().position-3.0)<0.35,5000);player->findChild<QComboBox*>("mediaRate")->setCurrentIndex(2);QTRY_VERIFY_WITH_TIMEOUT(qAbs(player->playbackState().rate-1.5)<0.01,5000);
        player->findChild<QSlider*>("mediaVolume")->setValue(35);QTRY_VERIFY_WITH_TIMEOUT(qAbs(player->playbackState().volume-0.35)<0.02,5000);player->setMuted(true);player->setLoop(true);QTRY_VERIFY_WITH_TIMEOUT(player->playbackState().muted&&player->playbackState().loop,5000);QTest::qWait(2000);
        player->findChild<QPushButton*>("mediaFullscreen")->click();QTRY_VERIFY_WITH_TIMEOUT(playerDialog->isFullScreen(),5000);QTest::qWait(1800);QTest::keyClick(playerDialog,Qt::Key_Escape);QTRY_VERIFY_WITH_TIMEOUT(guard&&!guard->isFullScreen(),5000);
        player->findChild<QPushButton*>("mediaStop")->click();QTRY_VERIFY_WITH_TIMEOUT(player->playbackState().paused&&player->playbackState().position<0.1,5000);QTest::qWait(1400);recording.target=&window;playerDialog->close();QTRY_VERIFY(guard.isNull());
        stage("10 / 完成：三份人物成片与可继续编辑的 Qt 工程");const auto fingerprint=editor.snapshot().sourceFingerprint;const auto project=document.project().manifestPath();QString error;QVERIFY2(recording.finish(config,&error),qPrintable(error));backend.closeProject();QVERIFY(!backend.isRunning());
        const auto video=output+"/Qt人物素材替换与播放器演示.mp4";QJsonObject report{{"verdict","PASS"},{"recording",video},{"recordingSha256",hash(video)},{"recordingElapsedMs",recording.elapsed},{"recordingFrames",recording.times.size()},{"recordingFullDecode",true},{"project",project},{"artifacts",artifacts},{"plans",plans},{"realModelRequests",2},{"explicitDriverApprovals",2},{"unchangedBeforeApproval",true},{"nonImagePropertiesPreserved",true},{"playerControlsVerified",QJsonArray{"play","pause","seek","rate","volume","mute","loop","fullscreen","escape","stop"}},{"previewFingerprint",QString::fromLatin1(fingerprint.toHex())},{"stages",stages},{"ownedStudioStopped",true},{"pixelsUploaded",false},{"scope","Replace registered character image material; no generative video identity transformation"},{"driver","scripted production Qt widgets/signals; actual Codex/Hypit; real own-window recording"}};
        for(const auto &path:{output+"/demo.json",repo+"/docs/evidence/m14/character-showcase.json"}){QSaveFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(report).toJson());QVERIFY(file.commit());}
 }
};
QTEST_MAIN(CharacterShowcaseDemo)
#include "CharacterShowcaseDemo.moc"
