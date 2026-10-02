#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QUrl>
#include <QVariant>

namespace qvw::backend::hypit {
class StudioWriter : public QObject {
    Q_OBJECT
public:
    enum WriteFailure { Conflict, Rejected, Indeterminate, InvalidRequest };
    Q_ENUM(WriteFailure)
    explicit StudioWriter(QObject *parent = nullptr);
    void adjustParameter(QUrl baseUrl, int revision, QString entityId, QString parameterId, QVariant value);
    void replaceSource(QUrl baseUrl, int revision, QString path, QString text);
    void cancel();
    void setTimeoutMs(int timeout) { m_timeoutMs = qMax(1, timeout); }
signals:
    void completed(int revision);
    void failed(WriteFailure kind, const QString &message, int httpStatus);
private:
    void write(const QUrl &baseUrl, int revision, const QByteArray &body, bool source);
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    quint64 m_generation = 0;
    int m_timeoutMs = 15000;
};
}
