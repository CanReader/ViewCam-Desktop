#pragma once

#include <QByteArray>

struct AVCodecContext;
struct AVFrame;
struct AVPacket;

// Opus decoder for the phone's microphone uplink (spec §4.1 format=6,
// Phone → Desktop direction): 20 ms Opus packets in, interleaved s16le out.
// Wraps FFmpeg; without FFmpeg available() is false, the desktop never
// advertises "opus" in audioCodecs, and phones keep sending PCM.
class AudioDecoder {
public:
    AudioDecoder() = default;
    ~AudioDecoder();
    AudioDecoder(const AudioDecoder &) = delete;
    AudioDecoder &operator=(const AudioDecoder &) = delete;

    // True when this build can decode Opus (drives the audioCodecs advert).
    static bool available();

    bool open(int sampleRate, int channels);
    void close();
    bool isOpen() const { return m_ctx != nullptr; }

    // One Opus packet in → decoded PCM s16le out (empty on decode error —
    // each packet is only 20 ms, dropping one is inaudible-to-minor).
    QByteArray decode(const QByteArray &packet);

private:
    AVCodecContext *m_ctx = nullptr;
    AVFrame *m_frame = nullptr;
    AVPacket *m_packet = nullptr;
    int m_channels = 0;
};
