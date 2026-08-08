# Desktop Telemetry & Analytics — Design

**Date:** 2026-08-08
**Status:** Approved, ready for implementation planning
**Scope:** ViewCam Desktop (C++17 / Qt 6)

## Problem

The mobile app has two sources of operational visibility: Google Play Console
(installs, active devices, DAU/MAU, audience growth, country breakdown) and
Sentry crash reporting via `AppTelemetry`.

The desktop app has neither. Because it is self-distributed from viewcam.tech
and GitHub, no store computes those numbers for us. Today the only evidence a
desktop user exists is an update-manifest fetch buried in Hostinger access logs.
We cannot answer: how many people are using ViewCam Desktop right now, yesterday,
this month; where they are; whether it works for them; or why they stop.

## Goal

Reproduce the Play Console KPI dashboard for desktop, and answer the questions a
store dashboard cannot:

| Play Console tile | Desktop source |
|---|---|
| Total installs, User acquisitions | `app_installed` |
| Active devices, DAU, MAU | `app_heartbeat`, unique install IDs |
| Audience growth rate | derived |
| Country breakdown | GeoIP at ingest |
| Store listing visitors → acquisitions | website analytics → `download_clicked` |
| Android vitals / crash-free rate | Sentry (phase 2) |

Beyond parity: **active-right-now**, **first-run failure reasons**, **version
adoption**, and **MJPEG vs H264 in the field**.

## Decisions

| Decision | Choice | Rationale |
|---|---|---|
| Backend | Sentry (crashes) + PostHog Cloud EU (product events) | Sentry mirrors mobile and keeps both apps in one org. PostHog gives DAU/MAU, funnels, retention, and GeoIP with no C++ SDK — plain HTTPS POST from Qt. Free tiers cover current scale with large headroom. |
| Consent | On by default, easy opt-out | Anonymous data with a first-run notice, a Settings toggle, and an updated privacy policy. Opt-in would make reach numbers a guess with an unknown multiplier. |
| Identity | Random UUID per install | Required for MAU and retention; a rotating token cannot compute them. Not derived from hardware, MAC, or hostname. |
| Sequencing | Product analytics first, crash reporting second | Phase 1 is ordinary Qt networking. Phase 2 (`sentry-native` + Crashpad + symbol upload) touches CMake, `release.sh`, and needs the separate MSVC machine. Coupling them lets the hard half block the easy half. |
| Self-hosting | Deferred | Railway + Postgres + Grafana fits the privacy positioning best, but means hand-building every dashboard tile and owning uptime: 1–2 weeks vs 2–3 days. The event schema below is vendor-neutral, so migrating later is a transport change, not a redesign. |

## Architecture

New subsystem at `Desktop/src/analytics/` + `Desktop/include/analytics/`,
mirroring the shape of mobile's `AppTelemetry` so both apps read the same way.

**Naming:** the subsystem is called *Analytics*, not *Telemetry*, because
`SettingsViewModel` already exposes `telemetryOverlay` — the on-screen
latency/bitrate HUD, an unrelated concept. Reusing "telemetry" would collide
inside the same class.

| Component | Responsibility |
|---|---|
| `Analytics` | Facade singleton. `init()`, `capture(event, props)`, `setEnabled()`. Every call wrapped so a telemetry failure can never take down the app. All methods no-op until `init()`, so they are safe to call from anywhere including error paths. **Thread-safe:** `capture()` marshals to the Analytics thread via a queued invocation, because `StreamReceiver`, `FramePipeline` and the audio sinks each run on their own threads and are exactly where errors surface. Without it the queue (a plain `QVector`) would race and `QNetworkAccessManager` would be touched cross-thread. Timestamps are taken at call time, not delivery time. `m_enabled`/`m_initialized` are atomic for the same reason mobile's `AppTelemetry` marks `isEnabled` `@Volatile`. |
| `AnalyticsClient` | Owns a `QNetworkAccessManager`. Batches events and POSTs to PostHog `/batch/`. Fire-and-forget: an unreachable endpoint is a no-op, never a stall. Flush every 30s or 20 events. |
| `EventQueue` | In-memory ring spilled to JSON under `QStandardPaths::AppDataLocation`. Survives crash and offline; flushed on next launch. Hard-capped at 200 events / 1 MB, dropping oldest, so an offline machine cannot grow it unbounded. |

### Integration points

- **Init** — `main.cpp`, immediately after the `setApplicationName` /
  `setOrganizationName` block (~line 139). `QSettings` resolution depends on
  those, and telemetry must be live before `engine.loadFromModule`. Identity
  values come from `ViewCamConfig.h`, as `UpdateChecker` already does.
- **Install ID** — `QUuid::createUuid()` written once to `analytics/installId`.
  A missing key means first run, which emits `app_installed`.
- **Heartbeat** — one beat fired immediately at init, then a `QTimer` at
  5-minute intervals emitting `app_heartbeat`. The leading beat is essential:
  without it, active-user metrics only count sessions longer than 5 minutes, so
  a user who opens ViewCam for a 4-minute call never appears in DAU at all. The
  5-minute interval gives "active right now" a 15-minute resolution at
  ~288 events/device/day.
