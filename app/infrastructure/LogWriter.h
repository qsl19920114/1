#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>

namespace qvw::infra {

/// Appends one structured line per event to a log file.
///
/// External commands retain an arguments array and exit code. Oversized values
/// are marked truncated with their original argument count; records are bounded
/// to 64 KiB and files rotate at 5 MiB with two backups.
class LogWriter {
public:
    explicit LogWriter(QString filePath);

    bool isReady() const { return m_ready; }
    QString filePath() const { return m_filePath; }
    QString lastError() const { return m_lastError; }

    void info(const QString &message);
    void warn(const QString &message);
    void error(const QString &message);

    /// Records a finished external process. `exitCode` < 0 means the process
    /// failed to start or was killed, which must not be logged as a normal exit.
    void command(const QString &program, const QStringList &arguments, int exitCode);

private:
    void append(const QString &level, const QString &message, QJsonObject fields = {});
    bool rotate(qint64 incomingBytes);
    void fail(const QString &message);

    QString m_filePath;
    bool m_ready = false;
    QString m_lastError;
};

}
