#pragma once
// ---------------------------------------------------------------------------
// RvmTracing — the 20 ms Hall tracing from the 5 kHz native ADC samples.
// Same rule as analysis/.../s01_tracing.py: a block is the plain mean of every
// native read whose timestamp falls in [20k ms, 20(k+1) ms) of the boot clock.
// No smoothing, no median, no interpolation. A block is a gap (NaN) when it
// holds fewer than half of its nominal 100 reads or touches a serial jump
// (lost reads). Route direction of a block = that of its last read
// (1 = CW, 2 = CCW, 0 = undeclared), as in the recordings.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <math.h>

namespace rvm_shape {

struct Block { uint32_t index; float hall; uint8_t dir; uint16_t n; };

class Tracing {
 public:
  static constexpr uint32_t BLOCK_US = 20000;
  static constexpr uint16_t MIN_READS = 50;     // half of 5 kHz x 20 ms

  // Feed one native read. Completed blocks (including empty ones) are passed to
  // emit(Block) in order. Returns the number emitted.
  template <class F> int add(uint64_t us, uint16_t adc, uint8_t dir, uint32_t serial, F&& emit) {
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
    sum_ += adc; ++n_; dir_ = dir;
    return emitted;
  }
  uint64_t firstBlockUs() const { return base_ * BLOCK_US; }

 private:
  bool started_ = false, bad_ = false;
  uint64_t base_ = 0; uint32_t cur_ = 0, lastSerial_ = 0;
  double sum_ = 0; uint16_t n_ = 0; uint8_t dir_ = 0;
  void reset() { sum_ = 0; n_ = 0; bad_ = false; dir_ = 0; }
  template <class F> int flush(F&& emit) {
    Block b; b.index = cur_; b.n = n_; b.dir = dir_;
    b.hall = (n_ >= MIN_READS && !bad_) ? float(sum_ / n_) : NAN;
    emit(b); return 1;
  }
};

}  // namespace rvm_shape
