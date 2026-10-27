// NaviDiagnosticLevel.h — a trailing median of 1 ms tick medians, as the
// level source for the 70x2 onset detector (diagnostic only).
//
// The 70x2 detector needs a level to compare each sample against. It used
// to take NaviBaseline's frozen median; that machinery is retired with the
// old detection path. This is its replacement: the median of the last
// 1 second of tick medians, recomputed 10x per second and cached between
// recomputes (a median recompute on every 1 kHz sample would be far too
// expensive for hallTask).
//
// DIAGNOSTIC ONLY. This level never gates, vetoes, or modifies the
// matched-filter recognizer's decisions. It exists so the 70x2 onsets can
// be compared against the recognizer's events during initial testing.
// No Arduino dependencies; the object (ring + scratch, ~4 KB) is meant to
// live in static storage, not on a task stack.

#pragma once

#include <stdint.h>
#include <algorithm>

namespace navi_power {

class DiagnosticLevel {
 public:
  static constexpr int kWindow = 1000;       // 1 s of tick medians
  static constexpr int kMinSamples = 100;    // before the level is trusted
  static constexpr int kRecomputeTicks = 100;  // recompute every 100 ms

  void add(int16_t v) {
    ring_[head_] = v;
    head_ = (head_ + 1) % kWindow;
    if (count_ < kWindow) ++count_;
    if (++sinceRecompute_ >= kRecomputeTicks) {
      sinceRecompute_ = 0;
      recompute_();
    }
  }

  float level() const { return level_; }
  bool haveLevel() const { return haveLevel_; }

 private:
  void recompute_() {
    if (count_ < kMinSamples) return;
    for (int i = 0; i < count_; ++i) {
      scratch_[i] = ring_[(head_ - count_ + i + 2 * kWindow) % kWindow];
    }
    std::nth_element(scratch_, scratch_ + count_ / 2, scratch_ + count_);
    level_ = (float)scratch_[count_ / 2];
    haveLevel_ = true;
  }

  int16_t ring_[kWindow];
  int16_t scratch_[kWindow];
  int head_ = 0;
  int count_ = 0;
  int sinceRecompute_ = 0;
  float level_ = 0;
  bool haveLevel_ = false;
};

}  // namespace navi_power
