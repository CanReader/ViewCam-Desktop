#pragma once

#include <QByteArray>
#include <cstdint>

struct FrameData {
    QByteArray jpegData;
    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t timestamp = 0; // microseconds
};
