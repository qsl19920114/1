#include "controllers/DocumentController.h"
#include "controllers/ProjectController.h"
#include "services/AssetService.h"
#include "infrastructure/AppConfig.h"
#include "infrastructure/LogWriter.h"
#include <QTest>
#include <QTemporaryDir>
#include <QImage>
#include <QSignalSpy>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QRegularExpression>

class ProjectE2ETest : public QObject {
    Q_OBJECT
private slots:
    void independentOriginalProject() {
        const QString repo = QStringLiteral(QVW_SOURCE_DIR);
        QTemporaryDir temporary(QDir(repo).filePath(".workbench/M2 中文 工程-XXXXXX"));
        QVERIFY(temporary.isValid());
        qvw::controllers::DocumentController document;
        QSignalSpy documentFailures(&document,&qvw::controllers::DocumentController::failed);
        const QString originalRoot=temporary.filePath("初始 工程");
        QVERIFY2(document.create(repo+"/templates/title-card",originalRoot,QStringLiteral("摄影社")),"create original project");
        QImage image(120,80,QImage::Format_RGB32); image.fill(QColor("#ed8844"));
        const QString originalImage=temporary.filePath("外部 原图.png"); QVERIFY(image.save(originalImage));
        QFile original(originalImage); QVERIFY(original.open(QIODevice::ReadOnly)); const auto originalBytes=original.readAll(); original.close();
        auto project=document.project(); qvw::domain::Asset asset; QString error;
        QVERIFY2(qvw::services::AssetService::importImage(project,originalImage,&asset,&error),qPrintable(error));
        QCOMPARE(project.assets.size(),1);
        QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(),originalBytes); original.close();
        // Fixture authors the slot before session start. This is template verification,
        // not a claim that the M3 native editor is already implemented.
        QFile source(QDir(project.rootPath).filePath(project.sourcePath));
        QVERIFY(source.open(QIODevice::ReadOnly)); auto authored=source.readAll(); source.close();
        authored.replace("./assets/default.png",("./"+asset.path).toUtf8());
        QVERIFY(source.open(QIODevice::WriteOnly)); QCOMPARE(source.write(authored),authored.size()); source.close();
        document.close();
        const QString movedRoot=temporary.filePath("移动后 工程"); QVERIFY(QDir().rename(originalRoot,movedRoot));
        QVERIFY(document.open(movedRoot+"/workbench.qvw.json")); QVERIFY(document.save());
        project=document.project(); QCOMPARE(project.assets.size(),1); QCOMPARE(project.assets.front().hash,asset.hash);
        const auto config=qvw::infra::loadAppConfig(repo+"/config/version-lock.json"); QVERIFY2(config.ok(),qPrintable(config.error));
        qvw::infra::LogWriter log(temporary.filePath("integration.jsonl"));
        qvw::controllers::ProjectController backend(config.config,log);
        QSignalSpy initialized(&backend,&qvw::controllers::ProjectController::initialized);
        QSignalSpy failed(&backend,&qvw::controllers::ProjectController::failed);
        QSignalSpy snapshots(&backend,&qvw::controllers::ProjectController::snapshotReady);
        QSignalSpy urls(&backend,&qvw::controllers::ProjectController::previewRequested);
        QSignalSpy payloads(&backend,&qvw::controllers::ProjectController::payloadReceived);
        connect(&backend,&qvw::controllers::ProjectController::message,this,[](const QString &text){qInfo().noquote()<<text;});
        connect(&backend,&qvw::controllers::ProjectController::failed,this,[](const QString &text){qWarning().noquote()<<text;});
        backend.initialize(); QTRY_VERIFY_WITH_TIMEOUT(!initialized.isEmpty()||!failed.isEmpty(),25000);
        QVERIFY2(failed.isEmpty(),"Hypit initialization failed");
        backend.openProject(project.rootPath,QDir(project.rootPath).filePath(project.runPath),QDir(project.rootPath).filePath(project.runtimePath),5730);
        QTRY_VERIFY_WITH_TIMEOUT(!snapshots.isEmpty()||!failed.isEmpty(),60000);
        QVERIFY2(failed.isEmpty(),"real Studio failed"); QVERIFY(!snapshots.isEmpty());
        const auto snapshot=qvariant_cast<qvw::domain::Snapshot>(snapshots.last().at(0));
        QCOMPARE(snapshot.space.width,1280); QCOMPARE(snapshot.space.height,720); QCOMPARE(snapshot.space.frameCount,240);
        QCOMPARE(snapshot.writableFieldCount(),5); QCOMPARE(snapshot.tracks.size(),1);
        const auto object=QJsonDocument::fromJson(payloads.last().at(0).toByteArray()).object();
        const auto preview=object.value("preview").toObject().value("srcdoc").toString();
        QVERIFY(preview.contains(QStringLiteral("校园创作社"))); QVERIFY(preview.contains("#35bca8")); QVERIFY(preview.contains("photo"));
        bool imageFieldFound=false;
        for(const auto &field:snapshot.tracks.front().clips.front().inspector)
            if(field.label==QStringLiteral("图片路径")){QCOMPARE(field.rawValue.toString(),"./"+asset.path); QVERIFY(field.writable);imageFieldFound=true;}
        QVERIFY(imageFieldFound);
        // The compiled photo URL must serve the exact imported bytes, not merely
        // leave a syntactically correct path in a field or broken image in HTML.
        const auto photo=QRegularExpression(QStringLiteral("<img[^>]+src=\"([^\"]+)\"")).match(preview);
        QVERIFY(photo.hasMatch()); QVERIFY(!urls.isEmpty());
        QNetworkAccessManager network; network.setProxy(QNetworkProxy::NoProxy);
        const auto base=urls.last().at(0).toUrl();
        auto *reply=network.get(QNetworkRequest(base.resolved(QUrl(photo.captured(1)))));
        QSignalSpy delivered(reply,&QNetworkReply::finished);
        QTRY_VERIFY_WITH_TIMEOUT(!delivered.isEmpty(),15000);
        QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),200);
        QCOMPARE(reply->readAll(),originalBytes); reply->deleteLater();
        const auto pid=backend.studioPid(); QVERIFY(pid>0);
        auto invalid=project; invalid.runtimePath="bad-runtime.json";
        QFile badRuntime(QDir(project.rootPath).filePath(invalid.runtimePath));
        QVERIFY(badRuntime.open(QIODevice::WriteOnly)); badRuntime.write("{}"); badRuntime.close();
        backend.openDocument(invalid);
        QVERIFY2(!backend.isRunning(),"switching to invalid B must not leave old Studio A active");
        QCOMPARE(failed.size(),1);
        backend.closeProject(); QVERIFY(!backend.isRunning());
        QJsonObject report{{"verdict","PASS"},{"projectCreated",true},{"originalUnchanged",true},{"relocatedAndReopened",true},
            {"realSession",true},{"compiledPreview",true},{"imageSlot",asset.path},{"imageHttpBytesMatch",true},{"writableFields",5},{"width",1280},{"height",720},{"frames",240},{"ownedProcessStopped",true},{"invalidSwitchStopsOldSession",true},{"screenshots",false}};
        QFile evidence(repo+"/docs/evidence/m2/e2e.json"); QVERIFY(evidence.open(QIODevice::WriteOnly)); evidence.write(QJsonDocument(report).toJson());
        QFile session(repo+"/docs/evidence/m2/session.json"); QVERIFY(session.open(QIODevice::WriteOnly)); session.write(payloads.last().at(0).toByteArray());
    }
};
QTEST_GUILESS_MAIN(ProjectE2ETest)
#include "ProjectE2ETest.moc"
