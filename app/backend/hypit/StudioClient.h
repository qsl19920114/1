#pragma once
#include "domain/Snapshot.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QUrl>

namespace qvw::backend::hypit {
class StudioClient : public QObject {
    Q_OBJECT
public:
    explicit StudioClient(QObject *parent = nullptr);
    void fetchSession(const QUrl &baseUrl);
    void cancel();
    void setTimeoutMs(int timeout) { m_timeoutMs = timeout; }
signals:
    void snapshotReady(const qvw::domain::Snapshot &snapshot);
    void payloadReceived(const QByteArray &payload);
    void failed(const QString &message);
private:
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    int m_timeoutMs = 15000;
    quint64 m_generation = 0;
};
}
