#pragma once
#include <QObject>
#include <QJsonObject>
#include <QStringList>

namespace qvw::infra {
class JsonProcess : public QObject {
    Q_OBJECT
public:
    explicit JsonProcess(QObject *parent = nullptr);
    ~JsonProcess() override;
    bool start(const QString &program, const QStringList &arguments, const QString &cwd, int deadlineMs = 30000);
    void cancel();
    bool isRunning() const;
signals:
    void result(const QJsonObject &object, int exitCode);
    void failed(const QString &message);
    void commandFinished(const QString &program, const QStringList &arguments, int exitCode);
private:
    class State;
    State *m;
};
}
