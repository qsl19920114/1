#include "infrastructure/LogWriter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>

namespace qvw::infra {

LogWriter::LogWriter(QString filePath) : m_filePath(std::move(filePath)) {
    const QDir parent = QFileInfo(m_filePath).dir();
    if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
        m_lastError = QStringLiteral("无法创建日志目录 %1").arg(parent.absolutePath());
        return;
    }
    QFile probe(m_filePath);
    if (!probe.open(QIODevice::Append | QIODevice::Text)) {
        m_lastError = QStringLiteral("无法写入日志 %1：%2").arg(m_filePath, probe.errorString());
        return;
    }
    m_ready = true;
}

void LogWriter::append(const QString &level, const QString &message, QJsonObject fields) {
    if (!m_ready) return;
    QFile file(m_filePath);
    if (!file.open(QIODevice::Append)) {
        m_ready = false;
        m_lastError = file.errorString();
        return;
    }
    fields.insert("timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    fields.insert("level", level);
    fields.insert("message", message);
    const QByteArray line = QJsonDocument(fields).toJson(QJsonDocument::Compact) + '\n';
    if (file.write(line) != line.size() || !file.flush()) {
        m_ready = false;
        m_lastError = file.errorString();
    }
}

void LogWriter::info(const QString &message) { append(QStringLiteral("INFO"), message); }
void LogWriter::warn(const QString &message) { append(QStringLiteral("WARN"), message); }
void LogWriter::error(const QString &message) { append(QStringLiteral("ERROR"), message); }

void LogWriter::command(const QString &program, const QStringList &arguments, int exitCode) {
    append(exitCode == 0 ? "INFO" : "ERROR", QStringLiteral("外部命令结束"),
           {{"program", program}, {"arguments", QJsonArray::fromStringList(arguments)}, {"exitCode", exitCode}});
}

}
