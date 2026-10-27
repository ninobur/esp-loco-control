#pragma once
// ---------------------------------------------------------------------------
// NaviTarget — geographic judgement and position advancement (Propositions
// 1, 2, 4, 8). There is exactly one expected next magnet at all times, fixed
// by the last confirmed position, the map, and the direction of travel.
//
// This replaces RvmNavigator.h's role but deliberately drops its Contradiction
// state. Both audits (Claude and Codex, 2026-10-24) agreed that state is the
// single most consequential conflict with the conference: a wrong-polarity
// accepted encounter there halts AUTO immediately and requires the operator
// to manually re-declare. Proposition 8, closed 2026-10-24: "Qualifying
// evidence confirms the target and advances the sequence; nonqualifying
// evidence leaves the confirmed position unchanged while observation
// continues. NAVI does not search for alternative magnet identities or
// attempt geographic recovery."
//
// So: a nonqualifying encounter (wrong polarity, or width disqualified once
// MIN_WIDTH_BLOCKS is set) is NOT a failure and does NOT stop anything. It is
// logged and NAVI keeps waiting for the same expected target. The ONLY way
// position-tracking authority is withdrawn from here is suspend() — a
// direction/session change, exactly as before — never a bad encounter.
//
// There is no arrival-boundary stop in this build (removed 2026-10-24,
// David's instruction: "missing a magnet is not our problem; correctly
// recognizing it is"). Geographic opening-to-opening timing instead
// participates directly in this class's own qualification decision, as a
// required identifier alongside polarity and width — not a separate
// stop authority. See NaviTimingPredictor.h.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include "RouteMap.h"
#include "NaviEncounter.h"
#include "NaviTimingPredictor.h" // TimingEvidence/TimingResult and PWM-90 survey profile types

namespace navi_power {

enum class TargetState : uint8_t { Unset = 0, Tracking };

enum class JudgementResult : uint8_t { NoClaim = 0, Confirmed, NonQualifying };

inline int8_t expectedHallSign(uint8_t mm) {   // same convention as r5.1: ROUTE 1 = North = ADC above baseline
  return navi_one::polarityAt(mm) ? 1 : -1;
}

class TargetTracker {
 public:
  void declare(uint8_t last, uint8_t next, int8_t dir) {
    last_ = last; next_ = next; dir_ = dir; state_ = TargetState::Tracking;
    // Fixed PWM-90 survey window: no startup calibration required.
    // A declaration identifies geography but does not invent an observed
    // 70x2 opening. The next confirmed target establishes the timing anchor.
    haveConfirmedOpen_ = false; confirmedOpenUs_ = 0; confirmedOpenPwm_ = 0;
    lastTiming_ = TimingEvidence{};
  }
  void suspend() { state_ = TargetState::Unset; }

  // Gate is time-driven and opens only once per confirmed directed segment.
  // First declared target has no prior observed opening: boot median is used.
  bool windowOpen(uint64_t nowUs) const {
    if (state_ != TargetState::Tracking) return false;
    if (!haveConfirmedOpen_) return true;
    if (confirmedOpenPwm_ != 90 || nowUs < confirmedOpenUs_) return false;
    const uint32_t lower = lowerGateMs_();
    return lower != 0 && nowUs - confirmedOpenUs_ >= uint64_t(lower) * 1000ULL;
  }
  uint32_t gateLowerMs() const { return haveConfirmedOpen_ ? lowerGateMs_() : 0; }

  // Called at the native 70x2 onset, never at waveform completion.
  JudgementResult onOnset(const CandidateOnset& e) {
    if (state_ != TargetState::Tracking) return JudgementResult::NoClaim;
    lastTiming_ = TimingEvidence{};
    const bool hasAnchor = haveConfirmedOpen_;
    lastTiming_.haveAnchor = hasAnchor;
    lastTiming_.fromMm = last_;
    lastTiming_.dir = dir_;
    lastTiming_.sourcePwm = confirmedOpenPwm_;
    lastTiming_.arrivalPwm = e.pwmAtOpen;
    if (hasAnchor && e.openUs >= confirmedOpenUs_) {
      const uint64_t ms = (e.openUs - confirmedOpenUs_) / 1000ULL;
      lastTiming_.measuredMs = ms > UINT32_MAX ? UINT32_MAX : uint32_t(ms);
      lastTiming_.lowerMs = lowerGateMs_();
      lastTiming_.haveProfile = lastTiming_.lowerMs != 0;
    }
    const bool timingOk = !hasAnchor || (e.pwmAtOpen == 90 && windowOpen(e.openUs));
    lastTiming_.result = timingOk ? TimingResult::Matches : TimingResult::Mismatches;
    if (e.pol != expectedHallSign(next_) || !timingOk) return JudgementResult::NonQualifying;
    confirmedOpenUs_ = e.openUs; confirmedOpenPwm_ = e.pwmAtOpen; haveConfirmedOpen_ = true;
    last_ = next_; next_ = navi_one::nextMarker(next_, dir_);
    return JudgementResult::Confirmed;
  }

  TargetState state() const { return state_; }
  uint8_t lastMm() const { return last_; }
  uint8_t nextMm() const { return next_; }
  int8_t dir() const { return dir_; }
  const TimingEvidence& lastTiming() const { return lastTiming_; }

 private:
  uint32_t lowerGateMs_() const {
    for (uint16_t i=0; i<TIMING_PROFILE_COUNT; ++i) {
      const auto& p=TIMING_PROFILES[i];
      if (p.fromMm == last_ && p.dir == dir_ && p.pwm == 90) {
        const float base=dir_ > 0 ? PWM90_SURVEY_BASE_CW_MS : PWM90_SURVEY_BASE_CCW_MS;
        return uint32_t(p.lowerFactor * base + 0.5f);
      }
    }
    return 0;
  }
  TargetState state_ = TargetState::Unset;
  uint8_t last_ = 0, next_ = 0;
  int8_t dir_ = 0;
  bool haveConfirmedOpen_ = false;
  uint64_t confirmedOpenUs_ = 0;
  uint8_t confirmedOpenPwm_ = 0;
  TimingEvidence lastTiming_;
};

}  // namespace navi_power
