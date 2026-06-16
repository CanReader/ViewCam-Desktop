#pragma once

#include <QObject>
#include <QImage>
#include <string>
#include <vector>

class V4L2LoopbackWriter : public QObject {
    Q_OBJECT

public:
    explicit V4L2LoopbackWriter(QObject *parent = nullptr);
    ~V4L2LoopbackWriter();

    bool open(const std::string &device = "");
    void close();
    bool isOpen() const;
    std::string devicePath() const;

public slots:
    void writeFrame(const QImage &image);

private:
    bool detectDevice(std::string &path);
    bool ensureModuleLoaded();
    bool isModuleLoaded() const;
    bool setFormat(int width, int height);
    void convertRgbToYuyv(const uchar *rgb, int width, int height);

    int m_fd = -1;
    std::string m_devicePath;
    int m_width = 0;
    int m_height = 0;
    int m_requestedWidth = 0;
    int m_requestedHeight = 0;
    bool m_formatSet = false;
    bool m_disabled = false;
    std::vector<uint8_t> m_yuyvBuffer;
};
