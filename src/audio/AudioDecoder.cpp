#include "audio/AudioDecoder.h"
#include "core/Logger.h"

#ifdef VIEWCAM_HAVE_FFMPEG

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
}

#include <cstring>
#include <vector>

namespace {
// Minimal 19-byte OpusHead (RFC 7845 §5.1), mapping family 0 — FFmpeg's Opus
// decoders read stream parameters from extradata, and a live stream has no
// container to provide one, so we synthesize it (mirror of the phone's decoder).
std::vector<uint8_t> opusHead(int sampleRate, int channels) {
    std::vector<uint8_t> h(19, 0);
    std::memcpy(h.data(), "OpusHead", 8);
    h[8] = 1;
    h[9] = uint8_t(channels);
    h[12] = uint8_t(sampleRate & 0xFF);
    h[13] = uint8_t((sampleRate >> 8) & 0xFF);
    h[14] = uint8_t((sampleRate >> 16) & 0xFF);
    h[15] = uint8_t((sampleRate >> 24) & 0xFF);
    return h;
}

const AVCodec *findDecoder() {
    const AVCodec *c = avcodec_find_decoder_by_name("libopus");
    if (!c) c = avcodec_find_decoder(AV_CODEC_ID_OPUS);
    return c;
}
} // namespace

AudioDecoder::~AudioDecoder() { close(); }

bool AudioDecoder::available() { return findDecoder() != nullptr; }

bool AudioDecoder::open(int sampleRate, int channels) {
    close();
    const AVCodec *codec = findDecoder();
    if (!codec) return false;

    m_ctx = avcodec_alloc_context3(codec);
    if (!m_ctx) return false;
    m_ctx->sample_rate = sampleRate;
    av_channel_layout_default(&m_ctx->ch_layout, channels);

    const auto head = opusHead(sampleRate, channels);
    m_ctx->extradata = static_cast<uint8_t *>(
        av_mallocz(head.size() + AV_INPUT_BUFFER_PADDING_SIZE));
    std::memcpy(m_ctx->extradata, head.data(), head.size());
    m_ctx->extradata_size = int(head.size());

    if (avcodec_open2(m_ctx, codec, nullptr) < 0) {
        VC_WARN("Opus decoder open failed");
        close();
        return false;
    }
    m_frame = av_frame_alloc();
    m_packet = av_packet_alloc();
    if (!m_frame || !m_packet) {
        close();
        return false;
    }
    m_channels = channels;
    VC_INFO("Opus mic decoder active ({} Hz, {} ch, {})", sampleRate, channels,
            codec->name);
    return true;
}

void AudioDecoder::close() {
    if (m_frame) av_frame_free(&m_frame);
    if (m_packet) av_packet_free(&m_packet);
    if (m_ctx) avcodec_free_context(&m_ctx);
}

QByteArray AudioDecoder::decode(const QByteArray &packet) {
    QByteArray out;
    if (!m_ctx || packet.isEmpty()) return out;

    if (av_new_packet(m_packet, packet.size()) < 0) return out;
    std::memcpy(m_packet->data, packet.constData(), size_t(packet.size()));
    if (avcodec_send_packet(m_ctx, m_packet) < 0) {
        av_packet_unref(m_packet);
        return out;
    }
    av_packet_unref(m_packet);

    while (avcodec_receive_frame(m_ctx, m_frame) == 0) {
        const int n = m_frame->nb_samples;
        const int old = out.size();
        out.resize(old + n * m_channels * 2);
        auto *dst = reinterpret_cast<int16_t *>(out.data() + old);
        // libopus emits interleaved s16; the native decoder emits fltp —
        // handle both so either FFmpeg build works.
        if (m_frame->format == AV_SAMPLE_FMT_S16) {
            std::memcpy(dst, m_frame->data[0], size_t(n) * m_channels * 2);
        } else if (m_frame->format == AV_SAMPLE_FMT_FLTP) {
            for (int c = 0; c < m_channels; ++c) {
                const auto *src = reinterpret_cast<const float *>(m_frame->data[c]);
                for (int i = 0; i < n; ++i) {
                    float v = src[i];
                    if (v > 1.0f) v = 1.0f;
                    if (v < -1.0f) v = -1.0f;
                    dst[i * m_channels + c] = int16_t(v * 32767.0f);
                }
            }
        } else if (m_frame->format == AV_SAMPLE_FMT_FLT) {
            const auto *src = reinterpret_cast<const float *>(m_frame->data[0]);
            for (int i = 0; i < n * m_channels; ++i) {
                float v = src[i];
                if (v > 1.0f) v = 1.0f;
                if (v < -1.0f) v = -1.0f;
                dst[i] = int16_t(v * 32767.0f);
            }
        } else {
            out.resize(old); // unexpected format — drop this frame
        }
        av_frame_unref(m_frame);
    }
    return out;
}

#else // !VIEWCAM_HAVE_FFMPEG

AudioDecoder::~AudioDecoder() = default;
bool AudioDecoder::available() { return false; }
bool AudioDecoder::open(int, int) { return false; }
void AudioDecoder::close() {}
QByteArray AudioDecoder::decode(const QByteArray &) { return {}; }

#endif
