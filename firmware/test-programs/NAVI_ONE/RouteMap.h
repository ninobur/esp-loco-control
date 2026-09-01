#pragma once
// ---------------------------------------------------------------------------
// RouteMap — the surveyed truth of the Lowline. Generated from QUORUM.ino,
// which carried these tables from the original survey; unchanged in value.
//
// This is a DECLARATION (decision 0056). It is maintained by a person, and the
// navigator cannot verify it. When the physical route changes and this file
// does not, this file is lying and the navigator's only correct response is to
// refuse and stop.
// ---------------------------------------------------------------------------
#include <stdint.h>

namespace navi_one {

static constexpr uint8_t  ROUTE_N = 171;

// Magnet polarity at each marker. 1 = North, 0 = South.
// Every window of length 10 is unique across the route, which is what makes a
// ten-marker sequence provable rather than merely consistent.
static const uint8_t ROUTE_POLARITY[ROUTE_N] = {
  1,1,0,0,0,1,1,0,0,0,0,1,1,1,1,0,0,1,0,0,
  0,1,0,1,1,1,0,0,1,1,1,1,1,0,1,1,0,0,0,0,
  0,0,1,0,1,0,0,0,1,1,0,1,0,0,0,0,1,0,0,1,
  0,1,0,1,0,1,1,0,0,1,0,1,0,1,1,0,1,1,1,0,
  1,1,1,1,0,0,0,1,1,0,1,1,0,0,1,0,1,1,0,0,
  1,0,0,1,0,0,0,1,1,1,1,1,1,1,0,1,0,0,1,1,
  1,0,0,0,1,0,1,1,0,1,0,1,1,0,0,1,1,0,0,0,
  0,0,1,0,1,1,1,1,0,1,0,1,1,1,0,1,0,1,0,0,
  1,0,1,0,0,0,0,1,1,1,0,
};

// Surveyed spacing in REAL millimetres between marker i and marker i+1 (CW).
// Sums to 52150 mm. Minimum 280, maximum 355, mean 304.
static const uint16_t ROUTE_SPACING_MM[ROUTE_N] = {
  330,340,330,315,325,330,315,300,300,295,
  300,290,300,315,315,325,310,300,300,320,
  315,315,305,300,295,300,300,300,300,315,
  330,320,315,310,300,300,300,300,300,300,
  300,300,300,300,300,300,300,300,300,300,
  300,300,300,300,300,300,300,295,320,300,
  315,320,315,325,315,305,300,305,300,300,
  295,295,300,300,300,300,300,300,300,305,
  300,300,305,300,300,330,300,300,305,300,
  300,300,300,300,300,300,300,300,300,300,
  300,300,295,300,300,300,300,300,305,300,
  300,320,320,300,300,300,300,300,300,300,
  300,300,300,300,300,300,280,300,300,290,
  300,300,300,300,300,300,300,300,300,300,
  300,300,300,300,305,300,305,300,295,300,
  300,300,305,300,300,305,320,290,320,300,
  300,305,330,330,320,325,315,355,330,330,
  330,
};

static inline uint8_t routeMod(int32_t v) {
  v %= ROUTE_N; if (v < 0) v += ROUTE_N; return (uint8_t)v;
}
// dir: +1 = CW (ascending), -1 = CCW (descending).
static inline uint8_t nextMarker(uint8_t mm, int8_t dir) {
  return routeMod((int32_t)mm + dir);
}
static inline uint8_t polarityAt(uint8_t mm) { return ROUTE_POLARITY[mm % ROUTE_N]; }
static inline char     poleChar(uint8_t p)    { return p ? 'N' : 'S'; }
// Distance from mm to the next marker in the given direction.
static inline uint16_t spanMm(uint8_t mm, int8_t dir) {
  return (dir > 0) ? ROUTE_SPACING_MM[mm % ROUTE_N]
                   : ROUTE_SPACING_MM[routeMod((int32_t)mm - 1)];
}

static inline const char* landmarkAt(uint8_t mm) {
  switch (mm) {
    case 0: return "Southpoint";
    case 15: return "Patio";
    case 63: return "Grillers";
    case 72: return "Westpoint";
    case 98: return "Northpoint";
    case 107: return "Arches";
    case 140: return "Eastpoint";
    case 157: return "Bamboo";
    default: return "";
  }
}

// ---------------------------------------------------------------------------
// SECTION CRUISE — the one place on this railway where the throttle depends on
// WHERE the locomotive is rather than only on what it was told.
//
// Climbing out of Grillers, CW, the locomotive needs more than the surveyed
// cruise: NAVI_2 recorded it stalling at 100 with three coaches. It also needs
// to give that back afterwards, and 0.4 and earlier simply dropped 120 to 90 in
// one request at the top — a step, not a ramp, and the operator could feel it.
//
// Operator's ruling, 2026-09-01:
//   "Lets make the throttle setting for the Grillers grade 110 PWM ... at MM 80,
//    start backing off on the throttle. 4 PWM per MM down to 90 PWM, but
//    decreasing 1 PWM at a time evenly across each MM so that there is not an
//    abrupt change of speed. At MM 85 and thereafter, except at stations, PWM is
//    90. on CW runs."
//
// 120 was measured as too much — "it zooms up the grade" — so the grade runs at
// 110. CCW is untouched: the same stretch is downhill and needs nothing.
//
// The smoothness comes from the ONE COUNT AT A TIME, not from the pacing being
// clever. Measured calibration (Otto with two coaches, 2026-06-30, 4,617
// samples) puts a marker at 0.9–1.3 s over this speed range, so 280 ms a count
// lands four counts inside a marker either way. The same data shows marker time
// varying 31% at a FIXED throttle depending where you are on the loop — larger
// than the 24% the speed change itself accounts for — so a per-step pacing
// table would have been precision the railway cannot honour.
// ---------------------------------------------------------------------------
static const uint8_t GRADE_FROM_CW     = 65;   // climb begins, CW
static const uint8_t GRADE_TOP_CW      = 80;   // top: backing off starts here
static const uint8_t GRADE_CRUISE_PWM  = 110;  // was 120; 120 zooms the grade
static const uint8_t GRADE_RAMP_PER_MM = 4;    // PWM shed per marker
static const uint8_t GRADE_RAMP_END_CW = 85;   // base cruise from here on
static const uint16_t GRADE_STEP_MS    = 280;  // one PWM count at a time

// The cruise PWM for a position. Pure: no state, no hardware, gate-testable.
// dir: +1 CW, -1 CCW, 0 unset. Anything but CW returns the base cruise.
inline uint8_t cruisePwmAt(uint8_t mm, int8_t dir, uint8_t baseCruise) {
  if (dir <= 0) return baseCruise;
  mm = (uint8_t)(mm % ROUTE_N);
  if (mm >= GRADE_FROM_CW && mm < GRADE_TOP_CW) return GRADE_CRUISE_PWM;
  if (mm >= GRADE_TOP_CW && mm < GRADE_RAMP_END_CW) {
    // MM80 -> 106, MM81 -> 102, MM82 -> 98, MM83 -> 94, MM84 -> 90.
    // The locomotive walks to each target across the following marker, so it
    // arrives at MM85 already at base cruise, which is what the ruling asks.
    const int steps = (int)mm - (int)GRADE_TOP_CW + 1;            // 1..5
    const int v = (int)GRADE_CRUISE_PWM - (int)GRADE_RAMP_PER_MM * steps;
    return (uint8_t)(v > (int)baseCruise ? v : baseCruise);
  }
  return baseCruise;
}

}  // namespace navi_one
