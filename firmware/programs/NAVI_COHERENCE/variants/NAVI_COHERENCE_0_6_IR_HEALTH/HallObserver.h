#pragma once
#include "../../../NAVI_ONE/variants/NAVI_ONE_X22/ExcursionDetector.h"

namespace ngr_nav {
// Reuse the measured acquisition machinery, not its former NAV authority.
// In particular, powered time is never treated as travelled distance.
inline navi_one::DetectorConfig hallConfig(int16_t departure) {
  navi_one::DetectorConfig c;
  c.departCounts=departure;c.refractoryMs=0;c.lostMs=0;
  return c;
}
}
