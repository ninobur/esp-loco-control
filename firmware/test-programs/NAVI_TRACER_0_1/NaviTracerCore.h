#pragma once
// ============================================================================
// NaviTracerCore — causal, non-IR NAVI target-acquisition recognizer.
//
// Direct C++ port of the validated research implementation:
//   research/navi_target_acquisition_20261021/target_acquisition.py
//   commit 6efc952 ("research: corrective investigation of the PWM-90
//   target-acquisition experiment"), branch
//   research/navi-target-acquisition-pwm90-20261021.
//
// This file implements ONLY the recognizer:
//   CLOSE -> TARGET -> BASELINE -> ACQUIRE -> CLOSE AND ADVANCE
// exactly as the validated Python implements it, and nothing else. It has no
// Arduino dependency (stdint/math/string only) so the identical header can be
// compiled standalone on the host for the Python/C++ equivalence tests in
// tests/navi_tracer_0_1/, and compiled into the ESP32 sketch unchanged.
//
// Deliberate differences from the Python reference, each one documented at
// the point it occurs (none change recognition decisions):
//   1. The ΔBIC Gaussian-vs-line shape diagnostic (target_acquisition.py's
//      `delta_bic`) is NOT ported. REPORT.md states it is "diagnostic only,
//      not a gate" — approved omission, NAVI_TRACER_0_1 authorization
//      2026-10-22, item 2. Candidate.samples (needed in Python only to feed
//      that fit) is therefore not needed either; the C++ Candidate keeps only
//      the fixed fields the state machine itself consumes.
//   2. `baseline_hist` is a fixed-capacity ring (BASELINE_HIST_CAP) rather
//      than an unbounded Python list. Establishment requires only
//      MIN_BASELINE_BINS (8) samples spanning MIN_BASELINE_SPAN_S (0.65s);
//      the chosen capacity (128, i.e. ~2.56s at the 20ms cadence this
//      recognizer is built for) is architecturally unreachable in normal
//      operation because accumulation stops the moment the span/count
//      thresholds are met. Documented, not a behavior change.
//   3. All recognition arithmetic uses `double`, matching numpy's float64
//      used throughout the Python reference, to minimize floating-point
//      divergence between the two implementations during equivalence
//      testing.
//   4. Candidate-closure's seed for the next baseline keeps only the last
//      RTB_QUIET_BINS (6) (t, hall) samples — `c.samples[-RTB_QUIET_BINS:]`
//      in Python — stored directly in a 6-slot ring on Candidate instead of
//      slicing a full sample list, since (per difference 1) the full list is
//      no longer kept at all.
//
// Everything else — thresholds, freeze-the-baseline-on-open semantics,
// return-to-baseline criteria, the total absence of any LOST/timeout/
// rejected-count mechanism — is ported verbatim from the Python module
// docstring and body, including the module's own threshold justifications
// reproduced below.
// ============================================================================
#include <stdint.h>
#include <math.h>

#include "RouteMap.h"   // navi_one::nextMarker, navi_one::polarityAt

