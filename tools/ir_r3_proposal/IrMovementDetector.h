#pragma once
// REVIEW PROPOSAL ONLY: IR TX R3 candidate detector. NOT production firmware.
// Production remains firmware/common/IrMovementDetector.h (R2, 3887f2a).
// Proposed in docs/IR_TX_R2_BENCH_FAILURE_REVIEW_20260924.md; this copy exists
// so the existing host suites and the captured-waveform replay can run against
// it. Differences from R2 are marked "R3:" and apply ONLY when
// retainStationary is true; retainStationary=false is bit-identical to R2.
#include <stdint.h>

namespace ir_movement {
enum Reason : uint8_t { PRIMING, INADEQUATE_CONTRAST, SATURATION,
                       SAMPLE_GAP, SIGNAL_STALE, REACQUIRING, TRACKING };

// TRACKING describes observable optical activity, not validated distance accuracy.
class Detector {
 public:
  static constexpr unsigned Window = 512;
  // R3: quiet hold requires a still window, not merely span<120. Bench quiet
  // snapshots were 15-47 in all but 10 of ~6,000 (2026-09-23 captures).
  static constexpr unsigned StillSpan = 63;
  // R3: holding revokes only after this many consecutive off-plateau samples.
  // Bench stationary excursions >100 counts were 1-2 samples long, never 3.
  static constexpr unsigned OffPlateauRun = 3;
  // R3: a reference is learned or refreshed only from a completion whose cycle
  // fits twice in the 512 ms window, so a decelerating wheel's partial cycles
  // (shrinking percentiles) cannot ratchet it inward.
  static constexpr uint64_t RefreshMaxIntervalUs = 256000;
  explicit Detector(bool retainPhase=false,bool retainStationary=false)
    : retainPhase_(retainPhase),retainStationary_(retainStationary) {}
  void sample(uint64_t us, uint16_t raw) {
    rise = fall = false;
    if (raw>4095) raw=4095;
    if (haveSample_ && (us <= lastSample_ || us-lastSample_ > 1500)) {
      ++gaps; invalidate(); fill_=0; index_=0;
      for (auto &v : hist_) v=0;
      reason=SAMPLE_GAP;
    }
    lastSample_=us; haveSample_=true;
    if (fill_==Window) --hist_[window_[index_]];
    else ++fill_;
    window_[index_]=raw>>4; ++hist_[raw>>4]; index_=(index_+1)%Window;
    if (!haveEnvelope_ || us-lastEnvelope_ >= 50000) {
      lastEnvelope_=us; haveEnvelope_=true;
      unsigned sum=0; int lo=-1, hi=-1;
      for (int b=0;b<256;++b) {
        sum+=hist_[b];
        if (lo<0 && sum>fill_*5/100) lo=b;
        if (hi<0 && sum>fill_*95/100) hi=b;
      }
      low=lo*16; high=hi*16+15;
    }
    if (raw==0 || raw>=4095) { ++saturated; invalidate(); reason=SATURATION; return; }
    if (fill_<128) { invalidate(); reason=PRIMING; return; }
    const bool contrast=high-low>=120;
    const bool holding=retainStationary_ && proven_ && !contrast;
    // Retention bridges quiet plateaus only; moving contrast uses live thresholds.
    if (holding) {
      const unsigned margin=(provenHigh_-provenLow_)/4;
      const bool nearLow=raw+margin>=provenLow_ && raw<=provenLow_+margin;
      const bool nearHigh=raw+margin>=provenHigh_ && raw<=provenHigh_+margin;
      if (!nearLow && !nearHigh) {
        // R3: a sustained departure revokes as in R2; an isolated conversion
        // is counted and makes no edge decision.
        if (++offPlateau_>=OffPlateauRun) { invalidate();reason=INADEQUATE_CONTRAST;return; }
        ++holdOutliers;
        reason=phaseLost_||high-low>StillSpan ? INADEQUATE_CONTRAST : SIGNAL_STALE;
        return;
      }
      offPlateau_=0;
      if (high-low>StillSpan) {
        // R3: quiet by the 120 gate but not still (fading or compressed
        // modulation). Unavailable; continuity ends once; the reference is kept.
        if (haveCompleted_) { ++aborts; haveCompleted_=false; candidate_=false; phaseLost_=true; }
        reason=INADEQUATE_CONTRAST;
        return;
      }
      if (phaseLost_) {
        // R3: still, at a trusted plateau, after continuity was lost. Start
        // from known phase only: armed at the low plateau; at the high plateau
        // wait for a low, so a restart never credits a partial pulse tail.
        open_=false; armed_=nearLow; phaseLost_=false;
      }
    }
    if (!contrast && !holding && (!retainPhase_ || !armed_)) { invalidate(); reason=INADEQUATE_CONTRAST; return; }
    if (contrast) {
      thresholdLow_=low+(high-low)/3;
      thresholdHigh_=low+2*(high-low)/3;
    } else if (holding) {
      // R3: while holding, edges are judged against the retained reference,
      // never against the last live thresholds, which a fading envelope has
      // left squeezed around the resting level (bench count-99 replay: two
      // noise "completions" after the stop with the live thresholds).
      thresholdLow_=provenLow_+(provenHigh_-provenLow_)/3;
      thresholdHigh_=provenLow_+2*(provenHigh_-provenLow_)/3;
    }
    const unsigned tl=thresholdLow_, th=thresholdHigh_;
    if (open_ && us-openAt_>=2500000 && !holding) {
      if (!retainStationary_) ++aborts;
      invalidate();
    }
    // Rearming always requires an observed low, including after a timeout.
    if (!armed_) { if(raw<tl) armed_=true; reason=holding?SIGNAL_STALE:REACQUIRING; return; }
    if (!open_ && raw>th) { open_=true; openAt_=us; ++rises; rise=true; }
    else if (open_ && raw<tl) {
      open_=false; ++completed; fall=true;
      // R3: steady = this cycle is short enough that the window holds whole
      // cycles: its high phase fits in a quarter window and, when continuity
      // is intact, so does the whole cycle in half. Unused in default mode.
      const bool steady=us-openAt_<=RefreshMaxIntervalUs/2 &&
                        (!haveCompleted_ || us-lastCompleted_<=RefreshMaxIntervalUs);
      lastCompleted_=us; haveCompleted_=true; phaseLost_=false;
      if(retainStationary_ && high-low>=300 && steady) {
        if ((proven_ && compatible(provenLow_,provenHigh_)) ||
            (candidate_ && compatible(candidateLow_,candidateHigh_))) {
          provenLow_=low;provenHigh_=high;proven_=true;
        }
        candidateLow_=low;candidateHigh_=high;candidate_=true;
      }
    }
    if (holding) reason=SIGNAL_STALE;
    else if (high-low<300) {
      // A short quality outage must remain visible even between radio reports.
      // R3: continuity ends (openAborts) but the optical reference is kept;
      // it can only validate a later still plateau, never bridge this outage.
      if (retainStationary_ && haveCompleted_) {
        ++aborts;haveCompleted_=false;candidate_=false;phaseLost_=true;
      }
      reason=INADEQUATE_CONTRAST;
    }
    else if (!haveCompleted_) reason=REACQUIRING;
    else reason=us-lastCompleted_>=2500000 ? SIGNAL_STALE : TRACKING;
  }
  uint64_t completed=0, rises=0, aborts=0, gaps=0, saturated=0;
  uint64_t holdOutliers=0;  // R3 diagnostic only; not on the wire, not a continuity counter
  unsigned low=0, high=0;
  bool rise=false, fall=false;
  Reason reason=PRIMING;
  bool inPulse() const { return open_; }
  // Review/test inspection only.
  bool referenceKnown() const { return proven_; }
  unsigned referenceLow() const { return provenLow_; }
  unsigned referenceHigh() const { return provenHigh_; }
  // Envelope timing, read by tools/ir_bench_replay.h.
  bool haveEnvelope_=false; uint64_t lastEnvelope_=0;
 private:
  bool compatible(unsigned lo,unsigned hi) const {
    const unsigned margin=(hi-lo)/4;
    return low+margin>=lo && low<=lo+margin &&
           high+margin>=hi && high<=hi+margin;
  }
  void invalidate() {
    // openAborts also records discarded continuity in stationary-retention mode.
    if(retainStationary_ && (armed_||open_||proven_)) ++aborts;
    open_=false;armed_=false;haveCompleted_=false;proven_=false;candidate_=false;
    offPlateau_=0;phaseLost_=false;
  }
  uint8_t window_[Window]{};
  uint16_t hist_[256]{};
  unsigned fill_=0,index_=0;
  uint64_t lastSample_=0,openAt_=0,lastCompleted_=0;
  bool haveSample_=false,armed_=false,open_=false,haveCompleted_=false;
  bool retainPhase_=false;
  bool retainStationary_=false,proven_=false,candidate_=false;
  bool phaseLost_=false;      // R3: continuity lost since the last completion
  unsigned offPlateau_=0;     // R3: consecutive off-plateau samples while holding
  unsigned candidateLow_=0,candidateHigh_=0;
  unsigned provenLow_=0,provenHigh_=0;
  unsigned thresholdLow_=0, thresholdHigh_=0;
};
} // namespace ir_movement
