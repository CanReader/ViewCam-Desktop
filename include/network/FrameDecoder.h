#pragma once

#include <QObject>
#include <QImage>
#include <QElapsedTimer>
#include <memory>
#include "core/FrameData.h"
#include "network/H264Decoder.h"

class FrameDecoder : public QObject {
    Q_OBJECT

public:
    explicit FrameDecoder(QObject *parent = nullptr);
    ~FrameDecoder() override;

    /** Compile-time capability — drives the codecs list sent to the phone. */
    static constexpr bool h264Supported() {
#ifdef VIEWCAM_HAVE_FFMPEG
        return true;
#else
        return false;
#endif
    }

public slots:
    void decode(const FrameData &frame);
    /** New connection / disconnect: drop stateful (H264) decoder state. */
    void resetStream();

signals:
    void imageReady(const QImage &image);
    /** H264 lost sync (mid-stream join / decode error) — relay a CONTROL
     *  {"keyframe":true} to the phone. Rate-limited to one per 500 ms. */
    void keyframeNeeded();

private:
    void emitUpright(QImage image, const FrameData &frame);

#ifdef VIEWCAM_HAVE_FFMPEG
    std::unique_ptr<H264Decoder> m_h264;
    QElapsedTimer m_keyframeAskTimer;
#endif
};
