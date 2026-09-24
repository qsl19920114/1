#include "controllers/ProjectController.h"
#include "infrastructure/LogWriter.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace qvw::controllers {
ProjectController::ProjectController(infra::AppConfig config, infra::LogWriter &log, QObject *parent)
    : QObject(parent), m_config(std::move(config)), m_log(log) {
    connect(&m_probe, &infra::HypitProbe::completed, this, [this](const infra::ProbeResult &result) {
        report(result.message);
        m_available = result.ok();
        emit availableChanged(m_available);
        if (m_available) emit initialized(); else emit failed(result.message);
    });
    connect(&m_studio, &backend::hypit::StudioProcess::output, this, &ProjectController::report);
    connect(&m_studio, &backend::hypit::StudioProcess::commandFinished, this,
            [this](const QString &program, const QStringList &args, int code) { m_log.command(program, args, code); });
    connect(&m_studio, &backend::hypit::StudioProcess::ready, this, [this](const QUrl &url) {
        if (!m_open) return;
        m_baseUrl = url;
        report(QStringLiteral("Studio 已就绪：%1（PID %2）").arg(url.toString()).arg(m_studio.processId()));
        emit previewRequested(url);
        refresh();
    });
    connect(&m_studio, &backend::hypit::StudioProcess::failed, this, [this](const QString &error) {
        m_open = false; m_baseUrl = QUrl(); m_client.cancel();
        m_log.error(error); emit projectClosed(); emit failed(error);
    });
    connect(&m_studio, &backend::hypit::StudioProcess::finished, this, [this](int code) {
        report(QStringLiteral("Studio 已退出，exit=%1").arg(code));
        if (m_open) { m_open = false; m_baseUrl = QUrl(); m_client.cancel(); emit projectClosed(); }
    });
    connect(&m_client, &backend::hypit::StudioClient::snapshotReady, this, [this](const domain::Snapshot &snapshot) {
        report(QStringLiteral("已读取真实会话：revision %1，轨道 %2，可写字段 %3")
            .arg(snapshot.revision).arg(snapshot.tracks.size()).arg(snapshot.writableFieldCount()));
        emit snapshotReady(snapshot);
    });
    connect(&m_client, &backend::hypit::StudioClient::payloadReceived, this, &ProjectController::payloadReceived);
    connect(&m_client, &backend::hypit::StudioClient::failed, this, [this](const QString &error) {
        m_log.error(error); emit failed(error);
    });
}
ProjectController::~ProjectController() { m_probe.cancel(); closeProject(); }
void ProjectController::report(const QString &text) { m_log.info(text); emit message(text); }
void ProjectController::initialize() {
    m_available = false; emit availableChanged(false);
    report(QStringLiteral("正在检查 Hypit 配置：%1").arg(m_config.configFilePath));
    m_probe.start(m_config, m_log);
}
void ProjectController::configure(const infra::AppConfig &config) {
    closeProject(); m_probe.cancel(); m_config = config; initialize();
}
void ProjectController::openProject(const QString &workspace, const QString &run, const QString &runtime, int port) {
    if (!m_available) { emit failed(QStringLiteral("请先完成 Hypit 自检。")); return; }
    // M1 opens verified local runtime profiles only; opening an arbitrary paid
    // provider would violate the project's explicitly local-only boundary.
    QFile file(runtime);
    if (!file.open(QIODevice::ReadOnly)) { emit failed(QStringLiteral("无法读取 Runtime 配置：%1").arg(runtime)); return; }
    const auto profile = QJsonDocument::fromJson(file.readAll()).object();
    const auto endpoints = profile.value("endpoints").toObject();
    if (profile.value("format").toString() != "hypit.runtime-local@1" || endpoints.isEmpty()) {
        emit failed(QStringLiteral("请选择含本地 endpoints 的 hypit.runtime-local@1 配置。")); return;
    }
    for (const auto &entry : endpoints) {
        const QString provider = entry.toObject().value("use").toString();
        if (provider != "@hypit/provider-media-local" && provider != "@hypit/provider-hyperframes-local") {
            emit failed(QStringLiteral("当前仅支持已验证的本地媒体与 Hyperframes Provider：%1").arg(provider)); return;
        }
    }
    if (!QFileInfo(workspace).isDir() || !QFileInfo(run).isFile() || port < 1 || port > 65535) {
        emit failed(QStringLiteral("工程目录、Run 文件或端口无效。")); return;
    }
    closeProject();
    m_open = true;
    report(QStringLiteral("正在打开工程：%1").arg(QFileInfo(run).absoluteFilePath()));
    if (!m_studio.start(m_config.launcherPath, QFileInfo(workspace).absoluteFilePath(),
                        QFileInfo(run).absoluteFilePath(), QFileInfo(runtime).absoluteFilePath(), port)) m_open = false;
}
void ProjectController::refresh() {
    if (m_open && !m_baseUrl.isEmpty()) m_client.fetchSession(m_baseUrl);
}
void ProjectController::closeProject() {
    m_open = false; m_baseUrl = QUrl(); m_client.cancel(); m_studio.stop(); emit projectClosed();
}
}
