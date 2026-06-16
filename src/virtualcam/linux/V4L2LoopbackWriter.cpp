#include "virtualcam/V4L2LoopbackWriter.h"
#include "core/Logger.h"

#include <QProcess>
#include <QPainter>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <thread>

V4L2LoopbackWriter::V4L2LoopbackWriter(QObject *parent)
    : QObject(parent)
{
}

V4L2LoopbackWriter::~V4L2LoopbackWriter() {
    close();
}

bool V4L2LoopbackWriter::open(const std::string &device) {
    if (m_fd >= 0) {
        VC_WARN("Virtual camera already open: {}", m_devicePath);
        return true;
    }

    std::string path = device;
    if (path.empty() && !detectDevice(path)) {
        if (!ensureModuleLoaded())
            return false;

        // wait for udev to create the device node
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        if (!detectDevice(path)) {
            VC_ERROR("v4l2loopback module loaded but no device appeared");
            return false;
        }
    }

    m_fd = ::open(path.c_str(), O_WRONLY);
    if (m_fd < 0) {
        VC_ERROR("Failed to open {}: {}", path, strerror(errno));
        return false;
    }

    m_devicePath = path;
    m_formatSet = false;
    m_disabled = false;
    VC_INFO("Virtual camera opened: {}", m_devicePath);
    return true;
}

void V4L2LoopbackWriter::close() {
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
        m_formatSet = false;
        m_width = 0;
        m_height = 0;
        m_requestedWidth = 0;
        m_requestedHeight = 0;
        VC_INFO("Virtual camera closed");
    }
}

bool V4L2LoopbackWriter::isOpen() const {
    return m_fd >= 0;
}

std::string V4L2LoopbackWriter::devicePath() const {
    return m_devicePath;
}

bool V4L2LoopbackWriter::isModuleLoaded() const {
    return std::filesystem::exists("/sys/module/v4l2loopback");
}

bool V4L2LoopbackWriter::ensureModuleLoaded() {
    if (isModuleLoaded()) {
        VC_DEBUG("v4l2loopback module already loaded");
        return true;
    }

    QProcess modinfo;
    modinfo.start("modinfo", {"v4l2loopback"});
    modinfo.waitForFinished(5000);
    if (modinfo.exitCode() != 0) {
        VC_ERROR("v4l2loopback is not installed. "
                 "Install it with: sudo pacman -S v4l2loopback-dkms");
        return false;
    }

    VC_INFO("Loading v4l2loopback module...");

    QProcess pkexec;
    pkexec.start("pkexec", {
        "modprobe", "v4l2loopback",
        "video_nr=10",
        "card_label=ViewCam",
        "exclusive_caps=1"
    });
    pkexec.waitForFinished(30000);

    if (pkexec.exitCode() != 0) {
        auto err = QString(pkexec.readAllStandardError()).trimmed();
        VC_ERROR("Failed to load v4l2loopback: {}",
                 err.isEmpty() ? "user cancelled or permission denied" : err.toStdString());
        return false;
    }

    VC_INFO("v4l2loopback module loaded successfully");
    return true;
}

bool V4L2LoopbackWriter::detectDevice(std::string &path) {
    for (int i = 0; i < 64; ++i) {
        std::string devPath = "/dev/video" + std::to_string(i);

        if (!std::filesystem::exists(devPath))
            continue;

        int fd = ::open(devPath.c_str(), O_WRONLY);
        if (fd < 0)
            continue;

        struct v4l2_capability cap {};
        if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
            if (cap.capabilities & V4L2_CAP_VIDEO_OUTPUT) {
                ::close(fd);
                path = devPath;
                VC_INFO("Auto-detected v4l2loopback device: {} ({})",
                        devPath, reinterpret_cast<const char *>(cap.card));
                return true;
            }
        }

        ::close(fd);
    }

    return false;
}

