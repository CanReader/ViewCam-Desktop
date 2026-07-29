#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QString>

#include "core/FrameData.h"

class FrameDecoder;
#ifdef __linux__
class V4L2LoopbackWriter;
#elif defined(_WIN32)
class DirectShowVirtualCam;
#endif

// Off-GUI-thread frame pipeline: decode -> upright -> aspect crop -> resolution
// cap -> mirror -> preview publish + virtual-camera write. Lives on its own
// QThread so per-frame CPU work (JPEG/H264 decode, scaling, the shared-memory
// vcam write and its cross-process mutex) can never stall the GUI event loop —
// which is also where the phone's TCP socket used to be serviced. A stalled
// loop meant an undrained socket, TCP backpressure onto the phone, and the
// phone dropping the connection the moment a consumer (Meet/Zoom/OBS) started
// pulling from the virtual camera.
//
// Backpressure policy (submitFrame): MJPEG frames are independent, so a
// latest-wins slot drops stale frames when decode falls behind — the socket
// thread never blocks and never queues unboundedly. H264 deltas depend on
// their predecessors, so they go through a bounded FIFO instead; on overflow
// the queue is cleared, the decoder reset, and a keyframe requested.
class FramePipeline : public QObject {
    Q_OBJECT

public:
#ifdef __linux__
    using VcamWriter = V4L2LoopbackWriter;
#elif defined(_WIN32)
    using VcamWriter = DirectShowVirtualCam;
#endif

    // `writer` is not owned; it must outlive this object and, once the
    // pipeline thread runs, is touched exclusively from that thread.
    explicit FramePipeline(VcamWriter *writer, QObject *parent = nullptr);
    ~FramePipeline() override;

    // Thread-safe; called from the network thread per received video frame.
    // Coalesces into the mailbox and schedules at most one drain event.
    void submitFrame(const FrameData &frame);

    // All setters are thread-safe (self-marshal to the pipeline thread).
    void openVcam();
    void setAspectRatio(const QString &ratio);
    void setMaxResolutionIndex(int index);
    void setPro(bool pro);
    void setMirror(bool mirror);
    void setBufferedFrames(int count);
    void setVcamEnabled(bool enabled);
    void setWatermarkEnabled(bool enabled);

public slots:
    // Connection ended / new connection starting: drop queued + buffered
    // frames, reset stateful decoders, blank the preview.
    void resetStream();

signals:
    // Emitted from the pipeline thread — connect with Auto/Queued to GUI.
    void previewReady(const QImage &image);
    void keyframeNeeded();
    void vcamOpened(bool ok, const QString &devicePath);

private:
    void drain();
    void process(const FrameData &frame);
    void onDecoded(const QImage &image);
    void publish(const QImage &frame);
    // Runs f on the pipeline thread when called from another; returns true if
    // the call was marshaled (caller should return immediately).
    template <typename F> bool marshal(F &&f);

    FrameDecoder *m_decoder;   // child of this — moves with the pipeline
    VcamWriter   *m_writer;    // not owned

    // Mailbox — guarded by m_mx, touched from network + pipeline threads.
    QMutex m_mx;
    FrameData m_latest;              // MJPEG latest-wins slot
    bool m_hasLatest = false;
    QList<FrameData> m_h264Queue;    // H264 bounded FIFO
    bool m_scheduled = false;
    bool m_h264Overflowed = false;

    // Pipeline-thread-only state.
    QString m_aspectRatio = QStringLiteral("full");
    int  m_maxResIndex = 1;
    bool m_pro = false;
    bool m_mirror = false;
    int  m_bufferedFrames = 0;
    bool m_vcamEnabled = true;
    bool m_vcamOpen = false;
    bool m_sawFirstFrame = false;
    QList<QImage> m_frameBuffer;     // jitter buffer (bufferedFrames setting)
    QElapsedTimer m_overflowKeyframeAsk;

    // Enough for ~3 s of 30 fps H264 before declaring the consumer wedged.
    static constexpr int kMaxH264Queue = 90;
};
