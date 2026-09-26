#pragma once
#include "ExcursionDetectorX22R.h"

namespace ngr_nav {
// Reuse the measured acquisition machinery, not its former NAV authority.
// In particular, powered time is never treated as travelled distance.
// 20Q3: the detector is X22R (X22 with the obsolete refractory removed); the
// only timing rule with authority is NAVI's 650 ms Hall-only fallback.
inline ngr_hall::DetectorConfig hallConfig(int16_t departure) {
  ngr_hall::DetectorConfig c;
  c.departCounts=departure;c.lostMs=0;
  return c;
}
}