namespace navi_tracer {

// ---------------------------------------------------------------------------
// Thresholds, each with the same justification target_acquisition.py states.
// None is derived from IR or the historical +-15% tolerance.
// ---------------------------------------------------------------------------

// Hall departure criterion: given verbatim by the assignment the research
// implemented ("at least 70 ADC counts... for two consecutive samples").
// Applied to 20-ms bin means (RvmTracing.h already bins this way).
static constexpr double HALL_DEPARTURE_ADC = 70.0;
static constexpr int DEPARTURE_CONSECUTIVE_BINS = 2;

// Minimum quiet span before a baseline fit is trusted: decision 0093's
// existing 650-ms Hall-only fallback guard, reused in the research unchanged.
static constexpr double MIN_BASELINE_SPAN_S = 0.65;
static constexpr int MIN_BASELINE_BINS = 8;

// Candidate minimum sustained (active) duration before ACQUIRED.
// NAVI_TRACER_0_1 authorization 2026-10-22, item 3: the independently
// validated DURATION-FLOOR configuration is the default, preserved exactly
// (0.02 s — the research's own evaluated floor, equal to the 2-consecutive-
// -bin departure criterion itself, i.e. no separate gate beyond it). The
// original 110 ms figure remains available as a build-time-only alternative
// and must not alter the default.
static constexpr double DURATION_FLOOR_S = 0.02;
static constexpr double MIN_ACQUIRE_DURATION_LEGACY_110MS_S = 0.11;
#if defined(NAVI_TRACER_USE_LEGACY_110MS_DURATION)
static constexpr double DEFAULT_MIN_ACQUIRE_DURATION_S = MIN_ACQUIRE_DURATION_LEGACY_110MS_S;
#else
static constexpr double DEFAULT_MIN_ACQUIRE_DURATION_S = DURATION_FLOOR_S;
#endif

// Return-to-baseline: 120 ms (6 consecutive 20-ms bins) of renewed
// baseline-consistent samples. Unchanged from the research (itself unchanged
// from 98db802).
static constexpr int RTB_QUIET_BINS = 6;
static constexpr double RTB_QUIET_SIGMA = 3.0;

// The 20 ms bin period this recognizer's RTB closure-time back-dating
// (below) assumes — literally hardcoded in target_acquisition.py as `0.02`,
// not a named constant there either; named here for clarity, same value.
static constexpr double BIN_PERIOD_S = 0.02;

static constexpr int BASELINE_HIST_CAP = 128;   // see difference (2) above

// Otto convention (RvmNavigator.h, commit 3a8953c): North (1) = ADC departs
// above baseline; South (0) = ADC departs below baseline.
static inline int8_t hallSignForPolarity(uint8_t polarity) {
  return polarity ? 1 : -1;
}

enum class EventKind : uint8_t {
  OPEN_CANDIDATE = 0,
  ACQUIRED = 1,
  CONTRADICTION = 2,
  DISPOSED = 3,
  CLOSE_AND_ADVANCE = 4,
};

struct Event {
  EventKind kind;
  double t = 0.0;
  uint8_t target_mm = 0;
  // Populated fields depend on `kind`, mirroring target_acquisition.py's
  // per-kind Event.detail dict:
  //   OPEN_CANDIDATE:      sign, matches_target
  //   ACQUIRED:            start_s, duration_s
  //   CLOSE_AND_ADVANCE:   start_s, acquired_s, duration_s
  //   CONTRADICTION/DISPOSED: start_s, sign (the candidate's resolved sign),
  //                           peak_departure, duration_s, acquired
  int8_t sign = 0;
  bool matches_target = false;
  double start_s = 0.0;
  double acquired_s = 0.0;
  bool acquired = false;
  double duration_s = 0.0;
  double peak_departure = 0.0;
};

class TargetAcquisition {
 public:
  TargetAcquisition(uint8_t last_confirmed_mm, int8_t direction,
                     double min_acquire_duration_s = DEFAULT_MIN_ACQUIRE_DURATION_S)
      : last_confirmed_mm_(last_confirmed_mm), direction_(direction),
        min_acquire_duration_s_(min_acquire_duration_s) {
    target_mm_ = navi_one::nextMarker(last_confirmed_mm_, direction_);
  }

  // Consume one 20 ms bin. `hall` is NAN for a missing bin (a genuine sensor
  // gap -- missing evidence, not a stop). Returns the number of events
  // produced (0, 1, or 2) into out[], out[] must hold at least 2 Events.
  int observe(double t, double hall, int pwm, Event* out) {
    if (pwm == 0) {
      // Motive PWM = 0: non-navigation interval. Retained for the trace by
      // the caller; contributes nothing here, and whatever candidate/
      // baseline state existed is left untouched.
      // NAVI_TRACER_0_1 authorization 2026-10-22, item 1: literal `pwm == 0`
      // gate, no WaveformPause-style buffering, no new stopped-PWM
      // threshold. Acknowledged field-test uncertainty -- see the sketch's
      // telemetry for 0 < PWM < 15 samples.
      return 0;
    }
    if (!isfinite(hall)) {
      // A missing bin censors evidence; it never bridges an encounter and
      // never discards one either -- just contributes nothing.
      return 0;
    }
    if (!candidateOpen_) return observeBaseline(t, hall, out);
    return observeCandidate(t, hall, out);
  }

  uint8_t lastConfirmedMm() const { return last_confirmed_mm_; }
  uint8_t targetMm() const { return target_mm_; }
  uint8_t targetPolarity() const { return navi_one::polarityAt(target_mm_); }
  int8_t direction() const { return direction_; }
  bool candidateOpen() const { return candidateOpen_; }
  bool baselineEstablished() const { return baselineFitValid_; }
  double baselineLevel() const { return baselineLevel_; }
  double baselineSlope() const { return baselineSlope_; }
  double baselineNoise() const { return baselineNoise_; }
  // Current departure magnitude for telemetry: against the live baseline
  // while watching, or against the frozen reference while a candidate is
  // open. Returns 0 if no baseline is established yet.
  double lastDeparture() const { return lastDeparture_; }

