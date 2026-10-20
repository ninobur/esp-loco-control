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
// INSIDE AN OPEN ENSEMBLE ONLY (operator, 2026-10-20). "On baseline: a flat
// tracing is simply baseline. Inside a magnetic ensemble: if the tracing stops
// changing, the locomotive may have stopped over the magnet; the encounter
// remains open until the tracing resumes progressing and the ensemble is
// completed." So a block is withheld only while an ensemble is OPEN:
//   OPEN   when a block entering the delay line departs >= CONT_OPEN_ADC (70,
//          the recognizer's excursion gate) from the reference level;
//   CLOSED when the recognizer accepts an ensemble (onEnsemble), which also
//          sets the reference to the recognizer's own baseline level and noise.
// While closed, the reference follows the moving baseline (blocks within
// CONT_TRACK_ADC, time constant CONT_TRACK_TAU) and nothing is withheld, so a
// drifting moving baseline (Otto +10 ADC at MM137-141, Toby's motor shift)
// can no longer be mistaken for a stall. Field cases: Otto CONTRADICTION at
// MM141 and Toby at MM064, 2026-10-20.
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
static constexpr double CONT_OPEN_ADC = 70.0;      // = RvmShape MIN_EXCURSION_ADC
static constexpr double CONT_TRACK_ADC = 35.0;     // closed: baseline tracking capture
static constexpr double CONT_TRACK_TAU = 50.0;     // closed: tracking time constant (blocks, 1 s)
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
    open_ = false;
    if (isfinite(level)) { level_ = level; haveLevel_ = true; }
    if (isfinite(noise)) noise_ = noise > CONT_NOISE_FLOOR ? noise : CONT_NOISE_FLOOR;
  }
  bool open() const { return open_; }
  uint8_t pendingDeclarations() const { return nPend_; }

  // Admit the newest block. emit(const Block&) receives each block that joins the
  // recognition waveform; onDeclare(const ContDecl&) is called when a declaration
  // takes effect, before its block is decided (the caller starts the recognizer's
  // baseline and declares the navigator there). At most one block is decided.
  template <class E, class D> void admit(const Block& b, E&& emit, D&& onDeclare) {
    for (uint8_t k = 0; k < nPend_; ++k) if (!pend_[k].assigned) { pend_[k].index = b.index; pend_[k].assigned = true; }
    ring_[(head_ + count_) % CONT_K] = b; ++count_; ++admitted_;
    if (haveLevel_ && declaredOnce_ && isfinite(b.hall) && fabs(double(b.hall) - level_) >= CONT_OPEN_ADC) open_ = true;
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
  bool haveLevel_ = false, declaredOnce_ = false, withholding_ = false, open_ = false;
  double level_ = 0, noise_ = CONT_NOISE_FLOOR;
  uint32_t withheldTotal_ = 0, withheldRun_ = 0, lastId_ = 0; float lastHall_ = NAN;

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
      if (pend_[k].assigned && pend_[k].index == b.index) {
        initBaseline(); declaredOnce_ = true; open_ = false;
        const ContDecl d = pend_[k];
        for (uint8_t q = k + 1; q < nPend_; ++q) pend_[q - 1] = pend_[q];
        --nPend_;
        onDeclare(d);
      } else ++k;
    }
    const bool flat = lastFlatStart_ >= ord - (CONT_K - 1);
    const float h = b.hall;
    const bool off = haveLevel_ && isfinite(h) && fabs(double(h) - level_) > CONT_M * noise_;
    if (!open_ && haveLevel_ && isfinite(h) && fabs(double(h) - level_) <= CONT_TRACK_ADC) level_ += (double(h) - level_) / CONT_TRACK_TAU;
    const bool withhold = RVM_CONTINUITY_ENABLED && declaredOnce_ && open_ && flat && off;
    lastId_ = b.index; lastHall_ = h;
    if (withhold) { ++withheldTotal_; withheldRun_ = withholding_ ? withheldRun_ + 1 : 1; }
    withholding_ = withhold;
    const Block out = b;
    head_ = uint16_t((head_ + 1) % CONT_K); --count_; ++decided_;
    if (!withhold) emit(out);
  }
};

}  // namespace rvm_shape
