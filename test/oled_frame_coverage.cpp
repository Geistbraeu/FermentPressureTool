#include "device/OledFrameCoverage.h"
#include "device/OledFrameTiming.h"

using OledMirror::FrameCoverage;

constexpr FrameCoverage feed(FrameCoverage state, uint16_t index,
                             uint16_t end) {
  return index == end ? state : feed(state.accept(index), index + 1, end);
}

constexpr FrameCoverage full = feed(FrameCoverage(), 0, 1024);
static_assert(full.complete && full.count == 1024 && !full.collecting,
              "Exactly one full refresh is complete");

constexpr FrameCoverage partial = feed(FrameCoverage(), 0, 384);
static_assert(!partial.complete && partial.count == 384,
              "A three-page update must never be published as complete");

constexpr FrameCoverage gap = partial.accept(512);
static_assert(!gap.complete && gap.count == 0 && !gap.collecting,
              "A missing page must invalidate the candidate");

constexpr FrameCoverage tail = feed(gap, 513, 1024);
static_assert(!tail.complete && tail.count == 0,
              "A tail cannot repair a candidate with a missing page");

constexpr FrameCoverage restart = partial.accept(0);
static_assert(!restart.complete && restart.count == 1,
              "Page zero must start a fresh capture, not accumulate coverage");

constexpr FrameCoverage newPartial = feed(restart, 1, 128);
static_assert(newPartial.count == 128 && !newPartial.complete,
              "Old pages must not complete a new refresh");

constexpr FrameCoverage recovered = feed(newPartial, 128, 1024);
static_assert(recovered.complete && recovered.count == 1024,
              "Recovery requires a complete new refresh");

static_assert(full.accept(0).count == 1 && !full.accept(0).complete,
              "The first byte of the next refresh must revoke candidate readiness");
static_assert(FrameCoverage().accept(256).count == 0,
              "Joining a refresh halfway through must wait for page zero");
static_assert(partial.accept(383).count == 0,
              "Duplicate or out-of-order visible addresses must be rejected");
static_assert(partial.accept(1024).count == 0,
              "An invalid visible address must be rejected");

static_assert(OledMirror::frameInterval(15, 0xFFFFFFF0UL) == 31,
              "Frame intervals must survive millis wrap");
static_assert(OledMirror::intervalBucket(0) == 0 &&
              OledMirror::intervalBucket(1500) == 0 &&
              OledMirror::intervalBucket(1501) == 1 &&
              OledMirror::intervalBucket(3000) == 1 &&
              OledMirror::intervalBucket(3001) == 2 &&
              OledMirror::intervalBucket(5000) == 2 &&
              OledMirror::intervalBucket(5001) == 3 &&
              OledMirror::intervalBucket(10000) == 3 &&
              OledMirror::intervalBucket(10001) == 4 &&
              OledMirror::intervalBucket(30000) == 4 &&
              OledMirror::intervalBucket(30001) == 5,
              "Every gap belongs to exactly one explicitly bounded bucket");
