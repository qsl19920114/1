#include "infrastructure/HypitProbe.h"
#include "infrastructure/LogWriter.h"
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace qvw::infra {
HypitProbe::HypitProbe(QObject *parent) : QObject(parent) {
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(20000);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        finish({ProbeStatus::InvocationFailed, {}, {}, QStringLiteral("Hypit 版本自检超时。")}, -1);
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            finish({ProbeStatus::InvocationFailed, {}, {}, QStringLiteral("无法启动 Hypit：%1").arg(m_process.errorString())}, -1);
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (!m_pending) return;
        if (status != QProcess::NormalExit || code != 0) {
            finish({ProbeStatus::InvocationFailed, {}, {}, QStringLiteral("Hypit 自检失败（exit=%1）：%2")
                .arg(code).arg(QString::fromUtf8(m_process.readAllStandardError()).trimmed())}, status == QProcess::NormalExit ? code : -1);
            return;
        }
        const auto payload = QJsonDocument::fromJson(m_process.readAllStandardOutput()).object();
        ProbeResult result;
        result.reportedVersion = payload.value("version").toString();
        result.distribution = payload.value("distribution").toString();
        if (result.reportedVersion.isEmpty()) {
            result.status = ProbeStatus::UnreadableOutput;
            result.message = QStringLiteral("hypit version --json 未返回有效的 version 字段。");
        } else if (result.reportedVersion != m_config.expectedHypitVersion) {
            result.status = ProbeStatus::VersionMismatch;
            result.message = QStringLiteral("Hypit 版本不符：实际 %1，版本锁要求 %2。请选择匹配版本的配置。")
                .arg(result.reportedVersion, m_config.expectedHypitVersion);
        } else {
            result.status = ProbeStatus::Ok;
            result.message = QStringLiteral("Hypit %1 自检通过（%2）").arg(result.reportedVersion, result.distribution);
        }
        finish(result, 0);
    });
}
HypitProbe::~HypitProbe() { cancel(); }
void HypitProbe::cancel() {
    m_pending = false;
    m_timeout.stop();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(2000);
        if (m_log) m_log->command(m_config.launcherPath, {"version", "--json"}, -1);
    }
}
void HypitProbe::start(const AppConfig &config, LogWriter &log) {
    cancel();
    m_config = config;
    m_log = &log;
    m_pending = true;
    const QFileInfo launcher(config.launcherPath);
    if (!launcher.isFile()) {
        finish({ProbeStatus::LauncherMissing, {}, {}, QStringLiteral("找不到 Hypit 启动器：%1。请检查 hypit.distributionPath。").arg(config.launcherPath)}, -1);
        return;
    }
    if (!launcher.isExecutable()) {
        finish({ProbeStatus::LauncherNotExecutable, {}, {}, QStringLiteral("Hypit 启动器不可执行：%1").arg(config.launcherPath)}, -1);
        return;
    }
    m_process.setWorkingDirectory(config.distributionPath);
    m_log->info(QStringLiteral("开始版本自检：%1 version --json").arg(config.launcherPath));
    m_timeout.start();
    m_process.start(config.launcherPath, {"version", "--json"});
}
void HypitProbe::finish(ProbeResult result, int exitCode) {
    if (!m_pending) return;
    m_pending = false;
    m_timeout.stop();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(2000);
    }
    m_log->command(m_config.launcherPath, {"version", "--json"}, exitCode);
    if (result.ok()) m_log->info(result.message); else m_log->error(result.message);
    emit completed(result);
}
}
