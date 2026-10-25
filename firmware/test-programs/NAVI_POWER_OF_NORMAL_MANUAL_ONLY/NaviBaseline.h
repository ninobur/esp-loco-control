#pragma once
// ---------------------------------------------------------------------------
// NaviBaseline — the LONG, inter-magnet measurement: "what is the
// representative ordinary-track ADC level right now." This answers a
// different question from NaviEncounter's SHORT local return-to-baseline
// measurement (David/Sam, 2026-10-24): both read the same Hall stream, but
// this class never decides whether an excursion has ended, and NaviEncounter
// never computes a median. Neither silently substitutes for or updates the
// other — and, per the correction of 2026-10-24, neither one's TIMING may
// delay, restart, or trim the other's. They may read the same observations;
// they do not synchronize on each other's decisions.
//
// COLLECTION WINDOW, CORRECTED 2026-10-24. Begins the instant the signal
// APPEARS to re-enter ordinary track -- one block within BASELINE_ENTRY_
// MARGIN_ADC of the current committed level, evaluated independently, not
// NaviEncounter's own RTB_RUN_BLOCKS-confirmed closure. Once that single
// block triggers entry, every subsequent gated block is admitted
// unconditionally (no further per-block deviation test) all the way to the
// next genuine 70x2 opening, which is the one and only hard boundary ending
// collection. A brief residual-tail or transition sample that slips in at
// the start is not filtered out here -- the median absorbs it. This means
// NaviEncounter's own confirmation (kept, for evaluation, in NaviEncounter.h)
// has NO effect on when this collection starts, restarts, or ends: verified
// in tests/navi_power_of_normal by running this class fed every block
// regardless of Encounter's active() state.
//
// STEADY-STATE GATING. Admission (once triggered) is still gated to PWM>0
// only: a stationary period contributes nothing, so sample count reflects
// travel, not elapsed time (agreed 2026-09-28 / 2026-10-18).
//
// BOOT EXCEPTION. The very first interval, before any magnet has closed, is
// collected while stationary (PWM==0) instead, starting immediately (there
// is no prior excursion to transition out of), and remains authoritative
// until the first magnet opens.
//
// LIVE, NOT ONE-CYCLE STALE. level() is recomputed from the CURRENT
// interval's own samples-so-far on a coarse periodic cadence, not deferred
// to a single end-of-interval computation (2026-10-24: "the new median must
// be available to recognize a departure from the current interval's
// background"). Before any current-interval sample has arrived, level()
// still returns the last committed value, so it is never undefined.
//
// BOUNDED MEMORY, NOT BOUNDED BY DURATION. A fixed-capacity ring buffer: the
// same number of slots whether a gap lasts one second or one minute, so
// elapsed time alone cannot manufacture additional weight. Checked against
// the real boot-1 corpus (2026-10-24): no real interval, including a 225.8s
// one, exceeds this capacity -- the PWM gate already excludes the dominant
// stationary portion of any long gap.
//
// NO ARBITRARY MINIMUM (corrected 2026-10-24: the previous 50-block floor
// was unevidenced and is removed). The committed level is retained
// unchanged ONLY when an interval produced literally zero valid moving
// samples -- the real, evidenced case (four such intervals in the boot-1
// corpus, every one a fully stationary dwell, not a short-but-real gap).
// Any nonzero count is promoted.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <algorithm>
#include <cmath>

namespace navi_power {

static constexpr uint32_t BASELINE_CAPACITY = 2048;            // ring buffer slots, bounded regardless of elapsed time
static constexpr uint32_t BASELINE_RECOMPUTE_BLOCKS = 50;      // live-update cadence during collection (~1 s of travel)
static constexpr float BASELINE_ENTRY_MARGIN_ADC = 20.0f;      // single-block "appears ordinary" test to START collecting

class BaselineTracker {
 public:
  // Call once at startup, before either task runs. No entry trigger needed:
  // there is no prior excursion to transition out of.
  void beginBootCollection() { firstInterval_ = true; collecting_ = true; count_ = 0; sinceRecompute_ = 0; }

  // Feed one 20 ms block's Hall mean and whether PWM was > 0 for it. Call
  // this for EVERY block, unconditionally -- including while NaviEncounter's
  // own Encounter is still active. That is what makes the independence real:
  // this class decides for itself, from {hallAdc, the current committed
  // level, pwmOn} alone, whether it is watching for entry or already
  // collecting. Nothing about NaviEncounter's state is consulted.
  void onBlock(float hallAdc, bool pwmOn) {
    if (!std::isfinite(hallAdc)) return;
    const bool gateOpen = firstInterval_ ? !pwmOn : pwmOn;
    if (!collecting_) {
      if (!gateOpen || fabsf(hallAdc - level_) > BASELINE_ENTRY_MARGIN_ADC) return;   // not yet "ordinary" -- keep watching
      collecting_ = true;   // triggered: this block is the first one admitted
    }
    if (!gateOpen) return;
    buf_[count_ % BASELINE_CAPACITY] = hallAdc;
    ++count_;
    if (++sinceRecompute_ >= BASELINE_RECOMPUTE_BLOCKS) { recompute_(); sinceRecompute_ = 0; }
  }

  // The live, continuously-updating estimate of the current interval's
  // ordinary-track level — what CandidateDetector compares against.
  float level() const { return level_; }
  bool haveLevel() const { return everRecomputed_; }

  // Called the instant a 70x2 opening is confirmed: stop collecting (one
  // final recompute first, so the committed value reflects every sample up
  // to this instant), retain the previous level only if literally nothing
  // was collected, and start WATCHING for the next interval's entry point
  // immediately -- this is automatic, not triggered by NaviEncounter's own
  // completion. outSampleCount is for diagnostics only.
  bool finalize(uint32_t& outSampleCount) {
    const bool valid = count_ > 0;
    if (valid) recompute_();   // zero samples: level_ is left exactly as it was -- retain, not invent
    outSampleCount = count_;
    firstInterval_ = false;
    collecting_ = false;      // next onBlock() call resumes watching for entry, with no external call required
    count_ = 0; sinceRecompute_ = 0;
    return valid;
  }

 private:
  void recompute_() {
    const uint32_t n = count_ < BASELINE_CAPACITY ? count_ : BASELINE_CAPACITY;
    if (n == 0) return;
    std::nth_element(buf_, buf_ + n / 2, buf_ + n);  // reorders buf_ in place; order never matters here
    level_ = buf_[n / 2];
    everRecomputed_ = true;
  }

  bool collecting_ = false, firstInterval_ = false, everRecomputed_ = false;
  uint32_t count_ = 0, sinceRecompute_ = 0;
  float level_ = 0;
  float buf_[BASELINE_CAPACITY];
};

}  // namespace navi_power
