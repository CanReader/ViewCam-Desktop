#include "analytics/Analytics.h"
#include "ViewCamConfig.h"
#include "analytics/AnalyticsClient.h"
#include "analytics/EventQueue.h"
#include "core/Logger.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>
#include <QUuid>

namespace {

// 5 minutes gives "active right now" a 15-minute resolution while staying at
// ~288 events/device/day — well inside free-tier limits at 100x current scale.
constexpr int kHeartbeatMs = 5 * 60 * 1000;
constexpr int kFlushMs     = 30 * 1000;
constexpr int kBatchSize   = 20;
constexpr int kMaxQueued   = 200;

constexpr auto kEnabledKey   = "analytics/enabled";
constexpr auto kInstallIdKey = "analytics/installId";

QString queuePath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
           QStringLiteral("/analytics-queue.json");
}

bool debugOnlyMode() {
    return qEnvironmentVariable("VIEWCAM_ANALYTICS_DEBUG") == QLatin1String("1");
}

} // namespace

Analytics::Analytics(QObject *parent) : QObject(parent) {}
Analytics::~Analytics() = default;

Analytics &Analytics::instance() {
    static Analytics a;
    return a;
}

QString Analytics::resolveInstallId(QSettings &s) {
    QString id = s.value(QLatin1String(kInstallIdKey)).toString();
    if (id.isEmpty()) {
        // Random per install. NOT derived from hardware, MAC, or hostname, so
        // it cannot be correlated to a machine or across reinstalls.
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        s.setValue(QLatin1String(kInstallIdKey), id);
    }
    return id;
}

QVariantMap Analytics::buildSuperProperties() {
    QVariantMap p;
    p.insert(QStringLiteral("app_version"),
             QString::fromLatin1(VIEWCAM_VERSION_STRING));
    p.insert(QStringLiteral("os"), QSysInfo::productType());
    p.insert(QStringLiteral("os_version"), QSysInfo::productVersion());
    p.insert(QStringLiteral("arch"), QSysInfo::currentCpuArchitecture());
    p.insert(QStringLiteral("channel"),
             QString::fromLatin1(VIEWCAM_UPDATE_CHANNEL));
    return p;
}

void Analytics::init() {
    if (m_initialized)
        return;

    QSettings s;
    m_enabled   = s.value(QLatin1String(kEnabledKey), true).toBool();
    m_debugOnly = debugOnlyMode();

    const bool hadId =
        !s.value(QLatin1String(kInstallIdKey)).toString().isEmpty();
    m_installId = resolveInstallId(s);
    m_startedMs = QDateTime::currentMSecsSinceEpoch();

    m_queue = std::make_unique<EventQueue>(queuePath(), kMaxQueued);
    m_queue->load();

    m_client = new AnalyticsClient(QString::fromLatin1(VIEWCAM_ANALYTICS_HOST),
                                   QString::fromLatin1(VIEWCAM_ANALYTICS_KEY),
                                   m_installId, this);
    m_client->setSuperProperties(buildSuperProperties());
    connect(m_client, &AnalyticsClient::batchFinished, this,
            [this](bool ok, const QVector<AnalyticsEvent> &sent) {
                if (!m_queue)
                    return;
                if (!ok)
                    m_queue->requeueFront(sent); // Retry on the next flush.
                // Re-persist either way. Without this the pre-send snapshot
                // stays on disk after a SUCCESSFUL send, so every event would
                // be re-sent on the next launch and duplicate forever.
                m_queue->save();
            });

    m_initialized = true;

    if (!hadId)
        capture(QStringLiteral("app_installed"));

    capture(QStringLiteral("app_started"),
            {{QStringLiteral("is_first_run"), !hadId},
             {QStringLiteral("locale"), QLocale::system().name()}});

    m_heartbeat = new QTimer(this);
    m_heartbeat->setInterval(kHeartbeatMs);
    connect(m_heartbeat, &QTimer::timeout, this,
            [this]() { capture(QStringLiteral("app_heartbeat")); });
    if (m_enabled)
        m_heartbeat->start();

    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(kFlushMs);
    connect(m_flushTimer, &QTimer::timeout, this, [this]() { flush(); });
    m_flushTimer->start();

    VC_INFO("Analytics {} (install {})",
            m_enabled ? "enabled" : "disabled (user opted out)",
            m_installId.left(8).toStdString());
}

void Analytics::capture(const QString &event, const QVariantMap &props) {
    if (!m_initialized || !m_enabled || !m_queue)
        return;

    AnalyticsEvent e;
    e.name        = event;
    e.props       = props;
    e.timestampMs = QDateTime::currentMSecsSinceEpoch();
    m_queue->append(e);

    if (m_debugOnly) {
        VC_INFO("Analytics [debug] {}", QJsonDocument(e.toJson())
                                            .toJson(QJsonDocument::Compact)
                                            .toStdString());
        return;
    }
    if (m_queue->size() >= kBatchSize)
        flush();
}

void Analytics::flush() {
    if (!m_initialized || !m_enabled || m_debugOnly || !m_queue || !m_client)
        return;

    // Persist BEFORE sending, not just on shutdown: a crash is exactly the
    // case the disk queue exists for, and shutdown() never runs then. The
    // matching re-save on batchFinished is what clears sent events from disk.
    //
    // The remaining trade-off is deliberate and narrow: crashing between
    // take() and the reply leaves those events on disk, so they send again
    // next launch. Duplicates are cheap — active-user counts are
    // unique-by-distinct_id and unaffected — whereas losing the events that
    // immediately preceded a crash loses exactly the data worth having.
    m_queue->save();

    const QVector<AnalyticsEvent> batch = m_queue->take(kBatchSize);
    if (!batch.isEmpty())
        m_client->send(batch);
}

void Analytics::setEnabled(bool on) {
    if (m_enabled == on)
        return;
    m_enabled = on;
    QSettings().setValue(QLatin1String(kEnabledKey), on);

    if (!on) {
        // Opting out drops the backlog: an opt-out that still uploads what it
        // already buffered is not an opt-out.
        if (m_heartbeat)
            m_heartbeat->stop();
        if (m_queue) {
            m_queue->take(kMaxQueued);
            m_queue->save();
        }
        VC_INFO("Analytics disabled by user; pending events discarded");
    } else {
        if (m_heartbeat)
            m_heartbeat->start();
        VC_INFO("Analytics enabled by user");
    }
}

int Analytics::pendingCount() const {
    return m_queue ? m_queue->size() : 0;
}

void Analytics::shutdown() {
    if (!m_initialized)
        return;
    if (m_heartbeat)
        m_heartbeat->stop();
    if (m_flushTimer)
        m_flushTimer->stop();

    if (m_enabled) {
        const qint64 secs =
            (QDateTime::currentMSecsSinceEpoch() - m_startedMs) / 1000;
        capture(QStringLiteral("app_exited"),
                {{QStringLiteral("session_seconds"), secs}});
    }
    // Persist rather than send: a POST on the quit path would either be
    // cancelled or delay exit. The queue flushes on the next launch.
    if (m_queue)
        m_queue->save();
    m_initialized = false;
}
