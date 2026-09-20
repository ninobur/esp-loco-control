#pragma once
#include <stdint.h>

namespace navi_one {
// Both arguments are locomotive-clock milliseconds and surveyed millimetres.
// PWM, historical speeds, polarity and waveform measurements are deliberately
// absent. Equality is reachable. Widen before multiplication.
inline bool physicallyUnreachable(uint32_t elapsedMs, uint16_t spacingMm,
                                  uint32_t physicalVmaxMmS) {
  return physicalVmaxMmS &&
         (uint64_t)elapsedMs * physicalVmaxMmS < (uint64_t)spacingMm * 1000ULL;
}
}
