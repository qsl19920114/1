#include "backend/hypit/StudioClient.h"
#include "backend/hypit/SnapshotMapper.h"
#include <QNetworkProxy>
#include <QJsonDocument>
#include <QJsonObject>
#include <memory>

namespace qvw::backend::hypit {
StudioClient::StudioClient(QObject *parent) : QObject(parent) {
    m_network.setProxy(QNetworkProxy::NoProxy);
}
void StudioClient::cancel() {
    ++m_generation;
    auto old = m_reply;
    m_reply = nullptr; // A cancelled/previous request must never publish stale state.
    if (old) { old->abort(); old->deleteLater(); }
}
void StudioClient::fetchSession(const QUrl &baseUrl) {
    cancel();
    if (!baseUrl.isValid() || baseUrl.scheme() != "http" || !baseUrl.userInfo().isEmpty()
        || (baseUrl.host() != "localhost" && baseUrl.host() != "127.0.0.1" && baseUrl.host() != "::1")) {
        emit failed(QStringLiteral("Studio 地址必须是本机 HTTP 地址。"));
        return;
    }
    QNetworkRequest request(baseUrl.resolved(QUrl("/__studio/session")));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(m_timeoutMs);
    request.setRawHeader("Accept", "application/json");
    auto *reply = m_network.get(request);
    const quint64 generation = m_generation;
    m_reply = reply;
    auto data = std::make_shared<QByteArray>();
    connect(reply, &QIODevice::readyRead, this, [this, reply, data] {
        if (m_reply != reply) return;
        *data += reply->readAll();
        if (data->size() > 16 * 1024 * 1024) {
            cancel();
            emit failed(QStringLiteral("Studio 会话响应超过 16 MiB，已停止读取。"));
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, data, generation] {
        reply->deleteLater();
        if (m_reply != reply) return;
        m_reply = nullptr;
        *data += reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status != 200 || reply->error() != QNetworkReply::NoError) {
            const QString serverError = QJsonDocument::fromJson(*data).object().value("error").toString();
            emit failed(QStringLiteral("读取 Studio 会话失败（HTTP %1）：%2").arg(status)
                .arg(serverError.isEmpty() ? reply->errorString() : serverError));
            return;
        }
        const auto mapped = mapSessionPayload(*data);
        if (!mapped.ok()) { emit failed(mapped.error); return; }
        emit payloadReceived(*data);
        if (generation != m_generation) return;
        emit snapshotReady(mapped.snapshot);
    });
}
}
