#include "infrastructure/LogWriter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QLockFile>

namespace qvw::infra {
namespace {
constexpr qint64 maximumFileBytes = 5 * 1024 * 1024;
constexpr qint64 maximumRecordBytes = 64 * 1024;
}

void LogWriter::fail(const QString &message) {
    m_ready = false;
    m_lastError = message;
    qWarning().noquote() << message;
}

LogWriter::LogWriter(QString filePath) : m_filePath(std::move(filePath)) {
    const QDir parent = QFileInfo(m_filePath).dir();
    if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
        fail(QStringLiteral("无法创建日志目录 %1").arg(parent.absolutePath()));
        return;
    }
    QLockFile lock(m_filePath + QStringLiteral(".lock"));
    if (!lock.tryLock(100)) {
        // Another application may be between rename and file creation. Defer
        // probing until an append acquires the same lock, without disabling it.
        m_ready = true;
        m_lastError = QStringLiteral("日志正在被其他实例使用，暂缓初始化：%1").arg(m_filePath);
        qWarning().noquote() << m_lastError;
        return;
    }
    const QFileInfo info(m_filePath);
    if (info.isSymLink() || (info.exists() && !info.isFile())) {
        fail(QStringLiteral("日志路径必须是普通文件：%1").arg(m_filePath));
        return;
    }
    QFile probe(m_filePath);
    if (!probe.open(QIODevice::Append | QIODevice::Text)) {
        fail(QStringLiteral("无法写入日志 %1：%2").arg(m_filePath, probe.errorString()));
        return;
    }
    m_ready = true;
}

bool LogWriter::rotate(qint64 incomingBytes) {
    const QFileInfo current(m_filePath);
    if (current.isSymLink() || (current.exists() && !current.isFile())) {
        fail(QStringLiteral("日志文件路径已改变或不安全：%1").arg(m_filePath));
        return false;
    }
    // A writer that crashed after rotating may leave only a backup. The held
    // lock allows this writer to safely recreate the current file.
    if (!current.exists()) return true;
    if (current.size() + incomingBytes <= maximumFileBytes) return true;
    const QString first = m_filePath + QStringLiteral(".1"), second = m_filePath + QStringLiteral(".2");
    for (const auto &path : {first, second}) {
        const QFileInfo backup(path);
        if (backup.isSymLink() || (backup.exists() && !backup.isFile())) {
            fail(QStringLiteral("日志备份路径不安全，无法轮转：%1").arg(path));
            return false;
        }
    }
    if ((QFileInfo::exists(second) && !QFile::remove(second))
        || (QFileInfo::exists(first) && !QFile::rename(first, second))
        || !QFile::rename(m_filePath, first)) {
        fail(QStringLiteral("日志轮转失败：%1").arg(m_filePath));
        return false;
    }
    return true;
}

void LogWriter::append(const QString &level, const QString &message, QJsonObject fields) {
    if (!m_ready) return;
    fields.insert("timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    fields.insert("level", level);
    fields.insert("message", message.left(4096));
    if (message.size() > 4096) fields.insert("truncated", true);
    const QByteArray line = QJsonDocument(fields).toJson(QJsonDocument::Compact) + '\n';
    if (line.size() > maximumRecordBytes) {
        fail(QStringLiteral("日志记录超过 64 KiB：%1").arg(m_filePath));
        return;
    }
    QLockFile lock(m_filePath + QStringLiteral(".lock"));
    if (!lock.tryLock(100)) {
        m_lastError = QStringLiteral("日志锁竞争超过 100 毫秒，本条记录未保存；后续可重试：%1").arg(m_filePath);
        qWarning().noquote() << m_lastError;
        return;
    }
    if (!rotate(line.size())) return;
    QFile file(m_filePath);
    if (!file.open(QIODevice::Append)) {
        fail(QStringLiteral("无法写入日志 %1：%2").arg(m_filePath, file.errorString()));
        return;
    }
    if (file.write(line) != line.size() || !file.flush()) {
        fail(QStringLiteral("写入日志失败 %1：%2").arg(m_filePath, file.errorString()));
        return;
    }
    m_lastError.clear();
}

void LogWriter::info(const QString &message) { append(QStringLiteral("INFO"), message); }
void LogWriter::warn(const QString &message) { append(QStringLiteral("WARN"), message); }
void LogWriter::error(const QString &message) { append(QStringLiteral("ERROR"), message); }

void LogWriter::command(const QString &program, const QStringList &arguments, int exitCode) {
    QJsonArray boundedArguments;
    int remaining = 4096;
    bool truncated = program.size() > 1024;
    for (const auto &argument : arguments) {
        if (boundedArguments.size() >= 128 || remaining == 0) { truncated = true; break; }
        const auto value = argument.left(qMin(1024, remaining));
        truncated = truncated || value.size() != argument.size();
        boundedArguments.append(value);
        remaining -= value.size();
    }
    append(exitCode == 0 ? "INFO" : "ERROR", QStringLiteral("外部命令结束"),
           {{"program", program.left(1024)}, {"arguments", boundedArguments}, {"exitCode", exitCode}, {"truncated", truncated}, {"argumentCount", arguments.size()}});
}

}
