#pragma once
#include "infrastructure/AppConfig.h"
#include <QObject>
#include <QProcess>
#include <QTimer>

namespace qvw::infra {
class LogWriter;
enum class ProbeStatus { Ok, LauncherMissing, LauncherNotExecutable, InvocationFailed, UnreadableOutput, VersionMismatch };
struct ProbeResult {
    ProbeStatus status = ProbeStatus::InvocationFailed;
    QString reportedVersion;
    QString distribution;
    QString message;
    bool ok() const { return status == ProbeStatus::Ok; }
};
// Startup checks run asynchronously: missing or slow tooling must not freeze the UI.
class HypitProbe : public QObject {
    Q_OBJECT
public:
    explicit HypitProbe(QObject *parent = nullptr);
    ~HypitProbe() override;
    void start(const AppConfig &config, LogWriter &log);
    void cancel();
    void setTimeoutMs(int timeout) { m_timeout.setInterval(timeout); }
signals:
    void completed(const qvw::infra::ProbeResult &result);
private:
    void finish(ProbeResult result, int exitCode);
    QProcess m_process;
    QTimer m_timeout;
    AppConfig m_config;
    LogWriter *m_log = nullptr;
    bool m_pending = false;
};
}
Q_DECLARE_METATYPE(qvw::infra::ProbeResult)
