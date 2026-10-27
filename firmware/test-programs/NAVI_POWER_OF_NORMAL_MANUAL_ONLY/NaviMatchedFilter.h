// NaviMatchedFilter.h — portable matched-filter Hall recognizer.
//
// Design: ARCHITECTURE.md section 2 (single 240 ms scale, pad 0.5,
// level-only window, r >= 0.7, |A| >= 70, NMS 120 ms, greedy causal
// declaration). Research reference: research/navi_matched_filter/mf.py
// (matched_filter, events_from_stack) and exp_continuous.py
// (causal_events). This file mirrors those functions exactly; the host
// harness (phase 1) verifies the agreement on the corpus.
//
// NO Arduino dependencies. Shared by the host harness and the firmware:
// hallTask feeds 1 ms tick medians, rvmTask drains recognized events.
//
// Pipeline:
//   1 ms tick median (5 Hall reads, hallTask) -> pair-average to 500 Hz
//   (2 ms bins) -> ring buffer -> lagging level-only correlation
//   (window centred 120 bins back) -> local maxima of |r| -> greedy
//   causal declaration when the window and the NMS neighbourhood are
//   complete (t_centre + 361 ms, i.e. t_peak + 359 ms).
//
// Cost: ~723 float ops per 2 ms bin (direct window sums, no running-sum
// state to drift — auditable over clever). ~360k FLOPS at 500 Hz.

#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>
#include "NaviWaveformTemplate.h"

// Median of 5 int16 values (insertion sort; the median is element 2).
// Used by the firmware's hallTask to reduce the 5 native reads of one 1 ms
// tick to a single tick median, and by the host harness to reproduce that
// conditioning exactly from recorded native samples.
inline int16_t medianOf5(int16_t v[5]) {
  for (int i = 1; i < 5; ++i) {
    int16_t t = v[i];
    int j = i - 1;
    while (j >= 0 && v[j] > t) {
      v[j + 1] = v[j];
      --j;
    }
    v[j + 1] = t;
  }
  return v[2];
}

struct NaviRecognizedEvent {
  uint64_t tPassageUs;     // template-peak time (same clock as the input)
  uint64_t tRecognizedUs;  // decision time (tPassage + 359 ms)
  float r;                 // shape score (Pearson correlation, level-only)
  float amplitude;         // least-squares template coefficient, ADC counts
  int8_t polarity;         // +1 North (ADC rises), -1 South
};

class NaviMatchedFilter {
 public:
  // Thresholds from the design (section 2.4, 9.1). Constants, not tuning.
  static constexpr float kRMin = 0.7f;
  static constexpr float kAMin = 70.0f;
  // NMS radius: 120 ms = 60 bins (design section 2.4, research nms=0.5*D).
  static constexpr int kNmsRadiusBins = 60;
  // Decision lag: a candidate is declared when the evaluated centre
  // reaches candidate centre + 60 bins (its NMS neighbourhood complete).
  static constexpr int kDecisionLagBins = 60;
  // t_centre -> t_declared = 241 ms (window half) + 120 ms (NMS) = 361 ms.
  static constexpr uint32_t kReadyLagUs = 361000u;
  // Template peak sits 1 bin (+2 ms) after the window centre.
  static constexpr uint32_t kPeakOffUs = 2000u;
  // A tick arriving more than this after the previous tick is a real gap
  // (ticks lost). See pushTickMedian.
  static constexpr uint32_t kGapUs = 100000u;

  NaviMatchedFilter() { reset(); }

  void reset() {
    binCount_ = 0;
    pendingTick_ = false;
    pendingVal_ = 0.0f;
    pendingUs_ = 0;
    haveLastTick_ = false;
    lastTickUs_ = 0;
    prevCentreValid_ = false;
    prevAbsR_ = -1.0f;
    prevPrevAbsR_ = -1.0f;
    prevR_ = 0.0f;
    prevA_ = 0.0f;
    prevCentreIdx_ = -1;
    prevCentreUs_ = 0;
    candCount_ = 0;
    candHead_ = 0;
    pendingCount_ = 0;
    declCount_ = 0;
    declHead_ = 0;
    evHead_ = 0;
    evTail_ = 0;
    for (int i = 0; i < kRingBins; ++i) {
      ringValid_[i] = false;
      ringVal_[i] = 0.0f;
      ringUs_[i] = 0;
    }
  }

