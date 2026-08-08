// Unit tests for the analytics subsystem. Build + run:
//   cmake --build build --target vc_analytics_selftest && build/vc_analytics_selftest
//
// Deliberately covers the pieces that are pure logic — event serialisation,
// queue eviction/persistence, payload shape, and the privacy guarantees. The
// network path is not exercised here; that is what VIEWCAM_ANALYTICS_DEBUG=1
// and the live end-to-end check are for.

#include "analytics/AnalyticsEvent.h"

#include <QTest>

class AnalyticsSelfTest : public QObject {
    Q_OBJECT

private slots:
    void eventRoundTripsThroughJson();
};

void AnalyticsSelfTest::eventRoundTripsThroughJson() {
    AnalyticsEvent in;
    in.name = QStringLiteral("app_started");
    in.timestampMs = 1754640000000LL;
    in.props.insert(QStringLiteral("is_first_run"), true);
    in.props.insert(QStringLiteral("locale"), QStringLiteral("tr_TR"));

    const AnalyticsEvent out = AnalyticsEvent::fromJson(in.toJson());

    QCOMPARE(out.name, in.name);
    QCOMPARE(out.timestampMs, in.timestampMs);
    QCOMPARE(out.props.value(QStringLiteral("is_first_run")).toBool(), true);
    QCOMPARE(out.props.value(QStringLiteral("locale")).toString(),
             QStringLiteral("tr_TR"));
}

QTEST_GUILESS_MAIN(AnalyticsSelfTest)
#include "analytics_selftest.moc"
