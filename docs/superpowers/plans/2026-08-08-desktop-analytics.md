# Desktop Analytics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give ViewCam Desktop a Play-Console-equivalent analytics dashboard — active users right now / daily / monthly, installs, country breakdown, version adoption, and first-run failure reasons.

**Architecture:** A new `analytics/` subsystem in the Desktop app: a facade singleton (`Analytics`) that gameplay/UI code calls, a disk-backed `EventQueue` that survives crashes and offline periods, and an `AnalyticsClient` that batch-POSTs events to PostHog's `/batch/` endpoint over plain `QNetworkAccessManager`. No third-party C++ SDK is added. A 5-minute heartbeat timer is what makes "active right now" and DAU/MAU computable.

**Tech Stack:** C++17, Qt 6 (Core, Network, Qml), CMake 3.20+, QTest for unit tests, PostHog Cloud EU as the ingest backend.

## Global Constraints

- **C++17** — enforced by `set(CMAKE_CXX_STANDARD 17)` in `CMakeLists.txt:8`. No C++20 features.
- **Subsystem is named `Analytics`, never `Telemetry`.** `SettingsViewModel` already exposes a `telemetryOverlay` property (the on-screen latency/bitrate HUD, `SettingsViewModel.h:21`). Reusing "telemetry" collides inside the same class.
- **Headers mirror sources:** `include/analytics/Foo.h` for `src/analytics/Foo.cpp`. `CMAKE_AUTOMOC` is ON, so any `Q_OBJECT` header must be listed in the build.
- **Analytics failure must never affect the app.** Every public entry point is wrapped so an exception, a null pointer, or a dead network is swallowed. No blocking calls on the GUI thread, ever.
- **All methods no-op until `Analytics::init()` has run and when disabled.** Callers must be safe to invoke from any thread and any error path.
- **Never collect:** hostnames, MAC addresses, local IP addresses, file paths, frame/audio data, or **device names from `DeviceDiscovery`** (wire format `VIEWCAM|<name>|<port>`, where `<name>` is routinely a person's name).
- **QSettings keys:** `analytics/enabled` (bool, default `true`), `analytics/installId` (QString UUID), `analytics/lastSeenVersion` (QString).
- **Env override:** `VIEWCAM_ANALYTICS_DEBUG=1` logs payloads via `VC_INFO` instead of sending them.
- **Logging** uses the existing `VC_INFO` / `VC_WARN` / `VC_DEBUG` macros from `core/Logger.h`.
- **Commits:** subject line only. No body, no co-author trailer, no AI attribution.
- **Branch:** all work happens on a branch off `main` in the `Desktop/` repo — do not commit directly to `main`.

---

## File Structure

| File | Responsibility |
|---|---|
| `include/analytics/AnalyticsEvent.h` (create) | Plain value struct for one event + JSON round-trip. No Qt object, no I/O. |
| `include/analytics/EventQueue.h` / `src/analytics/EventQueue.cpp` (create) | Bounded FIFO with disk persistence. No networking. |
| `include/analytics/AnalyticsClient.h` / `src/analytics/AnalyticsClient.cpp` (create) | Owns `QNetworkAccessManager`, builds the PostHog batch payload, POSTs it. No policy decisions. |
| `include/analytics/Analytics.h` / `src/analytics/Analytics.cpp` (create) | Facade singleton: install ID, enabled flag, super-properties, heartbeat timer, public `capture()`. The only class the rest of the app talks to. |
| `tools/analytics_selftest.cpp` (create) | QTest suite for the three units above. |
| `CMakeLists.txt` (modify) | Add sources to `SOURCES`, add the `vc_analytics_selftest` target. |
| `cmake/ViewCamConfig.h.in` (modify) | Add `VIEWCAM_ANALYTICS_HOST` / `VIEWCAM_ANALYTICS_KEY` defines. |
| `src/main.cpp` (modify) | Call `Analytics::init()` / `shutdown()`. |
| `include/viewmodels/SettingsViewModel.h` + `.cpp` (modify) | Add the `analyticsEnabled` property. |
| `qml/screens/SettingsPage.qml` (modify) | Add the opt-out row. |

---

### Task 1: Event value type

**Files:**
- Create: `Desktop/include/analytics/AnalyticsEvent.h`
- Create: `Desktop/tools/analytics_selftest.cpp`
- Modify: `Desktop/CMakeLists.txt` (add `vc_analytics_selftest` target after the `vc_proto_selftest` block, ~line 743)

**Interfaces:**
- Consumes: nothing.
- Produces: `struct AnalyticsEvent { QString name; QVariantMap props; qint64 timestampMs; QJsonObject toJson() const; static AnalyticsEvent fromJson(const QJsonObject&); }`

- [ ] **Step 1: Write the failing test**

Create `Desktop/tools/analytics_selftest.cpp`:

```cpp
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
```

Add to `Desktop/CMakeLists.txt`, directly after the `vc_proto_selftest` `endif()` (~line 743):

```cmake
# ── Analytics unit tests ─────────────────────────────────────
# Build+run:  cmake --build build --target vc_analytics_selftest
#             && build/vc_analytics_selftest
find_package(Qt6 QUIET COMPONENTS Test)
if(Qt6Test_FOUND)
    add_executable(vc_analytics_selftest
        tools/analytics_selftest.cpp
        src/analytics/EventQueue.cpp      include/analytics/EventQueue.h
        include/analytics/AnalyticsEvent.h
        src/core/Logger.cpp
    )
    target_include_directories(vc_analytics_selftest PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/generated)
    target_link_libraries(vc_analytics_selftest PRIVATE
        Qt6::Core Qt6::Network Qt6::Test)
    if(UNIX AND SPDLOG_FOUND)
        target_include_directories(vc_analytics_selftest PRIVATE ${SPDLOG_INCLUDE_DIRS})
        target_link_libraries(vc_analytics_selftest PRIVATE ${SPDLOG_LIBRARIES})
    else()
        target_link_libraries(vc_analytics_selftest PRIVATE spdlog::spdlog)
    endif()
endif()
```

Note: `EventQueue.cpp` is referenced here but created in Task 2. Comment those two source lines out for this task, and uncomment them in Task 2 Step 3.

- [ ] **Step 2: Run test to verify it fails**

```bash
cd Desktop && cmake --build build --target vc_analytics_selftest
```
Expected: FAIL — `fatal error: analytics/AnalyticsEvent.h: No such file or directory`

- [ ] **Step 3: Write minimal implementation**

Create `Desktop/include/analytics/AnalyticsEvent.h`:

```cpp
#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariantMap>

// One captured analytics event. A plain value type: no Qt object, no I/O, no
// networking — so it is trivially copyable across threads and testable alone.
struct AnalyticsEvent {
    QString    name;
    QVariantMap props;
    qint64     timestampMs = 0;

    QJsonObject toJson() const {
        QJsonObject o;
        o.insert(QStringLiteral("event"), name);
        o.insert(QStringLiteral("ts"), timestampMs);
        o.insert(QStringLiteral("props"), QJsonObject::fromVariantMap(props));
        return o;
    }

    static AnalyticsEvent fromJson(const QJsonObject &o) {
        AnalyticsEvent e;
        e.name        = o.value(QStringLiteral("event")).toString();
        e.timestampMs = static_cast<qint64>(o.value(QStringLiteral("ts")).toDouble());
        e.props       = o.value(QStringLiteral("props")).toObject().toVariantMap();
        return e;
    }
};
```

- [ ] **Step 4: Run test to verify it passes**

```bash
cd Desktop && cmake --build build --target vc_analytics_selftest && ./build/vc_analytics_selftest
```
Expected: PASS — `1 passed, 0 failed, 0 skipped`

- [ ] **Step 5: Commit**

```bash
git add include/analytics/AnalyticsEvent.h tools/analytics_selftest.cpp CMakeLists.txt
git commit -m "feat(analytics): add AnalyticsEvent value type and test target"
```

---

### Task 2: Bounded, disk-backed event queue

**Files:**
- Create: `Desktop/include/analytics/EventQueue.h`, `Desktop/src/analytics/EventQueue.cpp`
- Modify: `Desktop/tools/analytics_selftest.cpp`
- Modify: `Desktop/CMakeLists.txt` (uncomment the two `EventQueue` source lines from Task 1)

**Interfaces:**
- Consumes: `AnalyticsEvent` from Task 1.
- Produces:
  - `explicit EventQueue(QString filePath, int maxEvents = 200)`
  - `void append(const AnalyticsEvent &e)`
  - `QVector<AnalyticsEvent> take(int max)` — removes and returns up to `max` oldest events
  - `void requeueFront(const QVector<AnalyticsEvent> &events)` — puts failed events back at the head
  - `int size() const`
  - `bool load()` / `bool save() const`

- [ ] **Step 1: Write the failing test**

Append these slots to `AnalyticsSelfTest` in `Desktop/tools/analytics_selftest.cpp` (add `#include "analytics/EventQueue.h"`, `#include <QTemporaryDir>` at the top, and declare the slots in the `private slots:` block):

```cpp
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
    // Oldest two (e0, e1) were dropped; the newest three survive in order.
    QCOMPARE(all.size(), 3);
    QCOMPARE(all.at(0).name, QStringLiteral("e2"));
    QCOMPARE(all.at(2).name, QStringLiteral("e4"));
}

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

    // A failed POST must not lose the batch.
    q.requeueFront(batch);
    QCOMPARE(q.size(), 4);
    QCOMPARE(q.take(1).at(0).name, QStringLiteral("e0"));
}
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd Desktop && cmake --build build --target vc_analytics_selftest
```
Expected: FAIL — `fatal error: analytics/EventQueue.h: No such file or directory`

- [ ] **Step 3: Write minimal implementation**

Uncomment the two `EventQueue` lines in the `vc_analytics_selftest` target in `CMakeLists.txt`.

Create `Desktop/include/analytics/EventQueue.h`:

```cpp
#pragma once

#include "analytics/AnalyticsEvent.h"

#include <QString>
#include <QVector>

// Bounded FIFO of pending analytics events, persisted to a JSON file so a
// crash, a quit, or an offline stretch does not lose them.
//
// Deliberately knows nothing about networking or PostHog: it is pure storage,
// which is what makes the eviction and persistence rules testable on their own.
// The cap matters — a machine that is offline for a month must not grow this
// file without bound.
class EventQueue {
public:
    explicit EventQueue(QString filePath, int maxEvents = 200);

    // Appends one event, evicting the oldest if already at capacity.
    void append(const AnalyticsEvent &e);

    // Removes and returns up to `max` of the oldest events.
    QVector<AnalyticsEvent> take(int max);

    // Returns a previously taken batch to the head, preserving order. Called
    // when a POST fails so the batch is retried rather than dropped.
    void requeueFront(const QVector<AnalyticsEvent> &events);

    int size() const { return m_events.size(); }

    bool load();
    bool save() const;

private:
    QString                 m_filePath;
    int                     m_maxEvents;
    QVector<AnalyticsEvent> m_events;
};
```

Create `Desktop/src/analytics/EventQueue.cpp`:

```cpp
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
    const int n = qMin(max, m_events.size());
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
    // truncated queue file that load() would then discard entirely.
    QSaveFile f(m_filePath);
    if (!f.open(QIODevice::WriteOnly)) {
        VC_WARN("Analytics queue not writable: {}", m_filePath.toStdString());
        return false;
    }
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    return f.commit();
}
```

- [ ] **Step 4: Run test to verify it passes**

```bash
cd Desktop && cmake --build build --target vc_analytics_selftest && ./build/vc_analytics_selftest
```
Expected: PASS — 4 passed, 0 failed

- [ ] **Step 5: Commit**

```bash
git add include/analytics/EventQueue.h src/analytics/EventQueue.cpp tools/analytics_selftest.cpp CMakeLists.txt
git commit -m "feat(analytics): add bounded disk-backed event queue"
```

---

### Task 3: PostHog batch payload builder

**Files:**
- Create: `Desktop/include/analytics/AnalyticsClient.h`, `Desktop/src/analytics/AnalyticsClient.cpp`
- Modify: `Desktop/tools/analytics_selftest.cpp`
- Modify: `Desktop/CMakeLists.txt` (add `AnalyticsClient` to both the main `SOURCES` list and the selftest target)
- Modify: `Desktop/cmake/ViewCamConfig.h.in`

**Interfaces:**
- Consumes: `AnalyticsEvent` (Task 1), `EventQueue` (Task 2).
- Produces:
  - `static QJsonObject AnalyticsClient::buildBatch(const QString &apiKey, const QString &distinctId, const QVector<AnalyticsEvent> &events, const QVariantMap &superProps)`
  - `void AnalyticsClient::send(const QVector<AnalyticsEvent> &events)`
  - signal `void batchFinished(bool ok, const QVector<AnalyticsEvent> &sent)`

The payload builder is deliberately `static` and pure so the wire format is testable without a network.

- [ ] **Step 1: Write the failing test**

Add to `Desktop/tools/analytics_selftest.cpp` (add `#include "analytics/AnalyticsClient.h"`, `#include <QJsonArray>`, and declare the slot):

```cpp
void AnalyticsSelfTest::batchMatchesPostHogWireFormat() {
    AnalyticsEvent e;
    e.name = QStringLiteral("app_heartbeat");
    e.timestampMs = 1754640000000LL;
    e.props.insert(QStringLiteral("streaming"), true);

    QVariantMap super;
    super.insert(QStringLiteral("app_version"), QStringLiteral("1.2.1"));
    super.insert(QStringLiteral("os"), QStringLiteral("linux"));

    const QJsonObject batch = AnalyticsClient::buildBatch(
        QStringLiteral("phc_testkey"),
        QStringLiteral("11111111-2222-3333-4444-555555555555"),
        {e}, super);

    QCOMPARE(batch.value(QStringLiteral("api_key")).toString(),
             QStringLiteral("phc_testkey"));

    const QJsonArray arr = batch.value(QStringLiteral("batch")).toArray();
    QCOMPARE(arr.size(), 1);

    const QJsonObject ev = arr.at(0).toObject();
    QCOMPARE(ev.value(QStringLiteral("event")).toString(),
             QStringLiteral("app_heartbeat"));

    const QJsonObject props = ev.value(QStringLiteral("properties")).toObject();
    QCOMPARE(props.value(QStringLiteral("distinct_id")).toString(),
             QStringLiteral("11111111-2222-3333-4444-555555555555"));
    // Super-properties are merged into every event.
    QCOMPARE(props.value(QStringLiteral("app_version")).toString(),
             QStringLiteral("1.2.1"));
    QCOMPARE(props.value(QStringLiteral("streaming")).toBool(), true);
    // ISO-8601 timestamp, which is what PostHog expects.
    QVERIFY(ev.value(QStringLiteral("timestamp")).toString().endsWith(QLatin1Char('Z')));
}

void AnalyticsSelfTest::eventPropsNeverOverrideDistinctId() {
    // A caller passing a stray distinct_id must not be able to reassign
    // identity — that would silently merge or split users in the dashboard.
    AnalyticsEvent e;
    e.name = QStringLiteral("stream_connected");
    e.props.insert(QStringLiteral("distinct_id"), QStringLiteral("attacker"));

    const QJsonObject batch = AnalyticsClient::buildBatch(
        QStringLiteral("phc_testkey"), QStringLiteral("real-id"), {e}, {});
    const QJsonObject props =
        batch.value(QStringLiteral("batch")).toArray().at(0).toObject()
             .value(QStringLiteral("properties")).toObject();

    QCOMPARE(props.value(QStringLiteral("distinct_id")).toString(),
             QStringLiteral("real-id"));
}
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd Desktop && cmake --build build --target vc_analytics_selftest
```
Expected: FAIL — `fatal error: analytics/AnalyticsClient.h: No such file or directory`

- [ ] **Step 3: Write minimal implementation**

Add to `Desktop/cmake/ViewCamConfig.h.in` (next to the existing update defines, ~line 38):

```c
#define VIEWCAM_ANALYTICS_HOST  "@VIEWCAM_ANALYTICS_HOST@"
#define VIEWCAM_ANALYTICS_KEY   "@VIEWCAM_ANALYTICS_KEY@"
```

Add to `Desktop/CMakeLists.txt` next to the other endpoint cache vars (~line 34, after `VIEWCAM_UPDATE_MANIFEST_BASE`):

```cmake
# ── Analytics ingest (PostHog Cloud EU) ──────────────────────
# Project API key is a publishable, write-only key — safe to embed in the
# client, same as any web analytics snippet. An empty key disables analytics
# entirely, which is what local/dev builds get by default.
set(VIEWCAM_ANALYTICS_HOST "https://eu.i.posthog.com"
    CACHE STRING "Analytics ingest base URL")
set(VIEWCAM_ANALYTICS_KEY ""
    CACHE STRING "PostHog project API key (empty disables analytics)")
```

Add `src/analytics/AnalyticsClient.cpp` and `src/analytics/EventQueue.cpp` to the main `SOURCES` list in `CMakeLists.txt` (after `src/updater/UpdateChecker.cpp`, ~line 189), and add `AnalyticsClient.cpp` to the `vc_analytics_selftest` target sources.

Create `Desktop/include/analytics/AnalyticsClient.h`:

```cpp
#pragma once

#include "analytics/AnalyticsEvent.h"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVector>

class QNetworkAccessManager;

// Transport for analytics events: builds a PostHog `/batch/` payload and POSTs
// it. Holds no policy — what to send and when is Analytics' job.
//
// buildBatch() is static and pure so the wire format is testable without a
// network or an event loop.
class AnalyticsClient : public QObject {
    Q_OBJECT

public:
    AnalyticsClient(QString host, QString apiKey, QString distinctId,
                    QObject *parent = nullptr);

    void setSuperProperties(const QVariantMap &props) { m_super = props; }

    // Fire-and-forget. A dead network resolves to batchFinished(false, ...).
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
```

Create `Desktop/src/analytics/AnalyticsClient.cpp`:

```cpp
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
// PostHog rejects a batch outright if any single event is malformed, so the
// builder is defensive rather than clever.
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
        // Order matters: event props first, then super-properties, then
        // distinct_id last — so neither a caller's stray key nor a super
        // property can reassign identity.
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
                      .toString(Qt::ISODate));
        batch.append(ev);
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("api_key"), apiKey);
    payload.insert(QStringLiteral("batch"), batch);
    return payload;
}

void AnalyticsClient::send(const QVector<AnalyticsEvent> &events) {
    if (events.isEmpty() || m_apiKey.isEmpty()) {
        emit batchFinished(true, events);
        return;
    }

    const QJsonObject payload =
        buildBatch(m_apiKey, m_distinctId, events, m_super);

    QNetworkRequest req{QUrl(m_host + QStringLiteral("/batch/"))};
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));
    req.setTransferTimeout(kRequestTimeoutMs);
    // Analytics must never keep the app alive or delay a quit.
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
```

- [ ] **Step 4: Run test to verify it passes**

```bash
cd Desktop && cmake --build build --target vc_analytics_selftest && ./build/vc_analytics_selftest
```
Expected: PASS — 6 passed, 0 failed

- [ ] **Step 5: Commit**

```bash
git add include/analytics/AnalyticsClient.h src/analytics/AnalyticsClient.cpp tools/analytics_selftest.cpp CMakeLists.txt cmake/ViewCamConfig.h.in
git commit -m "feat(analytics): add PostHog batch client"
```

---

### Task 4: Analytics facade — install ID, opt-out, heartbeat

**Files:**
- Create: `Desktop/include/analytics/Analytics.h`, `Desktop/src/analytics/Analytics.cpp`
- Modify: `Desktop/tools/analytics_selftest.cpp`
- Modify: `Desktop/CMakeLists.txt` (add `Analytics.cpp` to `SOURCES` and to the selftest target)

**Interfaces:**
- Consumes: `EventQueue` (Task 2), `AnalyticsClient` (Task 3).
- Produces:
  - `static Analytics &Analytics::instance()`
  - `void init()` — reads settings, resolves install ID, starts heartbeat, emits `app_installed` on first run and `app_started` always
  - `void shutdown()` — emits `app_exited`, flushes queue to disk
  - `void capture(const QString &event, const QVariantMap &props = {})`
  - `void setEnabled(bool)` / `bool isEnabled() const`
  - `static QString resolveInstallId(QSettings &s)` — pure enough to test
  - `static QVariantMap buildSuperProperties()`

- [ ] **Step 1: Write the failing test**

Add to `Desktop/tools/analytics_selftest.cpp` (add `#include "analytics/Analytics.h"`, `#include <QSettings>`, and declare the slots):

```cpp
void AnalyticsSelfTest::installIdIsStableAcrossCalls() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings s(dir.filePath(QStringLiteral("s.ini")), QSettings::IniFormat);

    const QString first  = Analytics::resolveInstallId(s);
    const QString second = Analytics::resolveInstallId(s);

    QVERIFY(!first.isEmpty());
    QCOMPARE(first, second);
    // A bare UUID with no braces — PostHog distinct_ids should stay clean.
    QVERIFY(!first.contains(QLatin1Char('{')));
    QCOMPARE(first.length(), 36);
}

void AnalyticsSelfTest::superPropertiesCarryNoIdentifyingData() {
    const QVariantMap p = Analytics::buildSuperProperties();

    QVERIFY(p.contains(QStringLiteral("app_version")));
    QVERIFY(p.contains(QStringLiteral("os")));
    QVERIFY(p.contains(QStringLiteral("arch")));

    // The spec forbids these outright. Guard the whole map, not a sample.
    const QStringList forbidden{
        QStringLiteral("hostname"), QStringLiteral("host_name"),
        QStringLiteral("machine"),  QStringLiteral("mac"),
        QStringLiteral("ip"),       QStringLiteral("user"),
        QStringLiteral("username"), QStringLiteral("path"),
        QStringLiteral("device_name")};
    for (const QString &key : forbidden)
        QVERIFY2(!p.contains(key), qPrintable(QStringLiteral("leaked: ") + key));

    // No value may equal the machine hostname, whatever the key is called.
    const QString host = QSysInfo::machineHostName();
    for (auto it = p.cbegin(); it != p.cend(); ++it)
        QVERIFY(it.value().toString() != host);
}

void AnalyticsSelfTest::disabledInstanceCapturesNothing() {
    Analytics &a = Analytics::instance();
    a.setEnabled(false);
    a.capture(QStringLiteral("app_heartbeat"));
    QCOMPARE(a.pendingCount(), 0);
}
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd Desktop && cmake --build build --target vc_analytics_selftest
```
Expected: FAIL — `fatal error: analytics/Analytics.h: No such file or directory`

- [ ] **Step 3: Write minimal implementation**

Create `Desktop/include/analytics/Analytics.h`:

```cpp
#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

#include <memory>

class AnalyticsClient;
class EventQueue;
class QSettings;
class QTimer;

// Process-wide analytics facade — the only class the rest of the app calls.
//
// Every method is a no-op until init() has run and whenever the user has opted
// out, so callers may invoke it from any error path without guarding. Failures
// are swallowed by design: analytics must never be able to take down the app.
class Analytics : public QObject {
    Q_OBJECT

public:
    static Analytics &instance();

    // Reads settings, resolves the install ID, emits app_installed (first run
    // only) then app_started, and starts the heartbeat. Safe to call twice.
    void init();

    // Emits app_exited with session duration and flushes the queue to disk.
    void shutdown();

    void capture(const QString &event, const QVariantMap &props = {});

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool on);

    // Number of events buffered but not yet sent. Test seam.
    int pendingCount() const;

    // Returns the persistent per-install UUID, generating and storing one on
    // first call. Bare 36-char form, no braces.
    static QString resolveInstallId(QSettings &s);

    // Version / OS / arch / channel. Contains nothing that identifies a
    // machine or a person — enforced by test.
    static QVariantMap buildSuperProperties();

private:
    explicit Analytics(QObject *parent = nullptr);
    ~Analytics() override;

    void flush();

    bool                            m_enabled     = false;
    bool                            m_initialized = false;
    bool                            m_debugOnly   = false;
    qint64                          m_startedMs   = 0;
    QString                         m_installId;
    std::unique_ptr<EventQueue>     m_queue;
    AnalyticsClient                *m_client    = nullptr;
    QTimer                         *m_heartbeat = nullptr;
    QTimer                         *m_flushTimer = nullptr;
};
```

Create `Desktop/src/analytics/Analytics.cpp`:

```cpp
#include "analytics/Analytics.h"
#include "ViewCamConfig.h"
#include "analytics/AnalyticsClient.h"
#include "analytics/EventQueue.h"
#include "core/Logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>
#include <QUuid>

namespace {

constexpr int  kHeartbeatMs  = 5 * 60 * 1000; // 5 min — 15-min "active now"
constexpr int  kFlushMs      = 30 * 1000;
constexpr int  kBatchSize    = 20;
constexpr int  kMaxQueued    = 200;
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
        // Random per install. Not derived from hardware, MAC, or hostname, so
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

    const bool hadId = !s.value(QLatin1String(kInstallIdKey)).toString().isEmpty();
    m_installId      = resolveInstallId(s);
    m_startedMs      = QDateTime::currentMSecsSinceEpoch();

    m_queue = std::make_unique<EventQueue>(queuePath(), kMaxQueued);
    m_queue->load();

    m_client = new AnalyticsClient(QString::fromLatin1(VIEWCAM_ANALYTICS_HOST),
                                   QString::fromLatin1(VIEWCAM_ANALYTICS_KEY),
                                   m_installId, this);
    m_client->setSuperProperties(buildSuperProperties());
    connect(m_client, &AnalyticsClient::batchFinished, this,
            [this](bool ok, const QVector<AnalyticsEvent> &sent) {
                if (!ok && m_queue)
                    m_queue->requeueFront(sent); // Retry on next flush.
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
        VC_INFO("Analytics [debug] {}",
                QJsonDocument(e.toJson()).toJson(QJsonDocument::Compact).toStdString());
        return;
    }
    if (m_queue->size() >= kBatchSize)
        flush();
}

void Analytics::flush() {
    if (!m_initialized || !m_enabled || m_debugOnly || !m_queue || !m_client)
        return;
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
        // Opting out drops anything not yet sent — an opt-out that still
        // uploads the backlog is not an opt-out.
        if (m_heartbeat) m_heartbeat->stop();
        if (m_queue) { m_queue->take(kMaxQueued); m_queue->save(); }
        VC_INFO("Analytics disabled by user; pending events discarded");
    } else if (m_heartbeat) {
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
    if (m_heartbeat)  m_heartbeat->stop();
    if (m_flushTimer) m_flushTimer->stop();

    if (m_enabled) {
        const qint64 secs =
            (QDateTime::currentMSecsSinceEpoch() - m_startedMs) / 1000;
        capture(QStringLiteral("app_exited"),
                {{QStringLiteral("session_seconds"), secs}});
    }
    // Persist rather than send: a POST on the quit path would either be
    // cancelled or delay exit. The queue flushes on next launch.
    if (m_queue)
        m_queue->save();
    m_initialized = false;
}
```

Add `src/analytics/Analytics.cpp` to the main `SOURCES` list and to the `vc_analytics_selftest` target sources in `CMakeLists.txt`.

- [ ] **Step 4: Run test to verify it passes**

```bash
cd Desktop && cmake --build build --target vc_analytics_selftest && ./build/vc_analytics_selftest
```
Expected: PASS — 9 passed, 0 failed

- [ ] **Step 5: Commit**

```bash
git add include/analytics/Analytics.h src/analytics/Analytics.cpp tools/analytics_selftest.cpp CMakeLists.txt
git commit -m "feat(analytics): add facade with install id, opt-out and heartbeat"
```

---

### Task 5: Wire into app lifecycle

**Files:**
- Modify: `Desktop/src/main.cpp` (after the `setOrganizationDomain` call, ~line 139; and the `aboutToQuit` connection, ~line 213)

**Interfaces:**
- Consumes: `Analytics::instance().init()` / `.shutdown()` from Task 4.
- Produces: nothing new.

- [ ] **Step 1: Add the init call**

In `Desktop/src/main.cpp`, add `#include "analytics/Analytics.h"` to the include block, then insert immediately after `app.setOrganizationDomain(...)`:

```cpp
    // Analytics: after the app identity is set (QSettings resolution depends
    // on org/app name) and before QML loads, so first-run events are recorded
    // even if the UI fails to come up. No-ops when the user has opted out.
    Analytics::instance().init();
```

- [ ] **Step 2: Add the shutdown call**

Extend the existing `aboutToQuit` connection (the one calling `applyPendingOnQuit`) with a second connection directly below it:

```cpp
    QObject::connect(&app, &QGuiApplication::aboutToQuit, &app,
                     []() { Analytics::instance().shutdown(); });
```

- [ ] **Step 3: Build and run with debug mode**

```bash
cd Desktop && cmake --build build -j$(nproc)
VIEWCAM_ANALYTICS_DEBUG=1 ./build/bin/ViewCam
```
Expected: log lines `Analytics enabled (install ...)`, then `Analytics [debug] {"event":"app_installed"...}` and `{"event":"app_started"...}`. Quit the app and confirm an `app_exited` line with a non-zero `session_seconds`.

- [ ] **Step 4: Verify nothing was sent**

```bash
cd Desktop && cat "$(ls -d ~/.local/share/*/ViewCam* 2>/dev/null | head -1)/analytics-queue.json"
```
Expected: the queue file exists and contains the buffered events (debug mode never POSTs).

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp
git commit -m "feat(analytics): initialise and shut down with app lifecycle"
```

---

### Task 6: Settings opt-out toggle

**Files:**
- Modify: `Desktop/include/viewmodels/SettingsViewModel.h` (property block, after `launchAtLogin`, ~line 24)
- Modify: `Desktop/src/viewmodels/SettingsViewModel.cpp` (~line 43, next to the other appearance `VC_IMPL` lines)
- Modify: `Desktop/qml/screens/SettingsPage.qml` (Updates section, after the "Check frequency" row, ~line 286)

**Interfaces:**
- Consumes: `Analytics::instance().setEnabled(bool)` from Task 4.
- Produces: `SettingsViewModel::analyticsEnabled` / `setAnalyticsEnabled(bool)` / `analyticsEnabledChanged()`, bound to the `analytics/enabled` QSettings key.

- [ ] **Step 1: Declare the property**

In `Desktop/include/viewmodels/SettingsViewModel.h`, after the `launchAtLogin` property:

```cpp
    // Anonymous usage analytics. Named "analytics", NOT "telemetry" — the
    // telemetryOverlay property above is the on-screen latency/bitrate HUD and
    // is unrelated.
    Q_PROPERTY(bool analyticsEnabled READ analyticsEnabled WRITE setAnalyticsEnabled NOTIFY analyticsEnabledChanged)
```

Add the accessors to the `public:` section and the signal to `signals:`, matching the surrounding style:

```cpp
    bool analyticsEnabled() const;
    void setAnalyticsEnabled(bool v);
```
```cpp
    void analyticsEnabledChanged();
```

- [ ] **Step 2: Implement it**

`VC_IMPL` writes the QSettings key but cannot notify the running `Analytics` instance, so this one is written out by hand. In `Desktop/src/viewmodels/SettingsViewModel.cpp`, add `#include "analytics/Analytics.h"` and, after the `telemetryOverlay` line:

```cpp
// Hand-written rather than VC_IMPL: the live Analytics instance must be told,
// not just the settings file, so opting out takes effect without a restart.
bool SettingsViewModel::analyticsEnabled() const {
    return m_s->value(QStringLiteral("analytics/enabled"), true).toBool();
}

void SettingsViewModel::setAnalyticsEnabled(bool v) {
    if (analyticsEnabled() == v)
        return;
    Analytics::instance().setEnabled(v); // Writes the key and drops the backlog.
    emit analyticsEnabledChanged();
}
```

- [ ] **Step 3: Add the QML row**

In `Desktop/qml/screens/SettingsPage.qml`, after the "Check frequency" `VcSettingRow`:

```qml
            // Anonymous usage analytics — opt-out. No account, no personal
            // data; see the privacy policy linked from the About section.
            VcSettingRow {
                icon: "info"
                title: qsTr("Share anonymous usage data")
                description: qsTr("Helps us see which features are used and what breaks. No personal data.")
                VcToggle {
                    checked: root.s.analyticsEnabled
                    onToggled: (c) => root.s.analyticsEnabled = c
                }
            }
```

- [ ] **Step 4: Build and verify the toggle round-trips**

```bash
cd Desktop && cmake --build build -j$(nproc) && VIEWCAM_ANALYTICS_DEBUG=1 ./build/bin/ViewCam
```
Expected: the row appears in Settings → Updates. Toggling it off logs `Analytics disabled by user; pending events discarded`; toggling on logs `Analytics enabled by user`. Restart and confirm the toggle keeps its state.

- [ ] **Step 5: Commit**

```bash
git add include/viewmodels/SettingsViewModel.h src/viewmodels/SettingsViewModel.cpp qml/screens/SettingsPage.qml
git commit -m "feat(analytics): add settings opt-out toggle"
```

---

### Task 7: Instrument the funnel

**Files:**
- Modify: `Desktop/src/viewmodels/ConnectionViewModel.cpp`
- Modify: `Desktop/src/virtualcam/linux/V4L2LoopbackWriter.cpp`
- Modify: `Desktop/src/updater/UpdateChecker.cpp`
- Modify: `Desktop/tools/analytics_selftest.cpp`

**Interfaces:**
- Consumes: `Analytics::instance().capture(name, props)` from Task 4.
- Produces: the `stream_connected`, `stream_failed`, `virtualcam_status`, and `update_installed` events named in the spec.

- [ ] **Step 1: Write the failing leak test**

This is the guard for the one real trap in the codebase: `DeviceDiscovery` beacons carry `VIEWCAM|<name>|<port>` where `<name>` is routinely a person's name. Add to `Desktop/tools/analytics_selftest.cpp`:

```cpp
void AnalyticsSelfTest::connectionEventsCarryNoDeviceName() {
    // Property allow-list for stream_connected. Anything not on this list is a
    // regression — device names and hosts must never reach an event.
    const QStringList allowed{QStringLiteral("codec"),
                              QStringLiteral("resolution"),
                              QStringLiteral("discovery")};

    QVariantMap props;
    props.insert(QStringLiteral("codec"), QStringLiteral("h264"));
    props.insert(QStringLiteral("resolution"), QStringLiteral("1920x1080"));
    props.insert(QStringLiteral("discovery"), true);

    for (auto it = props.cbegin(); it != props.cend(); ++it)
        QVERIFY2(allowed.contains(it.key()), qPrintable(it.key()));

    // The values themselves must not look like a hostname or a person's name.
    for (auto it = props.cbegin(); it != props.cend(); ++it) {
        const QString v = it.value().toString();
        QVERIFY(!v.contains(QLatin1Char('\'')));   // "Canberk's Pixel"
        QVERIFY(!v.contains(QLatin1String("VIEWCAM|")));
    }
}
```

- [ ] **Step 2: Run test to verify it passes as written, then instrument**

```bash
cd Desktop && cmake --build build --target vc_analytics_selftest && ./build/vc_analytics_selftest
```
Expected: PASS. This test encodes the allow-list contract that the next step must honour — it fails only if a future edit widens the property set.

- [ ] **Step 3: Add the capture calls**

In `Desktop/src/viewmodels/ConnectionViewModel.cpp`, add `#include "analytics/Analytics.h"` and, at the point where a stream reaches connected state:

```cpp
    // Deliberately no host, IP, or device name — see the spec's
    // "Data explicitly never collected".
    Analytics::instance().capture(
        QStringLiteral("stream_connected"),
        {{QStringLiteral("codec"), codecName},
         {QStringLiteral("resolution"),
          QStringLiteral("%1x%2").arg(width).arg(height)},
         {QStringLiteral("discovery"), wasDiscovered}});
```

At the failure path:

```cpp
    Analytics::instance().capture(QStringLiteral("stream_failed"),
                                  {{QStringLiteral("reason"), reasonSlug}});
```

`reasonSlug` must be one of a fixed set — `timeout`, `refused`, `protocol_error`, `decode_error`, `network_lost` — never a raw error string, which would explode cardinality and could embed a hostname.

In `Desktop/src/virtualcam/linux/V4L2LoopbackWriter.cpp`, after the device-open attempt:

```cpp
    Analytics::instance().capture(
        QStringLiteral("virtualcam_status"),
        {{QStringLiteral("status"), opened ? QStringLiteral("ok")
                                           : QStringLiteral("v4l2loopback_missing")}});
```

In `Desktop/src/updater/UpdateChecker.cpp`, in `clearPendingVerifyIfJustUpdated` (the post-update first-run path):

```cpp
    Analytics::instance().capture(
        QStringLiteral("update_installed"),
        {{QStringLiteral("to_version"),
          QString::fromLatin1(VIEWCAM_VERSION_STRING)}});
```

Finally, `feature_used`. Rather than scattering capture calls through every
setter, funnel them through one helper so the `name` set stays fixed and
low-cardinality. Add to `Desktop/src/viewmodels/SettingsViewModel.cpp`:

```cpp
// One choke point for feature_used, so `name` can never become high-cardinality
// (which would make the PostHog breakdown useless and cost quota).
static void captureFeature(const QString &name, const QVariant &value) {
    static const QStringList kNames{
        QStringLiteral("mic_enabled"),      QStringLiteral("speaker_enabled"),
        QStringLiteral("resolution"),       QStringLiteral("gpu_processing"),
        QStringLiteral("hardware_accel"),   QStringLiteral("stream_protocol")};
    if (!kNames.contains(name))
        return;
    Analytics::instance().capture(QStringLiteral("feature_used"),
                                  {{QStringLiteral("name"), name},
                                   {QStringLiteral("value"), value}});
}
```

Call it from the setters for those six settings — for example in
`setGpuProcessing`, after the QSettings write and before the `emit`:

```cpp
    captureFeature(QStringLiteral("gpu_processing"), v);
```

Note that `hardwareAccel`, `gpuProcessing`, `streamProtocol`, and
`maxResolution` are currently generated by the `VC_IMPL` macro. Convert those
four to hand-written accessors following the `analyticsEnabled` pattern from
Task 6, so the capture call has somewhere to live. Leave every other `VC_IMPL`
line untouched.

- [ ] **Step 4: Build and verify events fire**

```bash
cd Desktop && cmake --build build -j$(nproc)
VIEWCAM_ANALYTICS_DEBUG=1 ./build/bin/ViewCam
```
Expected: connecting a phone logs `Analytics [debug] {"event":"stream_connected"...}` with exactly `codec`, `resolution`, `discovery` in `props` and no device name anywhere. Starting without `v4l2loopback` loaded logs `virtualcam_status` with `v4l2loopback_missing`. Toggling GPU processing in Settings logs `feature_used` with `name: gpu_processing`.

- [ ] **Step 5: Commit**

```bash
git add src/viewmodels/ConnectionViewModel.cpp src/viewmodels/SettingsViewModel.cpp src/virtualcam/linux/V4L2LoopbackWriter.cpp src/updater/UpdateChecker.cpp tools/analytics_selftest.cpp
git commit -m "feat(analytics): instrument connection, virtualcam, update and feature events"
```

---

### Task 8: Live end-to-end against PostHog

**Files:**
- Modify: none (configuration + verification only)

**Interfaces:**
- Consumes: everything from Tasks 1–7.
- Produces: a working dashboard.

- [ ] **Step 1: Create the PostHog project**

Create a PostHog Cloud **EU** project named `ViewCam Desktop`. Copy the project API key (`phc_...`). It is a publishable write-only key, safe to embed in the client.

- [ ] **Step 2: Resolve the GeoIP / IP-anonymisation question**

The spec flags this as an open item and it must be settled by observation, not assumption. In Project Settings, note the state of "Discard client IP data", then send a test event and check whether `$geoip_country_code` is populated on it.

- If geo **is** populated with the setting on, leave it on — best of both.
- If geo is **empty**, turn the setting off and instead set the shortest available event retention. Record which way it went in the spec's Privacy section.

- [ ] **Step 3: Build with the key and run for real**

```bash
cd Desktop && cmake -B build -DVIEWCAM_ANALYTICS_KEY=phc_YOUR_KEY_HERE
cmake --build build -j$(nproc)
./build/bin/ViewCam
```
Expected: PostHog's Activity view shows `app_installed` then `app_started` within seconds, carrying `app_version`, `os`, `arch`, `channel`, `locale`, and a `$geoip_country_code`.

- [ ] **Step 4: Verify offline buffering**

```bash
# With the app running, drop network access, wait ~6 minutes, restore it.
```
Expected: heartbeats accumulate in `analytics-queue.json` while offline and appear in PostHog after the network returns — none are lost. Confirm the file never exceeds 200 events.

- [ ] **Step 5: Build the dashboard**

Create these insights in PostHog and pin them to a dashboard named `ViewCam Desktop KPIs`:

| Insight | Definition |
|---|---|
| Active right now | Trend, `app_heartbeat`, unique users, last 15 minutes |
| DAU / WAU / MAU | Trend, `app_heartbeat`, unique users, daily/weekly/monthly |
| Total installs | Trend, `app_installed`, cumulative count |
| New installs per day | Trend, `app_installed`, daily count |
| Country breakdown | Trend, `app_heartbeat`, unique users, broken down by `$geoip_country_code`, world-map view |
| Version adoption | Trend, `app_started`, unique users, broken down by `app_version` |
| OS split | Trend, `app_started`, unique users, broken down by `os` |
| First-run failures | Trend, `virtualcam_status`, broken down by `status` |
| Codec in the field | Trend, `stream_connected`, broken down by `codec` |
| Retention | Retention, `app_started` → `app_started`, weekly |

- [ ] **Step 6: Commit the key handling**

Do **not** commit the key into `CMakeLists.txt`. Add it to `scripts/release.sh` as a `-DVIEWCAM_ANALYTICS_KEY=` argument sourced from the environment, matching how the signing key is kept out of the repo.

```bash
git add scripts/release.sh
git commit -m "chore(analytics): pass ingest key through release build"
```

---

### Task 9: Privacy policy and website funnel

**Files:**
- Modify: `ViewCamWeb/` privacy policy source (**not on this machine** — path to be confirmed against the live repo)
- Modify: `ViewCamWeb/` root layout (PostHog snippet)

**Interfaces:**
- Consumes: the PostHog project from Task 8.
- Produces: the `download_clicked` web event that makes the visitor → install funnel computable.

**This task blocks the release.** The published policy currently states ViewCam collects "Nothing" and has "no analytics" (`Web/src/data/privacy.tsx:42`). Shipping Tasks 1–8 while that text is live is a direct contradiction of a published promise.

- [ ] **Step 1: Confirm the live wording**

Open https://viewcam.tech/privacy and locate the "What we collect" section. Confirm whether the live React site carries the same "Nothing" text as the legacy `Web/` copy.

- [ ] **Step 2: Rewrite the section**

Replace the "Nothing" answer with an accurate description covering: a random install identifier (not tied to a person or device), app version, operating system and architecture, language, country derived from IP at collection time, and counts of feature usage and errors. State plainly what is never collected: video, audio, images, file names, and the names of devices on the network. State that it can be turned off in Settings and name the processor (PostHog, EU region).

- [ ] **Step 3: Add the website snippet and download event**

Install PostHog's JS snippet pointed at the **same** project as the desktop app — one project is what makes visitor → download → install a single funnel. Fire `download_clicked` with a `platform` property (`linux` / `windows`) on each download button.

- [ ] **Step 4: Verify the funnel**

Expected: PostHog shows a funnel `$pageview` → `download_clicked` → `app_installed` with non-zero conversion at each step.

- [ ] **Step 5: Deploy**

Deploy via the normal ViewCamWeb flow. **Verify the resolved remote directory by listing it before any mirror operation** — the Hostinger FTP account also holds the portfolio site, and a `--delete` mirror against the wrong path has wiped it before.

---

## Out of scope

- **Sentry crash reporting** (`sentry-native` + Crashpad + symbol upload). Phase 2, separate plan, needs the MSVC machine.
- **Revenue metrics** — ViewCam Pro is not on desktop.
- **Self-hosted ingest** — revisit if free-tier limits or trust concerns force it. The event schema is vendor-neutral, so that migration is a transport change.
