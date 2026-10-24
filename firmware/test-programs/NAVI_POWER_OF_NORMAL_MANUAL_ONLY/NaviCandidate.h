#pragma once
// ---------------------------------------------------------------------------
// NaviCandidate — the 70-count, two-consecutive-sample onset detector.
//
// The Power of Normal conference record (docs/navi/NAVI_CONFERENCE_POWER_OF_
// NORMAL_RECORD_20261024.md) and both independent audits of r5.1 (Claude and
// Codex, 2026-10-24) agree this mechanism does not exist in any prior sketch:
// the old recognizer applied its 70-count amplitude floor to 20 ms block
// means, after shape scoring, not as a two-consecutive-sample trigger on the
// native 5 kHz stream. This file is new, built directly from the conference
// record, not adapted from RvmShape.h.
//
// RULE. A slow reference level tracks the native Hall stream while no
// candidate is open (nothing is withheld from it: it simply stops updating
// once a candidate opens, and resumes once NaviEncounter closes one). An
// onset is declared the instant two CONSECUTIVE native samples both deviate
// from that reference by >= 70 ADC counts in the same direction. The first of
// the two samples, not the second, is the true opening instant — it is
// recorded, not discarded, which is the "pre-trigger preservation" the
// conference asked for: the detector does not need the first sample twice to
// notice it, but the encounter stage is told where it actually began.
//
// This stage makes no judgement about magnets, polarity-vs-the-map, width, or
// completion. It only says: "something started here, in this direction." The
// map- and completion-aware decisions belong to NaviEncounter and NaviTarget.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <math.h>

namespace navi_power {

static constexpr float CANDIDATE_THRESHOLD_ADC = 70.0f;   // operator's excursion floor, carried forward unchanged
static constexpr float CANDIDATE_LEVEL_ALPHA = 1.0f / 1250.0f;  // ~0.25 s time constant at 5 kHz: slow enough not to chase a real excursion, fast enough to track genuine baseline drift between encounters

struct CandidateOnset {
  uint32_t openSerial = 0;      // the FIRST of the two crossing samples (the true opening instant)
  int8_t pol = 0;                // +1 above the reference, -1 below
  float levelAtOpen = 0;         // the reference level the excursion departed from
};

class CandidateDetector {
 public:
  // Feed one native 5 kHz read. Returns true exactly once per onset, via `out`.
  // No effect while a candidate is open (open() / close() below) other than
  // bookkeeping; the caller must call close() once NaviEncounter decides the
  // candidate (accepted or abandoned) so the reference resumes tracking.
  bool onSample(uint16_t adc, uint32_t serial, CandidateOnset& out) {
    if (open_) return false;
    const float x = float(adc);
    if (!haveLevel_) { level_ = x; haveLevel_ = true; }
    const float dev = x - level_;
    const bool crosses = fabsf(dev) >= CANDIDATE_THRESHOLD_ADC;
    const int8_t sign = dev >= 0 ? 1 : -1;
    bool onset = false;
    if (crosses) {
      if (pendingCross_ && sign == pendingSign_) {
        out.openSerial = pendingSerial_; out.pol = sign; out.levelAtOpen = level_;
        open_ = true;
        onset = true;
      }
      // else: the FIRST crossing sample, or a crossing of the other sign.
      // Freeze the level here either way — a gradually rising excursion must
      // not be partly absorbed into the reference before its second sample
      // confirms the onset, or the effective threshold silently grows past
      // 70 counts for anything that isn't an instant step. (Found exactly
      // this way: an initial replay against the boot-1 corpus recognized
      // 911 onsets against 1,087 annotated openings — a 16% miss rate — and
      // tracing it back led here.)
    } else {
      level_ += (x - level_) * CANDIDATE_LEVEL_ALPHA;
    }
    pendingCross_ = crosses; pendingSign_ = sign; pendingSerial_ = serial;
    return onset;
  }
  // NaviEncounter has decided the candidate (either way); resume tracking.
  void close() { open_ = false; pendingCross_ = false; }
  bool isOpen() const { return open_; }
  float level() const { return level_; }

 private:
  bool haveLevel_ = false, open_ = false, pendingCross_ = false;
  float level_ = 0;
  int8_t pendingSign_ = 1;
  uint32_t pendingSerial_ = 0;
};

}  // namespace navi_power
