#pragma once

#include <QByteArray>
#include <QList>

struct AVCodecContext;
struct AVFrame;
struct AVPacket;

// Opus encoder for the phone-as-speaker feed (spec §4.1 format=6): turns
// 20 ms s16le PCM chunks into 20 ms Opus packets at the phone-requested
// bitrate — ~1.5 Mbit/s of PCM becomes 24-450 kbit/s on the Wi-Fi. Wraps
// FFmpeg's libopus; without FFmpeg (or libopus) open() fails and the caller
// keeps streaming PCM.
class AudioEncoder {
public:
    AudioEncoder() = default;
    ~AudioEncoder();
    AudioEncoder(const AudioEncoder &) = delete;
    AudioEncoder &operator=(const AudioEncoder &) = delete;

    bool open(int sampleRate, int channels, int bitrate);
    void close();
    bool isOpen() const { return m_ctx != nullptr; }
    int bitrate() const { return m_bitrate; }

    // One 20 ms interleaved s16le chunk in → normally exactly one Opus packet
    // out (libopus frame size at 48 kHz is the same 960 samples).
    QList<QByteArray> encode(const QByteArray &pcm);

private:
    AVCodecContext *m_ctx = nullptr;
    AVFrame *m_frame = nullptr;
    AVPacket *m_packet = nullptr;
    QByteArray m_pending; // partial frames carried between encode() calls
    int m_bitrate = 0;
    int m_channels = 0;
    long long m_pts = 0;
};
