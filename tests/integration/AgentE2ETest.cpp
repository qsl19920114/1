#include "agent/AgentController.h"
#include "agent/AgentTaskStore.h"
#include "agent/ToolDispatcher.h"
#include "controllers/ProjectController.h"
#include "infrastructure/AppConfig.h"
#include <QTest>
#include <QImage>
#include <QPainter>
#include <QSaveFile>
#include <QSignalSpy>
#include <QDateTime>
using namespace qvw;
class AgentE2ETest : public QObject {
    Q_OBJECT
    const domain::Clip *clip(const domain::Snapshot &s,const QString &id) const {for(const auto &t:s.tracks)for(const auto &c:t.clips)if(c.id==id||c.authoredId==id)return &c;return nullptr;}
    domain::InspectorField field(const domain::Snapshot &s,const QString &id,const QString &binding) const {const auto *c=clip(s,id);if(c)for(const auto &f:c->inspector)if(f.binding==binding)return f;return {};}
private slots:
    void realModelCreatesEditsReopensAndExports() {
        const QString repo=QStringLiteral(QVW_SOURCE_DIR);const auto folder=repo+"/.workbench/agent-creation-"+QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz");QVERIFY(QDir().mkpath(folder));
        infra::LogWriter log(folder+"/application.jsonl");const auto config=infra::loadAppConfig(repo+"/config/version-lock.json");QVERIFY2(config.ok(),qPrintable(config.error));
        controllers::ProjectController backend(config.config,log);controllers::DocumentController document;controllers::EditorController editor;controllers::ExportController exporter(config.config,log);
        agent::ModelClient model;agent::AgentController creator(document,editor,exporter,model,repo+"/templates");
        connect(&document,&controllers::DocumentController::projectLoaded,&backend,[&](const domain::Project &p){exporter.setProject(p);backend.openDocument(p,5790);});
        connect(&document,&controllers::DocumentController::documentClosed,&backend,[&]{backend.closeProject();exporter.clearProject();});
        connect(&backend,&controllers::ProjectController::previewRequested,&editor,[&](const QUrl &url){editor.attach(url,backend.workspace());});
        connect(&backend,&controllers::ProjectController::projectClosed,&editor,&controllers::EditorController::clear);
        connect(&backend,&controllers::ProjectController::snapshotReady,&editor,&controllers::EditorController::acceptSnapshot);
        connect(&backend,&controllers::ProjectController::failed,&creator,&agent::AgentController::backendFailed);
        connect(&creator,&agent::AgentController::message,this,[](const QString &m){qInfo().noquote()<<m;});
        QSignalSpy init(&backend,&controllers::ProjectController::initialized),backendFailed(&backend,&controllers::ProjectController::failed),agentFailed(&creator,&agent::AgentController::failed),edited(&editor,&controllers::EditorController::operationSucceeded),editFailed(&editor,&controllers::EditorController::failed);
        backend.initialize();QTRY_VERIFY_WITH_TIMEOUT(!init.isEmpty()||!backendFailed.isEmpty(),30000);QVERIFY(backendFailed.isEmpty());
        QStringList images;
        for(int i=0;i<3;++i){QImage image(720,960,QImage::Format_RGB32);image.fill(QColor(i==0?"#246b79":i==1?"#8754a3":"#af7443"));QPainter p(&image);p.setPen(QColor("#f5f5f5"));p.setFont(QFont("Arial",80,QFont::Bold));p.drawText(image.rect(),Qt::AlignCenter,QString("FRAME %1").arg(i+1));p.end();auto path=folder+QString("/本地背景%1.png").arg(i+1);QVERIFY(image.save(path));images.append(path);}
        QVERIFY(creator.setImages(images));creator.generate(QStringLiteral("用已选择的三张本地背景图片创建一个十五秒的校园创作社招新短片，竖屏，活泼青绿色。开场3秒、主体8秒、结尾4秒，每段用不同图片。结尾标题必须完整写‘周五晚七点，欢迎加入’。图片只是背景素材，不要猜测画面内容。"));
        QTRY_VERIFY_WITH_TIMEOUT(creator.status().phase=="review"||creator.status().phase=="failed"||creator.status().phase=="clarify",130000);
        QVERIFY2(creator.status().phase=="review",qPrintable(creator.status().message));QVERIFY(!document.hasProject());QVERIFY(creator.plan().has_value());QCOMPARE(creator.plan()->kind(),QString("create"));
        auto reviewed=creator.plan()->content;auto scenes=reviewed["scenes"].toArray();QCOMPARE(scenes.size(),3);int total=0;for(const auto &v:scenes)total+=v.toObject()["durationFrames"].toInt();QCOMPARE(total,450);
        // Exercise an actual card review: replace opening copy while keeping generated choices/timing.
        auto opening=scenes[0].toObject();opening["title"]="校园创作社 · 向光而行";scenes[0]=opening;reviewed["scenes"]=scenes;creator.updatePlan(reviewed);
        creator.approve(folder+"/校园故事工程");QTRY_VERIFY_WITH_TIMEOUT(creator.status().phase=="complete"||creator.status().phase=="failed"||creator.status().phase=="stale",90000);
        QVERIFY2(creator.status().phase=="complete",qPrintable(creator.status().message));QVERIFY(editFailed.isEmpty());QCOMPARE(creator.status().completed,15);QCOMPARE(editor.snapshot().space.width,720);QCOMPARE(editor.snapshot().space.height,1280);QCOMPARE(editor.snapshot().space.frameCount,450);
        int boundary=0;for(const auto &v:scenes){const auto o=v.toObject();const auto *c=clip(editor.snapshot(),o["sceneId"].toString());QVERIFY(c);QCOMPARE(c->startFrame,boundary);boundary+=o["durationFrames"].toInt();QCOMPARE(c->endFrameExclusive,boundary);QCOMPARE(field(editor.snapshot(),c->id,"title").rawValue.toString(),o["title"].toString());}
        const auto creationSource=editor.snapshot().sourceFiles;const auto ending=clip(editor.snapshot(),scenes.last().toObject()["sceneId"].toString())->id;creator.setSelection(ending);creator.setScope(ending);
        creator.generate(QStringLiteral("结尾太正式了，只把当前选中结尾组件的标题改为‘周五一起玩！’，副标题、图片、时长、颜色、其他场景都不要动。"));
        QTRY_VERIFY_WITH_TIMEOUT(creator.status().phase=="review"||creator.status().phase=="failed"||creator.status().phase=="clarify",130000);QVERIFY2(creator.status().phase=="review",qPrintable(creator.status().message));
        QCOMPARE(editor.snapshot().sourceFiles,creationSource);const auto ops=creator.plan()->content["operations"].toArray();QCOMPARE(ops.size(),1);QCOMPARE(ops.first().toObject()["entityId"].toString(),ending);QCOMPARE(ops.first().toObject()["fieldId"].toString(),field(editor.snapshot(),ending,"title").id);
        creator.approve();QTRY_VERIFY_WITH_TIMEOUT(creator.status().phase=="complete"||creator.status().phase=="failed",30000);QVERIFY2(creator.status().phase=="complete",qPrintable(creator.status().message));QCOMPARE(field(editor.snapshot(),ending,"title").rawValue.toString(),QString("周五一起玩！"));
        for(int i=0;i<2;i++){const auto id=scenes[i].toObject()["sceneId"].toString();QCOMPARE(field(editor.snapshot(),id,"title").rawValue.toString(),scenes[i].toObject()["title"].toString());}
        const auto finalFingerprint=editor.snapshot().sourceFingerprint;
        creator.undoLast();QTRY_VERIFY_WITH_TIMEOUT(creator.status().phase=="paused"||creator.status().phase=="failed",30000);QVERIFY2(creator.status().phase=="paused",qPrintable(creator.status().message));QCOMPARE(field(editor.snapshot(),ending,"title").rawValue.toString(),scenes.last().toObject()["title"].toString());const int beforeRedo=edited.size();editor.redo();QTRY_COMPARE_WITH_TIMEOUT(edited.size(),beforeRedo+1,30000);QCOMPARE(editor.snapshot().sourceFingerprint,finalFingerprint);
        const auto manifest=document.project().manifestPath();document.close();QVERIFY(!backend.isRunning());QVERIFY(document.open(manifest));QTRY_VERIFY_WITH_TIMEOUT(editor.isReady()&&editor.workspace()==document.project().rootPath,60000);creator.restore();
        QCOMPARE(editor.snapshot().sourceFingerprint,finalFingerprint);QVERIFY(!creator.status().canApprove);QVERIFY(!creator.status().taskId.isEmpty());
        QSignalSpy exported(&exporter,&controllers::ExportController::completed),exportFailed(&exporter,&controllers::ExportController::failed);creator.startExport(folder+"/校园创作社-15秒.mp4");
        QTRY_VERIFY_WITH_TIMEOUT(!exported.isEmpty()||!exportFailed.isEmpty(),240000);QVERIFY2(exportFailed.isEmpty(),qPrintable(exporter.task().error));QCOMPARE(exporter.task().phase,QString("complete"));QCOMPARE(exporter.task().sourceFingerprint,finalFingerprint);QVERIFY(QFileInfo(exporter.task().destination).size()>0);
        QJsonObject state;QString error;QVERIFY(agent::AgentTaskStore::load(document.project(),&state,&error));QCOMPARE(state["buildId"].toString(),exporter.task().buildId);
        const QJsonObject report{{"verdict","PASS"},{"provider","codex-current-chatgpt-login"},{"realModelRequests",2},{"testDriverConfirmsPlan",true},{"pixelsUploaded",false},{"modelPlan",reviewed},{"createSteps",15},{"finalFingerprint",QString::fromLatin1(finalFingerprint.toHex())},{"project",manifest},{"artifact",exporter.task().destination},{"buildId",exporter.task().buildId},{"width",720},{"height",1280},{"fps",30},{"frames",450},{"undoRedo",true},{"reopened",true},{"mediaValidation","ExportController ffprobe and full decode"},{"screenshots",false},{"semanticVisualVerdict","requires human viewing"}};
        QSaveFile file(repo+"/docs/evidence/m8/agent-e2e.json");const auto bytes=QJsonDocument(report).toJson();QVERIFY(file.open(QIODevice::WriteOnly));QCOMPARE(file.write(bytes),bytes.size());QVERIFY(file.commit());document.close();QVERIFY(!backend.isRunning());
    }
};
QTEST_MAIN(AgentE2ETest)
#include "AgentE2ETest.moc"
