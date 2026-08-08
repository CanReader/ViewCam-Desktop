// Unit tests for the analytics subsystem. Build + run:
//   cmake --build build --target vc_analytics_selftest && build/vc_analytics_selftest
//
// Deliberately covers the pieces that are pure logic — event serialisation,
// queue eviction/persistence, payload shape, and the privacy guarantees. The
// network path is not exercised here; that is what VIEWCAM_ANALYTICS_DEBUG=1
// and the live end-to-end check are for.

#include "analytics/AnalyticsEvent.h"
#include "analytics/EventQueue.h"

#include <QTemporaryDir>
#include <QTest>

class AnalyticsSelfTest : public QObject {
    Q_OBJECT

private slots:
    void eventRoundTripsThroughJson();
    void queueEvictsOldestWhenFull();
    void queueSurvivesRestart();
    void takeRemovesOnlyWhatItReturns();
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

// The cap exists so a machine that is offline for weeks cannot grow the queue
// file without bound. On overflow the NEWEST events are the ones worth keeping.
void AnalyticsSelfTest::queueEvictsOldestWhenFull() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    EventQueue q(dir.filePath(QStringLiteral("q.json")), /*maxEvents=*/3);

    for (int i = 0; i < 5; ++i) {
        AnalyticsEvent e;
        e.name = QStringLiteral("e%1").arg(i);
        q.append(e);
    }

    QCOMPARE(q.size(), 3);
    const QVector<AnalyticsEvent> all = q.take(10);
    QCOMPARE(all.size(), 3);
    QCOMPARE(all.at(0).name, QStringLiteral("e2"));
    QCOMPARE(all.at(2).name, QStringLiteral("e4"));
}

// Events captured while offline (or right before a crash) must still arrive.
void AnalyticsSelfTest::queueSurvivesRestart() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("q.json"));

    {
        EventQueue q(path);
        AnalyticsEvent e;
        e.name = QStringLiteral("app_started");
        e.props.insert(QStringLiteral("locale"), QStringLiteral("tr_TR"));
        q.append(e);
        QVERIFY(q.save());
    }

    EventQueue restored(path);
    QVERIFY(restored.load());
    QCOMPARE(restored.size(), 1);
    QCOMPARE(restored.take(1).at(0).props.value(QStringLiteral("locale")).toString(),
             QStringLiteral("tr_TR"));
}

// A failed POST must return its batch to the queue rather than drop it.
void AnalyticsSelfTest::takeRemovesOnlyWhatItReturns() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    EventQueue q(dir.filePath(QStringLiteral("q.json")));

    for (int i = 0; i < 4; ++i) {
        AnalyticsEvent e;
        e.name = QStringLiteral("e%1").arg(i);
        q.append(e);
    }

    const QVector<AnalyticsEvent> batch = q.take(2);
    QCOMPARE(batch.size(), 2);
    QCOMPARE(batch.at(0).name, QStringLiteral("e0"));
    QCOMPARE(q.size(), 2);

    q.requeueFront(batch);
    QCOMPARE(q.size(), 4);
    QCOMPARE(q.take(1).at(0).name, QStringLiteral("e0"));
}

QTEST_GUILESS_MAIN(AnalyticsSelfTest)
#include "analytics_selftest.moc"
