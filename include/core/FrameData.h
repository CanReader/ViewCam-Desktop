#pragma once

#include <QByteArray>
#include <cstdint>

struct FrameData {
  QByteArray jpegData;
  uint32_t width = 0;
  uint32_t height = 0;
  uint64_t timestamp = 0; // microseconds
  // The phone sends the sensor-oriented JPEG untouched (rotating on-device
  // cost it ~half its frame rate); the decoder rotates clockwise by this many
  // degrees after decode, then mirrors horizontally if requested (front cam).
  int rotationDegrees = 0; // 0 / 90 / 180 / 270
  bool mirror = false;
};
