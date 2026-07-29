#include "core/FramePipeline.h"

#include "core/Logger.h"
#include "network/FrameDecoder.h"
#ifdef __linux__
#include "virtualcam/V4L2LoopbackWriter.h"
#elif defined(_WIN32)
#include "virtualcam/DirectShowVirtualCam.h"
#endif

#include <QMetaObject>
#include <QThread>
#include <cmath>

FramePipeline::FramePipeline(VcamWriter *writer, QObject *parent)
    : QObject(parent)
    , m_decoder(new FrameDecoder(this))   // child: moves to the pipeline thread with us
    , m_writer(writer)
{
    // Same-thread direct connections — the decoder runs inline in process().
    connect(m_decoder, &FrameDecoder::imageReady, this, &FramePipeline::onDecoded,
            Qt::DirectConnection);
    connect(m_decoder, &FrameDecoder::keyframeNeeded, this, &FramePipeline::keyframeNeeded,
            Qt::DirectConnection);
    VC_DEBUG("FramePipeline created");
}

FramePipeline::~FramePipeline() = default;

template <typename F> bool FramePipeline::marshal(F &&f) {
    if (QThread::currentThread() == thread())
        return false;
    QMetaObject::invokeMethod(this, std::forward<F>(f), Qt::QueuedConnection);
    return true;
}

void FramePipeline::submitFrame(const FrameData &frame) {
    bool schedule = false;
    {
        QMutexLocker lock(&m_mx);
        if (frame.format == 1) {
            // H264: deltas need their predecessors — queue, never overwrite.
            m_h264Queue.append(frame);
            if (m_h264Queue.size() > kMaxH264Queue) {
                m_h264Queue.clear();
                m_h264Overflowed = true;
            }
        } else {
            // MJPEG: every frame is independent — newest wins, stale drops.
            m_latest = frame;
            m_hasLatest = true;
        }
        if (!m_scheduled) {
            m_scheduled = true;
            schedule = true;
        }
    }
    if (schedule)
        QMetaObject::invokeMethod(this, [this] { drain(); }, Qt::QueuedConnection);
}

void FramePipeline::drain() {
    for (;;) {
        FrameData frame;
        bool have = false;
        bool overflowed = false;
        {
            QMutexLocker lock(&m_mx);
            overflowed = m_h264Overflowed;
            m_h264Overflowed = false;
            if (!m_h264Queue.isEmpty()) {
                frame = m_h264Queue.takeFirst();
                have = true;
            } else if (m_hasLatest) {
                frame = m_latest;
                m_latest = FrameData{};   // release the payload ref
                m_hasLatest = false;
                have = true;
            } else {
                m_scheduled = false;
                return;
            }
        }
        if (overflowed) {
            // Consumer wedged long enough to overflow ~3 s of H264: the
            // reference chain is broken — reset and ask for a fresh IDR
            // (rate-limited so a sustained stall doesn't flood the phone).
            VC_WARN("H264 pipeline overflow — resetting decoder, requesting keyframe");
            m_decoder->resetStream();
            if (!m_overflowKeyframeAsk.isValid() || m_overflowKeyframeAsk.elapsed() > 500) {
                m_overflowKeyframeAsk.restart();
                emit keyframeNeeded();
            }
        }
        process(frame);
    }
}

void FramePipeline::process(const FrameData &frame) {
    // decode() emits imageReady synchronously -> onDecoded() below.
    m_decoder->decode(frame);
}

