#pragma once
#include <stdint.h>
namespace navi_eyes {
// Legacy controller payload types only. RVM does not run the retired Hall
// opening detector; polarity is metadata, never a recognition admission gate.
enum class HallOpeningPolarity : int8_t { Unknown=0, AboveReference=1, BelowReference=-1 };
struct TargetSpec {
  HallOpeningPolarity polarity=HallOpeningPolarity::Unknown;
  uint32_t distanceMm=0, sequence=0;
  uint8_t direction=0;
};
}