 private:
  struct Candidate {
    double start_s = 0.0;
    int8_t resolved_sign = 0;
    bool matches_target = false;
    uint8_t target_mm = 0;
    uint8_t target_polarity = 0;
    bool acquired = false;
    double acquired_s = 0.0;
    double peak_departure = 0.0;
    int quiet_run = 0;
    double last_active_t = 0.0;
    // Last RTB_QUIET_BINS (t, hall) samples, oldest-first once full -- see
    // difference (4) in the file header. Used only to seed the next
    // baseline on closure, exactly as the Python's `c.samples[-RTB_QUIET_BINS:]`.
    double tailT[RTB_QUIET_BINS] = {0};
    double tailHall[RTB_QUIET_BINS] = {0};
    int tailCount = 0;   // number of valid entries, saturates at RTB_QUIET_BINS
    int tailHead = 0;    // ring head for the next write
    void pushTail(double t, double hall) {
      tailT[tailHead] = t; tailHall[tailHead] = hall;
      tailHead = (tailHead + 1) % RTB_QUIET_BINS;
      if (tailCount < RTB_QUIET_BINS) ++tailCount;
    }
  };

  // Pending consecutive same-sign departures, watched while no candidate is
  // open (mirrors `self._pending_departures` in the Python).
  struct PendingDeparture { double t; int8_t sign; double hall; double departure; };

  void tryEstablishBaseline() {
    if (baselineFitValid_) return;
    if (baselineHistCount_ < MIN_BASELINE_BINS) return;
    const double span = baselineHistT_[baselineHistCount_ - 1] - baselineHistT_[0];
    if (span < MIN_BASELINE_SPAN_S) return;
    const double t0 = baselineHistT_[baselineHistCount_ - 1];
    double n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (int i = 0; i < baselineHistCount_; ++i) {
      const double x = baselineHistT_[i] - t0;
      const double y = baselineHistHall_[i];
      n += 1; sx += x; sy += y; sxx += x * x; sxy += x * y;
    }
    const double den = n * sxx - sx * sx;
    const double slope = den > 1e-12 ? (n * sxy - sx * sy) / den : 0.0;
    const double intercept = (sy - slope * sx) / (n > 0 ? n : 1);
    double ss = 0;
    for (int i = 0; i < baselineHistCount_; ++i) {
      const double x = baselineHistT_[i] - t0;
      const double y = baselineHistHall_[i];
      const double resid = y - (intercept + slope * x);
      ss += resid * resid;
    }
    const double noise = fmax(sqrt(ss / (n > 0 ? n : 1)), 1.0);
    baselineLevel_ = intercept; baselineSlope_ = slope; baselineNoise_ = noise;
    baselineFitT0_ = t0; baselineFitValid_ = true;
  }

  // level + slope*(t - t0), noise -- or false if no baseline is established.
  bool expected(double t, double* level, double* noise) const {
    if (!baselineFitValid_) return false;
    *level = baselineLevel_ + baselineSlope_ * (t - baselineFitT0_);
    *noise = baselineNoise_;
    return true;
  }

  int observeBaseline(double t, double hall, Event* out) {
    tryEstablishBaseline();
    double level, noise;
    if (!expected(t, &level, &noise)) {
      // Baseline not yet established: keep collecting the quiet stretch
      // BASELINE needs. No departure test is meaningful without a
      // reference. See difference (2): fixed-capacity ring, architecturally
      // unreachable to overflow here (establishment completes within ~33
      // samples at 20ms cadence, well under the 128 capacity).
      if (baselineHistCount_ < BASELINE_HIST_CAP) {
        baselineHistT_[baselineHistCount_] = t;
        baselineHistHall_[baselineHistCount_] = hall;
        ++baselineHistCount_;
      }
      return 0;
    }
    const double departure = hall - level;
    lastDeparture_ = departure;
    if (fabs(departure) >= HALL_DEPARTURE_ADC) {
      const int8_t sign = departure > 0 ? 1 : -1;
      if (pendingCount_ > 0 && pending_[pendingCount_ - 1].sign != sign) pendingCount_ = 0;
      if (pendingCount_ < int(sizeof(pending_) / sizeof(pending_[0]))) {
        pending_[pendingCount_++] = PendingDeparture{t, sign, hall, departure};
      }
      if (pendingCount_ >= DEPARTURE_CONSECUTIVE_BINS) {
        const double start_t = pending_[0].t;
        cand_ = Candidate{};
        cand_.start_s = start_t; cand_.resolved_sign = sign;
        cand_.target_mm = target_mm_; cand_.target_polarity = targetPolarity();
        cand_.last_active_t = t;
        cand_.matches_target = (sign == hallSignForPolarity(cand_.target_polarity));
        double peak = 0;
        for (int i = 0; i < pendingCount_; ++i) peak = fmax(peak, fabs(pending_[i].departure));
        cand_.peak_departure = peak;
        candidateOpen_ = true;
        pendingCount_ = 0;
        out[0] = Event{};
        out[0].kind = EventKind::OPEN_CANDIDATE;
        out[0].t = t; out[0].target_mm = target_mm_;
        out[0].sign = sign; out[0].matches_target = cand_.matches_target;
        return 1;
      }
    } else {
      // A frozen baseline does not re-absorb samples -- it is only ever
      // re-established after an encounter closes.
      pendingCount_ = 0;
    }
    return 0;
  }

