#ifndef OLED_MIRROR_H
#define OLED_MIRROR_H

#include <Arduino.h>
#include "device/OledFrameTiming.h"

namespace OledMirror {
  constexpr size_t FRAMEBUFFER_SIZE = 128 * 64 / 8;
  constexpr size_t RAM_WIDTH = 132;
  constexpr size_t RAM_FRAMEBUFFER_SIZE = RAM_WIDTH * 64 / 8;
  constexpr uint8_t VISIBLE_COLUMN_OFFSET = 2;

  struct Snapshot {
    uint8_t framebuffer[RAM_FRAMEBUFFER_SIZE]{};
    uint32_t sequence = 0;
    uint32_t lastUpdateMs = 0;
    uint32_t overflowCount = 0;
    uint32_t captureErrors = 0;
    uint32_t droppedFrames = 0;
    uint16_t capturedBytes = 0;
    uint8_t startLine = 0;
    uint8_t displayOffset = 0;
    uint8_t knownState = 0;
    bool ready = false;
    bool displayEnabled = false;
    bool displayAllOn = false;
    bool inverse = false;
    bool segmentRemap = false;
    bool comScanReverse = false;
  };

  struct Status {
    FrameTiming timing;
    uint32_t sequence = 0;
    uint32_t lastUpdateMs = 0;
    uint32_t overflowCount = 0;
    uint32_t captureErrors = 0;
    uint32_t droppedFrames = 0;
    uint16_t capturedBytes = 0;
    uint8_t startLine = 0;
    uint8_t displayOffset = 0;
    uint8_t knownState = 0;
    bool ready = false;
    bool recovering = false;
    bool displayEnabled = false;
    bool displayAllOn = false;
    bool inverse = false;
    bool segmentRemap = false;
    bool comScanReverse = false;
  };

  bool begin();
  void task(void* parameter);
  void getStatus(Status& status);
  void getSnapshot(Snapshot& snapshot);
}

#endif // OLED_MIRROR_H
