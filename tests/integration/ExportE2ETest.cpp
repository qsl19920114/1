#include "controllers/ExportController.h"
#include "controllers/ProjectController.h"
#include "controllers/EditorController.h"
#include "services/ProjectStore.h"
#include "services/AssetService.h"
#include "infrastructure/AppConfig.h"
#include "infrastructure/LogWriter.h"
#include <QTest>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QImage>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSet>
#include <QFileInfo>
class ExportE2ETest : public QObject {
    Q_OBJECT
private slots:
    void originalProjectToValidatedFilm() {
        const QString repo=QStringLiteral(QVW_SOURCE_DIR);
        QTemporaryDir temporary(repo+"/.workbench/M4 导出-XXXXXX");QVERIFY(temporary.isValid());
        qvw::domain::Project project;QString error;
        QVERIFY2(qvw::services::ProjectStore::create(repo+"/templates/title-card",temporary.filePath("校园 工程"),"M4 创作闭环",&project,&error),qPrintable(error));
        QImage image(100,100,QImage::Format_RGB32);image.fill(QColor("#e6a04f"));
        const auto input=temporary.filePath("原始 图片.png");QVERIFY(image.save(input));qvw::domain::Asset asset;
        QVERIFY2(qvw::services::AssetService::importImage(project,input,&asset,&error),qPrintable(error));
        const auto config=qvw::infra::loadAppConfig(repo+"/config/version-lock.json");QVERIFY2(config.ok(),qPrintable(config.error));
        struct RuntimeCleanup {
            QString launcher,runtime;QSet<QString> workspaces;
            ~RuntimeCleanup(){for(const auto &root:workspaces){QProcess p;p.start(launcher,{"runtime","down","--workspace",root,"--runtime",QDir(root).filePath(runtime),"--json"});p.waitForFinished(30000);}}
        } cleanup{config.config.launcherPath,project.runtimePath,{}};
        qvw::infra::LogWriter log(temporary.filePath("export.jsonl"));
        qvw::controllers::ProjectController backend(config.config,log);qvw::controllers::EditorController editor;
        qvw::controllers::ExportController exporter(config.config,log);
        connect(&backend,&qvw::controllers::ProjectController::previewRequested,&editor,[&](const QUrl &url){editor.attach(url,backend.workspace());});
        connect(&backend,&qvw::controllers::ProjectController::projectClosed,&editor,&qvw::controllers::EditorController::clear);
        connect(&backend,&qvw::controllers::ProjectController::snapshotReady,&editor,&qvw::controllers::EditorController::acceptSnapshot);
        connect(&exporter,&qvw::controllers::ExportController::message,this,[](const QString &s){qInfo().noquote()<<s;});
        connect(&exporter,&qvw::controllers::ExportController::failed,this,[](const QString &s){qWarning().noquote()<<s;});
        connect(&exporter,&qvw::controllers::ExportController::taskChanged,this,[](const qvw::domain::ExportTask &t){qInfo().noquote()<<"EXPORT"<<t.phase<<t.buildId<<t.error;});
        connect(&exporter,&qvw::controllers::ExportController::taskChanged,this,[&](const qvw::domain::ExportTask &t){if(!t.workspace.isEmpty())cleanup.workspaces.insert(t.workspace);});
        QSignalSpy initialized(&backend,&qvw::controllers::ProjectController::initialized),backendFailed(&backend,&qvw::controllers::ProjectController::failed);
        QSignalSpy edited(&editor,&qvw::controllers::EditorController::operationSucceeded),editFailed(&editor,&qvw::controllers::EditorController::failed);
        QSignalSpy done(&exporter,&qvw::controllers::ExportController::completed),failed(&exporter,&qvw::controllers::ExportController::failed);
        backend.initialize();QTRY_VERIFY_WITH_TIMEOUT(!initialized.isEmpty()||!backendFailed.isEmpty(),25000);QVERIFY(backendFailed.isEmpty());
        backend.openDocument(project,5750);QTRY_VERIFY_WITH_TIMEOUT(editor.snapshot().isLoaded()||!backendFailed.isEmpty(),60000);QVERIFY(backendFailed.isEmpty());
        auto field=[&](const QString &binding){for(const auto &track:editor.snapshot().tracks)for(const auto &clip:track.clips)for(const auto &f:clip.inspector)if(f.binding==binding)return f;return qvw::domain::InspectorField{};};
        auto edit=[&](const QString &binding,const QVariant &value){editor.edit(editor.snapshot().tracks.front().clips.front().id,field(binding).id,value);};
        edit("title",QStringLiteral("校园摄影社 · 成片验证"));QTRY_VERIFY_WITH_TIMEOUT(edited.size()==1||!editFailed.isEmpty(),30000);QCOMPARE(editFailed.size(),0);
        edit("color",QStringLiteral("#e47735"));QTRY_VERIFY_WITH_TIMEOUT(edited.size()==2||!editFailed.isEmpty(),30000);QCOMPARE(editFailed.size(),0);
        edit("image","./"+asset.path);QTRY_VERIFY_WITH_TIMEOUT(edited.size()==3||!editFailed.isEmpty(),30000);QCOMPARE(editFailed.size(),0);
        exporter.setProject(project);
        const auto film=temporary.filePath("真实 成片.mp4");exporter.startExport(editor.snapshot(),film);
        QTRY_VERIFY_WITH_TIMEOUT(!exporter.task().buildId.isEmpty()||!failed.isEmpty(),60000);QVERIFY2(failed.isEmpty(),qPrintable(failed.isEmpty()?QString():failed.last().first().toString()));
        const auto buildId=exporter.task().buildId,workspace=exporter.task().workspace;
        QVERIFY(!buildId.isEmpty());QVERIFY(workspace.startsWith(project.rootPath+"/"));
        exporter.stopObserving();QCOMPARE(done.size(),0);
        exporter.clearProject();exporter.setProject(project);exporter.resume();
        QCOMPARE(exporter.task().buildId,buildId);
        // Original project edits after submission cannot alter frozen inputs.
        const auto sourcePath=QDir(project.rootPath).filePath(project.sourcePath);
        QFile originalSource(sourcePath);QVERIFY(originalSource.open(QIODevice::ReadOnly));auto bytes=originalSource.readAll();originalSource.close();
        bytes.replace("校园摄影社 · 成片验证", "原工程后续编辑");
        QSaveFile later(sourcePath);QVERIFY(later.open(QIODevice::WriteOnly));later.write(bytes);QVERIFY(later.commit());
        QFile frozen(QDir(workspace).filePath(project.sourcePath));QVERIFY(frozen.open(QIODevice::ReadOnly));QVERIFY(frozen.readAll().contains("校园摄影社 · 成片验证"));frozen.close();
        QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty()||!failed.isEmpty(),240000);QVERIFY2(failed.isEmpty(),qPrintable(failed.isEmpty()?QString():failed.last().first().toString()));
        QCOMPARE(done.size(),1);QCOMPARE(exporter.task().phase,QString("complete"));QVERIFY(QFileInfo(film).size()>1000);
        const auto exampleDir=repo+"/.workbench/deliverables";QVERIFY(QDir().mkpath(exampleDir));
        const auto example=exampleDir+"/校园摄影社-验证成片.mp4";QFile::remove(example);QVERIFY(QFile::copy(film,example));
        QProcess probe;probe.start("/opt/homebrew/bin/ffprobe",{"-v","error","-show_streams","-show_format","-of","json",film});QVERIFY(probe.waitForFinished(15000));QCOMPARE(probe.exitCode(),0);
        QFile probeEvidence(repo+"/docs/evidence/m4/ffprobe.json");QVERIFY(probeEvidence.open(QIODevice::WriteOnly));probeEvidence.write(probe.readAllStandardOutput());
        editor.refresh();QTRY_VERIFY_WITH_TIMEOUT(!editor.isBusy(),15000);
        const auto cancelledFilm=temporary.filePath("不应交付.mp4");
        exporter.startExport(editor.snapshot(),cancelledFilm);
        QTRY_VERIFY_WITH_TIMEOUT(exporter.task().buildId!=buildId&&!exporter.task().buildId.isEmpty()||!failed.isEmpty(),60000);QVERIFY(failed.isEmpty());
        const auto cancelId=exporter.task().buildId;exporter.cancelBuild();
        QTRY_VERIFY_WITH_TIMEOUT(exporter.task().phase=="cancelled"||!failed.isEmpty(),60000);QVERIFY2(failed.isEmpty(),qPrintable(failed.isEmpty()?QString():failed.last().first().toString()));
        QCOMPARE(exporter.task().buildId,cancelId);QCOMPARE(exporter.task().phase,QString("cancelled"));QCOMPARE(done.size(),1);QVERIFY(!QFileInfo::exists(cancelledFilm));
        // A genuine compiler/plan failure in this fixture's own package must
        // never submit a Build or deliver a destination file.
        QFile activation(QDir(project.rootPath).filePath("packages/title-card/activation.js"));
        QVERIFY(activation.open(QIODevice::WriteOnly|QIODevice::Truncate));
        activation.write("export const broken = ;\n");activation.close();
        const auto rejectedFilm=temporary.filePath("失败不能交付.mp4");
        exporter.startExport(editor.snapshot(),rejectedFilm);
        QTRY_VERIFY_WITH_TIMEOUT(!failed.isEmpty(),60000);
        QCOMPARE(exporter.task().phase,QString("failed"));
        QVERIFY(exporter.task().buildId.isEmpty());QCOMPARE(done.size(),1);
        QVERIFY(!QFileInfo::exists(rejectedFilm));
        backend.closeProject();QVERIFY(!backend.isRunning());
        QFile evidence(repo+"/docs/evidence/m4/e2e.json");QVERIFY(evidence.open(QIODevice::WriteOnly));
        evidence.write(QJsonDocument(QJsonObject{{"verdict","PASS"},{"nativeTitleColorImage",true},{"frozenInputs",true},{"stoppedObservationIsNotCancellation",true},{"reopenedSameBuild",true},{"exactOutput","final.video"},{"buildId",buildId},{"validatedMp4",true},{"cancelledBuildId",cancelId},{"cancelledWithoutDelivering",true},{"realPlanFailureWithoutDelivery",true},{"screenshots",false},{"exampleFilm",example}}).toJson());
    }
};
QTEST_GUILESS_MAIN(ExportE2ETest)
#include "ExportE2ETest.moc"
