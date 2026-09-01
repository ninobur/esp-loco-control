#pragma once
// ---------------------------------------------------------------------------
// Stations — approach, stop, dwell and departure. Pure: no Arduino, no
// hardware, no globals. It is handed a position and a clock and returns an
// order; the .ino decides whether to obey it. Gate 10 drives it directly.
//
// Restored to NAVI_ONE at the operator's direction, 2026-09-01, from the
// pattern that flew on NAVI_2, with the changes he specified:
//
//   * STOPPING POINTS ARE TUNABLE PER STATION AND PER DIRECTION. NAVI_2 had one
//     stopOffset per station and a blanket "if CCW, subtract one" applied to
//     every platform alike. Both directions are now explicit columns, seeded to
//     the same standard (+1 past centre) so tuning starts from one number.
//   * No finalPwm step. NAVI_2 eased to a second, lower speed at M+1; the
//     station speed is now held from the zone all the way to the stop trigger.
//     ("Station throttle setting is 60 PWM until the last ramp.")
//   * Dwell 30 s, deliberately longer than NAVI_2's 15. The operator wants the
//     dwell to exercise the baseline latch of finding 08: "I think that the
//     longer dwell has a bigger risk for latch."
//
// WHERE THE STOP ACTUALLY HAPPENS
// stopOffset is where the ZERO RAMP BEGINS, not where the locomotive comes to
// rest. At 200 ms a count from 60 the ramp runs about twelve seconds and
// carries it a further marker or so. +1 therefore puts it down around +2 from
// centre -- between magnets rather than on one, which for a 30 s dwell with the
// baseline frozen is the safer place to be sitting.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include "RouteMap.h"
// LocoConfig.h is NOT included here: it pulls in Arduino.h and this header must
// stay host-testable. The .ino includes the config before this file; a gate
// includes the locomotive profile it wants to test.

namespace navi_one {

struct StationDefinition {
  const char* name;
  uint8_t centre;
  uint8_t pwmCW, pwmCCW;          // speed held from the zone to the stop trigger
  int8_t  stopOffsetCW, stopOffsetCCW;   // markers from centre; zero ramp starts here
};

// Grillers CCW is the one asymmetry: 72 climbing, 60 the other way, because
// "72 is wrong for downhill at Grillers". Everything else starts from the
// standard and gets tuned per platform per direction from observed landings.
static const StationDefinition STATIONS[] = {
  //  name        centre  pwmCW pwmCCW  stopCW stopCCW
  { "Patio",         15,    60,    60,      1,      1 },
  { "Grillers",      63,    60,    72,      1,      1 },
  { "Arches",       108,    60,    60,      1,      1 },
  { "Bamboo",       157,    60,    60,      1,      1 },
};
static const uint8_t STATION_COUNT = (uint8_t)(sizeof(STATIONS)/sizeof(STATIONS[0]));

static const int8_t   APPROACH_START     = -10;  // arm here
static const int8_t   ZONE_START         = -5;   // at station speed by here
static const int8_t   OVERSHOOT_ABANDON  = 5;    // past centre -> give up, stay honest
static const uint32_t STATION_DWELL_MS   = 30000UL;
static const uint16_t STATION_STOP_STEP_MS   = 200;  // the gentle brake
static const uint16_t STATION_DEPART_STEP_MS = 200;  // "Restart ... should have a slow ramp. 200."
static const uint32_t STATION_MAX_PHASE_MS   = 120000UL;

// Marker times across the approach, one per marker from -10 to -6. The table is
// MARKER TIMES, not per-count pacing, so the same table serves any starting
// throttle: pacing = APPROACH_MARKER_MS[i] / (counts shed this marker). Six
// counts for a 90 -> 60 approach; nine for the 105 -> 60 Patio runs off the CCW
// curve of decision 0067.
//
// PER-LOCOMOTIVE, from the config header -- a faster locomotive crosses each
// marker in less time and needs its own numbers. No default is provided on
// purpose: a build for a locomotive nobody has measured should fail here rather
// than quietly pace its station stops with Toby's timings.
//
// ONE table for all four platforms, by operator's ruling: "one table. It will
// provide adequate smoothing." The approaches do differ -- measured zone index
// runs 0.87 (Patio CCW, climbing) to 1.13 (Arches CW, dropping) -- but that
// +/-15% sits inside a table whose own span is 63%, and eight more constants
// would have to be kept true as the railway changes.
#ifndef NAVI_APPROACH_MARKER_MS
#error "NAVI_APPROACH_MARKER_MS must be defined in this locomotive's config header"
#endif
static const uint16_t APPROACH_MARKER_MS[5] = NAVI_APPROACH_MARKER_MS;

enum class StPhase : uint8_t { Idle = 0, Approach, Zone, Ramp, Dwell, Depart };

inline const char* stPhaseName(StPhase p) {
  switch (p) {
    case StPhase::Idle:     return "IDLE";
    case StPhase::Approach: return "APPROACH";
    case StPhase::Zone:     return "ZONE";
    case StPhase::Ramp:     return "ZERO_RAMP";
    case StPhase::Dwell:    return "DWELL";
    case StPhase::Depart:   return "DEPART";
  }
  return "?";
}

// Markers from the centre in the direction of travel. Negative before it,
// positive past it. dir: +1 CW (ascending), -1 CCW (descending).
inline int16_t offsetToCentre(uint8_t mm, int8_t dir, uint8_t centre) {
  int32_t d = (dir > 0) ? routeMod((int32_t)mm - (int32_t)centre)
                        : routeMod((int32_t)centre - (int32_t)mm);
  return (d > ROUTE_N / 2) ? (int16_t)(d - ROUTE_N) : (int16_t)d;
}

inline uint8_t stationPwm(const StationDefinition& s, int8_t dir) {
  return dir > 0 ? s.pwmCW : s.pwmCCW;
}
inline int8_t stopOffsetFor(const StationDefinition& s, int8_t dir) {
  return dir > 0 ? s.stopOffsetCW : s.stopOffsetCCW;
}

struct StationOrder {
  bool        setThrottle = false;
  uint8_t     pwm         = 0;
  uint16_t    stepMs      = 0;     // per-count pacing for this move
  const char* event       = nullptr;  // non-null: worth publishing
  const char* station     = "";
  int16_t     offset      = 0;
};

class StationMachine {
 public:
  void reset() { phase_ = StPhase::Idle; idx_ = -1; entryPwm_ = 0; }
  StPhase phase()      const { return phase_; }
  int8_t  stationIdx() const { return idx_; }
  bool    holding()    const { return phase_ == StPhase::Ramp || phase_ == StPhase::Dwell; }

