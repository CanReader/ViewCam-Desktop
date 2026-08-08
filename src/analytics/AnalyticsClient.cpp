#include "analytics/AnalyticsClient.h"
#include "core/Logger.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimeZone>
#include <QUrl>

#include <utility>

namespace {
// Short by design: analytics must never hold a socket long enough to matter.
constexpr int kRequestTimeoutMs = 10000;
} // namespace

AnalyticsClient::AnalyticsClient(QString host, QString apiKey,
                                 QString distinctId, QObject *parent)
    : QObject(parent)
    , m_host(std::move(host))
    , m_apiKey(std::move(apiKey))
    , m_distinctId(std::move(distinctId))
    , m_nam(new QNetworkAccessManager(this)) {}

QJsonObject AnalyticsClient::buildBatch(const QString &apiKey,
                                        const QString &distinctId,
                                        const QVector<AnalyticsEvent> &events,
                                        const QVariantMap &superProps) {
    QJsonArray batch;
    for (const AnalyticsEvent &e : events) {
        // Merge order matters: event props first, then super-properties, then
        // distinct_id last — so neither a caller's stray key nor a super
        // property can reassign identity and corrupt user counts.
        QVariantMap merged = e.props;
        for (auto it = superProps.cbegin(); it != superProps.cend(); ++it)
            merged.insert(it.key(), it.value());
        merged.insert(QStringLiteral("distinct_id"), distinctId);

        QJsonObject ev;
        ev.insert(QStringLiteral("event"), e.name);
        ev.insert(QStringLiteral("properties"),
                  QJsonObject::fromVariantMap(merged));
        ev.insert(QStringLiteral("timestamp"),
                  QDateTime::fromMSecsSinceEpoch(e.timestampMs, QTimeZone::UTC)
                      .toString(Qt::ISODateWithMs));
        batch.append(ev);
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("api_key"), apiKey);
    payload.insert(QStringLiteral("batch"), batch);
    return payload;
}

void AnalyticsClient::send(const QVector<AnalyticsEvent> &events) {
    // No key configured (dev builds) is a successful no-op, not a failure —
    // returning false here would requeue forever and grow the queue file.
    if (events.isEmpty() || m_apiKey.isEmpty() || m_host.isEmpty()) {
        emit batchFinished(true, events);
        return;
    }

    const QJsonObject payload =
        buildBatch(m_apiKey, m_distinctId, events, m_super);

    QNetworkRequest req{QUrl(m_host + QStringLiteral("/batch/"))};
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));
    req.setTransferTimeout(kRequestTimeoutMs);
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                     QNetworkRequest::AlwaysNetwork);

    QNetworkReply *reply =
        m_nam->post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply, events]() {
        const bool ok = reply->error() == QNetworkReply::NoError;
        if (!ok) {
            VC_DEBUG("Analytics batch failed ({} events): {}", events.size(),
                     reply->errorString().toStdString());
        }
        reply->deleteLater();
        emit batchFinished(ok, events);
    });
}
