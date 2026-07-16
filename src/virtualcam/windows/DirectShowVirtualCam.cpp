#include "virtualcam/DirectShowVirtualCam.h"
#include "virtualcam/SharedFrameProtocol.h"
#include "core/Logger.h"

#include <windows.h>
#include <QDateTime>
#include <QPainter>

DirectShowVirtualCam::DirectShowVirtualCam(QObject *parent)
    : QObject(parent)
{
    VC_DEBUG("DirectShowVirtualCam created");
}

static void drawFrameWatermark(QImage &img) {
    const QString text = "ViewCam";
    // Scale the watermark to the frame so it stays consistently visible on the
    // free tier at any resolution — a fixed 14px pill was nearly invisible at
    // 720p/1080p and applied no upgrade pressure. ~1/22 of frame height,
    // clamped so it's neither tiny nor obnoxious.
    const int fontPx = qBound(15, img.height() / 22, 40);
    const int margin = qMax(10, img.height() / 40);

    QFont font;
    font.setPixelSize(fontPx);
    font.setWeight(QFont::DemiBold);

    QFontMetrics fm(font);
    const int padX = fontPx * 2 / 3;
    const int padY = fontPx / 3;
    int tw = fm.horizontalAdvance(text);
    int rh = fm.height() + padY * 2;
    int rw = tw + padX * 2;
    int rx = img.width() - rw - margin;
    int ry = margin;

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 115));
    p.drawRoundedRect(rx, ry, rw, rh, rh / 2, rh / 2);
    p.setFont(font);
    p.setPen(QColor(255, 255, 255, 215));
    p.drawText(rx + padX, ry + padY + fm.ascent(), text);
}

DirectShowVirtualCam::~DirectShowVirtualCam() {
    close();
}

bool DirectShowVirtualCam::open() {
    if (m_open) return true;

    m_sharedMem = CreateFileMappingA(
        INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
        0, ViewCam::SHMEM_TOTAL_SIZE, ViewCam::SHMEM_NAME);
    if (!m_sharedMem) {
        VC_ERROR("Failed to create shared memory: {}", GetLastError());
        return false;
    }

    m_mappedPtr = MapViewOfFile(m_sharedMem, FILE_MAP_ALL_ACCESS,
                                0, 0, ViewCam::SHMEM_TOTAL_SIZE);
    if (!m_mappedPtr) {
        VC_ERROR("Failed to map shared memory: {}", GetLastError());
        close();
        return false;
    }

    // Zero the header so the filter knows no frame is ready yet
    memset(m_mappedPtr, 0, ViewCam::SHMEM_HEADER_SIZE);

    m_frameEvent = CreateEventA(nullptr, FALSE, FALSE, ViewCam::EVENT_NAME);
    if (!m_frameEvent) {
        VC_ERROR("Failed to create frame event: {}", GetLastError());
        close();
        return false;
    }

    m_mutex = CreateMutexA(nullptr, FALSE, ViewCam::MUTEX_NAME);
    if (!m_mutex) {
        VC_ERROR("Failed to create mutex: {}", GetLastError());
        close();
        return false;
    }

    m_open = true;
    m_frameNumber = 0;
    VC_INFO("DirectShow virtual camera opened (shared memory ready)");
    return true;
}

void DirectShowVirtualCam::close() {
    if (m_mappedPtr)  { UnmapViewOfFile(m_mappedPtr); m_mappedPtr = nullptr; }
    if (m_sharedMem)  { CloseHandle(m_sharedMem);     m_sharedMem = nullptr; }
    if (m_frameEvent) { CloseHandle(m_frameEvent);     m_frameEvent = nullptr; }
    if (m_mutex)      { CloseHandle(m_mutex);          m_mutex = nullptr; }
    m_open = false;
    VC_DEBUG("DirectShow virtual camera closed");
}

bool DirectShowVirtualCam::isOpen() const {
    return m_open;
}

void DirectShowVirtualCam::writeFrame(const QImage &image) {
    if (!m_open || !m_mappedPtr) return;

    constexpr int outW = static_cast<int>(ViewCam::DEFAULT_WIDTH);   // 1280
    constexpr int outH = static_cast<int>(ViewCam::DEFAULT_HEIGHT);  // 720
    constexpr int outStride = outW * 3;
    constexpr uint32_t dataSize = outStride * outH;

    // Scale input to fit 1280x720, preserving aspect ratio (letterbox/pillarbox)
    QImage scaled = image.scaled(outW, outH, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // Create 1280x720 output in BGR888 (DirectShow RGB24 = BGR byte order)
    QImage frame(outW, outH, QImage::Format_BGR888);
    frame.fill(Qt::black);

    // Center the scaled image on the canvas
    int offsetX = (outW - scaled.width()) / 2;
    int offsetY = (outH - scaled.height()) / 2;
    {
        QPainter painter(&frame);
        painter.drawImage(offsetX, offsetY, scaled);
    }

    // Burn watermark into the frame before virtual camera output.
    if (m_watermarkEnabled)
        drawFrameWatermark(frame);

    // Lock shared memory (16 ms timeout ~ one frame at 60 fps)
    DWORD wait = WaitForSingleObject(m_mutex, 16);
    if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED)
        return; // Skip frame rather than block

    // Write header
    auto *hdr = static_cast<ViewCam::SharedFrameHeader *>(m_mappedPtr);
    hdr->magic        = ViewCam::FRAME_MAGIC;
    hdr->width        = outW;
    hdr->height       = outH;
    hdr->stride       = outStride;
    hdr->format       = ViewCam::FORMAT_RGB24;
    hdr->frame_size   = dataSize;
    hdr->frame_number = ++m_frameNumber;
    hdr->timestamp_us = static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch()) * 1000;

    // Write pixel data — flip vertically (DirectShow expects bottom-up)
    uint8_t *dst = static_cast<uint8_t *>(m_mappedPtr) + ViewCam::SHMEM_HEADER_SIZE;
    for (int y = 0; y < outH; ++y) {
        const uint8_t *srcLine = frame.constScanLine(outH - 1 - y);
        memcpy(dst + y * outStride, srcLine, outStride);
    }

    ReleaseMutex(m_mutex);

    // Signal the DirectShow filter that a new frame is available
    SetEvent(m_frameEvent);
}
