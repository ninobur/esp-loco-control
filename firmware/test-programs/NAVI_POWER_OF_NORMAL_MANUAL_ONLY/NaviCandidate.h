#pragma once
// ---------------------------------------------------------------------------
// NaviCandidate — the 70-count, two-consecutive-sample onset detector.
//
// RULE. An onset is declared the instant two CONSECUTIVE samples, at the
// established rule's own 1 kHz cadence (NAVI_SIMPLIFIED, docs/NAVI_RVM_70_
// COUNT_GATE_20261020.md — this sketch's native acquisition runs at 5 kHz,
// so CANDIDATE_STRIDE decimates to the rule's own rate before evaluating),
// both deviate from the current baseline level by >= 70 ADC counts in the
// same direction. The first of the two samples is the true opening instant.
//
// NO LOCK, NO OWN REFERENCE. This stage holds no open/closed state and
// tracks no baseline of its own — both were unauthorized inventions in an
// earlier version of this file, found 2026-10-24 to recreate the exact
// lockout/stale-reference defects this build exists to remove (see
// docs/navi/NAVI_POWER_OF_NORMAL_ONSET_SHORTFALL_INVESTIGATION_20261024.md).
// The reference compared against is NaviBaseline's live estimate of the
// current ordinary interval — the long inter-magnet measurement — passed in
// fresh on every call. Detection self-resets the instant a sample drops back
// under threshold, so a later, genuinely separate departure is always
// noticed regardless of whether a previous one has been interpreted yet.
// This stage makes no judgement about completion, width, or geography —
// that belongs to NaviEncounter and NaviTarget.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <math.h>

namespace navi_power {

static constexpr float CANDIDATE_THRESHOLD_ADC = 70.0f;
static constexpr uint32_t CANDIDATE_NATIVE_HZ = 5000, CANDIDATE_RULE_HZ = 1000;
static constexpr uint32_t CANDIDATE_STRIDE = CANDIDATE_NATIVE_HZ / CANDIDATE_RULE_HZ;  // 5: decimate 5 kHz to the rule's own 1 kHz

struct CandidateOnset {
  uint32_t openSerial = 0;   // the FIRST of the two crossing samples (the true opening instant)
  int8_t pol = 0;             // +1 above the reference, -1 below
  float levelAtOpen = 0;      // the live baseline level the excursion departed from
};

class CandidateDetector {
 public:
  // Feed one native 5 kHz read and the current live baseline level (read
  // from NaviBaseline — never computed here). Returns true exactly once per
  // genuinely new departure, via `out`. Nothing about this call can be
  // suspended by anything downstream.
  bool onSample(uint16_t adc, float baselineLevel, uint32_t serial, CandidateOnset& out) {
    const bool evaluate = (strideCounter_++ % CANDIDATE_STRIDE) == 0;
    if (!evaluate) return false;
    const float dev = float(adc) - baselineLevel;
    const bool crosses = fabsf(dev) >= CANDIDATE_THRESHOLD_ADC;
    const int8_t sign = dev >= 0 ? 1 : -1;
    bool onset = false;
    if (crosses) {
      if (!excursionActive_ && pendingCross_ && pendingSign_ == sign) {
        out.openSerial = pendingSerial_; out.pol = sign; out.levelAtOpen = baselineLevel;
        onset = true;
        excursionActive_ = true;
      }
      pendingCross_ = true; pendingSign_ = sign; pendingSerial_ = serial;
    } else {
      pendingCross_ = false;
      excursionActive_ = false;   // back under threshold: the next departure will be noticed fresh
    }
    return onset;
  }

 private:
  bool pendingCross_ = false, excursionActive_ = false;
  int8_t pendingSign_ = 1;
  uint32_t pendingSerial_ = 0;
  uint32_t strideCounter_ = 0;
};

}  // namespace navi_power
