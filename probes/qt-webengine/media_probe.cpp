// T005b media decode probe.
//
// Loads a local MP4 into QWebEngineView via a <video> element and reports what
// the embedded Chromium actually decoded. Exit codes: 0 playable, 3 load
// failed, 4 decode error / no frames, 5 timeout.
//
// Usage: media_probe <video-file> [timeoutMs]

#include <QApplication>
#include <QFileInfo>
#include <QTimer>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QUrl>
#include <QDebug>

int main(int argc, char **argv) {
    if (argc < 2) {
        qInfo() << "usage: media_probe <video-file> [timeoutMs]";
        return 2;
    }
    const QFileInfo info(QString::fromLocal8Bit(argv[1]));
    if (!info.exists()) {
        qInfo().noquote() << "missing file" << info.filePath();
        return 2;
    }
    const int timeoutMs = argc > 2 ? QString::fromLocal8Bit(argv[2]).toInt() : 30000;

    QApplication app(argc, argv);

    QWebEngineView view;
    view.resize(640, 900);
    view.settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
    view.show();

    int exitCode = 1;
    bool finished = false;

    const QString videoUrl = QUrl::fromLocalFile(info.absoluteFilePath()).toString();
    const QString html = QStringLiteral(R"(<!doctype html>
<html><body style="margin:0;background:#000">
<video id="v" src="%1" autoplay muted playsinline style="width:100%%"></video>
<script>
window.probe = { state: "pending", detail: "" };
const v = document.getElementById("v");
v.addEventListener("error", () => {
  const e = v.error;
  window.probe = { state: "error", detail: e ? ("code=" + e.code + " " + e.message) : "unknown" };
});
v.addEventListener("loadedmetadata", () => {
  window.probe.meta = v.videoWidth + "x" + v.videoHeight + " dur=" + v.duration;
  window.probe.canPlayMp4 = v.canPlayType("video/mp4");
  window.probe.canPlayH264 = v.canPlayType('video/mp4; codecs="avc1.42E01E, mp4a.40.2"');
});
v.addEventListener("timeupdate", () => {
  if (v.currentTime > 0.2 && window.probe.state !== "error") window.probe.state = "playing";
});
</script></body></html>)").arg(videoUrl);

    QObject::connect(&view, &QWebEngineView::loadFinished, [&](bool ok) {
        if (finished) return;
        qInfo().noquote() << "loadFinished ok=" << (ok ? "true" : "false");
        if (!ok) {
            finished = true;
            exitCode = 3;
            app.quit();
            return;
        }
        QTimer::singleShot(6000, [&]() {
            if (finished) return;
            view.page()->runJavaScript(QStringLiteral("JSON.stringify(window.probe)"),
                                       [&](const QVariant &result) {
                if (finished) return;
                finished = true;
                const QString report = result.toString();
                qInfo().noquote() << "probe=" << report;
                exitCode = report.contains(QStringLiteral("\"playing\"")) ? 0 : 4;
                app.quit();
            });
        });
    });

    QTimer::singleShot(timeoutMs, [&]() {
        if (finished) return;
        finished = true;
        qInfo().noquote() << "timeout after" << timeoutMs << "ms";
        exitCode = 5;
        app.quit();
    });

    view.setHtml(html, QUrl::fromLocalFile(info.absolutePath() + QStringLiteral("/")));
    app.exec();
    qInfo().noquote() << "exitCode=" << exitCode;
    return exitCode;
}
