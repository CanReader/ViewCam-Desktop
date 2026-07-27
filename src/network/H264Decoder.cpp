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
    m_suppressUntilKey = false;
    for (auto &img : m_ring) img = QImage();
    m_ringIdx = 0;
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

        // FFmpeg's error concealment "succeeds" on broken input (dropped
        // reference, mid-stream dims change) and hands back a smeared picture.
        // Rendering it is the "noise burst" users saw after a resolution
        // switch. Suppress it, and keep suppressing until the next keyframe —
        // interim frames decode without NEW errors but inherit the garbage
        // through their references. m_needKeyframe makes FrameDecoder ask the
        // phone for an IDR, so the freeze lasts ~one round-trip, not a GOP.
        const bool corrupted = m_frame->decode_error_flags != 0;
        if (corrupted) {
            m_suppressUntilKey = true;
            m_needKeyframe = true;
        }
        if (m_suppressUntilKey) {
#ifdef AV_FRAME_FLAG_KEY
            const bool isKey = (m_frame->flags & AV_FRAME_FLAG_KEY) != 0;
#else
            const bool isKey = m_frame->key_frame != 0;
#endif
            if (!isKey || corrupted) continue;
            m_suppressUntilKey = false; // clean keyframe — stream is healthy again
        }

        // The decoder reports full-range streams as the deprecated YUVJ*
        // formats; swscale warns on every use of those. Map to the modern
        // equivalent and carry the range explicitly instead.
        AVPixelFormat srcFmt = static_cast<AVPixelFormat>(m_frame->format);
        bool jpegRange = false;
        switch (srcFmt) {
        case AV_PIX_FMT_YUVJ420P: srcFmt = AV_PIX_FMT_YUV420P; jpegRange = true; break;
        case AV_PIX_FMT_YUVJ422P: srcFmt = AV_PIX_FMT_YUV422P; jpegRange = true; break;
        case AV_PIX_FMT_YUVJ444P: srcFmt = AV_PIX_FMT_YUV444P; jpegRange = true; break;
        case AV_PIX_FMT_YUVJ440P: srcFmt = AV_PIX_FMT_YUV440P; jpegRange = true; break;
        default: break;
        }

        // SWS_FULL_CHR_H_INT: proper horizontal chroma interpolation (default
        // is a cheap doubling that stair-steps saturated edges).
        // SWS_ACCURATE_RND: correct rounding, removes a slight color bias.
        m_sws = sws_getCachedContext(
            m_sws, w, h, srcFmt,
            w, h, AV_PIX_FMT_BGRA,
            SWS_BILINEAR | SWS_FULL_CHR_H_INT | SWS_ACCURATE_RND,
            nullptr, nullptr, nullptr);
        if (!m_sws) continue;

        // COLOR RANGE: phone cameras output FULL-range YUV. Untagged streams
        // (older phone builds) were assumed limited-range by swscale, which
        // re-expanded already-full data — crushing shadows below Y=16 to pure
        // black and clipping highlights (visibly worse than the MJPEG path).
        // Trust an explicit MPEG tag; otherwise treat as full range. A YUVJ
        // source format is full range by definition.
        {
            const int srcRange =
                (!jpegRange && m_frame->color_range == AVCOL_RANGE_MPEG) ? 0 : 1;
            const int *coeffs = sws_getCoefficients(SWS_CS_ITU601);
            sws_setColorspaceDetails(m_sws, coeffs, srcRange, coeffs,
                                     /*dstRange full*/ 1, 0, 1 << 16, 1 << 16);
        }

        // Small ring: downstream (preview texture, snapshot, vcam) briefly
        // holds references to recent frames; a ring lets the slot be free
        // again by the time we cycle back, so steady state re-uses memory
        // instead of a fresh ~8MB allocation per frame. If a slot is still
        // shared, QImage detaches on write — no worse than before.
        QImage &slot = m_ring[m_ringIdx];
        m_ringIdx = (m_ringIdx + 1) % kRingSize;
        if (slot.width() != w || slot.height() != h || slot.format() != QImage::Format_RGB32)
            slot = QImage(w, h, QImage::Format_RGB32);

        uint8_t *dst[4] = { slot.bits(), nullptr, nullptr, nullptr };
        int dstStride[4] = { static_cast<int>(slot.bytesPerLine()), 0, 0, 0 };
        sws_scale(m_sws, m_frame->data, m_frame->linesize, 0, h, dst, dstStride);
        out = slot; // shared ref, no copy
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
