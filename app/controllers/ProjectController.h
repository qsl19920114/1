#pragma once
#include "infrastructure/AppConfig.h"
#include "infrastructure/HypitProbe.h"
#include "backend/hypit/StudioClient.h"
#include "backend/hypit/StudioProcess.h"

namespace qvw::infra { class LogWriter; }
namespace qvw::controllers {
class ProjectController : public QObject {
    Q_OBJECT
public:
    ProjectController(infra::AppConfig config, infra::LogWriter &log, QObject *parent = nullptr);
    ~ProjectController() override;
    void initialize();
    void configure(const infra::AppConfig &config);
    void openProject(const QString &workspace, const QString &run, const QString &runtime, int port = 5599);
    void refresh();
    void closeProject();
    qint64 studioPid() const { return m_studio.processId(); }
    bool isRunning() const { return m_studio.isRunning(); }
signals:
    void availableChanged(bool ready);
    void initialized();
    void message(const QString &text);
    void failed(const QString &text);
    void previewRequested(const QUrl &url);
    void snapshotReady(const qvw::domain::Snapshot &snapshot);
    void payloadReceived(const QByteArray &payload);
    void projectClosed();
private:
    void report(const QString &text);
    infra::AppConfig m_config;
    infra::LogWriter &m_log;
    infra::HypitProbe m_probe;
    backend::hypit::StudioProcess m_studio;
    backend::hypit::StudioClient m_client;
    QUrl m_baseUrl;
    bool m_available = false;
    bool m_open = false;
};
}
