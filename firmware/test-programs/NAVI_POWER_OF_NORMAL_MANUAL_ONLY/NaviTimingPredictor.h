#pragma once
// ---------------------------------------------------------------------------
// NaviTimingPredictor — geographic timing as qualification evidence
// (David, 2026-10-24, reconciled against the governing documents: the
// integrated proposal's §1 "movement evidence predicts where and when that
// target should arrive" and the decision register's movement-time-predictor
// row, "the conference supports movement-derived timing but not one
// formula... do not let timing create magnetic confirmation").
//
// WHAT THIS IS. After confirming marker k, NAVI expects marker k+dir. The
// measured opening-to-opening travel time for THAT SPECIFIC mapped interval
// (keyed by the FROM marker and direction — the two directions of the same
// physical gap are not assumed to take the same time) predicts when the
// next opening should occur. This is evidence for qualification, not a
// stop mechanism: nothing here requests a motor action. A stopped-clock
// read is not an emergency; it is one of four things that must agree.
//
// PREDICTION METHOD (per instruction, 2026-10-24): the observed opening-to-
// opening interval IS the prediction. No smoothing, no percentage margin, no
// invented acceptance window, no PWM-to-pace proportional assumption. The
// last real measurement for a given (fromMm, dir) is the next prediction for
// that same interval; it updates only when a traversal of that interval is
// actually confirmed, so the history stays tied to verified-correct
// intervals, not to a guess.
//
// NO TOLERANCE SET YET. Real variation was measured 2026-10-24 against
// today's field run (175 clean intervals of a 177-marker CW lap, excluding
// the one station-dwell outlier at mm125 and the known curve interval):
// pace median 235 mm/s, p10-p90 199-271 mm/s, CV 11.7%. That is evidence
// toward an acceptance window, not a decision — TIMING_TOLERANCE_ESTABLISHED
// stays false, and agreement() fails OPEN (reports true) until a real
// tolerance is approved, exactly mirroring this codebase's own precedent in
// NaviArrivalBoundary.h's BOUNDARY_UNSET: an unmeasured quantity must not
// invent a number, and must not silently block real confirmations either.
// This is a deliberate engineering choice to flag, not a hidden assumption.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include "RouteMap.h"

namespace navi_power {

static constexpr bool TIMING_TOLERANCE_ESTABLISHED = false;  // DO NOT set true without an approved acceptance window

struct TimingEvidence {
  bool havePrediction = false;
  uint32_t predictedMs = 0;
  uint32_t measuredMs = 0;
};

class TimingPredictor {
 public:
  // Call the instant a genuine opening occurs (not an echo), for the
  // interval FROM the last confirmed marker in the current direction.
  // `measuredMs` is the elapsed time since the previous genuine opening —
  // the opening-to-opening measurement for this specific interval.
  TimingEvidence evaluate(uint8_t fromMm, int8_t dir, uint32_t measuredMs) const {
    TimingEvidence ev;
    ev.measuredMs = measuredMs;
    const Entry& e = table_[slot(fromMm, dir)];
    ev.havePrediction = e.have;
    ev.predictedMs = e.have ? e.ms : 0;
    return ev;
  }

  // Whether the evidence agrees with the prediction. Fails open (true) when
  // no prediction exists yet for this interval, or when no tolerance has
  // been approved — see the class comment. Structurally present so the
  // decision is visible and recorded; not yet a numeric gate.
  bool agrees(const TimingEvidence& ev) const {
    if (!ev.havePrediction) return true;          // nothing observed yet for this interval: fail open
    if (!TIMING_TOLERANCE_ESTABLISHED) return true; // no approved acceptance window yet: fail open
    return true;  // placeholder for the approved tolerance comparison once TIMING_TOLERANCE_ESTABLISHED is true
  }

  // Call only when a traversal of (fromMm, dir) is actually CONFIRMED, so
  // the recorded history stays tied to verified-correct intervals.
  void observe(uint8_t fromMm, int8_t dir, uint32_t measuredMs) {
    Entry& e = table_[slot(fromMm, dir)];
    e.ms = measuredMs; e.have = true;
  }

 private:
  static uint16_t slot(uint8_t fromMm, int8_t dir) {
    // CW and CCW traversals of the same gap are not assumed equal; separate slots.
    return uint16_t(fromMm) * 2 + (dir > 0 ? 0 : 1);
  }
  struct Entry { bool have = false; uint32_t ms = 0; };
  Entry table_[navi_one::ROUTE_N * 2];
};

}  // namespace navi_power
