#include "backend/hypit/SnapshotMapper.h"
#include "controllers/ProjectController.h"
#include "controllers/DocumentController.h"
#include "controllers/EditorController.h"
#include "controllers/ExportController.h"
#include "controllers/ProposalController.h"
#include "controllers/SampleCreationController.h"
#include "services/SampleCatalog.h"
#include "infrastructure/AppConfig.h"
#include "infrastructure/LogWriter.h"
#include "infrastructure/RuntimePaths.h"
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
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QtWebEngineCore/qtwebenginecoreglobal.h>

int main(int argc, char **argv) {
    // QApplication consumes its own --session option. Preserve argv before it
    // does so, then give the untouched arguments to our parser (both spellings work).
    QStringList arguments;
    for (int i = 0; i < argc; ++i) arguments.append(QString::fromLocal8Bit(argv[i]));
    QApplication app(argc, argv);
    app.setApplicationName("Qt Video Workbench");app.setApplicationDisplayName(QStringLiteral("FrameLab · 灵感片场")); app.setApplicationVersion(QStringLiteral(QVW_APP_VERSION));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Qt 视频工作台：打开本地 Hypit Run 与 Studio 会话"));
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"selftest", QStringLiteral("截图并检查退出清理后结束。")});
    parser.addOption({"verify-startup",QStringLiteral("无截图验证真实工程、编译预览和退出清理。")});
    parser.addOption({"create-project",QStringLiteral("从应用模板创建新工程（目录必须不存在或为空）。"),"directory"});
    parser.addOption({"name",QStringLiteral("新工程名称。"),"name",QStringLiteral("校园摄影社")});
    parser.addOption({"report-out",QStringLiteral("启动验证JSON结果。"),"json"});
    parser.addOption({"out", QStringLiteral("截图 PNG 路径。"), "png"});
    parser.addOption({"session", QStringLiteral("离线会话 JSON，与 --run 互斥。"), "json"});
    parser.addOption({"config", QStringLiteral("版本锁 JSON 路径。"), "json"});
    parser.addOption({"log", QStringLiteral("JSONL 日志路径。"), "jsonl"});
    parser.addOption({"project", QStringLiteral("打开 workbench.qvw.json 工程清单。"), "json"});
    parser.addOption({"run", QStringLiteral("要打开的 .svrun 文件。"), "file"});
    parser.addOption({"workspace", QStringLiteral("工程目录，与 --run 同时指定。"), "directory"});
    parser.addOption({"runtime", QStringLiteral("本地 Runtime JSON，与 --run 同时指定。"), "json"});
    parser.addOption({"port", QStringLiteral("请求的端口，最终以 Studio stdout 为准。"), "number", "5599"});
    parser.addOption({"session-out", QStringLiteral("保存真实 GET 返回的会话 JSON。"), "json"});
    parser.process(arguments);
    const bool selftest = parser.isSet("selftest"), verify=parser.isSet("verify-startup"), verification=selftest||verify;
    const bool live = parser.isSet("run") || parser.isSet("project") || parser.isSet("create-project");
    const bool offline = parser.isSet("session") || (selftest && !live);
    bool portOk = false; const int port = parser.value("port").toInt(&portOk);
    if (!parser.positionalArguments().isEmpty() || (selftest && parser.value("out").isEmpty())
        || (parser.isSet("run") && (parser.value("run").isEmpty() || parser.value("workspace").isEmpty() || parser.value("runtime").isEmpty()))
        || (!parser.isSet("run") && (parser.isSet("workspace") || parser.isSet("runtime")))
        || (parser.isSet("project") && (parser.value("project").isEmpty() || parser.isSet("run")))
        || (parser.isSet("create-project")&&(parser.value("create-project").isEmpty()||parser.isSet("project")||parser.isSet("run")||parser.isSet("session")))
        || (verify&&(!live||selftest||parser.isSet("session")))
        || (live && parser.isSet("session")) || (parser.isSet("session") && parser.value("session").isEmpty())
        || !portOk || port < 1 || port > 65535) {
        qCritical().noquote() << "参数错误：selftest 需要 --out；--run/--workspace/--runtime 需同时指定；--project、--session、--run 互斥；端口为 1–65535。";
        return 2;
    }
    auto loaded = qvw::infra::loadAppConfig(parser.value("config"));
    QString logPath = parser.value("log");
    if (logPath.isEmpty()) logPath = loaded.ok() ? loaded.config.logFilePath
        : QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("workbench.jsonl");
    qvw::infra::LogWriter log(logPath);
    qvw::controllers::ProjectController controller(loaded.config, log);
    qvw::controllers::DocumentController document;
    qvw::controllers::EditorController editor;
    qvw::controllers::SampleCreationController creation(document,editor);
    document.setMediaTools(loaded.config.ffprobePath,loaded.config.ffmpegPath,loaded.config.processEnvironment);
    qvw::controllers::ProposalController proposals(editor);
    qvw::controllers::ExportController exporter(loaded.config,log);
    qvw::ui::MainWindow window;
    window.setSamples(qvw::services::SampleCatalog::discover(loaded.config.distributionPath));
    window.appendLog(QStringLiteral("日志：%1").arg(log.filePath()));
    if (!log.isReady()) window.appendLog(QStringLiteral("日志不可写：%1").arg(log.lastError()));
    bool finished = false, gotSnapshot = false, pageLoaded = false, jsPending = false, cleanupStopped = true;
    bool compiledReady=false,imagesReady=false,mediaReady=false,previewVideosReady=false,verificationSucceeded=false; qint64 ownedPid=0;QUrl studioUrl;qvw::domain::Snapshot lastSnapshot;
    QString verificationError;
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
        gotSnapshot = true;lastSnapshot=snapshot; editor.acceptSnapshot(snapshot);
    });
    QObject::connect(&controller, &qvw::controllers::ProjectController::previewRequested, &editor, [&](const QUrl &url) {
        studioUrl=url;
        editor.attach(url,controller.workspace());
    });
    QObject::connect(&controller, &qvw::controllers::ProjectController::projectClosed, &editor, &qvw::controllers::EditorController::clear);
    QObject::connect(&editor, &qvw::controllers::EditorController::snapshotReady, &window, &qvw::ui::MainWindow::showSnapshot);
    QObject::connect(&editor, &qvw::controllers::EditorController::stateChanged, &window, &qvw::ui::MainWindow::setEditorState);
    QObject::connect(&window,&qvw::ui::MainWindow::demoProposalRequested,&proposals,&qvw::controllers::ProposalController::generateDemo);
    QObject::connect(&window,&qvw::ui::MainWindow::importProposalRequested,&proposals,&qvw::controllers::ProposalController::importJson);
    QObject::connect(&window,&qvw::ui::MainWindow::confirmProposalRequested,&proposals,&qvw::controllers::ProposalController::confirm);
    QObject::connect(&window,&qvw::ui::MainWindow::discardProposalRequested,&proposals,&qvw::controllers::ProposalController::discard);
    QObject::connect(&proposals,&qvw::controllers::ProposalController::proposalChanged,&window,&qvw::ui::MainWindow::showProposal);
    QObject::connect(&proposals,&qvw::controllers::ProposalController::message,&window,[&](const QString &text){window.appendLog(text);log.info(text);});
    QObject::connect(&proposals,&qvw::controllers::ProposalController::failed,&window,[&](const QString &text){window.appendLog(QStringLiteral("提案未执行：%1").arg(text));log.warn(text);});
    QObject::connect(&window, &qvw::ui::MainWindow::editRequested, &editor, &qvw::controllers::EditorController::edit);
    QObject::connect(&window, &qvw::ui::MainWindow::sourceEditRequested, &editor, &qvw::controllers::EditorController::replaceSource);
    QObject::connect(&window, &qvw::ui::MainWindow::undoRequested, &editor, &qvw::controllers::EditorController::undo);
    QObject::connect(&window, &qvw::ui::MainWindow::redoRequested, &editor, &qvw::controllers::EditorController::redo);
    QObject::connect(&window, &qvw::ui::MainWindow::exportRequested, &exporter, [&](const QString &path){
        if(editor.isBusy()||document.importingVideo()){window.appendLog(QStringLiteral("请等待编辑或素材导入完成后导出。"));return;}
        exporter.startExport(editor.snapshot(),path);
    });
    QObject::connect(&window, &qvw::ui::MainWindow::cancelExportRequested, &exporter, &qvw::controllers::ExportController::cancelBuild);
    QObject::connect(&window, &qvw::ui::MainWindow::stopExportObservationRequested, &exporter, &qvw::controllers::ExportController::stopObserving);
    QObject::connect(&window, &qvw::ui::MainWindow::resumeExportRequested, &exporter, &qvw::controllers::ExportController::resume);
    QObject::connect(&window,&qvw::ui::MainWindow::clearExportCacheRequested,&exporter,&qvw::controllers::ExportController::clearFinishedCache);
    QObject::connect(&exporter, &qvw::controllers::ExportController::taskChanged, &window, &qvw::ui::MainWindow::showExportTask);
    QObject::connect(&exporter, &qvw::controllers::ExportController::message, &window, [&](const QString &text){window.appendLog(text);log.info(text);});
    QObject::connect(&exporter, &qvw::controllers::ExportController::failed, &window, [&](const QString &text){window.appendLog(QStringLiteral("导出未完成：%1").arg(text));log.error(text);});
    QObject::connect(&exporter, &qvw::controllers::ExportController::completed, &window, [&](const QString &path){window.appendLog(QStringLiteral("成片已通过参数与全片解码验证：%1").arg(path));});
    QObject::connect(&editor, &qvw::controllers::EditorController::message, &window, [&](const QString &text){window.appendLog(text);log.info(text);});
    QObject::connect(&editor, &qvw::controllers::EditorController::failed, &window, [&](const QString &text){window.appendLog(QStringLiteral("编辑未完成：%1").arg(text));log.error(text);});
    QObject::connect(&controller, &qvw::controllers::ProjectController::failed, &window, [&](const QString &error) {
        window.showError(error); qCritical().noquote() << error;
        verificationError=error;if (verification) finish(6);
    });
    QObject::connect(&controller, &qvw::controllers::ProjectController::payloadReceived, &window, [&](const QByteArray &data) {
        if (!parser.isSet("session-out")) return;
        QFile file(parser.value("session-out"));
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) {
            window.showError(QStringLiteral("无法保存真实会话文件。"));
            if (verification) finish(3);
        }
    });
    QObject::connect(&window, &qvw::ui::MainWindow::openRequested, &controller,
        [&](const QString &workspace, const QString &run, const QString &runtime) { document.close(); controller.openProject(workspace, run, runtime, port); });
    QObject::connect(&window, &qvw::ui::MainWindow::closeRequested, &controller, [&] { controller.closeProject(); document.close(); });
    QObject::connect(&window, &qvw::ui::MainWindow::newDocumentRequested, &document, [&](const QString &directory,const QString &name) {
        creation.cancel();document.create(QDir(qvw::infra::RuntimePaths::templateDirectory()).filePath("title-card"),directory,name);
    });
    QObject::connect(&window,&qvw::ui::MainWindow::newTemplateDocumentRequested,&document,[&](const QString &id,const QString &dir,const QString &name){
        if(editor.isBusy()||document.importingVideo()||exporter.isBusy())return;
        if(id!="title-card"&&id!="video-story"){window.showError("未知创作模板。");return;}
        creation.cancel();document.create(QDir(qvw::infra::RuntimePaths::templateDirectory()).filePath(id),dir,name);
    });
    QObject::connect(&window,&qvw::ui::MainWindow::sampleDocumentRequested,&creation,[&](const QString &sample,const QString &dir,const QString &name){
        if(exporter.isBusy())return;creation.create(sample,QDir(qvw::infra::RuntimePaths::templateDirectory()).filePath("video-story"),dir,name);
    });
    QObject::connect(&window,&qvw::ui::MainWindow::openDocumentRequested,&document,[&](const QString &manifest){creation.cancel();document.open(manifest);});
    QObject::connect(&creation,&qvw::controllers::SampleCreationController::message,&window,&qvw::ui::MainWindow::appendLog);
    QObject::connect(&creation,&qvw::controllers::SampleCreationController::failed,&window,&qvw::ui::MainWindow::showError);
    QObject::connect(&window, &qvw::ui::MainWindow::saveDocumentRequested, &document, &qvw::controllers::DocumentController::save);
    QObject::connect(&window, &qvw::ui::MainWindow::importImageRequested, &document, &qvw::controllers::DocumentController::importImage);
    QObject::connect(&window,&qvw::ui::MainWindow::importVideoRequested,&document,&qvw::controllers::DocumentController::importVideo);
    QObject::connect(&document,&qvw::controllers::DocumentController::videoImportStateChanged,&window,&qvw::ui::MainWindow::setImportBusy);
    QObject::connect(&window,&qvw::ui::MainWindow::cancelImportRequested,&document,[&]{creation.cancel();document.cancelVideoImport();window.appendLog("视频导入已取消，工程保持不变。");});
    QObject::connect(&document, &qvw::controllers::DocumentController::projectChanged, &window, &qvw::ui::MainWindow::showDocument);
    QObject::connect(&document, &qvw::controllers::DocumentController::documentClosed, &window, &qvw::ui::MainWindow::clearDocument);
    QObject::connect(&document, &qvw::controllers::DocumentController::documentClosed, &exporter, &qvw::controllers::ExportController::clearProject);
    QObject::connect(&document,&qvw::controllers::DocumentController::documentClosed,&proposals,&qvw::controllers::ProposalController::clearProject);
    QObject::connect(&document,&qvw::controllers::DocumentController::projectChanged,&proposals,&qvw::controllers::ProposalController::setProject);
    QObject::connect(&document, &qvw::controllers::DocumentController::projectLoaded, &exporter, &qvw::controllers::ExportController::setProject);
    QObject::connect(&document, &qvw::controllers::DocumentController::projectLoaded, &controller, [&](const qvw::domain::Project &project) {
        controller.openDocument(project,port);
    });
    QObject::connect(&document, &qvw::controllers::DocumentController::message, &window, [&](const QString &text) {
        window.appendLog(text); log.info(text);
    });
    QObject::connect(&document, &qvw::controllers::DocumentController::failed, &window, [&](const QString &text) {
        window.showError(QStringLiteral("工程操作失败：%1").arg(text)); log.error(text); qCritical().noquote()<<text;
        verificationError=text;if(verification) finish(3);
    });
    QObject::connect(&window, &qvw::ui::MainWindow::refreshRequested, &editor, [&] {
        if(editor.snapshot().isLoaded())editor.refresh();else controller.refresh();
    });
    QObject::connect(&window, &qvw::ui::MainWindow::configurationRequested, &controller, [&](const QString &path) {
        if (exporter.isBusy()||editor.isBusy()||document.importingVideo()) {
            window.appendLog(QStringLiteral("导出正在观察或验证，请先停止观察再切换后端配置。"));
            return;
        }
        const auto config = qvw::infra::loadAppConfig(path);
        if (!config.ok()) window.showError(config.error); else {creation.cancel();document.close();document.setMediaTools(config.config.ffprobePath,config.config.ffmpegPath,config.config.processEnvironment);window.setSamples(qvw::services::SampleCatalog::discover(config.config.distributionPath));exporter.setConfig(config.config);controller.configure(config.config);}
    });
    QObject::connect(&window, &qvw::ui::MainWindow::previewLoaded, &app, [&](bool ok) {
        if(finished)return;
        pageLoaded = ok;
        if (verification && !ok) {verificationError=QStringLiteral("Studio页面加载失败。");finish(6);}
    });
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &controller, [&] {
        finished = true; poll.stop(); deadline.stop();
        ownedPid = controller.studioPid();
        document.cancelVideoImport();creation.cancel();controller.closeProject();
        cleanupStopped = !controller.isRunning();
        qInfo() << "CLEANUP ownedPid=" << ownedPid << "running=" << controller.isRunning();
    });
    QObject::connect(&controller, &qvw::controllers::ProjectController::initialized, &app, [&] {
        if(parser.isSet("create-project"))document.create(QDir(qvw::infra::RuntimePaths::templateDirectory()).filePath("title-card"),parser.value("create-project"),parser.value("name"));
        else if (parser.isSet("project")) document.open(parser.value("project"));
        else if (live) controller.openProject(parser.value("workspace"), parser.value("run"), parser.value("runtime"), port);
    });
    QObject::connect(&deadline, &QTimer::timeout, &app, [&] { verificationError=QStringLiteral("真实预览验证超时。");qCritical().noquote()<<verificationError;finish(8); });
    QObject::connect(&poll, &QTimer::timeout, &app, [&] {
        if (finished || !gotSnapshot || !pageLoaded || jsPending || !window.previewView()) return;
        jsPending = true;
        const QPointer<QObject> guard(&poll);
        // stage.ts:40 embeds a same-origin preview iframe. Its compiled
        // composition proves the SPA rendered, beyond loadFinished's HTTP success.
        window.previewView()->page()->runJavaScript(
            "(() => { try { const d=document.querySelector('iframe')?.contentDocument; const imgs=d?Array.from(d.querySelectorAll('img')):[];const videos=d?Array.from(d.querySelectorAll('video')):[];const imagesOk=imgs.every(i=>i.complete&&i.naturalWidth>0);const videosOk=videos.every(v=>v.readyState>=2&&v.videoWidth>0);return {composition:!!d?.querySelector('[data-composition-id]'),images:imgs.length>0&&imagesOk,videos:videos.length>0&&videosOk,media:imgs.length+videos.length>0&&imagesOk&&videosOk}; } catch (_) { return {}; } })()",
            [&, guard](const QVariant &ready) {
                if (!guard || finished) return;
                jsPending = false;
                const auto result=ready.toMap();compiledReady=result.value("composition").toBool();imagesReady=result.value("images").toBool();mediaReady=result.value("media").toBool();previewVideosReady=result.value("videos").toBool();
                if (finished || !compiledReady || !mediaReady) return;
                qInfo() << "PREVIEW compiled composition present=true";
                poll.stop(); QTimer::singleShot(750, &app, [&] { if (!finished) {if(verify){verificationSucceeded=true;finish(0);}else capture();} });
            });
    });
    window.show();
    QTimer::singleShot(0, &app, [&] {
        if (verification) deadline.start();
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
        if (!loaded.ok()) { verificationError=loaded.error;window.showError(loaded.error); qCritical().noquote() << loaded.error; if (verification) finish(4); return; }
        if(verify)for(const auto &tool:{loaded.config.nodePath,loaded.config.ffmpegPath,loaded.config.ffprobePath}) {
            if(tool.isEmpty()||!QFileInfo(tool).isFile()||!QFileInfo(tool).isExecutable()){verificationError=QStringLiteral("缺少可执行的 Node/FFmpeg/ffprobe；请配置 tools 路径。");qCritical().noquote()<<verificationError;finish(4);return;}
        }
        if (!log.isReady() && verification) { verificationError=log.lastError();finish(4); return; }
        controller.initialize();
        if (verification) poll.start();
    });
    const int exitCode = app.exec();
    const bool incompleteVerification=verify&&exitCode==0&&!verificationSucceeded;
    const int finalCode=!cleanupStopped?7:incompleteVerification?9:exitCode;
    if(incompleteVerification&&verificationError.isEmpty())verificationError=QStringLiteral("验证未完成：窗口在真实预览就绪前被关闭或程序提前结束。");
    if(!cleanupStopped&&verificationError.isEmpty())verificationError=QStringLiteral("自有 Studio 未完全退出。");
    if(verify&&parser.isSet("report-out")) {
        QJsonObject report{{"format","qvw.startup-verification@1"},{"verdict",finalCode==0?"PASS":"FAIL"},{"exitCode",finalCode},{"error",verificationError},{"qtVersion",qVersion()},{"appVersion",app.applicationVersion()},{"resourceRoot",qvw::infra::RuntimePaths::resourceRoot()},{"templateDirectory",qvw::infra::RuntimePaths::templateDirectory()},{"config",loaded.config.configFilePath},{"distribution",loaded.config.distributionPath},{"node",loaded.config.nodePath},{"ffmpeg",loaded.config.ffmpegPath},{"ffprobe",loaded.config.ffprobePath},{"project",document.project().manifestPath()},{"studioUrl",studioUrl.toString()},{"revision",lastSnapshot.revision},{"sourceFingerprint",QString::fromLatin1(lastSnapshot.sourceFingerprint.toHex())},{"snapshot",gotSnapshot},{"pageLoaded",pageLoaded},{"compiledCompositionReady",compiledReady},{"imagesReady",imagesReady},{"mediaReady",mediaReady},{"previewVideosReady",previewVideosReady},{"ownedStudioPid",ownedPid},{"cleanupStopped",cleanupStopped},{"screenshots",false}};
        report["project"]=document.hasProject()?document.project().manifestPath():QString();
        report["verificationSucceeded"]=verificationSucceeded;
        report["webEngineVersion"]=qWebEngineVersion();
        report["chromiumVersion"]=qWebEngineChromiumVersion();
        report["chromiumSecurityPatchVersion"]=qWebEngineChromiumSecurityPatchVersion();
        QSaveFile out(parser.value("report-out"));const auto bytes=QJsonDocument(report).toJson();
        if(!out.open(QIODevice::WriteOnly)||out.write(bytes)!=bytes.size()||!out.commit()){qCritical()<<"Cannot save startup report";return 3;}
    }
    return finalCode;
}
