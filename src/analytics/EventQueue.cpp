#include "analytics/EventQueue.h"
#include "core/Logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include <utility>

EventQueue::EventQueue(QString filePath, int maxEvents)
    : m_filePath(std::move(filePath))
    , m_maxEvents(maxEvents > 0 ? maxEvents : 1) {}

void EventQueue::append(const AnalyticsEvent &e) {
    m_events.append(e);
    // Drop from the front: on overflow the newest data is the useful data.
    while (m_events.size() > m_maxEvents)
        m_events.removeFirst();
}

QVector<AnalyticsEvent> EventQueue::take(int max) {
    if (max <= 0 || m_events.isEmpty())
        return {};
    const int n = qMin(max, static_cast<int>(m_events.size()));
    QVector<AnalyticsEvent> batch = m_events.mid(0, n);
    m_events.remove(0, n);
    return batch;
}

void EventQueue::requeueFront(const QVector<AnalyticsEvent> &events) {
    if (events.isEmpty())
        return;
    QVector<AnalyticsEvent> merged = events;
    merged.append(m_events);
    // Re-apply the cap after merging, still dropping oldest first.
    while (merged.size() > m_maxEvents)
        merged.removeFirst();
    m_events = merged;
}

bool EventQueue::load() {
    QFile f(m_filePath);
    if (!f.exists())
        return true; // Nothing buffered yet is a success, not a failure.
    if (!f.open(QIODevice::ReadOnly)) {
        VC_WARN("Analytics queue unreadable: {}", m_filePath.toStdString());
        return false;
    }
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        // A corrupt queue is discarded rather than retried forever — losing
        // buffered analytics is always preferable to wedging the pipeline.
        VC_WARN("Analytics queue corrupt, discarding: {}",
                err.errorString().toStdString());
        m_events.clear();
        return false;
    }
    m_events.clear();
    const QJsonArray arr = doc.array();
    for (const QJsonValue &v : arr) {
        if (v.isObject())
            m_events.append(AnalyticsEvent::fromJson(v.toObject()));
    }
    while (m_events.size() > m_maxEvents)
        m_events.removeFirst();
    return true;
}

bool EventQueue::save() const {
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());
    QJsonArray arr;
    for (const AnalyticsEvent &e : m_events)
        arr.append(e.toJson());

    // QSaveFile: write-then-rename, so a crash mid-write cannot leave a
    // truncated file that the next load() would then discard entirely.
    QSaveFile f(m_filePath);
    if (!f.open(QIODevice::WriteOnly)) {
        VC_WARN("Analytics queue not writable: {}", m_filePath.toStdString());
        return false;
    }
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    return f.commit();
}
