#include "backend/hypit/SnapshotMapper.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFile>
#include <QTimer>
#include <QDebug>

namespace {

// Loads an offline session payload into the shell. Returns false on any failure
// so the caller can exit non-zero instead of silently screenshotting an empty
// window.
bool loadSessionInto(qvw::ui::MainWindow &window, const QString &sessionPath) {
    QFile file(sessionPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qInfo().noquote() << "selftest: cannot read" << sessionPath;
        return false;
    }
    const auto mapped = qvw::backend::hypit::mapSessionPayload(file.readAll());
    if (!mapped.ok()) {
        qInfo().noquote() << "selftest: mapping failed:" << mapped.error;
        return false;
    }
    qInfo().noquote() << "selftest: revision=" << mapped.snapshot.revision
                      << "tracks=" << mapped.snapshot.tracks.size()
                      << "writableFields=" << mapped.snapshot.writableFieldCount();
    window.showSnapshot(mapped.snapshot);
    window.appendLog(QStringLiteral("已载入会话 revision %1，可写字段 %2 个")
                         .arg(mapped.snapshot.revision)
                         .arg(mapped.snapshot.writableFieldCount()));
    return true;
}

}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Qt Video Workbench"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Qt 视频工作台"));
    parser.addHelpOption();
    parser.addVersionOption();

    // selftest is a bare flag rather than a value option: with two adjacent
    // value-taking options, Qt binds the second option name as the first one's
    // value and drops it without a trace (no isSet, no positionalArguments), so
    // a typo would silently screenshot an empty window and report success.
    QCommandLineOption selfTestOption(QStringLiteral("selftest"),
        QStringLiteral("渲染主窗口到 --out 指定的 PNG 后退出。"));
    QCommandLineOption outOption(QStringLiteral("out"),
        QStringLiteral("selftest 的输出 PNG 路径。"),
        QStringLiteral("png"));
    QCommandLineOption sessionOption(QStringLiteral("session"),
        QStringLiteral("载入一份 Studio 会话 JSON（离线文件）。"),
        QStringLiteral("json"));
    parser.addOption(selfTestOption);
    parser.addOption(outOption);
    parser.addOption(sessionOption);
    parser.process(app);

    qvw::ui::MainWindow window;
    window.appendLog(QStringLiteral("T008 骨架启动：Studio 连接留待 T010。"));
    window.show();

    if (!parser.isSet(selfTestOption)) return app.exec();

    // Headless verification for the T008 acceptance check: render the shell to a
    // PNG and exit, using widget->grab() so no desktop-automation permission is
    // needed and it can run in CI. Exit codes: 0 saved, 2 bad usage, 3 load or
    // save failed.
    const QString outPath = parser.value(outOption);
    if (outPath.isEmpty()) {
        qInfo().noquote() << "selftest: --out=<png> is required";
        return 2;
    }
    const QString sessionPath = parser.value(sessionOption);
    int code = 0;

    // The grab must happen from inside the running loop: widgets only receive
    // their real geometry after the first layout pass, and a grab taken before
    // that yields the same unlaid-out image regardless of the data loaded.
    QTimer::singleShot(600, &app, [&]() {
        if (!sessionPath.isEmpty() && !loadSessionInto(window, sessionPath)) {
            code = 3;
            app.quit();
            return;
        }
        const QImage shot = window.grab().toImage();
        const bool saved = !shot.isNull() && shot.save(outPath, "PNG");
        qInfo().noquote() << "selftest: size=" << shot.width() << "x" << shot.height()
                          << "saved=" << (saved ? "true" : "false") << outPath;
        if (!saved) code = 3;
        app.quit();
    });

    app.exec();
    return code;
}
