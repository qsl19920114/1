#include "controllers/EditorController.h"
#include "controllers/ProjectController.h"
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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QRegularExpression>
class EditorE2ETest : public QObject {
    Q_OBJECT
private slots:
    void nativeEditingAndRecovery() {
        const QString repo=QStringLiteral(QVW_SOURCE_DIR);
        QTemporaryDir temporary(repo+"/.workbench/M3 原生 编辑-XXXXXX");QVERIFY(temporary.isValid());
        qvw::domain::Project project;QString error;
        QVERIFY2(qvw::services::ProjectStore::create(repo+"/templates/title-card",temporary.filePath("工程"),"M3",&project,&error),qPrintable(error));
        QImage image(100,100,QImage::Format_RGB32);image.fill(QColor("#c87755"));
        const auto input=temporary.filePath("图片.png");QVERIFY(image.save(input));qvw::domain::Asset asset;
        QVERIFY2(qvw::services::AssetService::importImage(project,input,&asset,&error),qPrintable(error));
        auto config=qvw::infra::loadAppConfig(repo+"/config/version-lock.json");QVERIFY2(config.ok(),qPrintable(config.error));
        qvw::infra::LogWriter log(temporary.filePath("editor.jsonl"));
        qvw::controllers::ProjectController backend(config.config,log);
        qvw::controllers::EditorController editor;QUrl liveUrl;
        connect(&backend,&qvw::controllers::ProjectController::previewRequested,&editor,[&](const QUrl &url){liveUrl=url;editor.attach(url,backend.workspace());});
        connect(&backend,&qvw::controllers::ProjectController::projectClosed,&editor,&qvw::controllers::EditorController::clear);
        connect(&backend,&qvw::controllers::ProjectController::snapshotReady,&editor,&qvw::controllers::EditorController::acceptSnapshot);
        connect(&editor,&qvw::controllers::EditorController::failed,this,[](const QString &s){qInfo().noquote()<<"EDITOR expected-or-error:"<<s;});
        connect(&editor,&qvw::controllers::EditorController::message,this,[](const QString &s){qInfo().noquote()<<s;});
        QSignalSpy initialized(&backend,&qvw::controllers::ProjectController::initialized),backendFailed(&backend,&qvw::controllers::ProjectController::failed);
        QSignalSpy succeeded(&editor,&qvw::controllers::EditorController::operationSucceeded),failed(&editor,&qvw::controllers::EditorController::failed),states(&editor,&qvw::controllers::EditorController::stateChanged);
        backend.initialize();QTRY_VERIFY_WITH_TIMEOUT(!initialized.isEmpty()||!backendFailed.isEmpty(),25000);QVERIFY(backendFailed.isEmpty());
        backend.openDocument(project,5740);QTRY_VERIFY_WITH_TIMEOUT(editor.snapshot().isLoaded()||!backendFailed.isEmpty(),60000);QVERIFY(backendFailed.isEmpty());
        auto field=[&](const QString &binding){for(const auto &track:editor.snapshot().tracks)for(const auto &clip:track.clips)for(const auto &f:clip.inspector)if(f.binding==binding)return f;return qvw::domain::InspectorField{};};
        auto edit=[&](const QString &binding,const QVariant &value){editor.edit(editor.snapshot().tracks.front().clips.front().id,field(binding).id,value);};
        QCOMPARE(editor.snapshot().writableFieldCount(),5);
        edit("title",QStringLiteral("原生标题"));QTRY_COMPARE_WITH_TIMEOUT(succeeded.size(),1,30000);QCOMPARE(field("title").rawValue.toString(),QStringLiteral("原生标题"));
        edit("color","#e07744");QTRY_COMPARE_WITH_TIMEOUT(succeeded.size(),2,30000);QCOMPARE(field("color").rawValue.toString(),QString("#e07744"));
        edit("image","./"+asset.path);QTRY_COMPARE_WITH_TIMEOUT(succeeded.size(),3,30000);QCOMPARE(field("image").rawValue.toString(),"./"+asset.path);
        QNetworkAccessManager network;network.setProxy(QNetworkProxy::NoProxy);
        auto *sessionReply=network.get(QNetworkRequest(liveUrl.resolved(QUrl("/__studio/session"))));
        QSignalSpy sessionFinished(sessionReply,&QNetworkReply::finished);QTRY_VERIFY_WITH_TIMEOUT(!sessionFinished.isEmpty(),15000);
        QCOMPARE(sessionReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),200);
        const auto live=QJsonDocument::fromJson(sessionReply->readAll()).object();sessionReply->deleteLater();
        const auto html=live.value("preview").toObject().value("srcdoc").toString();
        QVERIFY(html.contains(QStringLiteral("原生标题")));QVERIFY(html.contains("#e07744"));
        const auto photo=QRegularExpression(QStringLiteral("<img[^>]+src=\"([^\"]+)\"")).match(html);QVERIFY(photo.hasMatch());
        auto *imageReply=network.get(QNetworkRequest(liveUrl.resolved(QUrl(photo.captured(1)))));
        QSignalSpy imageFinished(imageReply,&QNetworkReply::finished);QTRY_VERIFY_WITH_TIMEOUT(!imageFinished.isEmpty(),15000);
        QCOMPARE(imageReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),200);
        QFile original(input);QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(imageReply->readAll(),original.readAll());imageReply->deleteLater();
        editor.undo();QTRY_COMPARE_WITH_TIMEOUT(succeeded.size(),4,30000);QCOMPARE(field("image").rawValue.toString(),QString("./assets/default.png"));
        editor.redo();QTRY_COMPARE_WITH_TIMEOUT(succeeded.size(),5,30000);QCOMPARE(field("image").rawValue.toString(),"./"+asset.path);
        edit("entrance-frames",24.0);QTRY_COMPARE_WITH_TIMEOUT(succeeded.size(),6,30000);QCOMPARE(field("entrance-frames").rawValue.toString(),QString("24"));
        editor.undo();QTRY_COMPARE_WITH_TIMEOUT(succeeded.size(),7,30000);QCOMPARE(field("entrance-frames").rawValue.toString(),QString("18"));
        editor.redo();QTRY_COMPARE_WITH_TIMEOUT(succeeded.size(),8,30000);QCOMPARE(field("entrance-frames").rawValue.toString(),QString("24"));
        const auto beforeRejected=editor.snapshot().sourceFingerprint;
        edit("entrance-frames",0.0);QTRY_COMPARE_WITH_TIMEOUT(failed.size(),1,30000);QTRY_VERIFY(!editor.isBusy());
        QCOMPARE(succeeded.size(),8);QCOMPARE(field("entrance-frames").rawValue.toString(),QString("24"));QCOMPARE(editor.snapshot().sourceFingerprint,beforeRejected);
        QVERIFY(!states.isEmpty());QVERIFY(states.last().at(2).toBool());
        // The server's rollback writes may emit a late macOS file-watch event.
        // /session can still return the prior snapshot during that compilation;
        // /document exposes the busy 409. Wait/read only, never retry a mutation.
        QTest::qWait(200);
        int documentStatus=409;
        for(int attempt=0;attempt<30&&documentStatus==409;++attempt) {
            auto *readyReply=network.get(QNetworkRequest(liveUrl.resolved(QUrl("/__studio/document"))));
            QSignalSpy readyFinished(readyReply,&QNetworkReply::finished);QTRY_VERIFY_WITH_TIMEOUT(!readyFinished.isEmpty(),15000);
            documentStatus=readyReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();readyReply->deleteLater();
            if(documentStatus==409)QTest::qWait(100);
        }
        QCOMPARE(documentStatus,200);
        editor.refresh();QTRY_VERIFY_WITH_TIMEOUT(!editor.isBusy(),15000);QCOMPARE(failed.size(),1);
        const auto sourcePath=project.sourcePath;
        const auto beforeSource=editor.snapshot().sourceFiles.value(sourcePath);
        auto updated=beforeSource;updated.replace(QStringLiteral("用镜头记录热爱，让灵感在这里相遇。"),QStringLiteral("原生源码修改"));QVERIFY(updated!=beforeSource);
        editor.replaceSource(sourcePath,updated);QTRY_COMPARE_WITH_TIMEOUT(succeeded.size(),9,30000);QCOMPARE(editor.snapshot().sourceFiles.value(sourcePath),updated);
        editor.undo();QTRY_COMPARE_WITH_TIMEOUT(succeeded.size(),10,30000);QCOMPARE(editor.snapshot().sourceFiles.value(sourcePath),beforeSource);
        editor.redo();QTRY_COMPARE_WITH_TIMEOUT(succeeded.size(),11,30000);QCOMPARE(editor.snapshot().sourceFiles.value(sourcePath),updated);
        editor.replaceSource(sourcePath,"<invalid");QTRY_COMPARE_WITH_TIMEOUT(failed.size(),2,30000);QTRY_VERIFY_WITH_TIMEOUT(!editor.isBusy(),15000);
        QCOMPARE(succeeded.size(),11);QCOMPARE(editor.snapshot().sourceFiles.value(sourcePath),updated);
        QFile source(QDir(project.rootPath).filePath(sourcePath));QVERIFY(source.open(QIODevice::ReadOnly));QCOMPARE(source.readAll(),updated.toUtf8());source.close();
        // Deterministically exercise the real server's 409, independently of
        // the controller preflight that normally catches external edits first.
        qvw::backend::hypit::StudioWriter staleWriter;
        QSignalSpy conflict(&staleWriter,&qvw::backend::hypit::StudioWriter::failed);
        QSignalSpy staleSuccess(&staleWriter,&qvw::backend::hypit::StudioWriter::completed);
        staleWriter.adjustParameter(liveUrl,editor.snapshot().revision-1,editor.snapshot().tracks.front().clips.front().id,field("title").id,QStringLiteral("过期请求不能写入"));
        QTRY_COMPARE_WITH_TIMEOUT(conflict.size(),1,15000);
        QCOMPARE(conflict.first().at(0).value<qvw::backend::hypit::StudioWriter::WriteFailure>(),qvw::backend::hypit::StudioWriter::Conflict);
        QCOMPARE(conflict.first().at(2).toInt(),409);QCOMPARE(staleSuccess.size(),0);
        QVERIFY(source.open(QIODevice::ReadOnly));QCOMPARE(source.readAll(),updated.toUtf8());source.close();
        // A real external editor changes Source; the pending old form must not overwrite it.
        auto external=updated;external.replace(QStringLiteral("原生标题"),QStringLiteral("外部标题"));
        QSaveFile externalWrite(source.fileName());QVERIFY(externalWrite.open(QIODevice::WriteOnly));externalWrite.write(external.toUtf8());QVERIFY(externalWrite.commit());
        QTest::qWait(600);
        edit("title",QStringLiteral("不应覆盖外部标题"));QTRY_COMPARE_WITH_TIMEOUT(failed.size(),3,30000);QTRY_VERIFY(!editor.isBusy());
        QCOMPARE(succeeded.size(),11);QCOMPARE(field("title").rawValue.toString(),QStringLiteral("外部标题"));
        QVERIFY(!states.last().at(2).toBool());QVERIFY(!states.last().at(3).toBool());
        backend.closeProject();QVERIFY(!backend.isRunning());QVERIFY(!editor.snapshot().isLoaded());
        qvw::domain::Project reopened;QVERIFY(qvw::services::ProjectStore::load(project.manifestPath(),&reopened,&error));
        backend.openDocument(reopened,5740);QTRY_VERIFY_WITH_TIMEOUT(editor.snapshot().isLoaded()||!backendFailed.isEmpty(),60000);QVERIFY(backendFailed.isEmpty());
        QCOMPARE(field("title").rawValue.toString(),QStringLiteral("外部标题"));QCOMPARE(field("image").rawValue.toString(),"./"+asset.path);
        QCOMPARE(field("color").rawValue.toString(),QString("#e07744"));
        backend.closeProject();QVERIFY(!backend.isRunning());
        QFile evidence(repo+"/docs/evidence/m3/e2e.json");QVERIFY(evidence.open(QIODevice::WriteOnly));
        evidence.write(QJsonDocument(QJsonObject{{"verdict","PASS"},{"titleColorImageNativeWrites",true},{"compiledPreviewContainsChanges",true},{"imageBytesMatch",true},{"undoRedo",true},{"numericLiteralRoundTrip",true},{"mutation422Rollback",true},{"staleRevision409",true},{"protectedSourceAndRollback",true},{"externalConflictProtected",true},{"reopenedConsistent",true},{"ownedProcessStopped",true},{"screenshots",false}}).toJson());
    }
};
QTEST_GUILESS_MAIN(EditorE2ETest)
#include "EditorE2ETest.moc"