  // Called on every advance AND periodically, so the dwell clock and the ramp
  // completion are seen without waiting for a marker. Returns an order; an
  // order with setThrottle false and event null means "nothing to do".
  StationOrder tick(uint8_t mm, int8_t dir, uint8_t actualPwm,
                    uint8_t cruisePwm, uint32_t nowMs) {
    StationOrder o;
    if (dir == 0) { reset(); return o; }

    if (phase_ == StPhase::Idle) {
      for (uint8_t i = 0; i < STATION_COUNT; ++i) {
        const int16_t off = offsetToCentre(mm, dir, STATIONS[i].centre);
        if (off == APPROACH_START) {
          idx_ = (int8_t)i;
          // The approach ramps down from WHAT THE LOCOMOTIVE IS ACTUALLY DOING,
          // not from a hardcoded cruise. Patio CCW arrives at 105 off the curve
          // of decision 0067; taking it from 90 would put back the step that
          // decision exists to remove, just moved to the approach.
          entryPwm_ = cruisePwm > actualPwm ? cruisePwm : actualPwm;
          const uint8_t sp = stationPwm(STATIONS[i], dir);
          if (entryPwm_ < sp) entryPwm_ = sp;
          setPhase(StPhase::Approach, nowMs);
          lastOff_ = off;   // or the next tick, before a marker passes, re-issues this
          return order(o, dir, off, targetForApproach(off, dir), pacing(off, dir), "ARMED");
        }
      }
      return o;
    }

    const StationDefinition& st = STATIONS[idx_];
    const int16_t off = offsetToCentre(mm, dir, st.centre);
    o.station = st.name; o.offset = off;

    // Overshoot: say so and stand down rather than chase it.
    if (off > OVERSHOOT_ABANDON && phase_ != StPhase::Dwell && phase_ != StPhase::Depart) {
      reset(); o.event = "MISSED";
      o.setThrottle = true; o.pwm = cruisePwm; o.stepMs = STATION_DEPART_STEP_MS;
      return o;
    }
    // A phase that cannot complete must not hold the locomotive for ever.
    // Dwell has its own clock and Depart is deliberately exempt.
    if ((phase_ == StPhase::Approach || phase_ == StPhase::Zone || phase_ == StPhase::Ramp) &&
        nowMs - phaseAtMs_ > STATION_MAX_PHASE_MS) {
      reset(); o.event = "PHASE_TIMEOUT";
      o.setThrottle = true; o.pwm = cruisePwm; o.stepMs = STATION_DEPART_STEP_MS;
      return o;
    }

    const int8_t stopAt = stopOffsetFor(st, dir);

    switch (phase_) {
      case StPhase::Approach:
        if (off >= stopAt) { return startRamp(o, dir, off, nowMs); }
        if (off >= ZONE_START) {
          setPhase(StPhase::Zone, nowMs);
          return order(o, dir, off, stationPwm(st, dir), STATION_STOP_STEP_MS, "ZONE");
        }
        if (off > lastOff_ || lastOff_ == 127) {   // a marker has passed
          lastOff_ = off;
          return order(o, dir, off, targetForApproach(off, dir), pacing(off, dir), "APPROACH");
        }
        return o;

      case StPhase::Zone:
        if (off >= stopAt) return startRamp(o, dir, off, nowMs);
        return o;

      case StPhase::Ramp:
        if (actualPwm == 0) {
          setPhase(StPhase::Dwell, nowMs); dwellFromMs_ = nowMs;
          o.event = "DWELL_BEGIN";
        }
        return o;

      case StPhase::Dwell:
        if (nowMs - dwellFromMs_ >= STATION_DWELL_MS) {
          setPhase(StPhase::Depart, nowMs);
          return order(o, dir, off, cruisePwm, STATION_DEPART_STEP_MS, "DEPART");
        }
        return o;

      case StPhase::Depart:
        if (off >= stopAt + 3) { reset(); o.event = "DEPARTED"; }
        return o;

      default: return o;
    }
  }