  // Feed one 1 ms tick median (median of the 5 Hall reads in that tick).
  // tUs is the tick time on the caller's clock. Ticks must arrive in
  // order, ~1000 us apart. A jump of more than kGapUs is a real gap (ticks
  // lost, e.g. a median-queue overflow): the pre-gap bins are stale, so
  // the core resets to a fresh start and feeds this tick into the fresh
  // core. Short jumps need no handling: the correlation is over
  // consecutive fed bins, a few missing bins do not change the shape, and
  // the passage time comes from the bin's own timestamp, which stays
  // correct. (The tick clock jitters by up to ~4.5 ms in the recorded
  // Level 1 data from FreeRTOS scheduling, so a 2 ms threshold fired
  // constantly — 344 false gaps in 96.8 s, each invalidating 240 ms of
  // windows — and missed 13 of 20 magnets. 100 ms is 20x the largest
  // observed jitter.)
  void pushTickMedian(int16_t v, uint64_t tUs) {
    if (haveLastTick_ && tUs - lastTickUs_ > kGapUs) reset();
    lastTickUs_ = tUs;
    haveLastTick_ = true;
    if (!pendingTick_) {
      pendingVal_ = (float)v;
      pendingUs_ = tUs;
      pendingTick_ = true;
      return;
    }
    // Second tick of the pair: average to a 2 ms bin, timestamped by
    // the second tick.
    pushBin(0.5f * (pendingVal_ + (float)v), tUs);
    pendingTick_ = false;
  }

  // True while a declared event is waiting to be drained.
  bool hasEvent() const { return evHead_ != evTail_; }

  // Debug: the most recently evaluated centre (for host verification).
  // Returns the centre time, r, A, and whether the window was valid.
  struct DebugCorr {
    uint64_t centreUs;
    float r, a;
    bool valid;
  };
  DebugCorr lastCorr() const { return lastCorr_; }

  // Drain the oldest declared event. Only call when hasEvent() is true.
  NaviRecognizedEvent nextEvent() {
    NaviRecognizedEvent e = evQueue_[evHead_];
    evHead_ = (evHead_ + 1) % kEvQueue;
    return e;
  }

 private:
  static constexpr int kRingBins = 512;  // > 241 (window) + 60 (NMS) + margin
  static constexpr int kCandRing = 64;   // candidate history (see below)
  static constexpr int kEvQueue = 16;    // declared-event queue (cf. judgedQ)
  static constexpr int kDeclHist = 8;    // declared-centre history

  struct Candidate {
    int32_t centreIdx;
    float absR;
    float r;
    float a;
    uint64_t centreUs;
  };

  float ringVal_[kRingBins];
  bool ringValid_[kRingBins];
  uint64_t ringUs_[kRingBins];
  int32_t binCount_;  // bins pushed so far (monotonic index)

  // Tick pairing state.
  bool pendingTick_;
  float pendingVal_;
  uint64_t pendingUs_;
  uint64_t lastTickUs_;
  bool haveLastTick_;

  // Previous evaluated centres (for the local-max test).
  bool prevCentreValid_;
  float prevAbsR_, prevPrevAbsR_, prevR_, prevA_;
  int32_t prevCentreIdx_;
  uint64_t prevCentreUs_;

  // Debug: most recently evaluated centre.
  DebugCorr lastCorr_ = {0, 0.0f, 0.0f, false};

