#pragma once

// Shared between ViewCam app and DirectShow filter DLL.
// Do NOT include Qt or DirectShow headers here — plain C/C++ only.

#include <cstdint>

namespace ViewCam {

// Named objects for IPC (Local namespace — same session, no admin required)
static constexpr const char *SHMEM_NAME = "Local\\ViewCamSharedFrame";
static constexpr const char *EVENT_NAME = "Local\\ViewCamFrameReady";
static constexpr const char *MUTEX_NAME = "Local\\ViewCamMutex";

// Wide-char versions for the filter DLL (COM uses wide strings)
static constexpr const wchar_t *SHMEM_NAME_W = L"Local\\ViewCamSharedFrame";
static constexpr const wchar_t *EVENT_NAME_W = L"Local\\ViewCamFrameReady";
static constexpr const wchar_t *MUTEX_NAME_W = L"Local\\ViewCamMutex";

static constexpr uint32_t FRAME_MAGIC = 0x4D414356; // "VCAM" little-endian

static constexpr uint32_t DEFAULT_WIDTH  = 1280;
static constexpr uint32_t DEFAULT_HEIGHT = 720;
static constexpr uint32_t DEFAULT_FPS    = 30;

// Max supported frame: 1920x1080 RGB24 ≈ 6 MB
static constexpr uint32_t SHMEM_HEADER_SIZE    = 64;
static constexpr uint32_t SHMEM_MAX_FRAME_SIZE = 1920 * 1080 * 3;
static constexpr uint32_t SHMEM_TOTAL_SIZE     = SHMEM_HEADER_SIZE + SHMEM_MAX_FRAME_SIZE;

enum FrameFormat : uint32_t {
    FORMAT_RGB24 = 0,
};

#pragma pack(push, 1)
struct SharedFrameHeader {
    uint32_t    magic;          // Must be FRAME_MAGIC
    uint32_t    width;
    uint32_t    height;
    uint32_t    stride;         // Bytes per row (width * 3 for RGB24)
    uint32_t    format;         // FrameFormat enum
    uint32_t    frame_size;     // Total pixel data bytes after header
    uint64_t    frame_number;
    uint64_t    timestamp_us;   // Microseconds since epoch
    uint8_t     reserved[24];   // Pad to 64 bytes total
};
#pragma pack(pop)

static_assert(sizeof(SharedFrameHeader) == SHMEM_HEADER_SIZE,
              "SharedFrameHeader must be exactly 64 bytes");

} // namespace ViewCam