- **Free-tier budget** — PostHog allows 1,000,000 events/month and the account
  has no active subscription, so exceeding it *stops ingestion* rather than
  billing. The heartbeat dominates volume: roughly 580 events/user/month at 2h
  daily use, i.e. ~1,700 monthly active users before the cap. Past ~1,000 active
  users, raise `kHeartbeatMs` to 15 minutes (3× headroom; only "active right
  now" gets coarser, DAU/MAU are unaffected).
- **Toggle** — `analytics/enabled`, default true, surfaced through
  `SettingsViewModel` as a `VcSettingRow` + `VcToggle` in `SettingsPage.qml`.
  Existing pattern; no new QML components.
- **Shutdown** — `app_exited` with session duration on `aboutToQuit`, alongside
  the existing `applyPendingOnQuit` hook.

## Event schema

Super-properties on every event: `app_version`, `os`, `os_distro`, `os_version`,
`arch`, `channel`. GeoIP properties (`$geoip_country_code`,
`$geoip_country_name`, `$geoip_city_name`) are attached server-side at ingest —
the client sends nothing for them. Verified working: events carry `TR`.

`os` is the platform (`linux` / `windows` / `macos`), deliberately *not*
`QSysInfo::productType()`, which returns the distro id on Linux (`arch`,
`ubuntu`, `fedora`) and would make a Linux-vs-Windows breakdown unreadable. The
distro is kept separately in `os_distro` — it matters because v4l2loopback
packaging differs per distro.

| Event | Properties | Answers |
|---|---|---|
| `app_installed` | — | Total installs, user acquisitions |
| `app_started` | `is_first_run`, `os_version`, `locale` | Version adoption, OS split, translation priorities |
| `app_heartbeat` | `streaming`, `codec` | Active now, DAU, MAU, growth rate |
| `app_exited` | `session_seconds` | Session length |
| `stream_connected` | `codec`, `resolution`, `discovery` | Does it work |
| `stream_failed` | `reason` | Why it does not |
| `virtualcam_status` | `ok` \| `v4l2loopback_missing` \| `permission_denied` \| `directshow_unregistered` | First-run drop-off |
| `feature_used` | `name` (fixed set) | Mic, speaker, resolution, GPU usage |
| `update_installed` | `from_version`, `to_version` | Update pipeline health |

`locale` is sent independently of GeoIP because the two answer different
questions: GeoIP says where users are, locale says what language they read.
Since we ship `translations/viewcam_<locale>.qm`, locale is the actionable one
for deciding the next translation.

### Data explicitly never collected

Enforced in code, not merely intended:

- Hostnames, MAC addresses, local IP addresses
- **Device names from `DeviceDiscovery` beacons.** The wire format is
  `VIEWCAM|<name>|<port>` and `<name>` is routinely a person's name. This is the
  one real trap in the codebase: attaching a device name to `stream_connected`
  would be a natural thing to write and would leak personal data.
- File paths
- Anything from the frame pipeline — no video, no images, no audio

## Privacy

The published privacy policy currently states ViewCam collects "Nothing" and has
"no analytics" (`Web/src/data/privacy.tsx:42`). Shipping telemetry contradicts
that promise, so the policy rewrite **must land with or before the release**, not
after. The rewrite describes exactly what is collected — anonymous install ID,
OS, version, country, feature counters — and what is not.

The live site is built from `ViewCamWeb/`, which is not present on this machine;
`Web/` is the legacy copy. The live wording must be checked directly.

**Open item for implementation:** confirm whether PostHog's `anonymize_ips`
setting runs before or after GeoIP enrichment. If it strips the IP first, geo
resolution returns empty and we keep IPs under a short retention window instead.
The two orderings produce opposite outcomes, so this must be verified against
current PostHog behavior rather than assumed.

## Testing

- `VIEWCAM_ANALYTICS_DEBUG=1` logs payloads instead of sending them, so schema
  changes can be verified without polluting production data.
- Unit tests: queue persistence across restart, size-cap eviction, no-op
  behavior before `init()` and when disabled.
- A dedicated test asserting no `DeviceDiscovery` device name can reach an event
  payload.
- A separate PostHog project for development builds, selected by build type.

## Rollout

1. **Phase 1 — analytics subsystem.** Facade, client, queue, install ID,
   heartbeat, Settings toggle, first-run notice. Rides the next release off the
   current `CMakeLists.txt` version (1.2.1).
2. **Phase 1 — privacy policy rewrite** in `ViewCamWeb/`. Ships with the release.
3. **Phase 1 — website analytics.** PostHog snippet on viewcam.tech plus a
   `download_clicked` event carrying platform. Required for the funnel to start
   at "visitor" rather than "install"; without it, installs have no denominator.
4. **Phase 2 — crash reporting.** `sentry-native` with Crashpad, symbol upload
   wired into `release.sh`, validated on the MSVC machine. Separate release.

## Out of scope

- Revenue metrics — ViewCam Pro is not on desktop yet.
- Self-hosted ingest — revisit if telemetry becomes a trust concern or free-tier
  limits are reached.
- iOS/mobile changes — mobile telemetry is unaffected.
