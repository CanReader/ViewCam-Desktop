#pragma once
#include "ViewCamConfig.h"
#include <cstdint>

// Single source for network/protocol constants. Ports come from the generated
// config (CMake cache vars) so they are set in exactly one place.
// See design/CONNECTIVITY_PROTOCOL.md — this MUST match the mobile side.
namespace vc {

inline constexpr int kStreamPort = VIEWCAM_STREAM_PORT; // TCP frame stream
inline constexpr int kBeaconPort = VIEWCAM_BEACON_PORT; // UDP discovery beacons

// Discovery / liveness timing (ms)
inline constexpr int kBeaconIntervalMs = 1500; // phone broadcast cadence
inline constexpr int kCardTimeoutMs = 5000;    // drop a card after no beacons
inline constexpr int kHeartbeatMs = 1000;      // idle keep-alive cadence

// Discovery beacon: "VIEWCAM|<name>|<tcpPort>|<deviceId>"
inline constexpr const char *kBeaconTag = "VIEWCAM";
inline constexpr char kBeaconSep = '|';

// ── Wire frame format: 24-byte little-endian header + payload ──
inline constexpr int kFrameHeaderSize = 24;
inline constexpr int kMaxFrameBytes = 16 * 1024 * 1024; // 16 MB sanity cap
inline constexpr const char *kFrameMagic = "VCAM";

// Header field byte offsets
namespace hdr {
inline constexpr int kMagic = 0;      // 4B ASCII "VCAM"
inline constexpr int kPayloadLen = 4; // uint32 LE
inline constexpr int kTimestamp = 8;  // uint64 LE microseconds
inline constexpr int kWidth = 16;     // uint16 LE
inline constexpr int kHeight = 18;    // uint16 LE
inline constexpr int kFormat = 20;    // uint8
inline constexpr int kType = 21;      // uint8
} // namespace hdr

// header.format
enum class FrameFormat : uint8_t {
    Mjpeg = 0,
    H264 = 1,
    Hello = 2,
    Heartbeat = 3, // zero-length keep-alive OR JSON STATUS body
    Control = 4,   // Desktop -> Phone camera control (JSON body)
};

// header.type
enum class FrameType : uint8_t {
    Video = 0,
    Control = 1,
};

// Default capture profile (modest, Wi-Fi friendly) — easy to change.
inline constexpr int kDefaultCaptureWidth = 1280;
inline constexpr int kDefaultCaptureHeight = 720;
inline constexpr int kDefaultCaptureFps = 24;
inline constexpr int kDefaultJpegQuality = 70;

} // namespace vc
