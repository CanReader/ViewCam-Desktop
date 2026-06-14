<div align="center">

# ViewCam Studio · Desktop

### Your phone is the camera. Your laptop is the studio.

**A virtual webcam, finally worth the name.**

ViewCam Studio turns the phone in your pocket into a wireless, studio-grade webcam for your computer — no cables, no capture cards, no $400 hardware. Pair over Wi-Fi, hit record, and your phone's camera shows up as a real webcam in OBS, Zoom, Meet, Discord, or anything else.

[![Qt](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)](https://en.cppreference.com/)
[![QML](https://img.shields.io/badge/UI-Qt%20Quick%20%2F%20QML-41CD52)](https://doc.qt.io/qt-6/qtquick-index.html)
[![CUDA](https://img.shields.io/badge/GPU-CUDA%20%7C%20Vulkan-76B900?logo=nvidia&logoColor=white)](https://developer.nvidia.com/cuda-toolkit)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-555)](#building)
[![License](https://img.shields.io/badge/License-Proprietary-lightgrey)](#license)

</div>

---

## Why ViewCam

Webcams are stuck in 2010. The phone you already own has a better sensor, better optics, and better low-light performance than almost any USB webcam on the market. **ViewCam Studio is the desktop half** that receives your phone's live stream and exposes it to the rest of your system as a genuine camera device — so every app that can use a webcam can use your phone.

> **Design north star:** cinematic, weightless, trustworthy, quietly opinionated. Closer to Halide and Universal Audio Console than to a SaaS dashboard. Dark-first, because the app is a viewfinder.

## Features

🎥 **Real virtual camera** — feeds a true OS camera device (v4l2loopback on Linux, DirectShow on Windows). Works everywhere a webcam works.

📡 **Zero-config pairing** — phones announce themselves over the local network; live device cards appear the moment your phone opens the app. One click to connect.

🖼️ **GPU-rendered live preview** — low-latency MJPEG decode straight to a Qt Quick scene, plus a HUD with live bitrate, FPS, latency and resolution.

🔋 **Honest device telemetry** — battery, charging state, link quality (Strong / Fine / Low / Terrible) and active lens, streamed live from the phone. Unknown values show a clean `—`, never a junk default.

🎛️ **Remote camera control** — flip the phone's torch, focus lock, exposure lock and HDR right from the desktop panel. The desktop reflects what the phone can actually honor (e.g. torch is disabled on the front camera — no flash to switch on).

⚡ **GPU compute, vendor-agnostic** — a pluggable acceleration backend: **CUDA** on NVIDIA, **Vulkan compute** on AMD/Intel, CPU fallback everywhere else — chosen automatically at runtime. The seam for future filters (blur, virtual backgrounds, denoise, HDR tone-mapping) is already in place.

🎨 **Premium, opinionated UI** — a hand-built Qt Quick / QML interface on a warm-charcoal design system with light & dark themes, Geist typography, and tactile camera-inspired motion.

📦 **Ships like a real app** — project metadata baked in at build time; resources and translations packed into a single opaque binary bundle next to the executable. No loose assets to tamper with.

## Architecture

Modern **Qt 6 / Qt Quick (QML)** with an **MVVM** core — the UI is declarative QML, all logic lives in C++ ViewModels, and the networking/virtual-camera layers never touch the UI.

```
src/
├── core/         App controller, settings, frame data, logging
├── network/      StreamReceiver (TCP client + frame parser)
│                 DeviceDiscovery (UDP beacon listener)
│                 FrameDecoder (JPEG → QImage)
├── viewmodels/   AppInfo, connection, camera-control, settings (Q_PROPERTY → QML)
├── virtualcam/   V4L2LoopbackWriter (Linux) · DirectShowVirtualCam (Windows)
├── gpu/          GpuBackend abstraction: CudaBackend · VulkanComputeBackend · CpuBackend
└── qml/          Declarative UI, Theme singleton, reusable Vc* components
```

- **Metadata** is generated at configure time via CMake `configure_file` → `ViewCamConfig.h` (name, version, bundle id, git sha, Qt version, ports…).
- **Resources & translations** are compiled into an external opaque `viewcam.rcc` bundle and registered at startup — not embedded in the binary, not loose files.

## Streaming protocol

A compact binary protocol over the local network — see [`../design/CONNECTIVITY_PROTOCOL.md`](../design/CONNECTIVITY_PROTOCOL.md) for the full spec.

| Channel | Transport | Purpose |
|---|---|---|
| Discovery | UDP `8081` | Phones broadcast `VIEWCAM\|name\|port\|id` beacons; desktop lists them as cards |
| Stream | TCP `8080` | Phone serves frames; desktop connects as client |

Every frame on the stream is a **24-byte little-endian header** (magic `VCAM`, length, timestamp, dimensions, format, type) followed by its payload — a self-contained JPEG for video, JSON for `HELLO` / `STATUS` / `CONTROL`. The desktop both reads video/telemetry and writes camera-control commands on the same socket.

## Building

**Dependencies:** Qt 6 (Core, Gui, Qml, Quick, Network, Multimedia), CMake ≥ 3.20, a C++17 compiler. Optional: CUDA Toolkit and/or the Vulkan SDK for GPU acceleration; `v4l2loopback` for the Linux virtual camera.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
./bin/viewcam.sh        # launches bin/ViewCam with the right env
```

GPU acceleration is detected automatically. To build without CUDA: `-DVIEWCAM_ENABLE_CUDA=OFF` (Vulkan compute or CPU will be used instead).

**Linux virtual camera:** load the loopback module once —
```bash
sudo modprobe v4l2loopback card_label="ViewCam" exclusive_caps=1
```

## Roadmap

- [x] Discovery, pairing, MJPEG streaming → virtual camera
- [x] Live device telemetry (battery, link quality, lens)
- [x] Remote camera controls (torch, focus/exposure lock, HDR)
- [x] GPU backend abstraction (CUDA · Vulkan compute · CPU)
- [ ] GPU frame filters (blur, virtual backgrounds, denoise, HDR tone-map)
- [ ] Audio: phone microphone & speaker routing
- [ ] H.264 / H.265 transport

## License

Proprietary. © Sleak Software. All rights reserved.

---

<div align="center">
<sub>Part of the <b>ViewCam</b> project · the companion <a href="https://github.com/CanReader/ViewCam-Mobile">mobile app</a> is the camera.</sub>
</div>
