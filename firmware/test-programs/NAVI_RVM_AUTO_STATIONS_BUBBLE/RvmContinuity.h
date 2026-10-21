#pragma once
// ---------------------------------------------------------------------------
// HallContinuity — unified waveform continuity for stops and stalls.
// Mirror of analysis/navi_rvm_shape_20261018/unified/u03_mech.py (u_unified,
// U1), the mechanism evaluated in
// docs/NAVI_RVM_UNIFIED_WAVEFORM_CONTINUITY_EVALUATION_20261019.md and approved
// for implementation by David on 2026-10-19.
//
// PRINCIPLE: recognize the development of magnetic structure; preserve
// unfinished structure when development ceases; continue it when the pattern
// resumes. The cause (commanded stop, low-PWM stop, stall) is irrelevant, and
// this file reads nothing but the Hall tracing: no PWM, no commanded target,
// no IR, no speed.
//
// RULE. Every 20 ms block passes through a delay line of K-1 blocks. A block is
// a STATIONARY REPETITION, and is withheld from the recognizer, when
//   (1) it lies in some K-block span whose range (max - min) is <= BAND, and
//   (2) it lies off the baseline: |hall - baseline level| > M * baseline noise.
// Every other block passes, in order. On the baseline nothing is ever withheld
// (an extended baseline, moving or not, stays a baseline). There is no state
// machine and no resumption rule: when the tracing develops again its blocks
// are no longer inside any flat span and pass, so both ends of a plateau are
// removed by the same test. The unfinished ensemble, its baseline and the held
// candidate in the recognizer are untouched while blocks are withheld.
//
// BASELINE LEVEL AND NOISE (as evaluated). At a declaration (the operator
// declares the Hall sensor on a baseline), level = median and noise =
// sqrt(pi)/2 * mean |step| (floor 0.5, the recognizer's estimator) of the
// declared block and the 49 after it (finite blocks only, at least 10). Then
// the level follows an exponential mean (time constant 250 blocks = 5 s) of the
// blocks within M * noise of it.
//
// MOTOR OFF = PAUSED (operator 2026-10-20: "Can use PWM for standing"; the
// threshold is his 2026-10-18 ruling, STOPPED = applied PWM < 15). A block whose
// highest applied PWM is below CONT_STOPPED_PWM never enters the delay line,
// the recognizer or this rule: the motor-off Hall level (40-52 ADC from the
// moving level, creeping during a long dwell) cannot reach navigation. PWM is
// used only to know the motor is off, never as evidence of movement.
// A declaration made while paused takes effect at once (the next paused block);
// otherwise at the first admitted block at or after it.
//
// MOVING-BASELINE REFERENCE (2026-10-20, field: Otto MM141, Toby MM064, Arches
// 17:21). Both locomotives' Hall reads 40-52 ADC differently standing (motor
// off) than moving, even on plain baseline (Otto standing 1940 / moving ~1890;
// Toby 1915 / ~1863), and drifts 10-23 ADC while moving. Therefore:
//   * the reference level is the MOVING baseline: the recognizer's own baseline
//     level and noise at every recognized ensemble (onEnsemble), and nothing
//     else (no tracking between ensembles: a standing dwell creeps);
//   * a block is withheld when it lies in a flat 1 s span AND is more than
//     max(16 x noise, CONT_OFF_ADC = 25) off that reference: a motor-on
//     standing spell at arrival/departure (30-70 off), or a stall with the
//     sensor on a magnet;
//   * a moving drift (10-23 ADC seen) is not withheld (the old 16 x 0.5 = 8 ADC
//     margin withheld Otto's +10 drift before MM141);
//   * nothing is withheld before the first recognized ensemble after a
//     declaration (the declaration is made standing; Toby's standing level would
//     otherwise make all of his moving baseline "off").
// The withheld standing dwell is what lets the first magnet after a station be
// judged against the moving baseline (Otto leaving Arches past MM109).
// Not protected: a stall at PWM >= 15 on a flank within 25 ADC of the baseline.
// Margin note: Toby's largest moving step seen is +23 ADC, 2 ADC inside the floor.
//
// LOAD. One block in, at most one block out, per 20 ms: no backlog, no burst.
// Storage: K blocks.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <math.h>
#include "RvmTracing.h"

// Per-locomotive switch (LocoConfig.h). 0: nothing is ever withheld.
#ifndef RVM_CONTINUITY_ENABLED
#define RVM_CONTINUITY_ENABLED 1
#endif

