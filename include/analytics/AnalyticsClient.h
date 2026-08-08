#pragma once

#include "analytics/AnalyticsEvent.h"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVector>

class QNetworkAccessManager;

// Transport for analytics events: builds an ingest payload and POSTs it.
// Holds no policy — deciding what to send and when is Analytics' job.
//
// This is the ONLY vendor-specific class in the subsystem. buildBatch() and the
// endpoint path are the entire surface that would change to target a different
// analytics backend; everything else (identity, queueing, heartbeat, opt-out)
// is backend-agnostic by construction.
//
// buildBatch() is static and pure so the wire format is testable with no
// network and no event loop.
class AnalyticsClient : public QObject {
    Q_OBJECT

public:
    AnalyticsClient(QString host, QString apiKey, QString distinctId,
                    QObject *parent = nullptr);

    void setSuperProperties(const QVariantMap &props) { m_super = props; }

    // Fire-and-forget. A dead network resolves to batchFinished(false, ...);
    // it never blocks, never retries inline, and never surfaces to the user.
    void send(const QVector<AnalyticsEvent> &events);

    static QJsonObject buildBatch(const QString &apiKey,
                                  const QString &distinctId,
                                  const QVector<AnalyticsEvent> &events,
                                  const QVariantMap &superProps);

signals:
    // `sent` is echoed back so a failed batch can be requeued by the caller.
    void batchFinished(bool ok, const QVector<AnalyticsEvent> &sent);

private:
    QString                m_host;
    QString                m_apiKey;
    QString                m_distinctId;
    QVariantMap            m_super;
    QNetworkAccessManager *m_nam = nullptr;
};
