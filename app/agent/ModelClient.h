#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QProcessEnvironment>

namespace qvw::agent {
class ModelClient : public QObject {
    Q_OBJECT
public:
    explicit ModelClient(QObject *parent = nullptr);
    ~ModelClient() override;
    void request(const QString &prompt, const QJsonObject &schema);
    void cancel();
    bool isBusy() const;
    void setProgram(const QString &program);
    void setEnvironment(const QProcessEnvironment &environment);
    void setTimeoutMs(int timeoutMs);
signals:
    // Full text snapshot from an observed public agent_message event; never reasoning.
    void publicMessageReceived(const QString &text);
    void completed(const QJsonObject &result);
    void failed(const QString &message);
    void busyChanged(bool busy);
private:
    class State;
    State *m;
};
}
