#ifndef OLED_FRAME_TIMING_H
#define OLED_FRAME_TIMING_H

#include <stdint.h>

namespace OledMirror {
  constexpr uint8_t intervalBucket(uint32_t ms) {
    return ms <= 1500 ? 0 : ms <= 3000 ? 1 : ms <= 5000 ? 2
        : ms <= 10000 ? 3 : ms <= 30000 ? 4 : 5;
  }

  constexpr uint32_t frameInterval(uint32_t now, uint32_t previous) {
    return now - previous;
  }

  struct FrameTiming {
    uint32_t frames = 0;
    uint32_t firstFrameMs = 0;
    uint32_t lastFrameMs = 0;
    uint32_t lastIntervalMs = 0;
    uint32_t minIntervalMs = 0;
    uint32_t maxIntervalMs = 0;
    uint64_t totalIntervalMs = 0;
    uint32_t buckets[6]{};

    void record(uint32_t now) {
      if (frames == 0) {
        firstFrameMs = now;
      } else {
        lastIntervalMs = frameInterval(now, lastFrameMs);
        if (frames == 1 || lastIntervalMs < minIntervalMs) {
          minIntervalMs = lastIntervalMs;
        }
        if (lastIntervalMs > maxIntervalMs) {
          maxIntervalMs = lastIntervalMs;
        }
        totalIntervalMs += lastIntervalMs;
        ++buckets[intervalBucket(lastIntervalMs)];
      }
      lastFrameMs = now;
      ++frames;
    }
  };
}

#endif
