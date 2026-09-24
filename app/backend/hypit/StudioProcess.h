#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QUrl>

namespace qvw::backend::hypit {

class StudioProcess : public QObject {
    Q_OBJECT
public:
    explicit StudioProcess(QObject *parent = nullptr);
    ~StudioProcess() override;
    bool start(const QString &launcher, const QString &workspace, const QString &run,
               const QString &runtime, int requestedPort = 5599);
    void stop();
    bool isRunning() const;
    qint64 processId() const;

signals:
    void ready(const QUrl &url);
    void failed(const QString &message);
    void output(const QString &text);
    void finished(int exitCode);
    void commandFinished(const QString &program, const QStringList &args, int exitCode);

private:
    void consume(QByteArray &buffer, const QByteArray &bytes, bool stdoutChannel, bool flush = false);
    void failOnce(const QString &message);
    void finishOnce(int exitCode, QProcess::ExitStatus status);
    void cleanOwnedGroup();

    QProcess m_process;
    QTimer m_startupTimer;
    QByteArray m_stdout;
    QByteArray m_stderr;
    bool m_active = false;
    bool m_ready = false;
    bool m_failed = false;
    bool m_stopping = false;
    qint64 m_ownedGroup = 0;
};

}
