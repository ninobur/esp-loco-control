#include "../EwoStationStop.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace navi_eyes;

static bool near(double a, double b) {
  return std::fabs(a - b) < 1e-9;
}

int main() {
  const double expected[] = {40.0, 35.0, 30.0, 25.0,
                             20.0, 15.0, 10.0, 5.0};
  for (int offset = -5; offset <= 2; ++offset)
    assert(near(ewoStationTargetAtOffset(offset), expected[offset + 5]));

  const uint16_t interval = 300;
  assert(near(ewoStationTargetBetween(-5, 0, interval), 40.0));
  assert(near(ewoStationTargetBetween(-5, 150000, interval), 37.5));
  assert(near(ewoStationTargetBetween(-4, 300000, interval), 30.0));

  assert(near(ewoStationFinalTargetPkph(0), 5.0));
  assert(near(ewoStationFinalTargetPkph(75), 2.5));
  assert(near(ewoStationFinalTargetPkph(150), 0.0));
  assert(near(ewoStationFinalTargetPkph(225), 0.0));

  assert(ewoStationPwmTarget(60, 40.0, 35.0) > 60);
  assert(ewoStationPwmTarget(60, 40.0, 45.0) < 60);
  assert(ewoStationPwmTarget(60, 0.0, 0.0) == 0);

  EwoStationStopProfile stop;
  stop.begin(108, 1, 103, 1000000);  // Station -5, initial IR anchor.
  auto demand = stop.demand(103, 1000000, true, true,
                            40.0 * EWO_PKPH_MM_PER_SEC, 60);
  assert(demand.available && !demand.finalRamp && near(demand.targetPkph, 40.0));

  stop.noteAcceptedHall(104, 1300000);  // Station -4.
  demand = stop.demand(104, 1300000, true, true,
                       35.0 * EWO_PKPH_MM_PER_SEC, 50);
  assert(demand.available && near(demand.targetPkph, 35.0));

  // Reaching +2 without its accepted Hall event cannot start the final ramp.
  EwoStationStopProfile missingReference;
  missingReference.begin(108, 1, 110, 5000000);
  demand = missingReference.demand(110, 5000000, true, true,
                                   5.0 * EWO_PKPH_MM_PER_SEC, 20);
  assert(!demand.available && demand.referenceRequired &&
         !std::strcmp(demand.reason, "STATION_PLUS_TWO_HALL_REQUIRED"));

  stop.noteAcceptedHall(110, 5000000);  // Station +2: final physical reference.
  demand = stop.demand(110, 5000000, true, true,
                       5.0 * EWO_PKPH_MM_PER_SEC, 20);
  assert(demand.available && demand.finalRamp && near(demand.travelMm, 0.0) &&
         near(demand.targetPkph, 5.0));
  assert(stop.takeFinalStart());
  assert(!stop.takeFinalStart());

  demand = stop.demand(110, 5075000, true, true,
                       2.5 * EWO_PKPH_MM_PER_SEC, 10);
  assert(demand.finalRamp && near(demand.travelMm, 75.0) &&
         near(demand.targetPkph, 2.5) && !demand.stopReached);

  // Station +3 is not needed: IR distance alone completes the final stop.
  demand = stop.demand(111, 5150000, true, true, 0.0, 0);
  assert(demand.finalRamp && near(demand.travelMm, 150.0) &&
         near(demand.targetPkph, 0.0) && demand.stopReached);

  demand = stop.demand(111, 5150000, false, true, 0.0, 0);
  assert(!demand.available && !demand.referenceRequired &&
         !std::strcmp(demand.reason, "IR_REQUIRED_UNAVAILABLE"));

  std::puts("PASS: EWO measured station profile, continuous interpolation, +2 IR reference, 150 mm final stop");
}
