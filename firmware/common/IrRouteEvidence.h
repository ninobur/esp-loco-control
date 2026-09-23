#pragma once
#include <stddef.h>
#include "IrMovementContract.h"

namespace ir_movement {
enum Feasibility : uint8_t { UNKNOWN, POSSIBLE, EXCLUDED };
struct RouteCandidate {
  unsigned marker=0, steps=0;
  double travelMm=0;
  Feasibility feasibility=UNKNOWN;
};

// Positive progress in the supplied route direction is assumed. Wheel IR does
// not measure direction; the caller must establish it before using exclusions.
// extentMm must bound combined Hall localization and map errors for each tested
// candidate. Unknown extent or direction returns UNKNOWN, not a guessed identity.
inline size_t routeCandidates(const DistanceRange& distance,
                             const uint16_t* cwSpacing, size_t count,
                             unsigned anchor, int direction, unsigned maxSteps,
                             double extentMm, RouteCandidate* out, size_t capacity) {
  if(!cwSpacing || !count || anchor>=count || !out) return 0;
  unsigned marker=anchor;
  double travel=0;
  const bool geometryKnown=(direction==1 || direction==-1) &&
      isfinite(extentMm) && extentMm>=0;
  bool spacingsKnown=true;
  size_t written=0;
  for(unsigned step=0;written<capacity && step<=maxSteps;++step) {
    RouteCandidate c;c.marker=marker;c.steps=step;c.travelMm=travel;
    if(geometryKnown && spacingsKnown && distance.bounded()) {
      double low=travel>extentMm ? travel-extentMm:0;
      c.feasibility=couldReach(distance,low,travel+extentMm)?POSSIBLE:EXCLUDED;
    }
    out[written++]=c;
    if(step==maxSteps)break;
    unsigned segment=direction<0 ? (marker+count-1)%count : marker;
    if(!cwSpacing[segment])spacingsKnown=false;
    travel+=cwSpacing[segment];
    marker=direction<0 ? (marker+count-1)%count : (marker+1)%count;
  }
  return written;
}
} // namespace ir_movement
