#include "controllers/DocumentController.h"
#include <QTest>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QFile>
#include <QImage>

class DocumentControllerTest : public QObject {
    Q_OBJECT
private slots:
    void createsSavesClosesAndReopens() {
        QTemporaryDir root; QString error;
        qvw::controllers::DocumentController controller;
        QSignalSpy loaded(&controller, &qvw::controllers::DocumentController::projectLoaded);
        const auto dest = root.filePath(QStringLiteral("校园 项目"));
        QVERIFY(controller.create(QStringLiteral(QVW_SOURCE_DIR "/templates/title-card"), dest, QStringLiteral("摄影社")));
        QCOMPARE(loaded.count(), 1);
        QVERIFY(controller.hasProject());
        const auto path = controller.project().manifestPath();
        QVERIFY(controller.save());
        controller.close(); QVERIFY(!controller.hasProject());
        QVERIFY(controller.open(path)); QCOMPARE(controller.project().name, QStringLiteral("摄影社"));
        QCOMPARE(loaded.count(), 2);
    }
    void importsAndReopensAssets() {
        QTemporaryDir root; qvw::controllers::DocumentController controller;
        QVERIFY(controller.create(QStringLiteral(QVW_SOURCE_DIR "/templates/title-card"),root.filePath("project"),"assets"));
        QImage image(10,20,QImage::Format_RGB32); image.fill(Qt::red);
        const auto path=root.filePath("photo.png"); QVERIFY(image.save(path));
        QSignalSpy changed(&controller,&qvw::controllers::DocumentController::projectChanged);
        QVERIFY(controller.importImage(path)); QCOMPARE(changed.size(),1); QCOMPARE(controller.project().assets.size(),1);
        const auto manifest=controller.project().manifestPath(); controller.close(); QVERIFY(controller.open(manifest));
        QCOMPARE(controller.project().assets.front().width,10);
        QVERIFY(!controller.importImage(root.filePath("bad"))); QCOMPARE(controller.project().assets.size(),1);
    }
    void failedOpenKeepsCurrentDocument() {
        QTemporaryDir root; qvw::controllers::DocumentController controller;
        QVERIFY(controller.create(QStringLiteral(QVW_SOURCE_DIR "/templates/title-card"), root.filePath("valid"), "kept"));
        QSignalSpy failures(&controller, &qvw::controllers::DocumentController::failed);
        QVERIFY(!controller.open(root.filePath("missing.json")));
        QCOMPARE(failures.count(), 1); QCOMPARE(controller.project().name, QStringLiteral("kept"));
    }
};
QTEST_GUILESS_MAIN(DocumentControllerTest)
#include "DocumentControllerTest.moc"