  int observeCandidate(double t, double hall, Event* out) {
    Candidate& c = cand_;
    c.pushTail(t, hall);
    double level, noise;
    if (!expected(c.start_s, &level, &noise)) { level = hall; noise = 1.0; }
    const double departure = fabs(hall - level);
    lastDeparture_ = departure;
    c.peak_departure = fmax(c.peak_departure, departure);
    const bool stillActive = departure > fmax(HALL_DEPARTURE_ADC / 2.0, RTB_QUIET_SIGMA * noise);
    if (stillActive) { c.last_active_t = t; c.quiet_run = 0; }
    else { ++c.quiet_run; }

    int n = 0;
    const double activeDuration = c.last_active_t - c.start_s;
    if (c.matches_target && !c.acquired && activeDuration >= min_acquire_duration_s_) {
      c.acquired = true; c.acquired_s = c.last_active_t;
      out[n] = Event{};
      out[n].kind = EventKind::ACQUIRED;
      out[n].t = c.last_active_t; out[n].target_mm = target_mm_;
      out[n].start_s = c.start_s; out[n].duration_s = activeDuration;
      ++n;
    }

    if (c.quiet_run >= RTB_QUIET_BINS) {
      const double close_t = t - (RTB_QUIET_BINS - 1) * BIN_PERIOD_S;
      if (c.matches_target && c.acquired) {
        out[n] = Event{};
        out[n].kind = EventKind::CLOSE_AND_ADVANCE;
        out[n].t = close_t; out[n].target_mm = target_mm_;
        out[n].start_s = c.start_s; out[n].acquired_s = c.acquired_s;
        out[n].duration_s = close_t - c.start_s;
        ++n;
        last_confirmed_mm_ = target_mm_;
        target_mm_ = navi_one::nextMarker(last_confirmed_mm_, direction_);
      } else {
        out[n] = Event{};
        out[n].kind = c.matches_target ? EventKind::DISPOSED : EventKind::CONTRADICTION;
        out[n].t = close_t; out[n].target_mm = target_mm_;
        out[n].start_s = c.start_s; out[n].sign = c.resolved_sign;
        out[n].peak_departure = c.peak_departure;
        out[n].duration_s = close_t - c.start_s; out[n].acquired = c.acquired;
        ++n;
      }
      // The quiet samples that closed the candidate seed the next BASELINE
      // phase; nothing about the target changes on dispose.
      baselineHistCount_ = c.tailCount;
      for (int i = 0; i < c.tailCount; ++i) {
        const int idx = (c.tailHead - c.tailCount + i + RTB_QUIET_BINS) % RTB_QUIET_BINS;
        baselineHistT_[i] = c.tailT[idx];
        baselineHistHall_[i] = c.tailHall[idx];
      }
      baselineFitValid_ = false;
      candidateOpen_ = false;
    }
    return n;
  }

  uint8_t last_confirmed_mm_;
  int8_t direction_;
  double min_acquire_duration_s_;
  uint8_t target_mm_ = 0;

  double baselineHistT_[BASELINE_HIST_CAP];
  double baselineHistHall_[BASELINE_HIST_CAP];
  int baselineHistCount_ = 0;
  bool baselineFitValid_ = false;
  double baselineLevel_ = 0.0, baselineSlope_ = 0.0, baselineNoise_ = 1.0, baselineFitT0_ = 0.0;
  double lastDeparture_ = 0.0;

  PendingDeparture pending_[DEPARTURE_CONSECUTIVE_BINS + 1];
  int pendingCount_ = 0;

  bool candidateOpen_ = false;
  Candidate cand_;
};

}  // namespace navi_tracer