namespace rvm_shape {

static constexpr uint16_t CONT_K = 50;            // span: 1 s
static constexpr float CONT_BAND = 24.0f;          // ADC: largest range still "unchanged"
static constexpr double CONT_M = 16.0;             // off-baseline margin, x baseline noise
static constexpr uint16_t CONT_INIT = 50;          // blocks establishing the declared baseline
static constexpr double CONT_TAU = 250.0;          // baseline-level time constant (blocks)
static constexpr double CONT_NOISE_FLOOR = 0.5;
static constexpr uint8_t CONT_STOPPED_PWM = 15;     // operator 2026-10-18: STOPPED = applied PWM < 15
static constexpr double CONT_OFF_ADC = 25.0;       // floor of the off-baseline margin: above moving drift (10-23 ADC seen), below motor-on standing (30-70)
static_assert(CONT_K >= CONT_INIT, "the declared baseline must lie inside the delay line");

// One pending operator declaration; applied when its block leaves the delay line.
struct ContDecl { uint32_t index; uint8_t last, next; int8_t dir; bool assigned; };

class HallContinuity {
 public:
  static constexpr uint32_t UNASSIGNED = 0xFFFFFFFFu;

  bool begin(Block* storage) { ring_ = storage; return ring_ != nullptr; }

  // Operator declaration, taking effect at the next admitted block.
  bool requestDeclaration(uint8_t last, uint8_t next, int8_t dir) {
    if (nPend_ >= MAX_PEND) return false;
    pend_[nPend_++] = ContDecl{UNASSIGNED, last, next, dir, false}; return true;
  }
  // Declaration at a known block index (offline replay).
  bool declareAt(uint32_t index, uint8_t last, uint8_t next, int8_t dir) {
    if (nPend_ >= MAX_PEND) return false;
    pend_[nPend_++] = ContDecl{index, last, next, dir, true}; return true;
  }
  void cancelDeclarations() { nPend_ = 0; }
  // The recognizer accepted an ensemble: it is complete. Close, and take the
  // recognizer's reference baseline (the moving baseline it judged it against).
  void onEnsemble(double level, double noise) {
    if (isfinite(level)) { level_ = level; haveLevel_ = true; }
    if (isfinite(noise)) noise_ = noise > CONT_NOISE_FLOOR ? noise : CONT_NOISE_FLOOR;
    armed_ = true;
  }
  bool armed() const { return armed_; }
  uint8_t pendingDeclarations() const { return nPend_; }

  // Admit the newest block. emit(const Block&) receives each block that joins the
  // recognition waveform; onDeclare(const ContDecl&) is called when a declaration
  // takes effect, before its block is decided (the caller starts the recognizer's
  // baseline and declares the navigator there). At most one block is decided.
  template <class E, class D> void admit(const Block& b, E&& emit, D&& onDeclare) {
    if (b.pwm < CONT_STOPPED_PWM) {                  // motor off: not part of the waveform
      ++pausedTotal_; paused_ = true;
      // A declaration is made standing (motor off). It must take effect now, or
      // navigation never tracks and AUTO (which needs TRACKING) can never start
      // (field 2026-10-20, r5). Decide what is still in the delay line (it is
      // from before the stop), then apply the pending declarations here.
      if (nPend_) {
        while (count_) decideOldest(emit, onDeclare);
        while (nPend_) {
          const ContDecl d = pend_[0];
          for (uint8_t q = 1; q < nPend_; ++q) pend_[q - 1] = pend_[q];
          --nPend_;
          initBaseline(); declaredOnce_ = true; armed_ = false;
          onDeclare(d);
        }
      }
      return;
    }
    paused_ = false;
    for (uint8_t k = 0; k < nPend_; ++k) if (!pend_[k].assigned) { pend_[k].index = b.index; pend_[k].assigned = true; }
    ring_[(head_ + count_) % CONT_K] = b; ++count_; ++admitted_;
    if (count_ == CONT_K) {
      float lo = INFINITY, hi = -INFINITY; bool finite = true;
      for (uint16_t k = 0; k < CONT_K; ++k) {
        const float h = ring_[(head_ + k) % CONT_K].hall;
        if (!isfinite(h)) { finite = false; break; }
        if (h < lo) lo = h;
        if (h > hi) hi = h;
      }
      const int64_t start = int64_t(admitted_) - CONT_K;          // ordinal of the span's first block
      if (finite && (hi - lo) <= CONT_BAND) lastFlatStart_ = start;
      decideOldest(emit, onDeclare);
    }
  }
  // End of a recording (offline replay): decide what is still in the delay line.
  template <class E, class D> void flush(E&& emit, D&& onDeclare) { while (count_) decideOldest(emit, onDeclare); }
  template <class E, class D> bool flushOne(E&& emit, D&& onDeclare) { if (!count_) return false; decideOldest(emit, onDeclare); return true; }

