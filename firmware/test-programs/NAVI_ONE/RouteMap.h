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

}  // namespace navi_one
