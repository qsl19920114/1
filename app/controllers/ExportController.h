#pragma once
#include "domain/ExportTask.h"
#include "domain/Project.h"
#include "infrastructure/AppConfig.h"
#include "infrastructure/LogWriter.h"
#include <QObject>
namespace qvw::controllers {
class ExportController:public QObject {
    Q_OBJECT
public:
    ExportController(infra::AppConfig config,infra::LogWriter &log,QObject *parent=nullptr);
    ~ExportController() override;
    void setConfig(infra::AppConfig config);
    void setProject(domain::Project project);
    void clearProject();
    void startExport(const domain::Snapshot &snapshot,QString destination);
    void cancelBuild();
    void stopObserving();
    void resume();
    bool isBusy() const;
    const domain::ExportTask &task() const;
    void setMediaTools(QString ffprobe,QString ffmpeg);
signals:
    void taskChanged(const qvw::domain::ExportTask &task);
    void busyChanged(bool busy);
    void completed(const QString &path);
    void failed(const QString &message);
    void message(const QString &message);
private:
    class State;State *m;
};
}