bool V4L2LoopbackWriter::setFormat(int width, int height) {
    struct v4l2_format fmt {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmt.fmt.pix.width = static_cast<unsigned>(width);
    fmt.fmt.pix.height = static_cast<unsigned>(height);
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    fmt.fmt.pix.sizeimage = static_cast<unsigned>(width * height * 2);

    if (ioctl(m_fd, VIDIOC_S_FMT, &fmt) < 0) {
        if (errno == EBUSY) {
            // format is locked, read back what the consumer set
            struct v4l2_format current {};
            current.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            if (ioctl(m_fd, VIDIOC_G_FMT, &current) == 0) {
                m_width = static_cast<int>(current.fmt.pix.width);
                m_height = static_cast<int>(current.fmt.pix.height);
                m_formatSet = true;
                m_yuyvBuffer.resize(static_cast<size_t>(m_width * m_height * 2));

                if (m_width == width && m_height == height) {
                    VC_INFO("Virtual camera format already set: {}x{} (locked by consumer)", m_width, m_height);
                    return true;
                }

                VC_WARN("Virtual camera locked at {}x{}, requested {}x{} - will scale",
                        m_width, m_height, width, height);
                return true;
            }
            VC_ERROR("VIDIOC_S_FMT busy and VIDIOC_G_FMT failed: {}", strerror(errno));
        } else {
            VC_ERROR("VIDIOC_S_FMT failed: {}", strerror(errno));
        }
        return false;
    }

    m_width = width;
    m_height = height;
    m_formatSet = true;

    m_yuyvBuffer.resize(static_cast<size_t>(width * height * 2));
    VC_INFO("Virtual camera format set: {}x{} YUYV", width, height);
    return true;
}

void V4L2LoopbackWriter::convertRgbToYuyv(const uchar *rgb, int width, int height) {
    uint8_t *out = m_yuyvBuffer.data();
    int totalPixels = width * height;

    for (int i = 0; i < totalPixels; i += 2) {
        int idx = i * 3;
        int r0 = rgb[idx];
        int g0 = rgb[idx + 1];
        int b0 = rgb[idx + 2];
        int r1 = rgb[idx + 3];
        int g1 = rgb[idx + 4];
        int b1 = rgb[idx + 5];

        // BT.601 conversion
        int y0 = ((66 * r0 + 129 * g0 + 25 * b0 + 128) >> 8) + 16;
        int y1 = ((66 * r1 + 129 * g1 + 25 * b1 + 128) >> 8) + 16;
        int u  = ((-38 * r0 - 74 * g0 + 112 * b0 + 128) >> 8) + 128;
        int v  = ((112 * r0 - 94 * g0 - 18 * b0 + 128) >> 8) + 128;

        int outIdx = i * 2;
        out[outIdx]     = static_cast<uint8_t>(y0);
        out[outIdx + 1] = static_cast<uint8_t>(u);
        out[outIdx + 2] = static_cast<uint8_t>(y1);
        out[outIdx + 3] = static_cast<uint8_t>(v);
    }
}

void V4L2LoopbackWriter::writeFrame(const QImage &image) {
    if (m_fd < 0 || m_disabled)
        return;

    if (image.isNull())
        return;

    QImage rgb = image.format() == QImage::Format_RGB888
        ? image
        : image.convertToFormat(QImage::Format_RGB888);

    int w = rgb.width();
    int h = rgb.height();

    // YUYV needs even width
    if (w % 2 != 0) {
        rgb = rgb.copy(0, 0, w - 1, h);
        w = rgb.width();
    }

    if (!m_formatSet || w != m_requestedWidth || h != m_requestedHeight) {
        m_requestedWidth = w;
        m_requestedHeight = h;
        if (!setFormat(w, h)) {
            m_disabled = true;
            VC_ERROR("Virtual camera disabled, format setup failed");
            return;
        }
    }

    if (w != m_width || h != m_height) {
        // Letterbox: scale down to fit entirely within the target, centre
        // on a black canvas.  Preserves full content when orientation
        // differs (e.g. landscape frame inside portrait v4l2 format).
        double scaleX = static_cast<double>(m_width) / w;
        double scaleY = static_cast<double>(m_height) / h;
        double scale  = std::min(scaleX, scaleY);
        int fitW = static_cast<int>(w * scale);
        int fitH = static_cast<int>(h * scale);
        if (fitW % 2 != 0) fitW--;

        QImage canvas(m_width, m_height, QImage::Format_RGB888);
        canvas.fill(Qt::black);

        QImage scaled = rgb.scaled(fitW, fitH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (scaled.format() != QImage::Format_RGB888)
            scaled = scaled.convertToFormat(QImage::Format_RGB888);

        int offX = (m_width  - fitW) / 2;
        int offY = (m_height - fitH) / 2;
        QPainter p(&canvas);
        p.drawImage(offX, offY, scaled);
        p.end();

        rgb = canvas;
        w = m_width;
        h = m_height;
    }

    // handle scanline padding
    if (rgb.bytesPerLine() == w * 3) {
        convertRgbToYuyv(rgb.constBits(), w, h);
    } else {
        std::vector<uint8_t> compact(static_cast<size_t>(w * h * 3));
        for (int y = 0; y < h; ++y) {
            std::memcpy(compact.data() + y * w * 3, rgb.constScanLine(y), static_cast<size_t>(w * 3));
        }
        convertRgbToYuyv(compact.data(), w, h);
    }

    ssize_t written = ::write(m_fd, m_yuyvBuffer.data(), m_yuyvBuffer.size());
    if (written < 0) {
        VC_ERROR("Failed to write frame to virtual camera: {}", strerror(errno));
    }
}
