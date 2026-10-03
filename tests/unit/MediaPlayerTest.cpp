#include "ui/MediaPlayerDialog.h"
#include "ui/MediaPlayerWidget.h"
#include <QComboBox>
#include <QCheckBox>
#include <QFile>
#include <QLabel>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QSlider>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QWebEnginePage>
#include <QWebEngineView>
#include <cmath>
#include <limits>

using namespace qvw::ui;
class MediaPlayerTest : public QObject {
    Q_OBJECT
    QTemporaryDir m_dir;
    QString m_movie;
private slots:
    void initTestCase() {
        // A real decodable local fixture: no model, network, or fake media states.
        m_movie = m_dir.filePath("movie ' 中文 & #.mp4");
        QString ffmpeg = QStandardPaths::findExecutable("ffmpeg");
        if (ffmpeg.isEmpty() && QFile::exists("/opt/homebrew/bin/ffmpeg")) ffmpeg = "/opt/homebrew/bin/ffmpeg";
        QVERIFY2(!ffmpeg.isEmpty(), "Real playback test requires local ffmpeg");
        QProcess process;
        process.start(ffmpeg, {"-y", "-v", "error", "-f", "lavfi", "-i", "testsrc2=size=160x90:rate=15", "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=44100", "-t", "6", "-c:v", "libx264", "-pix_fmt", "yuv420p", "-c:a", "aac", "-movflags", "+faststart", m_movie});
        QVERIFY(process.waitForFinished(30000));
        QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
    }
    void unavailableMediaDisablesNativePlayback() {
        MediaPlayerDialog dialog(m_dir.filePath("missing.mp4"), "test");
        auto *play = dialog.findChild<QPushButton*>("mediaPlayPause");
        QVERIFY(play);
        QVERIFY(!play->isEnabled());
        dialog.show();
        QTRY_VERIFY_WITH_TIMEOUT(!dialog.player()->playbackState().error.isEmpty(), 10000);
        QVERIFY(!play->isEnabled());
        QVERIFY(!dialog.findChild<QSlider*>("mediaSeek")->isEnabled());
        QVERIFY(dialog.findChild<QLabel*>("mediaStatus")->text().contains("无法"));
    }
    void realPlaybackAndNativeControls() {
        MediaPlayerWidget player(m_movie);
        player.resize(640, 420); player.show();
        auto *play = player.findChild<QPushButton*>("mediaPlayPause");
        QVERIFY(!play->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(player.playbackState().ready, 15000);
        QVERIFY(player.findChild<QWebEngineView*>());
        QVERIFY(std::abs(player.playbackState().duration - 6) < .2);
        QVERIFY(player.playbackState().paused);
        QVERIFY(play->isEnabled());
        play->click();
        QTRY_VERIFY_WITH_TIMEOUT(!player.playbackState().paused && player.playbackState().position > .25, 5000);
        play->click();
        QTRY_VERIFY(player.playbackState().paused);
        const double paused = player.playbackState().position;
        QTest::qWait(300);
        QVERIFY(std::abs(player.playbackState().position - paused) < .15);
        player.findChild<QSlider*>("mediaSeek")->setValue(400);
        QTRY_VERIFY(std::abs(player.playbackState().position - 2.4) < .15);
        player.findChild<QPushButton*>("mediaBack")->click();
        QTRY_VERIFY(player.playbackState().position < .1);
        player.findChild<QPushButton*>("mediaForward")->click();
        QTRY_VERIFY(std::abs(player.playbackState().position - 5) < .15);
        player.findChild<QComboBox*>("mediaRate")->setCurrentIndex(3);
        player.findChild<QSlider*>("mediaVolume")->setValue(35);
        player.findChild<QPushButton*>("mediaMute")->click();
        player.findChild<QCheckBox*>("mediaLoop")->setChecked(true);
        QTRY_COMPARE(player.playbackState().rate, 2.0);
        QTRY_VERIFY(std::abs(player.playbackState().volume - .35) < .001);
        QTRY_VERIFY(player.playbackState().muted && player.playbackState().loop);
        for (int cycle = 0; cycle < 12; ++cycle) {
            player.seek(5.8); play->click();
            QTRY_VERIFY_WITH_TIMEOUT(player.playbackState().position < 2 && !player.playbackState().paused, 4000);
            QVERIFY2(player.findChild<QPushButton*>("mediaStop")->isEnabled(), "Stop must remain available during loop seek/buffering");
            player.findChild<QPushButton*>("mediaStop")->click();
            QTRY_VERIFY(player.playbackState().paused && player.playbackState().position < .1);
        }
        player.findChild<QCheckBox*>("mediaLoop")->setChecked(false);
        player.seek(5.8); play->click();
        QTRY_VERIFY_WITH_TIMEOUT(player.playbackState().ended && player.playbackState().paused, 4000);
        play->click();
        QTRY_VERIFY_WITH_TIMEOUT(!player.playbackState().ended && player.playbackState().position < 2 && !player.playbackState().paused, 4000);
        player.stop();
        QTRY_VERIFY(player.playbackState().paused && player.playbackState().position < .1);
        player.seek(-100);
        QTRY_VERIFY(player.playbackState().position < .1);
        player.seek(100);
        QTRY_VERIFY(std::abs(player.playbackState().position - player.playbackState().duration) < .1);
        player.setPlaybackRate(std::numeric_limits<double>::quiet_NaN());
        player.seek(std::numeric_limits<double>::infinity());
        QTest::qWait(200);
        QCOMPARE(player.playbackState().rate, 2.0);
        QVERIFY(std::isfinite(player.playbackState().position));
    }
    void corruptMediaReportsActualError() {
        const auto path = m_dir.filePath("broken.mp4");
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("not an mp4"); file.close();
        MediaPlayerWidget player(path); player.show();
        QTRY_VERIFY_WITH_TIMEOUT(!player.playbackState().error.isEmpty(), 10000);
        QVERIFY(!player.playbackState().ready);
        QVERIFY(!player.findChild<QPushButton*>("mediaPlayPause")->isEnabled());
    }
    void closeWhileFullscreenDestroysPlayer() {
        auto *dialog = new MediaPlayerDialog(m_movie, "test");
        QPointer<MediaPlayerDialog> guard(dialog);
        QPointer<QWebEnginePage> page(dialog->findChild<QWebEngineView*>()->page());
        dialog->show();
        QTRY_VERIFY_WITH_TIMEOUT(dialog->player()->playbackState().ready, 15000);
        dialog->findChild<QPushButton*>("mediaFullscreen")->click();
        QTRY_VERIFY(dialog->isFullScreen());
        dialog->close();
        QTRY_VERIFY_WITH_TIMEOUT(guard.isNull(), 2500);
        QVERIFY(page.isNull());
    }
    void fullscreenEscapeAndCloseCleanup() {
        auto *dialog = new MediaPlayerDialog(m_movie, "test");
        QPointer<MediaPlayerDialog> guard(dialog);
        QPointer<QWebEnginePage> page(dialog->findChild<QWebEngineView*>()->page());
        dialog->show();
        QTRY_VERIFY_WITH_TIMEOUT(dialog->player()->playbackState().ready, 15000);
        dialog->findChild<QPushButton*>("mediaFullscreen")->click();
        QTRY_VERIFY(dialog->isFullScreen());
        QTest::keyClick(dialog, Qt::Key_Escape);
        QTRY_VERIFY_WITH_TIMEOUT(guard && !guard->isFullScreen(), 2500);
        QVERIFY(guard);
        dialog->player()->playPause();
        QTRY_VERIFY(!dialog->player()->playbackState().paused);
        dialog->close();
        QTRY_VERIFY(guard.isNull());
        QTRY_VERIFY(page.isNull());
        // Pending JavaScript callbacks must be safe during immediate teardown too.
        for (int i = 0; i < 3; ++i) {
            auto *transient = new MediaPlayerWidget(m_movie);
            transient->show(); QTest::qWait(20); delete transient;
        }
        QTest::qWait(250);
    }
};
QTEST_MAIN(MediaPlayerTest)
#include "MediaPlayerTest.moc"
