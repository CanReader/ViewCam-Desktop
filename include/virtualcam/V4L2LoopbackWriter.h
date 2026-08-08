#pragma once

#include <QObject>
#include <QImage>
#include <string>
#include <vector>

extern "C" {
struct SwsContext;
}

class V4L2LoopbackWriter : public QObject {
    Q_OBJECT

public:
    explicit V4L2LoopbackWriter(QObject *parent = nullptr);
    ~V4L2LoopbackWriter();

    bool open(const std::string &device = "");
    void close();
    bool isOpen() const;
    std::string devicePath() const;
    void setWatermarkEnabled(bool enabled) { m_watermarkEnabled = enabled; }

public slots:
    void writeFrame(const QImage &image);

private:
    bool detectDevice(std::string &path);
    bool ensureModuleLoaded();
    bool isModuleLoaded() const;
    bool setFormat(int width, int height);
    bool adoptDriverFormat(const struct v4l2_format &fmt);
    void convertRgbToYuyv(const uchar *rgb, int width, int height);

    int m_fd = -1;
    std::string m_devicePath;
    int m_width = 0;
    int m_height = 0;
    int m_requestedWidth = 0;
    int m_requestedHeight = 0;
    bool m_formatSet = false;
    bool m_disabled = false;
    // Set by ensureModuleLoaded(): whether v4l2loopback is present on the
    // system at all. Lets a load failure be attributed to "not installed"
    // versus "installed but modprobe was refused/cancelled".
    bool m_moduleInstalled = false;
    bool m_watermarkEnabled = true;
    std::vector<uint8_t> m_yuyvBuffer;
    // Cached BGRA→YUYV422 converter (SIMD, one pass). Replaces the old
    // convertToFormat(RGB888) + scalar per-pixel loop (2 passes, ~3-6ms/frame
    // at 1080p). Null when built without FFmpeg (scalar fallback used).
    SwsContext *m_sws = nullptr;
};