 private:
  StPhase  phase_ = StPhase::Idle;
  int8_t   idx_ = -1;
  uint8_t  entryPwm_ = 0;
  int16_t  lastOff_ = 127;
  uint32_t phaseAtMs_ = 0, dwellFromMs_ = 0;

  void setPhase(StPhase p, uint32_t nowMs) {
    phase_ = p; phaseAtMs_ = nowMs;
    if (p == StPhase::Approach) lastOff_ = 127;
  }
  StationOrder& order(StationOrder& o, int8_t dir, int16_t off,
                      uint8_t pwm, uint16_t stepMs, const char* ev) {
    o.setThrottle = true; o.pwm = pwm; o.stepMs = stepMs; o.event = ev;
    o.offset = off; o.station = idx_ >= 0 ? STATIONS[idx_].name : "";
    (void)dir; return o;
  }
  StationOrder& startRamp(StationOrder& o, int8_t dir, int16_t off, uint32_t nowMs) {
    setPhase(StPhase::Ramp, nowMs);
    return order(o, dir, off, 0, STATION_STOP_STEP_MS, "ZERO_RAMP");
  }
  // Linear from the entry throttle to station speed across the five markers
  // -10..-6, so station speed is reached one marker before the zone.
  uint8_t targetForApproach(int16_t off, int8_t dir) const {
    const uint8_t sp = stationPwm(STATIONS[idx_], dir);
    if (off < APPROACH_START) return entryPwm_;
    if (off >= ZONE_START)    return sp;
    const int step = (int)off - (int)APPROACH_START + 1;      // 1..5
    const int v = (int)entryPwm_ - ((int)entryPwm_ - (int)sp) * step / 5;
    return (uint8_t)(v < (int)sp ? sp : v);
  }
  // Pace each count across the marker it belongs to. The table is marker TIMES,
  // so the same table serves a 6-count and a 9-count approach alike.
  uint16_t pacing(int16_t off, int8_t dir) const {
    int i = (int)off - (int)APPROACH_START;                   // 0..4
    if (i < 0) i = 0;
    if (i > 4) i = 4;
    const uint8_t sp = stationPwm(STATIONS[idx_], dir);
    int counts = ((int)entryPwm_ - (int)sp) / 5;
    if (counts < 1) counts = 1;
    return (uint16_t)(APPROACH_MARKER_MS[i] / counts);
  }
};

}  // namespace navi_one
