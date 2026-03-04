#pragma once

#include <QObject>
#include <QImage>

#ifdef _WIN32
#include <windows.h>
#endif

class DirectShowVirtualCam : public QObject {
    Q_OBJECT

public:
    explicit DirectShowVirtualCam(QObject *parent = nullptr);
    ~DirectShowVirtualCam();

    bool open();
    void close();
    bool isOpen() const;

public slots:
    void writeFrame(const QImage &image);

private:
#ifdef _WIN32
    HANDLE m_sharedMem  = nullptr;
    HANDLE m_frameEvent = nullptr;
    HANDLE m_mutex      = nullptr;
    void  *m_mappedPtr  = nullptr;
#endif
    bool     m_open        = false;
    uint64_t m_frameNumber = 0;
};
