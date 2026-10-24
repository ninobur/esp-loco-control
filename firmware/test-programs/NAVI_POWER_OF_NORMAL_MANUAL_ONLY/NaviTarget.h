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
// The only thing permitted to stop AUTO for a navigation reason at all is the
// arrival-boundary clock in NaviArrivalBoundary.h, which this class feeds
// (onConfirm resets its per-interval clock) but never calls or is called by
// for the forward/contradiction logic itself. Geographic judgement and the
// protective stop are kept as separate authorities, as the conference and
// both audits require.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include "RouteMap.h"
#include "NaviEncounter.h"

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
  }
  void suspend() { state_ = TargetState::Unset; }

  // A completed encounter is in: confirm it against the single expected
  // target, or leave the target exactly as it is. Width disqualification is
  // inert until MIN_WIDTH_BLOCKS is set (NaviEncounter.h); when it is, a
  // width failure is handled identically to a polarity failure here — both
  // are "nonqualifying," neither is a stop.
  JudgementResult onEncounter(const CompletedEncounter& e) {
    if (state_ != TargetState::Tracking) return JudgementResult::NoClaim;
    if (e.abandoned) return JudgementResult::NoClaim;    // never reached return-to-baseline: not evidence either way
    if (e.pol == expectedHallSign(next_) && e.meetsMinWidth) {
      last_ = next_; next_ = navi_one::nextMarker(next_, dir_);
      return JudgementResult::Confirmed;
    }
    return JudgementResult::NonQualifying;                // target unchanged; keep observing for it
  }

  TargetState state() const { return state_; }
  uint8_t lastMm() const { return last_; }
  uint8_t nextMm() const { return next_; }
  int8_t dir() const { return dir_; }

 private:
  TargetState state_ = TargetState::Unset;
  uint8_t last_ = 0, next_ = 0;
  int8_t dir_ = 0;
};

}  // namespace navi_power
