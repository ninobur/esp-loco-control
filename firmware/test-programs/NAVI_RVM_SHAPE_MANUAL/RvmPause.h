#pragma once
// ---------------------------------------------------------------------------
// WaveformPause — STOP/GO continuity.
//
// STOP (operator ruling 2026-10-18: STOPPED = applied PWM < 15, "for sure"):
// a STOPPED block is not appended to the recognition waveform. Everything
// already in the waveform — the preceding baseline, the unfinished ensemble,
// the held candidate — is preserved exactly. Raw Hall is still acquired and
// recorded.
//
// GO (assignment 2026-10-18, pattern-based resumption): PWM >= 15 does NOT by
// itself resume the waveform; Otto stays still for seconds after it. While
// paused, the Hall blocks are an observation of the STATIONARY state at the
// stop (on Otto, PWM < 15 has always been stationary): their range [min, max]
// is that state's own level and fluctuation, observed at this very stop. After
// GO, a block inside that range is a stationary repetition and is withheld.
// The pattern has resumed when the tracing leaves the stationary state and
// stays out on the same side in the next block too (a single block outside is
// a fluctuation: Otto's recorded stationary Hall exceeds its own earlier range
// now and then). From the first of those two blocks on, every block is
// appended, joining the preserved waveform with no stationary plateau.
// Nothing here is an amplitude, duration, speed or PWM criterion for the
// resumption; the reference is the stationary state observed at the stop.
// A re-stop before resumption continues the same stationary observation.
//
// Startup: from the operator's declaration until the first moving block, the
// stationary tracing IS the declared starting baseline (BL0) and is appended.
//
// PWM is never passed to the recognizer or the navigator.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <math.h>
#include "RvmTracing.h"

namespace rvm_shape {

static constexpr uint8_t STOPPED_BELOW_PWM = 15;

enum class PauseState : uint8_t { Running = 0, Startup, Stopped, Awaiting };

class WaveformPause {
 public:
  void declared() { state_ = PauseState::Startup; havePending_ = false; }
  // Decide what this block contributes to the recognition waveform. Writes 0, 1
  // or 2 blocks (oldest first) to out and returns how many.
  int admit(const Block& b, Block out[2]) {
    const bool stopped = b.pwm < STOPPED_BELOW_PWM;
    switch (state_) {
      case PauseState::Startup:
        if (!stopped) state_ = PauseState::Running;
        out[0] = b; return 1;                          // declared stationary baseline, then moving
      case PauseState::Running:
        if (!stopped) { out[0] = b; return 1; }
        state_ = PauseState::Stopped; n_ = 0; lo_ = INFINITY; hi_ = -INFINITY; havePending_ = false;
        observe(b.hall); return 0;
      case PauseState::Stopped:
        if (stopped) { observe(b.hall); return 0; }
        if (n_ == 0) { state_ = PauseState::Running; out[0] = b; return 1; }   // nothing observed: no reference
        state_ = PauseState::Awaiting; havePending_ = false;
        return awaiting(b, out);
      case PauseState::Awaiting:
        if (stopped) {                                 // stopped again before resuming: same stop
          if (havePending_) { ++withheld_; havePending_ = false; }
          state_ = PauseState::Stopped; observe(b.hall); return 0;
        }
        return awaiting(b, out);
    }
    out[0] = b; return 1;
  }
  PauseState state() const { return state_; }
  bool paused() const { return state_ == PauseState::Stopped || state_ == PauseState::Awaiting; }
  float stationaryLow() const { return lo_; }
  float stationaryHigh() const { return hi_; }
  uint32_t stationaryBlocks() const { return n_; }
  uint32_t withheldAfterGo() const { return withheld_; }
  uint32_t resumptions() const { return resumptions_; }

 private:
  PauseState state_ = PauseState::Running;
  float lo_ = INFINITY, hi_ = -INFINITY; uint32_t n_ = 0, withheld_ = 0, resumptions_ = 0;
  bool havePending_ = false; Block pending_{}; int8_t pendingSide_ = 0;
  void observe(float h) { if (isfinite(h)) { if (h < lo_) lo_ = h; if (h > hi_) hi_ = h; ++n_; } }
  int8_t side(float h) const { return (!isfinite(h) || (h >= lo_ && h <= hi_)) ? 0 : (h > hi_ ? 1 : -1); }
  int awaiting(const Block& b, Block out[2]) {
    const int8_t s = side(b.hall);
    if (havePending_) {
      if (s != 0 && s == pendingSide_) {               // left the stationary state and stayed out: resumed
        out[0] = pending_; out[1] = b; havePending_ = false;
        state_ = PauseState::Running; ++resumptions_; return 2;
      }
      ++withheld_; havePending_ = false;               // the pending block was a fluctuation
    }
    if (s != 0) { pending_ = b; pendingSide_ = s; havePending_ = true; return 0; }
    ++withheld_; return 0;                             // stationary repetition
  }
};

}  // namespace rvm_shape
