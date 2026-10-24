#pragma once
// ---------------------------------------------------------------------------
// NaviArrivalBoundary — the ONLY mechanism in this build permitted to stop
// AUTO for a navigation reason (Proposition 6B, closed 2026-10-24). A
// nonqualifying encounter never stops anything (NaviTarget.h); only failing
// to confirm the single expected target before its empirically-set latest
// permissible arrival does. This class never touches geographic position —
// it requests a motor stop and nothing else, exactly the separation both
// audits found missing in r5.1 (there, the Navigator's Contradiction state
// did both jobs at once).
//
// WHAT IS MISSING, ON PURPOSE. The conference closed the PRINCIPLE (stop only
// on a validated latest-arrival boundary, pace-normalized, per mapped
// interval) but explicitly left the NUMERICAL METHOD open: Codex's own data
// showed the candidate three-sigma rule exceeded 4.19%/2.95% of the time —
// far above a true 3-sigma tail — so that number must not simply be assumed
// here. BOUNDARY_TABLE below is intentionally UNPOPULATED (every entry
// BOUNDARY_UNSET). An unset interval means this class enforces no boundary
// there and says so (unsetIntervals()), rather than guessing a margin. Filling
// the table is a data-driven follow-up (fit against the historical per-
// interval travel-time corpus), not a decision made in this file.
//
// PACE NORMALIZATION. The clock compares ELAPSED PACE-ADJUSTED TIME, not wall
// time, to the boundary: elapsed wall time is scaled by (reference PWM /
// current average PWM since the target was set), so a locomotive running
// slower than the historical reference does not trip a boundary meant for
// normal pace. This mirrors the conference's own framing ("the magnet is
// late but within normal variation ... conditions are still normal") rather
// than inventing a separate concept.
//
// HARD GATE. ARRIVAL_BOUNDARY_VALIDATED is false. The sketch's admitAuto()
// must refuse AUTO while it is false, regardless of any other state. Per
// David's instruction (2026-10-24): the boundary must be validated against
// recorded evidence before the new NAVI is allowed to run AUTO on hardware.
// Flipping this flag is not an engineering judgement call available to the
// agent that writes this file — it requires that validation to exist first.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include "RouteMap.h"

namespace navi_power {

static constexpr bool ARRIVAL_BOUNDARY_VALIDATED = false;  // DO NOT FLIP without a passing replay against recorded evidence

static constexpr uint32_t BOUNDARY_UNSET = 0xFFFFFFFFu;    // this interval: no boundary enforced; a fact, not a guess
static uint32_t BOUNDARY_TABLE_MS[navi_one::ROUTE_N];       // [mm] = latest permissible pace-adjusted ms from leaving mm to confirming nextMarker(mm, dir). Zero-initialized; populate via a generated table once fit, as RvmShapeModel.h was for the old recognizer. CW and CCW currently share one slot per mm pending a decision on whether they need separate tables — an open question, not resolved here.

inline void boundaryTableInit() {    // call once at startup until a generated table replaces this
  for (uint8_t i = 0; i < navi_one::ROUTE_N; ++i) BOUNDARY_TABLE_MS[i] = BOUNDARY_UNSET;
}

class ArrivalBoundary {
 public:
  // Call when the target changes (declare, or a Confirmed judgement) to reset
  // the clock against the NEW expected interval.
  void startInterval(uint8_t fromMm, uint32_t nowMs, uint8_t referencePwm) {
    fromMm_ = fromMm; startMs_ = nowMs; running_ = true;
    paceAccumPwm_ = 0; paceSamples_ = 0; referencePwm_ = referencePwm;
  }
  void stop() { running_ = false; }

  // Sample the current applied PWM periodically (e.g. once per loop pass)
  // while running, to build the pace-adjustment ratio.
  void samplePace(uint8_t appliedPwm) {
    if (!running_ || appliedPwm == 0) return;        // standing time does not count as elapsed travel pace
    paceAccumPwm_ += appliedPwm; ++paceSamples_;
  }

  // True if the latest permissible pace-adjusted arrival has passed without a
  // confirmation. False whenever the interval's boundary is unset — an
  // unmeasured interval must fail open, not invent a stop.
  bool exceeded(uint32_t nowMs) const {
    if (!running_) return false;
    const uint32_t boundary = BOUNDARY_TABLE_MS[fromMm_];
    if (boundary == BOUNDARY_UNSET) return false;
    const uint32_t elapsedMs = nowMs - startMs_;
    if (paceSamples_ == 0 || referencePwm_ == 0) return elapsedMs > boundary;   // no pace data yet: compare raw elapsed, conservatively
    const double avgPwm = double(paceAccumPwm_) / double(paceSamples_);
    if (avgPwm <= 0) return elapsedMs > boundary;
    const double paceAdjustedMs = double(elapsedMs) * (double(referencePwm_) / avgPwm);
    return paceAdjustedMs > double(boundary);
  }
  bool running() const { return running_; }
  uint8_t intervalFromMm() const { return fromMm_; }

 private:
  bool running_ = false;
  uint8_t fromMm_ = 0, referencePwm_ = 90;
  uint32_t startMs_ = 0;
  uint32_t paceAccumPwm_ = 0, paceSamples_ = 0;
};

}  // namespace navi_power