  // Candidate ring: every candidate found, in centre order, newest at
  // candHead_-1. The oldest pendingCount_ entries are undecided; the
  // rest are decided but retained, because the research's rule (b) lets
  // a stronger *skipped* candidate block a later one (causal_events:
  // `known` holds every candidate processed so far, declared or not).
  // Maxima are at least 2 bins apart, so at most 30 candidates can fall
  // inside any 60-bin decision window; 64 entries is provably enough.
  Candidate cand_[kCandRing];
  int candHead_;      // next write slot
  int candCount_;     // entries in the ring (<= kCandRing)
  int pendingCount_;  // undecided entries (the newest ones)

  // Declared centres, ring (for rule (a)).
  int32_t declHist_[kDeclHist];
  int declHead_;
  int declCount_;

  NaviRecognizedEvent evQueue_[kEvQueue];
  int evHead_, evTail_;

  void pushBin(float v, uint64_t tUs) {
    int slot = (int)(binCount_ % kRingBins);
    ringVal_[slot] = v;
    ringValid_[slot] = true;
    ringUs_[slot] = tUs;
    ++binCount_;
    // Evaluate the centre 120 bins back: its window [c-120, c+120] is
    // complete exactly when bin c+120 arrives.
    int32_t c = (int32_t)binCount_ - 1 - NAVI_TEMPLATE_HALF;
    if (c >= NAVI_TEMPLATE_HALF) evaluateCentre(c);
    decideReadyCandidates();
  }

  // Compute r and A for the window centred at bin index c.
  // Returns false when the window is incomplete or touches an invalid bin.
  bool correlateCentre(int32_t c, float& rOut, float& aOut) const {
    const int L = NAVI_TEMPLATE_LEN;  // 241
    double sum = 0.0, sumSq = 0.0, dot = 0.0;
    for (int k = -NAVI_TEMPLATE_HALF; k <= NAVI_TEMPLATE_HALF; ++k) {
      int32_t idx = c + k;
      int slot = (int)(idx % kRingBins);
      if (idx < 0 || !ringValid_[slot]) return false;
      double x = (double)ringVal_[slot];
      sum += x;
      sumSq += x * x;
      dot += x * (double)NAVI_TEMPLATE_TN[k + NAVI_TEMPLATE_HALF];
    }
    // Level-only Pearson correlation (research mf.matched_filter,
    // detrend='mean'): r = dot(x, Tn) / sqrt(sum(x^2) - sum(x)^2 / L),
    // A = dot(x, Tn) / ||T - mean(T)||. Tn is unit-norm, so dot(x, Tn)
    // alone is NOT the amplitude — it overstates A by the template norm
    // (~5.24x). sum(Tn) = 0, so both are invariant to any constant offset
    // of x — no baseline is needed.
    double res2 = sumSq - sum * sum / (double)L;
    if (res2 <= 1e-6) {
      rOut = 0.0f;
      aOut = (float)(dot / (double)NAVI_TEMPLATE_NORM);
      return true;
    }
    rOut = (float)(dot / std::sqrt(res2));
    aOut = (float)(dot / (double)NAVI_TEMPLATE_NORM);
    return true;
  }

  uint64_t centreTimeUs(int32_t c) const {
    return ringUs_[(int)(c % kRingBins)];
  }

  // Evaluate one centre. A centre becomes a candidate only once its
  // right neighbour is known, so this tests the *previous* centre
  // against the one just evaluated (local max: |r| >= left, |r| > right;
  // research: absr >= roll(absr,1) and absr > roll(absr,-1)).
  void evaluateCentre(int32_t c) {
    float r = 0.0f, a = 0.0f;
    bool ok = correlateCentre(c, r, a);
    lastCorr_ = {centreTimeUs(c), r, a, ok};
    float absR = ok ? std::fabs(r) : -1.0f;
    if (prevCentreValid_) {
      if (prevAbsR_ >= 0.0f && prevAbsR_ >= prevPrevAbsR_ &&
          prevAbsR_ > absR) {
        addCandidate(prevCentreIdx_, prevAbsR_, prevR_, prevA_,
                     prevCentreUs_);
      }
      prevPrevAbsR_ = prevAbsR_;
    }
    prevAbsR_ = absR;
    prevR_ = r;
    prevA_ = a;
    prevCentreIdx_ = c;
    prevCentreUs_ = centreTimeUs(c);
    prevCentreValid_ = true;
  }

