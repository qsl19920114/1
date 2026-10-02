#include "controllers/ProposalController.h"
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
#include <QJsonDocument>
#include <QJsonObject>
class ProposalE2ETest : public QObject {
    Q_OBJECT
private slots:
    void explicitConfirmationUsesRealEditorHistory() {
        const QString repo=QStringLiteral(QVW_SOURCE_DIR);
        QTemporaryDir temporary(repo+"/.workbench/M5 提案-XXXXXX");QVERIFY(temporary.isValid());
        qvw::domain::Project project;QString error;
        QVERIFY2(qvw::services::ProjectStore::create(repo+"/templates/title-card",temporary.filePath("校园 工程"),"M5 提案",&project,&error),qPrintable(error));
        QImage image(100,100,QImage::Format_RGB32);image.fill(QColor("#54a38a"));
        const auto input=temporary.filePath("图片.png");QVERIFY(image.save(input));qvw::domain::Asset asset;
        QVERIFY2(qvw::services::AssetService::importImage(project,input,&asset,&error),qPrintable(error));
        const auto config=qvw::infra::loadAppConfig(repo+"/config/version-lock.json");QVERIFY2(config.ok(),qPrintable(config.error));
        qvw::infra::LogWriter log(temporary.filePath("proposal.jsonl"));
        qvw::controllers::ProjectController backend(config.config,log);qvw::controllers::EditorController editor;
        qvw::controllers::ProposalController proposals(editor);proposals.setProject(project);
        connect(&backend,&qvw::controllers::ProjectController::previewRequested,&editor,[&](const QUrl &url){editor.attach(url,backend.workspace());});
        connect(&backend,&qvw::controllers::ProjectController::projectClosed,&editor,&qvw::controllers::EditorController::clear);
        connect(&backend,&qvw::controllers::ProjectController::snapshotReady,&editor,&qvw::controllers::EditorController::acceptSnapshot);
        QSignalSpy initialized(&backend,&qvw::controllers::ProjectController::initialized),backendFailed(&backend,&qvw::controllers::ProjectController::failed);
        QSignalSpy edited(&editor,&qvw::controllers::EditorController::operationSucceeded),editFailed(&editor,&qvw::controllers::EditorController::failed);
        QSignalSpy rejected(&proposals,&qvw::controllers::ProposalController::failed);
        backend.initialize();QTRY_VERIFY_WITH_TIMEOUT(!initialized.isEmpty()||!backendFailed.isEmpty(),25000);QVERIFY(backendFailed.isEmpty());
        backend.openDocument(project,5760);QTRY_VERIFY_WITH_TIMEOUT(editor.snapshot().isLoaded()||!backendFailed.isEmpty(),60000);QVERIFY(backendFailed.isEmpty());
        auto field=[&](const QString &binding){for(const auto &track:editor.snapshot().tracks)for(const auto &clip:track.clips)for(const auto &f:clip.inspector)if(f.binding==binding)return f;return qvw::domain::InspectorField{};};
        const auto initialTitle=field("title").rawValue.toString();const auto beforeFingerprint=editor.snapshot().sourceFingerprint;
        proposals.generateDemo(QStringLiteral("标题改为校园摄影社 · 提案确认"));QVERIFY2(proposals.pending(),qPrintable(proposals.summary()));
        QVERIFY(proposals.summary().contains(QStringLiteral("模拟")));QCOMPARE(edited.size(),0);QCOMPARE(editor.snapshot().sourceFingerprint,beforeFingerprint);
        const auto initialSource=editor.snapshot().sourceFiles.value(project.sourcePath);QFile source(QDir(project.rootPath).filePath(project.sourcePath));
        QVERIFY(source.open(QIODevice::ReadOnly));QCOMPARE(QString::fromUtf8(source.readAll()),initialSource);source.close();
        proposals.confirm();QTRY_VERIFY_WITH_TIMEOUT(edited.size()==1||!editFailed.isEmpty(),30000);QCOMPARE(editFailed.size(),0);QVERIFY(!proposals.pending());
        QCOMPARE(field("title").rawValue.toString(),QStringLiteral("校园摄影社 · 提案确认"));
        editor.undo();QTRY_COMPARE_WITH_TIMEOUT(edited.size(),2,30000);QCOMPARE(field("title").rawValue.toString(),initialTitle);
        proposals.generateDemo(QStringLiteral("图片使用第1张"));QVERIFY(proposals.pending());proposals.confirm();
        QTRY_COMPARE_WITH_TIMEOUT(edited.size(),3,30000);QCOMPARE(field("image").rawValue.toString(),"./"+asset.path);
        editor.undo();QTRY_COMPARE_WITH_TIMEOUT(edited.size(),4,30000);QCOMPARE(field("image").rawValue.toString(),QString("./assets/default.png"));
        proposals.generateDemo(QStringLiteral("主题色改为#e47735"));QVERIFY(proposals.pending());
        editor.edit(editor.snapshot().tracks.front().clips.front().id,field("title").id,QStringLiteral("人工编辑使提案失效"));
        QTRY_COMPARE_WITH_TIMEOUT(edited.size(),5,30000);QVERIFY(!proposals.pending());const auto beforeConfirm=editor.snapshot().sourceFingerprint;
        proposals.confirm();QCOMPARE(rejected.size(),1);QCOMPARE(editor.snapshot().sourceFingerprint,beforeConfirm);QCOMPARE(edited.size(),5);
        proposals.clearProject();backend.closeProject();QVERIFY(!backend.isRunning());
        QFile evidence(repo+"/docs/evidence/m5/e2e.json");QVERIFY(evidence.open(QIODevice::WriteOnly));
        evidence.write(QJsonDocument(QJsonObject{{"verdict","PASS"},{"provider","local-demo-simulated"},{"realModelVerified",false},{"unchangedBeforeConfirmation",true},{"confirmedThroughRealEditor",true},{"imageWhitelist",true},{"undo",true},{"staleProposalInvalidated",true},{"ownedStudioStopped",true},{"screenshots",false}}).toJson());
    }
};
QTEST_GUILESS_MAIN(ProposalE2ETest)
#include "ProposalE2ETest.moc"
