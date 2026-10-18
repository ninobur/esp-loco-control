#pragma once
// ---------------------------------------------------------------------------
// WaveformPause — STOP/GO continuity (operator ruling 2026-10-18).
// STOPPED = applied PWM < 15 ("PWM < 15 for sure"; the first ruling was < 25). While STOPPED, the 20 ms block is NOT appended to
// the recognition waveform: the developing ensemble, the preceding baseline and
// the held candidate are preserved exactly as they were, and stationary Hall
// values neither extend a magnetic plateau nor become baseline. Raw Hall is
// still acquired and recorded. When PWM returns to the moving range the
// waveform continues from the preserved state.
//
// A block is admitted when any of its reads had PWM >= 15. Startup is the one
// exception: from the operator's declaration until the first moving block, the
// stationary tracing IS the declared starting baseline (BL0), so it is admitted.
//
// PWM's only role is this pause. It is never passed to the recognizer or the
// navigator and has no part in recognizing a magnet, its polarity, the
// MM-BL-MM pattern or geographic advancement.
// ---------------------------------------------------------------------------
#include <stdint.h>

namespace rvm_shape {

static constexpr uint8_t STOPPED_BELOW_PWM = 15;

class WaveformPause {
 public:
  void declared() { startup_ = true; }
  // true: append this block to the recognition waveform
  bool admit(uint8_t blockMaxPwm) {
    if (blockMaxPwm >= STOPPED_BELOW_PWM) { startup_ = false; paused_ = false; return true; }
    paused_ = !startup_;
    return startup_;
  }
  bool paused() const { return paused_; }
  bool startup() const { return startup_; }
 private:
  bool startup_ = false, paused_ = false;
};

}  // namespace rvm_shape
