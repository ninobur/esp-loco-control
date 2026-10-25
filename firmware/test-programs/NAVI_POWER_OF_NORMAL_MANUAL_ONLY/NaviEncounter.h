#pragma once
// ---------------------------------------------------------------------------
// NaviEncounter — the SHORT, local measurement: "has the magnetic excursion
// ended and has ordinary background returned." This answers a different
// question from NaviBaseline's long inter-magnet median (David/Sam,
// 2026-10-24): this class never computes a baseline and never updates
// NaviBaseline's estimate. The level it judges against is the single value
// handed to it at open() — NaviBaseline's live level at the instant this
// encounter's onset fired — and that value does not change for the life of
// the encounter, because nothing new is collected about the ordinary track
// while a magnet is actually present.
//
// WIDTH. Proposition 6A/7: minimum physically plausible width is necessary,
// not sufficient, and its numeric value is left to be measured, not
// invented. MIN_WIDTH_BLOCKS is 0 (disabled) here.
//
// COMPLETION. Observed return to the committed level for RTB_RUN_BLOCKS
// consecutive blocks within RTB_MARGIN_ADC — evidence-based, not
// timer-based (Prop 6A).
//
// NO TIMEOUT. There is no abandon/MAX_ENCOUNTER_BLOCKS state in this build.
// The reconciled governing specification (REC-14, 2026-10-24) explicitly
// excludes a universal timeout as an authorized mechanism. `abandoned` is
// kept on CompletedEncounter, always false, so NaviTarget.h needs no change
// — nothing in this file ever sets it true. The arrival-boundary stop
// mechanism itself was removed 2026-10-24 ("missing a magnet is not our
// problem; correctly recognizing it is"); this class never had a timeout
// concept of its own and still doesn't.
//
// TIMING EVIDENCE (2026-10-24). `open()` now takes the elapsed time since
// the previous genuine opening, measured by the caller — this class does
// not compute it, only carries it through to CompletedEncounter so
// NaviTarget's qualification can use it alongside polarity, width and
// completion. See NaviTimingPredictor.h.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <math.h>
#include "NaviObservation.h"
#include "NaviCandidate.h"

namespace navi_power {

static constexpr uint16_t MIN_WIDTH_BLOCKS = 0;       // OPEN QUESTION: measured value not yet set; 0 = no rejection
static constexpr uint16_t RTB_RUN_BLOCKS = 10;         // 200 ms of blocks within margin counts as "returned"
static constexpr float RTB_MARGIN_ADC = 20.0f;         // within this of the committed level counts as "at baseline"

struct CompletedEncounter {
  uint32_t openBlockId = 0, closeBlockId = 0;
  uint16_t widthBlocks = 0;
  int8_t pol = 0;
  float peakAdc = 0;             // diagnostic only: never gates admission or completion
  bool meetsMinWidth = true;     // always true while MIN_WIDTH_BLOCKS == 0; carried for when it is set
  bool abandoned = false;        // always false in this build; kept only for NaviTarget.h's existing interface
  uint32_t timingElapsedMs = 0;  // elapsed time since the previous genuine opening (caller-measured)
  bool timingValid = false;      // false for the first opening since boot, which has no previous opening to measure from
};

class Encounter {
 public:
  void open(const CandidateOnset& onset, uint32_t openBlockId, uint32_t timingElapsedMs = 0, bool timingValid = false) {
    active_ = true;
    pol_ = onset.pol; level_ = onset.levelAtOpen;
    openBlockId_ = openBlockId; width_ = 0; peak_ = 0; rtbRun_ = 0;
    timingElapsedMs_ = timingElapsedMs; timingValid_ = timingValid;
  }
  bool active() const { return active_; }

  // Feed one 20 ms block while an encounter is open. Returns true once
  // observed completion is reached; `out` is valid then. No timeout path:
  // this returns false indefinitely until the signal is actually observed
  // to return.
  bool push(const Block& b, CompletedEncounter& out) {
    if (!active_) return false;
    ++width_;
    if (isfinite(b.hall)) {
      const float dev = fabsf(b.hall - level_);
      if (dev > peak_) peak_ = dev;
      if (dev <= RTB_MARGIN_ADC) ++rtbRun_; else rtbRun_ = 0;
    }
    if (rtbRun_ < RTB_RUN_BLOCKS) return false;
    out.openBlockId = openBlockId_;
    out.closeBlockId = openBlockId_ + width_;
    out.widthBlocks = uint16_t(width_ > RTB_RUN_BLOCKS ? width_ - RTB_RUN_BLOCKS : width_);  // width to the excursion's own return, not including the settled run
    out.pol = pol_; out.peakAdc = peak_;
    out.meetsMinWidth = MIN_WIDTH_BLOCKS == 0 || out.widthBlocks >= MIN_WIDTH_BLOCKS;
    out.abandoned = false;
    out.timingElapsedMs = timingElapsedMs_;
    out.timingValid = timingValid_;
    active_ = false;
    return true;
  }

 private:
  bool active_ = false;
  int8_t pol_ = 0;
  float level_ = 0, peak_ = 0;
  uint32_t openBlockId_ = 0;
  uint32_t width_ = 0, rtbRun_ = 0;
  uint32_t timingElapsedMs_ = 0;
  bool timingValid_ = false;
};

}  // namespace navi_power
