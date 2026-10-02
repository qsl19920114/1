#include "backend/hypit/StudioWriter.h"
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QTimer>
#include <cmath>
#include <limits>
#include <memory>

namespace qvw::backend::hypit {
namespace {
constexpr qsizetype responseLimit = 1024 * 1024;
bool isScalar(const QVariant &value) {
    switch (value.typeId()) {
    case QMetaType::Nullptr: case QMetaType::Bool: case QMetaType::QString: return true;
    case QMetaType::Int: case QMetaType::UInt: case QMetaType::LongLong: case QMetaType::ULongLong:
    case QMetaType::Long: case QMetaType::ULong: case QMetaType::Short: case QMetaType::UShort:
        return std::isfinite(value.toDouble()) && std::abs(value.toDouble()) <= 9007199254740991.0;
    case QMetaType::Float: case QMetaType::Double:
        return std::isfinite(value.toDouble());
    default: return false;
    }
}
bool isRelativeSource(const QString &path) {
    // Snapshot membership is enforced by the controller. Reject portable path
    // escapes here as well, including Windows paths when running on macOS.
    const QString portable = QString(path).replace('\\', '/');
    return !portable.isEmpty() && !QDir::isAbsolutePath(portable)
        && !portable.contains(':') && !portable.contains(QChar::Null)
        && !portable.split('/').contains("..");
}
struct WriteState {
    QByteArray data;
    bool timedOut = false;
    bool oversized = false;
};
}

StudioWriter::StudioWriter(QObject *parent) : QObject(parent) {
    m_network.setProxy(QNetworkProxy::NoProxy);
}

void StudioWriter::adjustParameter(QUrl baseUrl, int revision, QString entityId, QString parameterId, QVariant value) {
    if (entityId.isEmpty() || parameterId.isEmpty() || !isScalar(value)) {
        emit failed(InvalidRequest, QStringLiteral("属性写入需要组件、参数标识和有限的 JSON 标量值。"), 0);
        return;
    }
    const QJsonObject payload{{"type", "parameter.adjust"}, {"revision", revision},
        {"entityId", entityId}, {"parameterId", parameterId}, {"value", QJsonValue::fromVariant(value)}};
    write(baseUrl, revision, QJsonDocument(payload).toJson(QJsonDocument::Compact), false);
}

void StudioWriter::replaceSource(QUrl baseUrl, int revision, QString path, QString text) {
    if (!isRelativeSource(path)) {
        emit failed(InvalidRequest, QStringLiteral("源码写入需要会话中的相对路径。"), 0);
        return;
    }
    const QJsonObject payload{{"revision", revision}, {"path", path}, {"text", text}};
    write(baseUrl, revision, QJsonDocument(payload).toJson(QJsonDocument::Compact), true);
}

void StudioWriter::cancel() {
    ++m_generation;
    auto old = m_reply;
    m_reply = nullptr;
    // Cancellation only suppresses this client's result; it cannot undo a write
    // already accepted by Studio. The caller must refresh before another write.
    if (old) { old->abort(); old->deleteLater(); }
}

void StudioWriter::write(const QUrl &baseUrl, int revision, const QByteArray &body, bool source) {
    if (m_reply) {
        emit failed(InvalidRequest, QStringLiteral("已有 Studio 写入进行中，请等待完成。"), 0);
        return;
    }
    if (revision < 0 || !baseUrl.isValid() || baseUrl.scheme() != "http" || !baseUrl.userInfo().isEmpty()
        || (baseUrl.host() != "localhost" && baseUrl.host() != "127.0.0.1" && baseUrl.host() != "::1")) {
        emit failed(InvalidRequest, QStringLiteral("Studio 写入需要本机 HTTP 地址和有效 revision。"), 0);
        return;
    }
    QNetworkRequest request(baseUrl.resolved(QUrl(source ? "/__studio/source" : "/__studio/mutation")));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Accept", "application/json");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    auto *reply = source ? m_network.put(request, body) : m_network.post(request, body);
    reply->setReadBufferSize(responseLimit + 1);
    m_reply = reply;
    const quint64 generation = ++m_generation;
    auto state = std::make_shared<WriteState>();
    // An absolute deadline also bounds servers that continuously drip bytes.
    auto *deadline = new QTimer(reply);
    deadline->setSingleShot(true);
    connect(deadline, &QTimer::timeout, this, [this, reply, generation, state] {
        if (generation != m_generation || m_reply != reply) return;
        state->timedOut = true;
        reply->abort();
    });
    connect(reply, &QIODevice::readyRead, this, [this, reply, generation, state] {
        if (generation != m_generation || m_reply != reply) return;
        state->data += reply->read(responseLimit + 1 - state->data.size());
        if (state->data.size() > responseLimit) {
            state->oversized = true;
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, revision, source, state, deadline] {
        deadline->stop();
        reply->deleteLater();
        if (generation != m_generation || m_reply != reply) return;
        m_reply = nullptr; // Complete state transition before any reentrant user callback.
        if (reply->isOpen()) state->data += reply->read(responseLimit + 1 - state->data.size());
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto document = QJsonDocument::fromJson(state->data);
        const auto object = document.object();
        QString detail = object.value("error").toString();
        if (detail.isEmpty()) detail = QStringLiteral("HTTP %1：%2").arg(status).arg(reply->errorString());
        if (state->timedOut || state->oversized || state->data.size() > responseLimit) {
            emit failed(Indeterminate, state->timedOut
                ? QStringLiteral("Studio 写入超时，结果未知；请刷新会话，不能自动重试。")
                : QStringLiteral("Studio 写入响应超过 1 MiB，结果未知；请刷新会话。"), status);
            return;
        }
        // Qt sets a network error for HTTP 4xx/5xx. Classify the HTTP result
        // first so a 409/422 retains its documented recovery semantics.
        if (status == 409) { emit failed(Conflict, detail, status); return; }
        if (status == 422) { emit failed(source ? Indeterminate : Rejected, detail, status); return; }
        if (status >= 400 && status < 500) { emit failed(InvalidRequest, detail, status); return; }
        if (status != (source ? 202 : 200) || reply->error() != QNetworkReply::NoError) {
            emit failed(Indeterminate, detail, status);
            return;
        }
        const auto returned = object.value("revision");
        const double number = returned.toDouble(-1);
        // server.ts:454 returns the same revision when a mutation has no patches.
        if (!document.isObject() || !returned.isDouble() || !std::isfinite(number)
            || number < revision || number < 1 || number > std::numeric_limits<int>::max()
            || (source && number == revision)
            || std::floor(number) != number || object.contains("error")) {
            emit failed(Indeterminate, QStringLiteral("Studio 写入响应缺少有效 revision，结果未知；请刷新会话。"), status);
            return;
        }
        emit completed(static_cast<int>(number));
    });
    deadline->start(m_timeoutMs);
}
}
