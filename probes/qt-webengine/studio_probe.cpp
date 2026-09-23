// T005a Studio embedding probe.
//
// Loads the real Studio URL in QWebEngineView, waits for load + a settling
// delay, then grabs the widget into a PNG. Exit code 0 only when the load
// succeeded and a non-blank image was written.
//
// Usage: studio_probe <url> <out.png> [timeoutMs]

#include <QApplication>
#include <QImage>
#include <QTimer>
#include <QWebEngineView>
#include <QtGlobal>
#include <QDebug>

namespace {

bool looksBlank(const QImage &image) {
    if (image.isNull() || image.width() < 8 || image.height() < 8) return true;
    const QRgb first = image.pixel(0, 0);
    // Sample a grid; a real Studio paints more than one flat colour.
    for (int y = 0; y < image.height(); y += qMax(1, image.height() / 32)) {
        for (int x = 0; x < image.width(); x += qMax(1, image.width() / 32)) {
            if (image.pixel(x, y) != first) return false;
        }
    }
    return true;
}

}

int main(int argc, char **argv) {
    if (argc < 3) {
        qInfo() << "usage: studio_probe <url> <out.png> [timeoutMs]";
        return 2;
    }
    const QString url = QString::fromLocal8Bit(argv[1]);
    const QString outPath = QString::fromLocal8Bit(argv[2]);
    const int timeoutMs = argc > 3 ? QString::fromLocal8Bit(argv[3]).toInt() : 40000;

    QApplication app(argc, argv);

    QWebEngineView view;
    view.resize(1280, 900);
    view.show();

    int exitCode = 1;
    bool finished = false;

    QObject::connect(&view, &QWebEngineView::loadFinished, [&](bool ok) {
        if (finished) return;
        qInfo().noquote() << "loadFinished ok=" << (ok ? "true" : "false");
        if (!ok) {
            finished = true;
            exitCode = 3;
            app.quit();
            return;
        }
        // Give the SPA time to compile and paint before grabbing.
        QTimer::singleShot(6000, [&]() {
            if (finished) return;
            finished = true;
            const QImage shot = view.grab().toImage();
            const bool blank = looksBlank(shot);
            const bool saved = shot.save(outPath, "PNG");
            qInfo().noquote() << "grab size=" << shot.width() << "x" << shot.height()
                              << "blank=" << (blank ? "true" : "false")
                              << "saved=" << (saved ? "true" : "false");
            exitCode = (saved && !blank) ? 0 : 4;
            app.quit();
        });
    });

    QTimer::singleShot(timeoutMs, [&]() {
        if (finished) return;
        finished = true;
        qInfo().noquote() << "timeout after" << timeoutMs << "ms";
        exitCode = 5;
        app.quit();
    });

    view.load(QUrl(url));
    app.exec();
    qInfo().noquote() << "exitCode=" << exitCode;
    return exitCode;
}
