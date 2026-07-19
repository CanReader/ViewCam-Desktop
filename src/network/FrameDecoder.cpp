#include "network/FrameDecoder.h"
#include "core/Constants.h"
#include "core/Logger.h"

#include <QTransform>

FrameDecoder::FrameDecoder(QObject *parent)
    : QObject(parent)
{
}

FrameDecoder::~FrameDecoder() = default;

/** Sensor-to-upright transform (header byte 22), shared by both codecs.
 *  Right-angle rotations are lossless pixel remaps, and doing this on the
 *  desktop roughly doubled the phone's achievable frame rate. */
void FrameDecoder::emitUpright(QImage image, const FrameData &frame) {
    if (frame.rotationDegrees != 0) {
        QTransform t;
        t.rotate(frame.rotationDegrees);
        image = image.transformed(t, Qt::FastTransformation);
    }
    if (frame.mirror) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        image = image.flipped(Qt::Horizontal);
#else
        image = image.mirrored(true, false); // flipped() only exists in 6.9+
#endif
    }
    emit imageReady(image);
}

void FrameDecoder::decode(const FrameData &frame) {
    if (frame.format == static_cast<uint8_t>(vc::FrameFormat::H264)) {
#ifdef VIEWCAM_HAVE_FFMPEG
        if (!m_h264) m_h264 = std::make_unique<H264Decoder>();
        QImage image;
        if (m_h264->decode(frame.jpegData, image)) {
            emitUpright(std::move(image), frame);
        } else if (m_h264->needsKeyframe()) {
            // One ask per 500ms: a burst of undecodable deltas (mid-stream
            // join) must not flood the phone with sync-frame requests.
            if (!m_keyframeAskTimer.isValid() || m_keyframeAskTimer.elapsed() > 500) {
                m_keyframeAskTimer.restart();
                m_h264->clearKeyframeFlag();
                emit keyframeNeeded();
            }
        }
#else
        VC_WARN("H264 frame but this build has no FFmpeg — dropping");
#endif
        return;
    }

    QImage image;
    if (image.loadFromData(frame.jpegData, "JPEG")) {
        emitUpright(std::move(image), frame);
    } else {
        VC_WARN("Failed to decode JPEG frame ({} bytes, {}x{})",
                frame.jpegData.size(), frame.width, frame.height);
    }
}

void FrameDecoder::resetStream() {
#ifdef VIEWCAM_HAVE_FFMPEG
    if (m_h264) m_h264->reset();
    m_keyframeAskTimer.invalidate();
#endif
}