  bool paused() const { return paused_; }                    // the last block offered had the motor off
  uint32_t pausedTotal() const { return pausedTotal_; }
  bool withholding() const { return withholding_; }          // the last decided block was withheld
  uint32_t withheldTotal() const { return withheldTotal_; }
  uint32_t withheldRun() const { return withheldRun_; }       // length of the current / last withheld run
  uint32_t lastDecidedId() const { return lastId_; }
  float lastHall() const { return lastHall_; }
  bool baselineKnown() const { return haveLevel_; }
  double level() const { return level_; }
  double noise() const { return noise_; }
  uint16_t delayBlocks() const { return count_; }

 private:
  static constexpr uint8_t MAX_PEND = 4;
  Block* ring_ = nullptr;
  uint16_t head_ = 0, count_ = 0;
  uint64_t admitted_ = 0, decided_ = 0;            // ordinals of blocks admitted / decided
  int64_t lastFlatStart_ = -1000000;
  ContDecl pend_[MAX_PEND]; uint8_t nPend_ = 0;
  bool haveLevel_ = false, declaredOnce_ = false, withholding_ = false, armed_ = false;
  double level_ = 0, noise_ = CONT_NOISE_FLOOR;
  uint32_t withheldTotal_ = 0, withheldRun_ = 0, lastId_ = 0; float lastHall_ = NAN;
  uint32_t pausedTotal_ = 0; bool paused_ = false;

  void initBaseline() {                            // declared block and the CONT_INIT-1 after it
    double v[CONT_INIT]; uint16_t n = 0;
    for (uint16_t k = 0; k < count_ && k < CONT_INIT; ++k) {
      const float h = ring_[(head_ + k) % CONT_K].hall;
      if (isfinite(h)) v[n++] = h;
    }
    if (n < 10) return;                            // keep the previous level, if any
    double s = 0; for (uint16_t k = 1; k < n; ++k) s += fabs(v[k] - v[k - 1]);
    double nz = 1.7724538509055159 / 2.0 * s / double(n - 1);
    noise_ = nz > CONT_NOISE_FLOOR ? nz : CONT_NOISE_FLOOR;
    for (uint16_t i = 1; i < n; ++i) { const double x = v[i]; int j = int(i) - 1; while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; --j; } v[j + 1] = x; }
    level_ = (n % 2) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
    haveLevel_ = true;
  }

  template <class E, class D> void decideOldest(E&& emit, D&& onDeclare) {
    const Block& b = ring_[head_];
    const int64_t ord = int64_t(decided_);
    // declarations whose block this is take effect first
    for (uint8_t k = 0; k < nPend_;) {
      if (pend_[k].assigned && pend_[k].index <= b.index) {        // at, or the first admitted block after
        initBaseline(); declaredOnce_ = true; armed_ = false;
        const ContDecl d = pend_[k];
        for (uint8_t q = k + 1; q < nPend_; ++q) pend_[q - 1] = pend_[q];
        --nPend_;
        onDeclare(d);
      } else ++k;
    }
    const bool flat = lastFlatStart_ >= ord - (CONT_K - 1);
    const float h = b.hall;
    const double offMargin = CONT_M * noise_ > CONT_OFF_ADC ? CONT_M * noise_ : CONT_OFF_ADC;
    const bool near = haveLevel_ && isfinite(h) && fabs(double(h) - level_) <= offMargin;
    const bool off = haveLevel_ && isfinite(h) && !near;
    const bool withhold = RVM_CONTINUITY_ENABLED && declaredOnce_ && armed_ && flat && off;
    // No tracking between ensembles: a long dwell creeps the standing level (Otto, Arches 17:11-17:21:
    // 1892 -> 1940 over 10 min) and tracking it made the moving baseline look 'off'. The reference is
    // only ever the recognizer's moving baseline (onEnsemble), or the declared level before the first one.
    lastId_ = b.index; lastHall_ = h;
    if (withhold) { ++withheldTotal_; withheldRun_ = withholding_ ? withheldRun_ + 1 : 1; }
    withholding_ = withhold;
    const Block out = b;
    head_ = uint16_t((head_ + 1) % CONT_K); --count_; ++decided_;
    if (!withhold) emit(out);
  }
};

}  // namespace rvm_shape
