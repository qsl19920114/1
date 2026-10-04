#include "agent/StoryTemplate.h"
#include <QTest>
#include <QFile>
#include <QTemporaryDir>
#include <QJsonObject>
#include <QJsonDocument>
class StoryTemplateTest : public QObject {
    Q_OBJECT
private slots:
    void sceneOrderAndDurationAreAuthoredBeforeStudio() {
        QJsonArray scenes;for(const auto &id:{"scene-3","scene-1","scene-2"})scenes.append(QJsonObject{{"sceneId",id},{"assetId","selected"},{"title","<script>&\"文字"},{"subtitle","副标题"},{"color","#35bca8"},{"durationFrames",150},{"fontSize",54}});
        QTemporaryDir temp;QString error;
        QVERIFY2(qvw::agent::StoryTemplate::prepare(QStringLiteral(QVW_SOURCE_DIR)+"/templates/story-reel",scenes,temp.filePath("trusted"),&error),qPrintable(error));
        QFile source(temp.filePath("trusted/main.svml"));QVERIFY(source.open(QIODevice::ReadOnly));const auto text=source.readAll();
        QVERIFY(text.contains("width=\"720\" height=\"1280\""));QVERIFY(text.contains("end=\"450f\""));
        QVERIFY(text.indexOf("id=\"scene-3\"")<text.indexOf("id=\"scene-1\""));
        QVERIFY(text.contains("start=\"program.start+150f\" end=\"program.start+300f\""));
        int cards=0;
        for(const auto &line:text.split('\n'))if(line.trimmed().startsWith("<card:Card ")){
            ++cards;
            QVERIFY(line.contains("image-fit=\"cover\""));
            QVERIFY(line.contains("image-position-x=\"50\""));
            QVERIFY(line.contains("image-position-y=\"50\""));
        }
        QCOMPARE(cards,3);
        QFile metadata(temp.filePath("trusted/template.json"));QVERIFY(metadata.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(metadata.readAll()).object()["version"].toString(),QStringLiteral("1.1.0"));
        QVERIFY(!text.contains("<script>")); // model text enters through EditorController after creation, never as code.
        QVERIFY(!qvw::agent::StoryTemplate::prepare(QStringLiteral(QVW_SOURCE_DIR)+"/templates/story-reel",scenes,temp.filePath("trusted"),&error));
    }
    void knownTemplateVersions_data() {
        QTest::addColumn<QString>("version");
        QTest::addColumn<bool>("accepted");
        QTest::addColumn<bool>("framing");
        QTest::newRow("legacy-1.0")<<QStringLiteral("1.0.0")<<true<<false;
        QTest::newRow("framing-1.1")<<QStringLiteral("1.1.0")<<true<<true;
        QTest::newRow("unknown-minor")<<QStringLiteral("1.2.0")<<false<<false;
        QTest::newRow("unknown-major")<<QStringLiteral("2.0.0")<<false<<false;
        QTest::newRow("missing-version")<<QString()<<false<<false;
    }
    void knownTemplateVersions() {
        QFETCH(QString,version);QFETCH(bool,accepted);QFETCH(bool,framing);
        QTemporaryDir installed,destination;QVERIFY(installed.isValid());QVERIFY(destination.isValid());
        QFile metadata(installed.filePath("template.json"));QVERIFY(metadata.open(QIODevice::WriteOnly));
        const auto bytes=QJsonDocument(QJsonObject{{"id","story-reel"},{"version",version}}).toJson();
        QCOMPARE(metadata.write(bytes),bytes.size());metadata.close();
        QJsonArray scenes;
        for(const auto &id:{"scene-1","scene-2","scene-3"})scenes.append(QJsonObject{{"sceneId",id},{"assetId","selected"},{"title","场景"},{"subtitle","副标题"},{"color","#35bca8"},{"durationFrames",150},{"fontSize",54}});
        QString error;
        QCOMPARE(qvw::agent::StoryTemplate::prepare(installed.path(),scenes,destination.filePath("trusted"),&error),accepted);
        if(!accepted){QVERIFY(!error.isEmpty());QVERIFY(!QFile::exists(destination.filePath("trusted/template.json")));return;}
        QVERIFY(error.isEmpty());
        QFile source(destination.filePath("trusted/main.svml"));QVERIFY(source.open(QIODevice::ReadOnly));
        const auto text=source.readAll();
        QCOMPARE(text.count("image-fit=\"cover\""),framing?3:0);
        QCOMPARE(text.count("image-position-x=\"50\""),framing?3:0);
        QCOMPARE(text.count("image-position-y=\"50\""),framing?3:0);
    }
};
QTEST_GUILESS_MAIN(StoryTemplateTest)
#include "StoryTemplateTest.moc"
