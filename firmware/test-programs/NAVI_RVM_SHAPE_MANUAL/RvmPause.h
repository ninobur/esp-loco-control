#pragma once
// ---------------------------------------------------------------------------
// WaveformPause — STOP/GO continuity of the recognition waveform.
//
// STOP (operator ruling 2026-10-18: STOPPED = applied PWM < 15, "for sure"):
// a STOPPED block is not appended. The preceding baseline, the unfinished
// ensemble and the held candidate are preserved exactly; raw Hall is still
// acquired and recorded. While stopped, the Hall blocks are an observation of
// the STATIONARY state at this stop: their range [min, max] is that state's own
// level and fluctuation (on Otto, PWM < 15 has always been stationary).
//
// GO (assignment 2026-10-18, pattern-based resumption): PWM >= 15 does not by
// itself resume the waveform; Otto stays still for seconds after it. A block
// inside the stationary range is a repetition and is withheld; the pattern has
// resumed when the tracing leaves the range and stays out on the same side in
// the next block. From the first of those two blocks on, every block is
// appended.
//
// ARRIVAL (mirror rule, approved 2026-10-18): Otto also stands still on arrival
// while the applied PWM is still >= 15 (he stops near PWM 26). From the moment
// the operator COMMANDS a stop (target PWM < 15) the blocks are held back
// instead of appended, up to HOLD_MAX of the most recent (older ones are
// released in order). At GO the held blocks are judged against the same
// stationary range, newest first: blocks inside it, or single excursions, are
// stationary repetitions and are dropped; the walk stops at the first block
// that is outside the range together with the block before it on the same side
// (the magnetic structure was still developing there). What remains is
// appended, before the resumed waveform. If the stop command is withdrawn
// before PWM falls below 15, everything held is released unchanged.
//
// Startup: from the operator's declaration until the first moving block, the
// stationary tracing IS the declared starting baseline (BL0) and is appended.
// A declaration first releases anything still held back, unchanged.
//
// PWM and the commanded target are read only here. Nothing in this file is a
// recognition criterion: it decides which 20 ms blocks the recognizer sees,
// and the reference for that is the stationary state observed at the stop.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <math.h>
#include "RvmTracing.h"

namespace rvm_shape {

static constexpr uint8_t STOPPED_BELOW_PWM = 15;
static constexpr uint16_t HOLD_MAX = 512;   // must hold Otto's arrival plateau (measured max 291 blocks)

enum class PauseState : uint8_t { Running = 0, Startup, Stopped, Awaiting };

class WaveformPause {
 public:
  // Storage for HOLD_MAX blocks, supplied once by the caller (heap on the ESP32).
  bool begin(Block* storage) { hold_ = storage; return hold_ != nullptr; }
  // Operator declaration: anything held back is released first (it was traversed
  // before the stop and belongs to the old context); then the startup baseline.
  template <class F> void declared(F&& emit) { releaseAll(emit); state_ = PauseState::Startup; havePending_ = false; }

  // Feed one tracing block. emit(const Block&) is called for every block that
  // joins the recognition waveform, oldest first.
  template <class F> void admit(const Block& b, F&& emit) {
    const bool stopped = b.pwm < STOPPED_BELOW_PWM;
    switch (state_) {
      case PauseState::Startup:
        if (!stopped) state_ = PauseState::Running;
        emit(b); return;
      case PauseState::Running:
        if (stopped) { state_ = PauseState::Stopped; n_ = 0; lo_ = INFINITY; hi_ = -INFINITY; havePending_ = false; observe(b.hall); return; }
        if (b.cmd < STOPPED_BELOW_PWM && hold_) {        // stop commanded: hold back
          if (held_ == HOLD_MAX) { emit(heldAt(0)); head_ = (head_ + 1) % HOLD_MAX; --held_; ++releasedEarly_; }
          hold_[(head_ + held_) % HOLD_MAX] = b; ++held_;
          return;
        }
        releaseAll(emit); emit(b); return;
      case PauseState::Stopped:
        if (stopped) { observe(b.hall); return; }
        if (n_ == 0) { releaseAll(emit); state_ = PauseState::Running; emit(b); return; }
        trimArrival(); releaseAll(emit);
        state_ = PauseState::Awaiting; havePending_ = false;
        awaiting(b, emit); return;
      case PauseState::Awaiting:
        if (stopped) { if (havePending_) { ++withheld_; havePending_ = false; } state_ = PauseState::Stopped; observe(b.hall); return; }
        awaiting(b, emit); return;
    }
  }
  PauseState state() const { return state_; }
  bool paused() const { return state_ == PauseState::Stopped || state_ == PauseState::Awaiting; }
  bool holding() const { return held_ > 0; }
  float stationaryLow() const { return lo_; }
  float stationaryHigh() const { return hi_; }
  uint32_t stationaryBlocks() const { return n_; }
  uint32_t withheldAfterGo() const { return withheld_; }
  uint32_t trimmedOnArrival() const { return trimmed_; }
  uint32_t lastArrivalTrim() const { return lastTrim_; }
  uint32_t releasedEarly() const { return releasedEarly_; }
  uint32_t resumptions() const { return resumptions_; }

 private:
  PauseState state_ = PauseState::Running;
  float lo_ = INFINITY, hi_ = -INFINITY;
  uint32_t n_ = 0, withheld_ = 0, resumptions_ = 0, trimmed_ = 0, lastTrim_ = 0, releasedEarly_ = 0;
  bool havePending_ = false; Block pending_{}; int8_t pendingSide_ = 0;
  Block* hold_ = nullptr; uint16_t head_ = 0, held_ = 0;

  const Block& heldAt(uint16_t k) const { return hold_[(head_ + k) % HOLD_MAX]; }
  void observe(float h) { if (isfinite(h)) { if (h < lo_) lo_ = h; if (h > hi_) hi_ = h; ++n_; } }
  int8_t side(float h) const { return (!isfinite(h) || (h >= lo_ && h <= hi_)) ? 0 : (h > hi_ ? 1 : -1); }
  template <class F> void releaseAll(F&& emit) {
    for (uint16_t k = 0; k < held_; ++k) emit(heldAt(k));
    held_ = 0; head_ = 0;
  }
  void trimArrival() {                                 // newest first, mirror of the resumption rule
    int m = int(held_) - 1;
    while (m >= 0) {
      const int8_t s = side(heldAt(uint16_t(m)).hall);
      if (s != 0 && m >= 1 && side(heldAt(uint16_t(m - 1)).hall) == s) break;
      --m;
    }
    lastTrim_ = held_ - uint16_t(m + 1); trimmed_ += lastTrim_;
    held_ = uint16_t(m + 1);
  }
  template <class F> void awaiting(const Block& b, F&& emit) {
    const int8_t s = side(b.hall);
    if (havePending_) {
      if (s != 0 && s == pendingSide_) {               // left the stationary state and stayed out: resumed
        emit(pending_); emit(b); havePending_ = false;
        state_ = PauseState::Running; ++resumptions_; return;
      }
      ++withheld_; havePending_ = false;               // the pending block was a fluctuation
    }
    if (s != 0) { pending_ = b; pendingSide_ = s; havePending_ = true; return; }
    ++withheld_;                                       // stationary repetition
  }
};

}  // namespace rvm_shape