  void addCandidate(int32_t centreIdx, float absR, float r, float a,
                    uint64_t centreUs) {
    if (absR < kRMin || std::fabs(a) < kAMin) return;
    // If the ring is full, the oldest entry must already be decided
    // (at most 30 candidates fit in any decision window); drop it.
    if (candCount_ == kCandRing) {
      candCount_ = kCandRing - 1;
      if (pendingCount_ > candCount_) pendingCount_ = candCount_;
    }
    Candidate& cd = cand_[candHead_];
    cd.centreIdx = centreIdx;
    cd.absR = absR;
    cd.r = r;
    cd.a = a;
    cd.centreUs = centreUs;
    candHead_ = (candHead_ + 1) % kCandRing;
    ++candCount_;
    ++pendingCount_;
  }

  // Index of the i-th oldest candidate in the ring (i = 0 is oldest).
  int candSlot(int i) const {
    return (candHead_ - candCount_ + i + 2 * kCandRing) % kCandRing;
  }

  // Decide every candidate whose NMS neighbourhood is complete, in
  // centre order (ready order). Research causal_events semantics:
  // declare unless (a) a declared event lies within 60 bins, or (b) a
  // stronger knowable candidate (any earlier candidate, declared or
  // not) lies within 60 bins. Declarations are final.
  void decideReadyCandidates() {
    if (binCount_ < 1) return;
    int32_t evaluated = (int32_t)binCount_ - 1 - NAVI_TEMPLATE_HALF;
    while (pendingCount_ > 0) {
      // Oldest undecided candidate.
      int oldest = candSlot(candCount_ - pendingCount_);
      int32_t c = cand_[oldest].centreIdx;
      if (evaluated < c + kDecisionLagBins) break;  // not ready yet
      float absR = cand_[oldest].absR;
      bool blocked = false;
      // (a) declared event within the NMS radius?
      for (int i = 0; i < declCount_; ++i) {
        int32_t d = declHist_[(declHead_ - declCount_ + i + 2 * kDeclHist) %
                              kDeclHist];
        if (std::abs(c - d) <= kNmsRadiusBins) { blocked = true; break; }
      }
      // (b) stronger knowable candidate within the NMS radius?
      // Knowable = processed already = centre <= c (ready order is
      // centre order), so only earlier candidates can block.
      if (!blocked) {
        for (int i = 0; i < candCount_; ++i) {
          int j = candSlot(i);
          if (j == oldest) continue;
          if (cand_[j].centreIdx >= c) continue;  // not yet knowable
          if (std::abs(cand_[j].centreIdx - c) <= kNmsRadiusBins &&
              cand_[j].absR > absR) {
            blocked = true;
            break;
          }
        }
      }
      if (!blocked) declare(oldest);
      --pendingCount_;
    }
  }

  void declare(int ringIdx) {
    const Candidate& cd = cand_[ringIdx];
    NaviRecognizedEvent e;
    e.tPassageUs = cd.centreUs + kPeakOffUs;
    e.tRecognizedUs = cd.centreUs + kReadyLagUs;
    e.r = cd.r;
    e.amplitude = cd.a;
    e.polarity = (cd.a >= 0.0f) ? (int8_t)+1 : (int8_t)-1;
    declHist_[declHead_] = cd.centreIdx;
    declHead_ = (declHead_ + 1) % kDeclHist;
    if (declCount_ < kDeclHist) ++declCount_;
    // Enqueue (drop the oldest if the queue is full — the queue is
    // drained every loop iteration, so a full queue means the consumer
    // is stuck; count it, do not block the recognizer).
    int nextTail = (evTail_ + 1) % kEvQueue;
    if (nextTail == evHead_) evHead_ = (evHead_ + 1) % kEvQueue;
    evQueue_[evTail_] = e;
    evTail_ = nextTail;
  }
};
