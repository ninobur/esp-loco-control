#pragma once
// ---------------------------------------------------------------------------
// NaviObservation — the 20 ms Hall tracing from the 5 kHz native ADC samples.
// Ported unchanged from NAVI_RVM_AUTO_STATIONS_BUBBLE r5.1 RvmTracing.h: the
// prior audits (Claude and Codex, 2026-10-24) both found this stage already
// unconditional on PWM and on navigation state, consistent with the
// conference's continuous-observation requirement (The Power of Normal,
// Principle 9). Nothing here is new logic; only the name and file changed.
//
// A block is the plain mean of every native read whose timestamp falls in
// [20k ms, 20(k+1) ms) of the boot clock. No smoothing, no median, no
// interpolation. A block is a gap (NaN) when it holds fewer than half of its
// nominal 100 reads or touches a serial jump (lost reads). Route direction of
// a block = that of its last read (1 = CW, 2 = CCW, 0 = undeclared). pwm = the
// highest applied PWM among the block's reads; cmd = the commanded (target)
// PWM at the block's last read. Both are carried for diagnostics only — no
// downstream stage in this build gates admission on PWM.
//
// OBSERVATION LOSS (Codex audit, 2026-10-24, finding C3's third point): the
// native-read-to-block path here cannot itself lose data, but the caller's
// handoff to the candidate/encounter stage (a queue) can, if that queue is
// ever full. That is not a property of this class; see ObservationHealth
// below, which the caller updates and the arrival-boundary clock reads, so a
// silent drop is visible rather than silently discarding evidence.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <math.h>

namespace navi_power {

struct Block { uint32_t index; float hall; uint8_t dir; uint16_t n; uint8_t pwm; uint8_t cmd; };

class Tracing {
 public:
  static constexpr uint32_t BLOCK_US = 20000;
  static constexpr uint16_t MIN_READS = 50;     // half of 5 kHz x 20 ms

  // Feed one native read. Completed blocks (including empty ones) are passed to
  // emit(Block) in order. Returns the number emitted.
  template <class F> int add(uint64_t us, uint16_t adc, uint8_t dir, uint32_t serial, uint8_t pwm, uint8_t cmd, F&& emit) {
    const uint64_t blk = us / BLOCK_US;
    int emitted = 0;
    if (!started_) { started_ = true; base_ = blk; cur_ = 0; reset(); lastSerial_ = serial; }
    else {
      const bool jump = serial != lastSerial_ + 1;
      lastSerial_ = serial;
      const uint64_t rel = blk - base_;
      if (rel > cur_) {
        if (jump) bad_ = true;                 // the block before the jump
        emitted += flush(emit);
        while (cur_ + 1 < rel) { ++cur_; reset(); emitted += flush(emit); }   // empty blocks
        ++cur_; reset();
      }
      if (jump) bad_ = true;                   // the block after the jump
    }
    sum_ += adc; ++n_; dir_ = dir; cmd_ = cmd; if (pwm > pwm_) pwm_ = pwm;
    return emitted;
  }
  uint64_t firstBlockUs() const { return base_ * BLOCK_US; }

 private:
  bool started_ = false, bad_ = false;
  uint64_t base_ = 0; uint32_t cur_ = 0, lastSerial_ = 0;
  double sum_ = 0; uint16_t n_ = 0; uint8_t dir_ = 0, pwm_ = 0, cmd_ = 0;
  void reset() { sum_ = 0; n_ = 0; bad_ = false; dir_ = 0; pwm_ = 0; cmd_ = 0; }
  template <class F> int flush(F&& emit) {
    Block b; b.index = cur_; b.n = n_; b.dir = dir_; b.pwm = pwm_; b.cmd = cmd_;
    b.hall = (n_ >= MIN_READS && !bad_) ? float(sum_ / n_) : NAN;
    emit(b); return 1;
  }
};

// Visible accounting for any block that fails to reach the candidate/encounter
// stage (a full handoff queue). This does not fix the loss; it makes it an
// observable fact rather than a silent one, per Codex's C3 finding. The
// arrival-boundary clock treats a nonzero recent drop count as reduced
// confidence in elapsed-time accounting, never as a reason to stop by itself.
struct ObservationHealth {
  uint32_t blocksEmitted = 0, blocksDropped = 0;
  void noteEmitted() { ++blocksEmitted; }
  void noteDropped() { ++blocksDropped; }
};

}  // namespace navi_power
