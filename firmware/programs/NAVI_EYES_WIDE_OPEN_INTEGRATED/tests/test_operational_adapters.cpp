#include <cassert>
#include <cstring>
#include <cstdio>

#define NAVI_APPROACH_MARKER_MS {1390,1539,1725,1960,2271}
#include "../../NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/Ops.h"
#include "../../NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/Stations.h"
#include "../NaviIntegratedCore.h"

using namespace navi_one;
using namespace navi_eyes;

int main() {
  bool emergency = false;
  assert(!parseEstop("unreadable", emergency) && emergency);
  Ops o;
  o.sessionDir = 1;
  assert(admitStartMarker(o) == nullptr);
  o.actualPwm = 40;
  assert(admitStartMarker(o) != nullptr);
  o.actualPwm = 0;
  o.commandedPwm = 40;
  assert(admitMotorDirection(o) != nullptr);
  o.commandedPwm = 0;
  o.positionKnown = true;
  assert(admitAuto(o) == nullptr);
  o.enrolled = true;
  assert(admitThrottle(o) != nullptr);
  o.enrolled = false;
  o.lowVoltage = true;
  assert(admitAuto(o) != nullptr && admitThrottle(o) != nullptr);

  for (int direction : {-1, 1}) {
    for (const StationDefinition& station : STATIONS) {
      NaviIntegratedCore navi;
      const uint8_t inside = routeMod(int(station.centre) - 3 * direction);
      navi.declare(inside, direction, 100000);
      StationMachine machine;
      StationOrder order = machine.tick(navi.mm(), navi.direction(), 0, 90, 100);
      assert(order.setThrottle && order.pwm == stationPwm(station, direction) &&
             machine.phase() == StPhase::Zone);
      const uint8_t stop = routeMod(int(station.centre) +
                           stopOffsetFor(station, direction) * direction);
      order = machine.tick(stop, direction, stationPwm(station, direction), 90, 200);
      assert(order.setThrottle && order.pwm == 0 &&
             order.event && !std::strcmp(order.event, "ZERO_RAMP"));
      order = machine.tick(stop, direction, 0, 90, 300);
      assert(order.event && !std::strcmp(order.event, "DWELL_BEGIN"));
      order = machine.tick(stop, direction, 0, 90, 5300);
      assert(order.event && !std::strcmp(order.event, "DEPART"));
    }
  }
  std::puts("PASS: EWO operations admission and start-within-station adapter");
}
