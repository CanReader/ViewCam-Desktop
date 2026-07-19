#pragma once

#include <QByteArray>
#include <QImage>

#ifdef VIEWCAM_HAVE_FFMPEG

extern "C" {
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;
}

/**
 * Stateful FFmpeg H264 elementary-stream decoder. Each VIDEO frame with
 * format=1 carries exactly one Annex-B access unit (the phone prepends
 * SPS/PPS to every IDR, so joining mid-stream recovers on the next keyframe).
 *
 * Tuned for latency, not throughput: AV_CODEC_FLAG_LOW_DELAY and no frame
 * threading (frame threads buffer N-1 frames of delay — poison for a live
 * webcam). Slice threading stays enabled; it has no latency cost.
 *
 * Not a QObject: lives inside FrameDecoder and runs wherever decode() runs.
 */
class H264Decoder {
public:
    H264Decoder();
    ~H264Decoder();

    H264Decoder(const H264Decoder &) = delete;
    H264Decoder &operator=(const H264Decoder &) = delete;

    /**
     * Feed one access unit. Returns true and fills `out` (RGB32, upright as
     * encoded — caller applies the orient-byte transform) when a picture is
     * complete. False = no picture yet (decoder warming up) OR error; check
     * needsKeyframe() to distinguish.
     */
    bool decode(const QByteArray &accessUnit, QImage &out);

    /** True after a decode error / mid-stream join: ask the phone for an IDR. */
    bool needsKeyframe() const { return m_needKeyframe; }
    void clearKeyframeFlag() { m_needKeyframe = false; }

    /** Drop all state (new connection / stream restart). */
    void reset();

private:
    bool ensureContext();

    AVCodecContext *m_ctx = nullptr;
    AVFrame *m_frame = nullptr;
    AVPacket *m_packet = nullptr;
    SwsContext *m_sws = nullptr;
    bool m_needKeyframe = false;
};

#endif // VIEWCAM_HAVE_FFMPEG
