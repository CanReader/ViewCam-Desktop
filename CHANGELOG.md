# Changelog

All notable changes to ViewCam Studio (desktop).

## [1.0.5] — 2026-07-09

### Improved
- Frame rotation and mirroring now happen on the desktop after decode (protocol orient byte). This offloads the phone's heaviest per-frame work and roughly doubles mobile frame rates — requires the mobile app 1.0.4+; older phones keep working unchanged.
- Virtual camera format handling: the driver's actual format is adopted after negotiation (no more corrupted output on drivers that adjust the requested size), and a consumer-locked non-YUYV format is detected and reported instead of fed garbage.
- The virtual camera now initializes after the window appears — no more silent multi-second startup freeze behind a privilege prompt on first run.
- Connection stats report the upright frame size, and CI builds the Windows installer on version tags.

### Fixed
- Crash (use-after-free) when closing the app while streaming.
- "Connecting…" could hang forever if a phone accepted the connection but never completed the handshake; the watchdog now covers that gap and auto-reconnects.
- The previous session's last video frame could briefly flash into a new connection's preview; the preview now clears on disconnect.
- Stream resync could drop an extra frame when a frame boundary straddled a network read.
- The connected phone reappeared as a duplicate sidebar entry after a Wi-Fi roam changed its IP; devices are now matched by their stable ID.
- Connecting to a device no longer silently overwrites the "Listen port" preference.
- Preview artifacts when the stream resolution changed at the exact moment a texture upload failed.
- Build compatibility restored with Qt 6.5–6.8.

## [1.0.4] — 2026-06-28

- Pro feature gating (1080p streaming gated as Pro; debug builds unlocked).
- Fixed reconnect counter double-increment that made auto-reconnect give up too early.

## [1.0.3] — 2026-06-27

- Bundled Qt image-format plugins so the packaged build decodes MJPEG correctly.

## [1.0.1] — 2026-06-23

- First published release for Windows and Linux: signed self-update (Ed25519), device discovery, live preview, camera controls, virtual camera output (DirectShow / v4l2loopback).
