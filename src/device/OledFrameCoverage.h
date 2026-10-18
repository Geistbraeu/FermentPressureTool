#ifndef OLED_FRAME_COVERAGE_H
#define OLED_FRAME_COVERAGE_H

#include <stdint.h>

namespace OledMirror {
  // A full SH1106 refresh must contain all visible bytes in page order.
  struct FrameCoverage {
    static constexpr uint16_t SIZE = 1024;
    uint16_t count;
    bool collecting;
    bool complete;

    constexpr FrameCoverage(uint16_t bytes = 0, bool active = false,
                            bool ready = false)
        : count(bytes), collecting(active), complete(ready) {}

    constexpr FrameCoverage accept(uint16_t index) const {
      return index == 0
          ? FrameCoverage(1, true, false)
          : collecting && index == count && index < SIZE
              ? FrameCoverage(index + 1, index + 1 < SIZE, index + 1 == SIZE)
              : FrameCoverage();
    }
  };
}

#endif
