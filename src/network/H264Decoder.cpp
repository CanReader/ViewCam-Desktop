#include "network/H264Decoder.h"

#ifdef VIEWCAM_HAVE_FFMPEG

#include "core/Logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

H264Decoder::H264Decoder() = default;

H264Decoder::~H264Decoder() {
    reset();
}

void H264Decoder::reset() {
    if (m_sws) {
        sws_freeContext(m_sws);
        m_sws = nullptr;
    }
    if (m_frame) av_frame_free(&m_frame);
    if (m_packet) av_packet_free(&m_packet);
    if (m_ctx) avcodec_free_context(&m_ctx);
    m_needKeyframe = false;
}

bool H264Decoder::ensureContext() {
    if (m_ctx) return true;

    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        VC_ERROR("FFmpeg has no H264 decoder in this build");
        return false;
    }
    m_ctx = avcodec_alloc_context3(codec);
    if (!m_ctx) return false;

    // Low latency: emit pictures as soon as they're decodable, and never use
    // frame threading (each frame thread adds a full frame of delay).
    m_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    m_ctx->thread_type = FF_THREAD_SLICE;
    m_ctx->thread_count = 0; // auto slice threads — no latency penalty

    if (avcodec_open2(m_ctx, codec, nullptr) < 0) {
        VC_ERROR("avcodec_open2(h264) failed");
        avcodec_free_context(&m_ctx);
        return false;
    }
    m_frame = av_frame_alloc();
    m_packet = av_packet_alloc();
    return m_frame && m_packet;
}

bool H264Decoder::decode(const QByteArray &accessUnit, QImage &out) {
    if (!ensureContext()) return false;

    // av_new_packet allocates size + AV_INPUT_BUFFER_PADDING_SIZE zeroed
    // bytes — REQUIRED by FFmpeg. Pointing the packet straight at the wire
    // buffer (no padding) let the bitstream reader read past the end and
    // segfault (caught by the local smoke test).
    if (av_new_packet(m_packet, accessUnit.size()) < 0) return false;
    memcpy(m_packet->data, accessUnit.constData(),
           static_cast<size_t>(accessUnit.size()));

    int err = avcodec_send_packet(m_ctx, m_packet);
    av_packet_unref(m_packet);
    if (err < 0) {
        // Typical mid-stream join: refs to frames we never saw. Ask for an IDR.
        VC_WARN("h264 send_packet failed ({}); requesting keyframe", err);
        m_needKeyframe = true;
        return false;
    }

    // Drain everything available, keep only the newest picture (latest-wins,
    // same policy as the rest of the pipeline).
    bool got = false;
    while ((err = avcodec_receive_frame(m_ctx, m_frame)) == 0) {
        const int w = m_frame->width;
        const int h = m_frame->height;
        if (w <= 0 || h <= 0) continue;

        m_sws = sws_getCachedContext(
            m_sws, w, h, static_cast<AVPixelFormat>(m_frame->format),
            w, h, AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!m_sws) continue;

        if (out.width() != w || out.height() != h || out.format() != QImage::Format_RGB32)
            out = QImage(w, h, QImage::Format_RGB32);

        uint8_t *dst[4] = { out.bits(), nullptr, nullptr, nullptr };
        int dstStride[4] = { static_cast<int>(out.bytesPerLine()), 0, 0, 0 };
        sws_scale(m_sws, m_frame->data, m_frame->linesize, 0, h, dst, dstStride);
        got = true;
    }
    if (err != AVERROR(EAGAIN) && err != AVERROR_EOF && err != 0) {
        VC_WARN("h264 receive_frame failed ({}); requesting keyframe", err);
        m_needKeyframe = true;
    }
    if (got) m_needKeyframe = false;
    return got;
}

#endif // VIEWCAM_HAVE_FFMPEG
