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

  // A completed encounter is in: confirm it against the single expected
  // target, or leave the target exactly as it is. Width disqualification is
  // inert until MIN_WIDTH_BLOCKS is set (NaviEncounter.h); when it is, a
  // width failure is handled identically to a polarity failure here — both
  // are "nonqualifying," neither is a stop.
  //
  // TIMING: the directed interval starts at the prior CONFIRMED 70x2 opening,
  // not at an arbitrary observed signal. Once a field profile exists, timing
  // joins polarity, width, and completion as a required identifier. Before
  // then it remains explicitly UNPROFILED rather than gaining an invented
  // pass/fail number.
  JudgementResult onEncounter(const CompletedEncounter& e) {
    if (state_ != TargetState::Tracking) return JudgementResult::NoClaim;
    if (e.abandoned) return JudgementResult::NoClaim;    // never reached return-to-baseline: not evidence either way
    // PWM-90 gate uses the measured lower survey interval for this directed
    // segment. This is an empirical opening time, not a proven physical Vmax.
    // The first accepted encounter establishes an opening-time anchor.
    lastTiming_ = TimingEvidence{};
    bool windowOpen = !haveConfirmedOpen_;
    if (haveConfirmedOpen_) {
      lastTiming_.haveAnchor = true;
      lastTiming_.fromMm = last_;
      lastTiming_.dir = dir_;
      lastTiming_.sourcePwm = confirmedOpenPwm_;
      lastTiming_.arrivalPwm = e.pwmAtOpen;
      if (e.openUs >= confirmedOpenUs_) {
        const uint64_t ms = (e.openUs - confirmedOpenUs_) / 1000ULL;
        lastTiming_.measuredMs = ms > UINT32_MAX ? UINT32_MAX : uint32_t(ms);
      }
      // A PWM-90-only window must not silently qualify other speeds.
      if (e.openUs >= confirmedOpenUs_ &&
          confirmedOpenPwm_ == 90 && e.pwmAtOpen == 90) {
        for (uint16_t i = 0; i < TIMING_PROFILE_COUNT; ++i) {
          const auto& p = TIMING_PROFILES[i];
          if (p.fromMm != last_ || p.dir != dir_ || p.pwm != 90) continue;
          lastTiming_.haveProfile = true;
          const float base = dir_ > 0 ? PWM90_SURVEY_BASE_CW_MS : PWM90_SURVEY_BASE_CCW_MS;
          lastTiming_.lowerMs = uint32_t(p.lowerFactor * base + 0.5f);
          lastTiming_.expectedMs = uint32_t(p.factor * base + 0.5f);
          lastTiming_.upperMs = uint32_t(p.upperFactor * base + 0.5f);
          windowOpen = lastTiming_.measuredMs >= lastTiming_.lowerMs;
          lastTiming_.result = windowOpen ? TimingResult::Matches : TimingResult::Mismatches;
          break;
        }
      }
      if (!lastTiming_.haveProfile) lastTiming_.result = TimingResult::Unprofiled;
    }
    const bool timingOk = windowOpen;
    if (e.pol == expectedHallSign(next_) && e.meetsMinWidth && timingOk) {
      confirmedOpenUs_ = e.openUs; confirmedOpenPwm_ = e.pwmAtOpen; haveConfirmedOpen_ = true;
      last_ = next_; next_ = navi_one::nextMarker(next_, dir_);
      return JudgementResult::Confirmed;
    }
    return JudgementResult::NonQualifying;                // target unchanged; keep observing for it
  }

  TargetState state() const { return state_; }
  uint8_t lastMm() const { return last_; }
  uint8_t nextMm() const { return next_; }
  int8_t dir() const { return dir_; }
  const TimingEvidence& lastTiming() const { return lastTiming_; }

 private:
  TargetState state_ = TargetState::Unset;
  uint8_t last_ = 0, next_ = 0;
  int8_t dir_ = 0;
  bool haveConfirmedOpen_ = false;
  uint64_t confirmedOpenUs_ = 0;
  uint8_t confirmedOpenPwm_ = 0;
  TimingEvidence lastTiming_;
};

}  // namespace navi_power
