#pragma once
// ---------------------------------------------------------------------------
// RvmNavigator — geographic progression from recognized ensembles. Observation
// only: it has no motor, throttle, brake or E-STOP access and nothing in this
// sketch reads its state to drive the locomotive.
//
//   declare(last, next, dir)  operator: the Hall sensor stands in the baseline
//                             between `last` and `next`, travelling `dir`.
//   onEnsemble(pol)           a complete ensemble was recognized. If its
//                             principal polarity is the ROUTE polarity of
//                             `next`, then next IS the MM just passed: the
//                             piece last -> BL -> next is complete, and next
//                             becomes the first MM of the following piece.
//                             Otherwise: CONTRADICTION. The waveform and the
//                             context are reported; no further geographic
//                             claim until the operator declares again.
//   suspend(reason)           direction changed while declared: no geographic
//                             claim until the operator declares again.
// There is no recovery, no skipping, no alternative interpretation.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include "RouteMap.h"

namespace rvm_shape {

enum class NavState : uint8_t { Unset = 0, Tracking, Contradiction };
enum class NavResult : uint8_t { NoClaim = 0, Advanced, Contradiction };

inline int8_t expectedHallSign(uint8_t mm) {      // Otto: ROUTE 1 = North = ADC above baseline
  return navi_one::polarityAt(mm) ? 1 : -1;
}

class Navigator {
 public:
  void declare(uint8_t last, uint8_t next, int8_t dir) {
    last_ = last; next_ = next; dir_ = dir; state_ = NavState::Tracking; contradictedMm_ = 0xFF;
  }
  void suspend() { state_ = NavState::Unset; }
  NavResult onEnsemble(int8_t pol) {
    if (state_ != NavState::Tracking) return NavResult::NoClaim;
    if (pol == expectedHallSign(next_)) {
      last_ = next_; next_ = navi_one::nextMarker(next_, dir_);
      return NavResult::Advanced;
    }
    contradictedMm_ = next_; state_ = NavState::Contradiction;
    return NavResult::Contradiction;
  }
  NavState state() const { return state_; }
  uint8_t lastMm() const { return last_; }      // the most recent MM behind the Hall sensor
  uint8_t nextMm() const { return next_; }      // the MM the map expects next
  int8_t dir() const { return dir_; }
  uint8_t contradictedMm() const { return contradictedMm_; }
 private:
  NavState state_ = NavState::Unset;
  uint8_t last_ = 0, next_ = 0, contradictedMm_ = 0xFF; int8_t dir_ = 0;
};

}  // namespace rvm_shape
