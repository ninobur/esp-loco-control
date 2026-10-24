#pragma once
// ---------------------------------------------------------------------------
// NaviEncounter — observes a candidate from its onset through observed
// return-to-baseline, and packages the result for NaviTarget's judgement.
//
// New file, built from the conference record. It replaces RvmShape.h's role
// entirely but keeps none of its machinery: no DCT features, no Gaussian
// mixture, no log-likelihood ratio, no held/replace window selection. There
// is exactly one open encounter at a time, started by NaviCandidate's onset
// and ended here by observed evidence, not by a fitted window's timing.
//
// WIDTH. The conference (Proposition 6A, amended by 7) treats minimum
// physically plausible width as NECESSARY, NOT SUFFICIENT — and leaves its
// numeric value to be measured, not invented. MIN_WIDTH_BLOCKS is therefore
// 0 (disabled) in this build: the width is measured and carried on every
// completed encounter so it can be reviewed, but nothing is rejected on it
// yet. Setting it to a real value is a data-driven follow-up, not a decision
// made here. Shape/Gaussian fit has NO role at all, not even diagnostic, in
// this implementation — the conference permitted keeping it as an optional
// diagnostic, but this build does not add complexity that isn't load-bearing.
//
// COMPLETION. "Observed return to baseline": RTB_RUN_BLOCKS consecutive
// blocks each within RTB_MARGIN_ADC of the level the excursion departed from
// (NaviCandidate::levelAtOpen, NOT a re-estimated or drifting baseline — the
// encounter is judged against where it started). This is evidence-based
// completion, not timer-based: there is no "wait half the window's own width"
// rule as in the old recognizer.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <math.h>
#include "NaviObservation.h"
#include "NaviCandidate.h"

namespace navi_power {

static constexpr uint16_t MIN_WIDTH_BLOCKS = 0;      // OPEN QUESTION: measured value not yet set; 0 = no rejection
static constexpr uint16_t RTB_RUN_BLOCKS = 10;        // 200 ms of blocks within margin counts as "returned"
static constexpr float RTB_MARGIN_ADC = 20.0f;        // within this of levelAtOpen counts as "at baseline"
static constexpr uint16_t MAX_ENCOUNTER_BLOCKS = 500; // 10 s: abandon (not reject) an encounter that never returns

struct CompletedEncounter {
  uint32_t openBlockId = 0, closeBlockId = 0;
  uint16_t widthBlocks = 0;
  int8_t pol = 0;
  float peakAdc = 0;             // diagnostic only: never gates admission or completion
  bool meetsMinWidth = true;     // always true while MIN_WIDTH_BLOCKS == 0; carried for when it is set
  bool abandoned = false;        // MAX_ENCOUNTER_BLOCKS exceeded without return-to-baseline: a fact to report, not a magnet
};

class Encounter {
 public:
  void open(const CandidateOnset& onset, uint32_t openBlockId) {
    active_ = true;
    pol_ = onset.pol; level_ = onset.levelAtOpen;
    openBlockId_ = openBlockId; width_ = 0; peak_ = 0; rtbRun_ = 0;
  }
  bool active() const { return active_; }

  // Feed one 20 ms block while an encounter is open. Returns true once the
  // encounter is decided (completed or abandoned); `out` is valid then.
  bool push(const Block& b, CompletedEncounter& out) {
    if (!active_) return false;
    ++width_;
    if (isfinite(b.hall)) {
      const float dev = fabsf(b.hall - level_);
      if (dev > peak_) peak_ = dev;
      if (dev <= RTB_MARGIN_ADC) ++rtbRun_; else rtbRun_ = 0;
    }
    const bool returned = rtbRun_ >= RTB_RUN_BLOCKS;
    const bool timedOut = width_ >= MAX_ENCOUNTER_BLOCKS;
    if (!returned && !timedOut) return false;
    out.openBlockId = openBlockId_;
    out.closeBlockId = openBlockId_ + width_;
    out.widthBlocks = uint16_t(width_ > RTB_RUN_BLOCKS ? width_ - RTB_RUN_BLOCKS : width_);  // width to the excursion's own return, not including the settled run
    out.pol = pol_; out.peakAdc = peak_;
    out.meetsMinWidth = MIN_WIDTH_BLOCKS == 0 || out.widthBlocks >= MIN_WIDTH_BLOCKS;
    out.abandoned = timedOut && !returned;
    active_ = false;
    return true;
  }

 private:
  bool active_ = false;
  int8_t pol_ = 0;
  float level_ = 0, peak_ = 0;
  uint32_t openBlockId_ = 0;
  uint32_t width_ = 0, rtbRun_ = 0;
};

}  // namespace navi_power
