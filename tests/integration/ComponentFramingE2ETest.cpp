#include "workflow/AgentWorkbenchBridge.h"
#include "agent/AgentController.h"
#include "controllers/ProjectController.h"
#include "controllers/SampleCreationController.h"
#include "infrastructure/AppConfig.h"
#include "infrastructure/WorkspaceStore.h"
#include "ui/MainWindow.h"
#include "ui/AgentPanel.h"
#include "ui/ComponentBrowser.h"
#include <QtTest>
#include <QDateTime>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QTreeWidget>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
using namespace qvw;
// Explicit opt-in. Production widgets, local Studio, and current Codex login.
class ComponentFramingE2ETest : public QObject {
 Q_OBJECT
private slots:
 void realFramingWorkflow() {
    const QString repo=QVW_SOURCE_DIR;
    const auto output=repo+"/.workbench/deliverables-m15/"+QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    QVERIFY(QDir().mkpath(output));
        const auto loaded=infra::loadAppConfig(repo+"/.workbench/local-agent-config.json");QVERIFY2(loaded.ok(),qPrintable(loaded.error));const auto config=loaded.config;infra::LogWriter log(output+"/application.jsonl");
        controllers::DocumentController document;document.setMediaTools(config.ffprobePath,config.ffmpegPath,config.processEnvironment);
        controllers::ProjectController backend(config,log);controllers::EditorController editor;controllers::SampleCreationController creation(document,editor);controllers::ExportController exporter(config,log);
        agent::ModelClient model;model.setProgram(config.codexPath);model.setEnvironment(config.processEnvironment);agent::AgentController creator(document,editor,exporter,model,repo+"/templates");ui::MainWindow window;window.resize(1580,960);workflow::bindAgentWorkbench(window,creator);
        infra::WorkspaceStore history(output+"/workspace.json");connect(&window,&ui::MainWindow::historyRecorded,&window,[&](const auto &r){history.record(r);window.showHistory(history.history());});
        connect(&document,&controllers::DocumentController::projectChanged,&window,&ui::MainWindow::showDocument);
        connect(&document,&controllers::DocumentController::projectLoaded,&backend,[&](const auto &p){exporter.setProject(p);backend.openDocument(p,5797);});
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

    connect(&window,&ui::MainWindow::editRequested,&editor,&controllers::EditorController::edit);
    connect(&model,&agent::ModelClient::completed,&window,[&](const QJsonObject &result){
        QSaveFile f(output+"/model-result.json");if(f.open(QIODevice::WriteOnly)){f.write(QJsonDocument(result).toJson());f.commit();}
    });
    window.show();
    backend.initialize();
    QTRY_VERIFY_WITH_TIMEOUT(initialized.count()||backendErrors.count(),30000);
    QVERIFY(backendErrors.isEmpty());
    QVERIFY(document.create(repo+"/templates/title-card",output+"/构图工程","FrameLab · 人物构图"));
    QTRY_VERIFY_WITH_TIMEOUT(editor.isReady()||backendErrors.count(),60000);
    QVERIFY(backendErrors.isEmpty());
    auto field=[&](const QString &binding){
        for(const auto &t:editor.snapshot().tracks)for(const auto &c:t.clips)for(const auto &f:c.inspector)
            if(f.binding==binding)return QPair<QString,domain::InspectorField>{c.id,f};
        return QPair<QString,domain::InspectorField>{};
    };
    auto stable=[&]{
        QElapsedTimer deadline;deadline.start();int equal=0,revision=-1;QByteArray fingerprint;
        while(deadline.elapsed()<10000){
            QTest::qWait(200);if(editor.isBusy())continue;editor.refresh();
            while(editor.isBusy()&&deadline.elapsed()<10000)QTest::qWait(25);
            const auto snapshot=editor.snapshot();
            if(snapshot.revision==revision&&snapshot.sourceFingerprint==fingerprint)++equal;else equal=0;
            revision=snapshot.revision;fingerprint=snapshot.sourceFingerprint;
            if(equal>=3&&window.previewVersion().isConfirmed()&&window.previewVersion().sourceFingerprint==fingerprint)return true;
        }return false;
    };
    QVERIFY(stable());
    QSignalSpy imported(&document,&controllers::DocumentController::assetImported);
    QVERIFY(document.importImage(repo+"/.workbench/character-showcase/materials/portrait-variant-1.png"));
    QVERIFY(stable());
    const auto portrait=imported.last()[0].value<domain::Asset>();
    editor.edit(field("image").first,field("image").second.id,"./"+portrait.path);
    QTRY_VERIFY_WITH_TIMEOUT(!editor.isBusy(),30000);QVERIFY(editErrors.isEmpty());QVERIFY(stable());
    for(const auto &key:{"image-fit","image-position-x","image-position-y"}){
        QVERIFY(field(key).second.isEditable());
    }
    QCOMPARE(field("image-fit").second.rawValue.toString(),QString("cover"));
    const auto entity=field("image-fit").first;
    auto *browser=window.componentBrowser();QVERIFY(browser);
    auto *search=browser->findChild<QLineEdit*>("componentSearch");QVERIFY(search);
    const auto beforeSearch=editor.snapshot().sourceFingerprint;
    search->setText("no-component-matches-this");
    QVERIFY(browser->selectedEntityId().isEmpty());
    auto *inspector=window.findChild<QTreeWidget*>("componentInspector");QVERIFY(inspector);
    QCOMPARE(inspector->topLevelItemCount(),0);
    search->setText(entity);QVERIFY(browser->selectEntity(entity));
    QSignalSpy activated(browser,&ui::ComponentBrowser::componentActivated);
    QTest::keyClick(browser->tree(),Qt::Key_Return);QCOMPARE(activated.count(),1);
    QCOMPARE(activated.last()[0].toString(),entity);
    QCOMPARE(editor.snapshot().sourceFingerprint,beforeSearch);
    auto nativePosition=[&](const QString &binding,double value){
        const auto label=field(binding).second.label;
        for(int i=0;i<inspector->topLevelItemCount();++i){
            auto *row=inspector->topLevelItem(i);if(row->text(0)!=label)continue;
            auto *spin=qobject_cast<QDoubleSpinBox*>(inspector->itemWidget(row,2));
            if(!spin)return false;spin->setValue(value);QMetaObject::invokeMethod(spin,"editingFinished");return true;
        }return false;
    };
    QVERIFY(nativePosition("image-position-x",25));
    QTRY_COMPARE_WITH_TIMEOUT(field("image-position-x").second.rawValue.toDouble(),25.,30000);
    QVERIFY(stable());QVERIFY(nativePosition("image-position-y",10));
    QTRY_COMPARE_WITH_TIMEOUT(field("image-position-y").second.rawValue.toDouble(),10.,30000);
    QVERIFY(stable());QVERIFY(editErrors.isEmpty());
    auto fixedProperties=[&]{
        QJsonObject result;
        for(const auto &t:editor.snapshot().tracks)for(const auto &c:t.clips)for(const auto &f:c.inspector)
            if(!f.binding.startsWith("image-position-")&&f.binding!="image-fit")result.insert(c.id+"/"+f.binding,QJsonValue::fromVariant(f.rawValue));
        return result;
    };
    const auto originalProperties=fixedProperties();QJsonArray artifacts;
    auto hash=[](const QString &path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return QString();QCryptographicHash h(QCryptographicHash::Sha256);h.addData(&f);return QString::fromLatin1(h.result().toHex());};
    for(int variant=0;variant<2;++variant){
        if(variant){
            const auto before=editor.snapshot();
            QVERIFY(window.agentPanel()->composeGoal("仅修改当前图片组件：image-fit 改为 contain，image-position-x 和 image-position-y 都改为 50。其他文案、图片绑定、颜色、动画和时长保持不变。请生成精确的三步修改方案。"));
            auto *generate=window.findChild<QPushButton*>("agentGenerate");QVERIFY(generate&&generate->isEnabled());generate->click();
            QTRY_VERIFY_WITH_TIMEOUT(creator.status().phase=="review"||creator.status().phase=="failed"||creator.status().phase=="clarify",135000);
            QVERIFY2(creator.status().phase=="review",qPrintable(creator.status().message));
            QCOMPARE(editor.snapshot().sourceFiles,before.sourceFiles);
            QCOMPARE(editor.snapshot().sourceFingerprint,before.sourceFingerprint);
            QCOMPARE(creator.plan()->content["operations"].toArray().size(),3);
            auto *approve=window.findChild<QPushButton*>("planApprove");QVERIFY(approve&&approve->isEnabled());approve->click();
            QTRY_VERIFY_WITH_TIMEOUT(creator.status().phase=="complete"||creator.status().phase=="failed"||creator.status().phase=="stale",45000);
            QVERIFY2(creator.status().phase=="complete",qPrintable(creator.status().message));
            QCOMPARE(field("image-fit").second.rawValue.toString(),QString("contain"));
            QCOMPARE(field("image-position-x").second.rawValue.toDouble(),50.);
            QCOMPARE(field("image-position-y").second.rawValue.toDouble(),50.);
            QCOMPARE(fixedProperties(),originalProperties);QVERIFY(stable());
        }
        const auto film=output+(variant?"/02-完整显示.mp4":"/01-铺满裁切.mp4");
        const auto previous=done.count();window.exportRequested(film);
        QTRY_VERIFY_WITH_TIMEOUT(done.count()>previous||exportErrors.count(),180000);
        QVERIFY2(exportErrors.isEmpty(),qPrintable(exporter.task().error));
        QCOMPARE(exporter.task().phase,QString("complete"));
        QCOMPARE(exporter.task().sourceFingerprint,editor.snapshot().sourceFingerprint);
        artifacts.append(QJsonObject{{"path",film},{"sha256",hash(film)},{"fullDecode",true},{"buildId",exporter.task().buildId},{"sourceFingerprint",QString::fromLatin1(editor.snapshot().sourceFingerprint.toHex())}});
    }
    QVERIFY(stable());const auto valid=editor.snapshot();const auto errors=editErrors.count();const auto successes=edits.count();
    editor.edit(field("image-position-y").first,field("image-position-y").second.id,101.0);
    QTRY_VERIFY_WITH_TIMEOUT(editErrors.count()>errors,30000);QVERIFY(stable());
    QCOMPARE(editor.snapshot().sourceFiles,valid.sourceFiles);
    QCOMPARE(editor.snapshot().sourceFingerprint,valid.sourceFingerprint);
    QCOMPARE(field("image-position-y").second.rawValue.toDouble(),50.);
    QCOMPARE(edits.count(),successes);
    const auto rangeMessage=editErrors.last()[0].toString();
    QVERIFY2(rangeMessage.contains("must be at most 100"),qPrintable(rangeMessage));
    QString sourcePath,badSource;
    for(auto it=valid.sourceFiles.cbegin();it!=valid.sourceFiles.cend();++it){
        if(it.value().contains("image-position-y=\"50\"")){sourcePath=it.key();badSource=it.value();break;}
    }
    QVERIFY(!sourcePath.isEmpty());badSource.replace("image-position-y=\"50\"","image-position-y=\"101\"");
    editor.replaceSource(sourcePath,badSource);
    QTRY_VERIFY_WITH_TIMEOUT(editErrors.count()>errors+1,30000);QVERIFY(stable());
    const auto rollbackMessage=editErrors.last()[0].toString();
    QVERIFY2(rollbackMessage.contains("image-position-y")&&rollbackMessage.contains("已恢复原文件"),qPrintable(rollbackMessage));
    QCOMPARE(editor.snapshot().sourceFiles,valid.sourceFiles);
    QCOMPARE(editor.snapshot().sourceFingerprint,valid.sourceFingerprint);QCOMPARE(edits.count(),successes);
    for(const auto &artifact:artifacts){const auto a=artifact.toObject();QCOMPARE(hash(a["path"].toString()),a["sha256"].toString());}
    const auto project=document.project().manifestPath();const auto plan=creator.plan()->content;
    backend.closeProject();QVERIFY(!backend.isRunning());
    QJsonObject report{{"verdict","PASS"},{"project",project},{"artifacts",artifacts},{"plan",plan},
        {"realModelRequests",1},{"explicitDriverApprovals",1},{"unchangedBeforeApproval",true},
        {"nonFramingPropertiesPreserved",true},{"nativePositionEdits",2},{"searchClearsHiddenSelection",true},
        {"keyboardActivation",true},{"invalidPositionRollback",true},{"rollbackMessage",rollbackMessage},{"invalidParameterRejected",true},{"rangeMessage",rangeMessage},{"artifactsUnchangedAfterRejection",true},
        {"previewFingerprint",QString::fromLatin1(valid.sourceFingerprint.toHex())},{"ownedStudioStopped",true},{"pixelsUploaded",false},
        {"driver","production Qt controls and signals; actual current Codex and local Hypit"}};
    for(const auto &path:{output+"/demo.json",repo+"/docs/evidence/m15/component-framing.json"}){
        QSaveFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(report).toJson());QVERIFY(f.commit());
    }
 }
};
QTEST_MAIN(ComponentFramingE2ETest)
#include "ComponentFramingE2ETest.moc"
