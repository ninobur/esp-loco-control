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
  // Departure throttle. ZERO means "as per routing" -- take whatever the
  // section cruise says, which is the case at every platform but one.
  uint8_t departCW, departCCW;
};

// Grillers is the one asymmetric platform, in three separate ways, all of them
// clockwise and all of them the grade:
//
//   * 72 CCW and 60 CW, because "72 is wrong for downhill at Grillers".
//   * stop at -1 in BOTH directions, against the +1 standard, for two
//     different reasons.
//
//     CW, 2026-09-01: the +1 stop put Toby ON the climb. "The problem was Toby
//     was attempting to start the grade while on the grade. At that point,
//     traction not adequate to launch 3 coaches." A marker short of centre
//     gives him level track to launch from and a run at the grade.
//
//     CCW, later the same day: the same +1 stop overshot. "What is different
//     about Grillers is that it is a station at the end of the grade. Toby is
//     also stopping too late for the station geography." Counter-clockwise is
//     the DESCENT and he holds 72 into it, so he carried two markers past the
//     ramp point -- ZERO_RAMP at MM62, rest at MM60 -- against one marker
//     clockwise. That landing put the sensor in MM59's field and the 37.7 s
//     dwell passage was rejected on shape, losing the marker. Finding 11.
//     Moving the ramp two markers earlier, to MM64, is the operator's
//     instruction and is explicitly an experiment: "Let'"'"'s see if the Hall
//     sensor still lands too close to a magnet."
//   * departs to 110 CW. "PWM 110 is sustained after Grillers CW departure
//     until MM80 then use current ramp down." The section cruise reaches 110
//     at MM65 on its own (decision 0066), so this only covers the markers
//     between the stop and the band; the MM80 ramp-down is untouched.
//
// Everything else starts from the standard and gets tuned per platform per
// direction from observed landings.
// COUNTER-CLOCKWISE STOP OFFSETS ARE MEASURED, NOT ASSUMED
// --------------------------------------------------------
// Landings from the 0.8 CCW run of 2026-09-01, identical on both laps, so these
// are measurements rather than one-off observations:
//
//     station    ramp at   came to rest   coast
//     Arches       +1          +2           1 marker
//     Grillers     -1          +2           3 markers
//     Patio        +1          +2           1 marker
//     Bamboo       +1          +3           2 markers
//
// The operator then set each one from where it actually landed: "Patio CCW only
// adjust to stop one magnet sooner. Bamboo CCW only adjust to stop 2 magnets
// sooner. Arches CCW only stop 1 magnet sooner. Grillers problem currently
// solved." Clockwise offsets are untouched by that ruling.
static const StationDefinition STATIONS[] = {
  //  name        centre  pwmCW pwmCCW  stopCW stopCCW  depCW depCCW   0 = as per routing
  { "Patio",         15,    60,    60,      1,      0,      0,     0 },
  { "Grillers",      63,    60,    72,     -1,     -1,    110,     0 },
  { "Arches",       108,    60,    60,      1,      0,      0,     0 },
  { "Bamboo",       157,    60,    60,      1,     -1,      0,     0 },
};
static const uint8_t STATION_COUNT = (uint8_t)(sizeof(STATIONS)/sizeof(STATIONS[0]));

static const int8_t   APPROACH_START     = -10;  // arm here
static const int8_t   ZONE_START         = -5;   // at station speed by here
static const int8_t   OVERSHOOT_ABANDON  = 5;    // past centre -> give up, stay honest
static const uint32_t STATION_DWELL_MS   = 30000UL;
static const uint16_t STATION_STOP_STEP_MS   = 200;  // the gentle brake
// "Restart from the station should have a slow ramp. 200."
//
// A 300 ms slope was tried on 2026-09-01 and withdrawn the same day on field
// observation: "Do not change Toby ramp slope out of Grillers." The slope was
// never the fault. "The problem was Toby was attempting to start the grade
// while on the grade. At that point, traction not adequate to launch 3
// coaches." The remedy is where he stops and what he departs to, below.
static const uint16_t STATION_DEPART_STEP_MS = 200;
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
// The throttle a departure asks for: the platform's own figure when it has one,
// otherwise the section cruise it was handed.
inline uint8_t departPwmFor(const StationDefinition& s, int8_t dir, uint8_t cruisePwm) {
  const uint8_t d = dir > 0 ? s.departCW : s.departCCW;
  return d ? d : cruisePwm;
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
  // cruisePwm -- the section cruise where the locomotive is. Sets the approach
  //              entry speed, is what an abandoned approach hands back, and is
  //              what a departure asks for unless the platform overrides it.
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
          return order(o, dir, off, departPwmFor(st, dir, cruisePwm),
                       STATION_DEPART_STEP_MS, "DEPART");
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
