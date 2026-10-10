#pragma once

#include <stdint.h>

#include "../NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/RouteMap.h"
#include "../NAVI_EYES_WIDE_OPEN/NaviCore.h"

namespace navi_eyes {

// Route data only. NAVI owns which target is current and when it changes.
// Both Hall sensors are empirically mounted alike. The legacy Otto
// HALL_POLARITY_INVERTED flag was unused and must not become EWO authority.
inline TargetSpec mappedTargetAfter(uint8_t currentMm, int8_t routeDirection) {
  TargetSpec target;
  if (routeDirection != 1 && routeDirection != -1) return target;
  const uint8_t next = navi_one::nextMarker(currentMm, routeDirection);
  const bool north = navi_one::polarityAt(next) != 0;
  target.polarity = north ? HallOpeningPolarity::AboveReference
                          : HallOpeningPolarity::BelowReference;
  target.distanceMm = navi_one::spanMm(currentMm, routeDirection);
  target.sequence = next;
  target.direction = routeDirection > 0 ? 1 : 2;
  return target;
}

}  // namespace navi_eyes