void FramePipeline::onDecoded(const QImage &image) {
    if (!m_sawFirstFrame) {
        m_sawFirstFrame = true;
        VC_INFO("First video frame decoded: {}x{} -> preview + virtual camera",
                image.width(), image.height());
    }

    // Aspect-ratio preset: centered crop AFTER decode. The phone only pre-crops
    // the MJPEG path (where each frame is independent); H264 always arrives as
    // the full frame because resizing the phone's hardware encoder mid-stream
    // desynced the decoder. Cropping here is a cheap lossless copy, makes both
    // codecs behave identically, and works even for phones that don't understand
    // the "aspect" control at all. No-op when the frame already matches (the
    // MJPEG pre-crop case) or the ratio is "full". Frames are already upright
    // here (FrameDecoder rotates), so the ratio applies directly.
    QImage src = image;
    if (m_aspectRatio != QStringLiteral("full") && !src.isNull()) {
        const QStringList parts = m_aspectRatio.split(QLatin1Char(':'));
        if (parts.size() == 2) {
            const double target = parts[0].toDouble() / qMax(1.0, parts[1].toDouble());
            const double have = double(src.width()) / double(src.height());
            if (target > 0.0 && std::abs(have - target) > 0.01) {
                int cw = src.width();
                int ch = src.height();
                if (have > target)
                    cw = int(src.height() * target) & ~1;
                else
                    ch = int(src.width() / target) & ~1;
                cw = qBound(2, cw, src.width());
                ch = qBound(2, ch, src.height());
                src = src.copy((src.width() - cw) / 2, (src.height() - ch) / 2, cw, ch);
            }
        }
    }

    // Cap at the max output resolution setting (0=720p, 1=1080p, 2=4K).
    static const int kResW[] = {1280, 1920, 3840};
    static const int kResH[] = {720,  1080, 2160};
    int ri = qBound(0, m_maxResIndex, 2);
    // 4K is a Pro feature. A free session (no phone Pro entitlement) is capped at
    // 1080p regardless of the persisted setting — otherwise a device that once had
    // a Pro phone connected keeps its 4K ceiling forever. Read live per frame so
    // the cap re-applies the instant Pro ends (the QML row gate is cosmetic only).
    if (!m_pro)
        ri = qMin(ri, 1);
    QImage frame = (src.width() > kResW[ri] || src.height() > kResH[ri])
        ? src.scaled(kResW[ri], kResH[ri], Qt::KeepAspectRatio, Qt::SmoothTransformation)
        : src;

    if (m_bufferedFrames == 0) {
        // Flush any queued frames before passing the new one through.
        while (!m_frameBuffer.isEmpty())
            publish(m_frameBuffer.takeFirst());
        publish(frame);
    } else {
        // Jitter buffer: queue the incoming frame and display the oldest once
        // we have enough to absorb a burst. Caps at 2×bufSize to avoid unbounded
        // growth if the consumer stalls.
        m_frameBuffer.append(frame);
        while (m_frameBuffer.size() > m_bufferedFrames * 2)
            m_frameBuffer.removeFirst();
        if (m_frameBuffer.size() > m_bufferedFrames)
            publish(m_frameBuffer.takeFirst());
    }
}

void FramePipeline::publish(const QImage &frame) {
    // Apply the user "Mirror image" flip to the FRAME (not just the preview's
    // sourceRect) so the preview AND the virtual-camera output agree — otherwise
    // apps like Zoom/OBS saw the un-mirrored feed while the local preview was
    // flipped. Front-camera mirroring is already baked in by FrameDecoder.
    QImage out = frame;
    if (m_mirror) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        out = frame.flipped(Qt::Horizontal);
#else
        out = frame.mirrored(true, false);
#endif
    }
    emit previewReady(out);
    if (m_vcamOpen && m_vcamEnabled)
        m_writer->writeFrame(out);
}

void FramePipeline::resetStream() {
    // Cross-thread callers (receiver disconnected, GUI connect flow) land here
    // queued; the mailbox must still be cleared under the lock either way.
    if (marshal([this] { resetStream(); })) return;
    {
        QMutexLocker lock(&m_mx);
        m_latest = FrameData{};
        m_hasLatest = false;
        m_h264Queue.clear();
        m_h264Overflowed = false;
    }
    m_frameBuffer.clear();
    m_decoder->resetStream();
    m_sawFirstFrame = false;
    // Blank the preview NOW: the last decoded frame otherwise lingers in
    // FrameView and flashes into the next connection (potentially another
    // phone's image) until that session's first frame decodes.
    emit previewReady(QImage());
}

void FramePipeline::openVcam() {
    if (marshal([this] { openVcam(); })) return;
    const bool ok = m_writer->open();
    m_vcamOpen = ok;
    QString devicePath;
#ifdef __linux__
    if (ok)
        devicePath = QString::fromStdString(m_writer->devicePath());
#endif
    emit vcamOpened(ok, devicePath);
}

void FramePipeline::setAspectRatio(const QString &ratio) {
    if (marshal([this, ratio] { setAspectRatio(ratio); })) return;
    m_aspectRatio = ratio;
}

void FramePipeline::setMaxResolutionIndex(int index) {
    if (marshal([this, index] { setMaxResolutionIndex(index); })) return;
    m_maxResIndex = index;
}

void FramePipeline::setPro(bool pro) {
    if (marshal([this, pro] { setPro(pro); })) return;
    m_pro = pro;
}

void FramePipeline::setMirror(bool mirror) {
    if (marshal([this, mirror] { setMirror(mirror); })) return;
    m_mirror = mirror;
}

void FramePipeline::setBufferedFrames(int count) {
    if (marshal([this, count] { setBufferedFrames(count); })) return;
    m_bufferedFrames = count;
}

void FramePipeline::setVcamEnabled(bool enabled) {
    if (marshal([this, enabled] { setVcamEnabled(enabled); })) return;
    m_vcamEnabled = enabled;
}

void FramePipeline::setWatermarkEnabled(bool enabled) {
    if (marshal([this, enabled] { setWatermarkEnabled(enabled); })) return;
    m_writer->setWatermarkEnabled(enabled);
}
