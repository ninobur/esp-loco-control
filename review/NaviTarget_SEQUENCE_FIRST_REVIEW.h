#pragma once
// ---------------------------------------------------------------------------
// NaviTarget — experimental geographic sequence acquisition (review only).
// Every completed Hall encounter contributes one observed polarity.
// A ten-encounter sequence is matched to the surveyed Lowline map in the
// declared travel direction. Only a unique match establishes a confirmed MM.
// No timing map, IR gating, or missed/duplicate correction in this first edit.
// The operator's declared position is a hypothesis, not a confirmation.
// DO NOT DEPLOY before independent review and integration testing.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include "RouteMap.h"
#include "NaviEncounter.h"
#include "NaviTimingPredictor.h"

namespace navi_power {

enum class TargetState : uint8_t { Unset = 0, Tracking };

enum class JudgementResult : uint8_t { NoClaim = 0, Confirmed, NonQualifying };

inline int8_t expectedHallSign(uint8_t mm) {   // same convention as r5.1: ROUTE 1 = North = ADC above baseline
  return navi_one::polarityAt(mm) ? 1 : -1;
}

class TargetTracker {
 public:
  static constexpr uint8_t kSequenceLength = 10;

  void declare(uint8_t last, uint8_t next, int8_t dir) {
    last_ = last;
    next_ = next;
    dir_ = dir;
    state_ = (dir == 1 || dir == -1) ? TargetState::Tracking : TargetState::Unset;
    observedCount_ = 0;
    locationConfirmed_ = false;
    haveConfirmedOpen_ = false;
    confirmedOpenUs_ = 0;
    confirmedOpenPwm_ = 0;
    lastTiming_ = predictor_.noAnchor();
  }

  void suspend() {
    state_ = TargetState::Unset;
    observedCount_ = 0;
    locationConfirmed_ = false;
  }

  JudgementResult onEncounter(const CompletedEncounter& e) {
    if (state_ != TargetState::Tracking || e.abandoned)
      return JudgementResult::NoClaim;
    if (!e.meetsMinWidth || (e.pol != 1 && e.pol != -1))
      return JudgementResult::NonQualifying;

    // Do not use the declared target polarity to discard contrary evidence.
    // Keep one polarity per completed physical encounter, newest at the end.
    if (observedCount_ < kSequenceLength) {
      observed_[observedCount_++] = e.pol;
    } else {
      for (uint8_t i = 1; i < kSequenceLength; ++i)
        observed_[i - 1] = observed_[i];
      observed_[kSequenceLength - 1] = e.pol;
    }
    if (observedCount_ < kSequenceLength)
      return JudgementResult::NonQualifying;

    uint16_t matches = 0;
    uint8_t matchedEnd = 0;
    for (uint16_t end = 0; end < navi_one::ROUTE_N; ++end) {
      bool fits = true;
      for (uint8_t i = 0; i < kSequenceLength; ++i) {
        const uint8_t mm = navi_one::routeMod(
            int32_t(end) - int32_t(kSequenceLength - 1 - i) * dir_);
        if (observed_[i] != expectedHallSign(mm)) {
          fits = false;
          break;
        }
      }
      if (fits) {
        ++matches;
        matchedEnd = uint8_t(end);
      }
    }

    // An ambiguous or unmatched observation must not advance geographic MM.
    // It also withdraws confirmation of the previous location: this sequence
    // has not corroborated it. Do not assert a replacement location.
    if (matches != 1) {
      locationConfirmed_ = false;
      return JudgementResult::NonQualifying;
    }

    last_ = matchedEnd;
    next_ = navi_one::nextMarker(last_, dir_);
    locationConfirmed_ = true;
    // This first architectural edit does not claim timing validation.
    lastTiming_ = predictor_.noAnchor();
    confirmedOpenUs_ = e.openUs;
    confirmedOpenPwm_ = e.pwmAtOpen;
    haveConfirmedOpen_ = true;
    return JudgementResult::Confirmed;
  }

  TargetState state() const { return state_; }
  bool locationConfirmed() const { return locationConfirmed_; }
  uint8_t lastMm() const { return last_; }
  uint8_t nextMm() const { return next_; }
  int8_t dir() const { return dir_; }
  const TimingEvidence& lastTiming() const { return lastTiming_; }

 private:
  TargetState state_ = TargetState::Unset;
  bool locationConfirmed_ = false;
  uint8_t last_ = 0, next_ = 0;
  int8_t dir_ = 0;
  int8_t observed_[kSequenceLength] = {};
  uint8_t observedCount_ = 0;
  bool haveConfirmedOpen_ = false;
  uint64_t confirmedOpenUs_ = 0;
  uint8_t confirmedOpenPwm_ = 0;
  TimingPredictor predictor_;
  TimingEvidence lastTiming_;
};

}  // namespace navi_power
