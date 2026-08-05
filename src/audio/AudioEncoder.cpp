#include "audio/AudioEncoder.h"
#include "core/Logger.h"

#ifdef VIEWCAM_HAVE_FFMPEG

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}

#include <cstring>

AudioEncoder::~AudioEncoder() { close(); }

bool AudioEncoder::open(int sampleRate, int channels, int bitrate) {
    close();

    // libopus (not FFmpeg's native encoder): the reference implementation,
    // better quality at the low bitrates the Audio tab offers.
    const AVCodec *codec = avcodec_find_encoder_by_name("libopus");
    if (!codec) codec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
    if (!codec) {
        VC_WARN("No Opus encoder in this FFmpeg build — speaker stays PCM");
        return false;
    }

    m_ctx = avcodec_alloc_context3(codec);
    if (!m_ctx) return false;
    m_ctx->sample_rate = sampleRate;
    av_channel_layout_default(&m_ctx->ch_layout, channels);
    m_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
    m_ctx->bit_rate = bitrate;
    m_ctx->time_base = AVRational{1, sampleRate};
    // 10 ms frames to match the capture chunking 1:1 (half the framing delay).
    av_opt_set_double(m_ctx->priv_data, "frame_duration", 10.0, 0);
    // Low-delay tuning: no lookahead worth blocking on for a live feed.
    av_opt_set(m_ctx->priv_data, "application", "lowdelay", 0);

    if (avcodec_open2(m_ctx, codec, nullptr) < 0) {
        VC_WARN("Opus encoder open failed — speaker stays PCM");
        close();
        return false;
    }

    m_frame = av_frame_alloc();
    m_packet = av_packet_alloc();
    if (!m_frame || !m_packet) {
        close();
        return false;
    }
    m_frame->format = AV_SAMPLE_FMT_S16;
    m_frame->sample_rate = sampleRate;
    av_channel_layout_default(&m_frame->ch_layout, channels);
    m_frame->nb_samples = m_ctx->frame_size;
    if (av_frame_get_buffer(m_frame, 0) < 0) {
        close();
        return false;
    }

    m_bitrate = bitrate;
    m_channels = channels;
    m_pts = 0;
    m_pending.clear();
    VC_INFO("Opus encoder active: {} Hz, {} ch, {} kbit/s, frame {} samples",
            sampleRate, channels, bitrate / 1000, m_ctx->frame_size);
    return true;
}

void AudioEncoder::close() {
    if (m_frame) av_frame_free(&m_frame);
    if (m_packet) av_packet_free(&m_packet);
    if (m_ctx) avcodec_free_context(&m_ctx);
    m_pending.clear();
    m_bitrate = 0;
}

QList<QByteArray> AudioEncoder::encode(const QByteArray &pcm) {
    QList<QByteArray> out;
    if (!m_ctx) return out;

    m_pending.append(pcm);
    const int frameBytes = m_ctx->frame_size * m_channels * 2;

    while (m_pending.size() >= frameBytes) {
        if (av_frame_make_writable(m_frame) < 0) break;
        std::memcpy(m_frame->data[0], m_pending.constData(), size_t(frameBytes));
        m_pending.remove(0, frameBytes);
        m_frame->pts = m_pts;
        m_pts += m_ctx->frame_size;

        if (avcodec_send_frame(m_ctx, m_frame) < 0) break;
        while (avcodec_receive_packet(m_ctx, m_packet) == 0) {
            out.append(QByteArray(reinterpret_cast<const char *>(m_packet->data),
                                  m_packet->size));
            av_packet_unref(m_packet);
        }
    }
    return out;
}

#else // !VIEWCAM_HAVE_FFMPEG

AudioEncoder::~AudioEncoder() = default;
bool AudioEncoder::open(int, int, int) { return false; }
void AudioEncoder::close() {}
QList<QByteArray> AudioEncoder::encode(const QByteArray &) { return {}; }

#endif
