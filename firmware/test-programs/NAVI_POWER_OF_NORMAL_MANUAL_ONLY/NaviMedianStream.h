// NaviMedianStream.h — portable consumer for the tick-median stream.
//
// hallTask stamps each 1 ms tick median with a per-tick sequence number
// (monotonic, +1 per tick). This consumer detects a gap in the sequence
// — medians lost, e.g. to a median-queue overflow — and resets the
// recognizer at the exact boundary: every pre-loss median is fed
// normally, and the first post-loss median is fed into a fresh core, so
// no correlation window is evaluated across the loss.
//
// A timestamp jump alone is NOT evidence of loss (the tick clock jitters
// by up to ~4.5 ms); the sequence number is the loss signal. This
// replaces the earlier drop-counter check, which could reset the
// recognizer before the consumer reached the discontinuity: the counter
// is global and increments in the producer, so it could fire while
// valid pre-loss medians were still queued, discarding a partial
// waveform that was already fed. The counter is still kept for
// telemetry (median_q_drop); it no longer drives the reset.
//
// No Arduino dependencies. Shared by the firmware (rvmTask drains the
// median queue through this) and the host harness (the integration test
// drives it through a simulated bounded queue).
#pragma once

#include <cstdint>
#include "NaviMatchedFilter.h"

class NaviMedianStream {
 public:
  explicit NaviMedianStream(NaviMatchedFilter& core) : core_(core) {}

  // Feed one tick median. seq is the per-tick sequence number stamped by
  // the producer (+1 per tick, monotonic, unsigned so the 2^32 wrap is
  // handled by the arithmetic). If seq is not exactly one more than the
  // previous median's seq, medians were lost in between: reset the
  // recognizer before feeding this (the first post-loss) median. The
  // pre-loss medians already fed are preserved. The first median after
  // construction (or reset()) is always accepted with no reset.
  void feed(int16_t median, uint32_t seq, uint64_t tUs) {
    if (haveLastSeq_ && seq != lastSeq_ + 1) core_.noteDataLoss();
    haveLastSeq_ = true;
    lastSeq_ = seq;
    core_.pushTickMedian(median, tUs);
  }

  void reset() {
    haveLastSeq_ = false;
    lastSeq_ = 0;
  }

 private:
  NaviMatchedFilter& core_;
  bool haveLastSeq_ = false;
  uint32_t lastSeq_ = 0;
};
