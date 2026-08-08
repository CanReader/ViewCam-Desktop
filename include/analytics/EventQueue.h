#pragma once

#include "analytics/AnalyticsEvent.h"

#include <QString>
#include <QVector>

// Bounded FIFO of pending analytics events, persisted to a JSON file so a
// crash, a quit, or an offline stretch does not lose them.
//
// Deliberately knows nothing about networking or any vendor: it is pure
// storage, which is what makes the eviction and persistence rules testable on
// their own. The cap matters — a machine offline for a month must not grow this
// file without bound, and analytics must never be the reason a disk fills up.
class EventQueue {
public:
    explicit EventQueue(QString filePath, int maxEvents = 200);

    // Appends one event, evicting the oldest if already at capacity.
    void append(const AnalyticsEvent &e);

    // Removes and returns up to `max` of the oldest events.
    QVector<AnalyticsEvent> take(int max);

    // Returns a previously taken batch to the head, preserving order. Called
    // when a POST fails so the batch is retried rather than silently dropped.
    void requeueFront(const QVector<AnalyticsEvent> &events);

    int size() const { return static_cast<int>(m_events.size()); }

    bool load();
    bool save() const;

private:
    QString                 m_filePath;
    int                     m_maxEvents;
    QVector<AnalyticsEvent> m_events;
};
