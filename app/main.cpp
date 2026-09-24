#include "backend/hypit/SnapshotMapper.h"
#include "controllers/ProjectController.h"
#include "infrastructure/AppConfig.h"
#include "infrastructure/LogWriter.h"
#include "ui/MainWindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>
#include <QPointer>
#include <QWebEngineView>
#include <QWebEnginePage>

int main(int argc, char **argv) {
    // QApplication consumes its own --session option. Preserve argv before it
    // does so, then give the untouched arguments to our parser (both spellings work).
    QStringList arguments;
    for (int i = 0; i < argc; ++i) arguments.append(QString::fromLocal8Bit(argv[i]));
    QApplication app(argc, argv);
    app.setApplicationName("Qt Video Workbench"); app.setApplicationVersion("0.2.0");
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Qt 视频工作台：打开本地 Hypit Run 与 Studio 会话"));
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"selftest", QStringLiteral("截图并检查退出清理后结束。")});
    parser.addOption({"out", QStringLiteral("截图 PNG 路径。"), "png"});
    parser.addOption({"session", QStringLiteral("离线会话 JSON，与 --run 互斥。"), "json"});
    parser.addOption({"config", QStringLiteral("版本锁 JSON 路径。"), "json"});
    parser.addOption({"log", QStringLiteral("JSONL 日志路径。"), "jsonl"});
    parser.addOption({"run", QStringLiteral("要打开的 .svrun 文件。"), "file"});
    parser.addOption({"workspace", QStringLiteral("工程目录，与 --run 同时指定。"), "directory"});
    parser.addOption({"runtime", QStringLiteral("本地 Runtime JSON，与 --run 同时指定。"), "json"});
    parser.addOption({"port", QStringLiteral("请求的端口，最终以 Studio stdout 为准。"), "number", "5599"});
    parser.addOption({"session-out", QStringLiteral("保存真实 GET 返回的会话 JSON。"), "json"});
    parser.process(arguments);
    const bool selftest = parser.isSet("selftest"), live = parser.isSet("run");
    const bool offline = parser.isSet("session") || (selftest && !live);
    bool portOk = false; const int port = parser.value("port").toInt(&portOk);
    if (!parser.positionalArguments().isEmpty() || (selftest && parser.value("out").isEmpty())
        || (live && (parser.value("run").isEmpty() || parser.value("workspace").isEmpty() || parser.value("runtime").isEmpty()))
        || (!live && (parser.isSet("workspace") || parser.isSet("runtime")))
        || (live && parser.isSet("session")) || (parser.isSet("session") && parser.value("session").isEmpty())
        || !portOk || port < 1 || port > 65535) {
        qCritical().noquote() << "参数错误：selftest 需要 --out；--run/--workspace/--runtime 需同时指定；--session 与 --run 互斥；端口为 1–65535。";
        return 2;
    }
    auto loaded = qvw::infra::loadAppConfig(parser.value("config"));
    QString logPath = parser.value("log");
    if (logPath.isEmpty()) logPath = loaded.ok() ? loaded.config.logFilePath
        : QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("workbench.jsonl");
    qvw::infra::LogWriter log(logPath);
    qvw::controllers::ProjectController controller(loaded.config, log);
    qvw::ui::MainWindow window;
    window.appendLog(QStringLiteral("日志：%1").arg(log.filePath()));
    if (!log.isReady()) window.appendLog(QStringLiteral("日志不可写：%1").arg(log.lastError()));
    bool finished = false, gotSnapshot = false, pageLoaded = false, jsPending = false, cleanupStopped = true;
    QTimer poll, deadline;
    poll.setInterval(500); deadline.setSingleShot(true); deadline.setInterval(60000);
    auto finish = [&](int code) {
        if (finished) return;
        finished = true; poll.stop(); deadline.stop();
        window.close();
        app.exit(code);
    };
    auto capture = [&] {
        const QString path = parser.value("out");
        QDir().mkpath(QFileInfo(path).absolutePath());
        const auto shot = window.grab();
        const bool saved = !shot.isNull() && shot.save(path, "PNG");
        qInfo().noquote() << "SCREENSHOT saved=" << saved << path;
        finish(saved ? 0 : 3);
    };
    QObject::connect(&controller, &qvw::controllers::ProjectController::message, &window, [&](const QString &text) {
        window.appendLog(text); qInfo().noquote() << text;
    });
    QObject::connect(&controller, &qvw::controllers::ProjectController::availableChanged, &window, &qvw::ui::MainWindow::setBackendAvailable);
    QObject::connect(&controller, &qvw::controllers::ProjectController::projectClosed, &window, &qvw::ui::MainWindow::clearProject);
    QObject::connect(&controller, &qvw::controllers::ProjectController::previewRequested, &window, &qvw::ui::MainWindow::showPreview);
    QObject::connect(&controller, &qvw::controllers::ProjectController::snapshotReady, &window, [&](const qvw::domain::Snapshot &snapshot) {
        gotSnapshot = true; window.showSnapshot(snapshot);
    });
    QObject::connect(&controller, &qvw::controllers::ProjectController::failed, &window, [&](const QString &error) {
        window.showError(error); qCritical().noquote() << error;
        if (selftest) finish(6);
    });
    QObject::connect(&controller, &qvw::controllers::ProjectController::payloadReceived, &window, [&](const QByteArray &data) {
        if (!parser.isSet("session-out")) return;
        QFile file(parser.value("session-out"));
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) {
            window.showError(QStringLiteral("无法保存真实会话文件。"));
            if (selftest) finish(3);
        }
    });
    QObject::connect(&window, &qvw::ui::MainWindow::openRequested, &controller,
        [&](const QString &workspace, const QString &run, const QString &runtime) { controller.openProject(workspace, run, runtime, port); });
    QObject::connect(&window, &qvw::ui::MainWindow::closeRequested, &controller, &qvw::controllers::ProjectController::closeProject);
    QObject::connect(&window, &qvw::ui::MainWindow::refreshRequested, &controller, &qvw::controllers::ProjectController::refresh);
    QObject::connect(&window, &qvw::ui::MainWindow::configurationRequested, &controller, [&](const QString &path) {
        const auto config = qvw::infra::loadAppConfig(path);
        if (!config.ok()) window.showError(config.error); else controller.configure(config.config);
    });
    QObject::connect(&window, &qvw::ui::MainWindow::previewLoaded, &app, [&](bool ok) {
        pageLoaded = ok;
        if (selftest && !ok) finish(6);
    });
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &controller, [&] {
        finished = true; poll.stop(); deadline.stop();
        const qint64 ownedPid = controller.studioPid();
        controller.closeProject();
        cleanupStopped = !controller.isRunning();
        qInfo() << "CLEANUP ownedPid=" << ownedPid << "running=" << controller.isRunning();
    });
    QObject::connect(&controller, &qvw::controllers::ProjectController::initialized, &app, [&] {
        if (live) controller.openProject(parser.value("workspace"), parser.value("run"), parser.value("runtime"), port);
    });
    QObject::connect(&deadline, &QTimer::timeout, &app, [&] { qCritical() << "selftest: live preview timeout"; finish(8); });
    QObject::connect(&poll, &QTimer::timeout, &app, [&] {
        if (finished || !gotSnapshot || !pageLoaded || jsPending || !window.previewView()) return;
        jsPending = true;
        const QPointer<QObject> guard(&poll);
        // stage.ts:40 embeds a same-origin preview iframe. Its compiled
        // composition proves the SPA rendered, beyond loadFinished's HTTP success.
        window.previewView()->page()->runJavaScript(
            "(() => { try { return !!document.querySelector('iframe')?.contentDocument?.querySelector('[data-composition-id]'); } catch (_) { return false; } })()",
            [&, guard](const QVariant &ready) {
                if (!guard) return;
                jsPending = false;
                if (finished || !ready.toBool()) return;
                qInfo() << "PREVIEW compiled composition present=true";
                poll.stop(); QTimer::singleShot(750, &app, [&] { if (!finished) capture(); });
            });
    });
    window.show();
    QTimer::singleShot(0, &app, [&] {
        if (selftest) deadline.start();
        if (offline) {
            if (parser.isSet("session")) {
                QFile file(parser.value("session"));
                if (!file.open(QIODevice::ReadOnly)) { window.showError(file.errorString()); if (selftest) finish(3); return; }
                const auto mapped = qvw::backend::hypit::mapSessionPayload(file.readAll());
                if (!mapped.ok()) { window.showError(mapped.error); if (selftest) finish(3); return; }
                window.showSnapshot(mapped.snapshot);
                qInfo() << "OFFLINE revision=" << mapped.snapshot.revision << "writableFields=" << mapped.snapshot.writableFieldCount();
            }
            window.appendLog(QStringLiteral("当前为离线会话查看，不连接 Studio。"));
            if (selftest) QTimer::singleShot(400, &app, capture);
            return;
        }
        if (!loaded.ok()) { window.showError(loaded.error); qCritical().noquote() << loaded.error; if (selftest) finish(4); return; }
        if (!log.isReady() && selftest) { finish(4); return; }
        controller.initialize();
        if (selftest) poll.start();
    });
    const int exitCode = app.exec();
    return cleanupStopped ? exitCode : 7;
}
