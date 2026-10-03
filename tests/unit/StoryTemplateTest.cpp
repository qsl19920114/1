#include "agent/StoryTemplate.h"
#include <QTest>
#include <QFile>
#include <QTemporaryDir>
#include <QJsonObject>
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
        QVERIFY(!text.contains("<script>")); // model text enters through EditorController after creation, never as code.
        QVERIFY(!qvw::agent::StoryTemplate::prepare(QStringLiteral(QVW_SOURCE_DIR)+"/templates/story-reel",scenes,temp.filePath("trusted"),&error));
    }
};
QTEST_GUILESS_MAIN(StoryTemplateTest)
#include "StoryTemplateTest.moc"
