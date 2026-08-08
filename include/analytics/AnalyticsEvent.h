#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariantMap>

// One captured analytics event.
//
// A plain value type: no QObject, no I/O, no networking — so it is cheap to
// copy, safe to hand between threads, and testable on its own. The JSON form
// is the on-disk queue format, NOT the wire format; translating to a vendor
// payload is AnalyticsClient's job, which keeps the backend swappable.
struct AnalyticsEvent {
    QString     name;
    QVariantMap props;
    qint64      timestampMs = 0;

    QJsonObject toJson() const {
        QJsonObject o;
        o.insert(QStringLiteral("event"), name);
        o.insert(QStringLiteral("ts"), timestampMs);
        o.insert(QStringLiteral("props"), QJsonObject::fromVariantMap(props));
        return o;
    }

    static AnalyticsEvent fromJson(const QJsonObject &o) {
        AnalyticsEvent e;
        e.name = o.value(QStringLiteral("event")).toString();
        e.timestampMs =
            static_cast<qint64>(o.value(QStringLiteral("ts")).toDouble());
        e.props = o.value(QStringLiteral("props")).toObject().toVariantMap();
        return e;
    }
};
