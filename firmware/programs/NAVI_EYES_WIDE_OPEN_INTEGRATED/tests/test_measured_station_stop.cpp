#include "../EwoStationStop.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace navi_eyes;

static bool near(double a, double b, double epsilon = 1e-6) {
  return std::fabs(a - b) <= epsilon;
}

static uint64_t umPerPulse(uint64_t pulses) {
  return pulses * uint64_t(EWO_IR_PITCH_UM);
}

static EwoStationStopProfile finalAtZero(uint64_t irUm, uint64_t pulses,
                                         int pwm = 60, double speed = 25.0) {
  EwoStationStopProfile stop;
  stop.begin(108, 1, 108, irUm, pulses, pwm, speed, 400.0, 60, 200, 1000000);
  stop.noteAcceptedHall(108, irUm, EWO_IR_PITCH_UM);
  return stop;
}

int main() {
  // A single continuous approach starts at the accepted -10 Hall event and
  // reaches the station-speed target at -5. It does not contain 40/35/... PWM
  // steps or a speed-to-PWM lookup.
  EwoStationStopProfile stop;
  const uint64_t approachIr = umPerPulse(100);
  stop.noteAcceptedHall(98, approachIr, EWO_IR_PITCH_UM);  // Station -10.
  stop.begin(108, 1, 98, approachIr, 100, 90, 40.0, 400.0, 60, 200, 1000000);
  auto demand = stop.demand(98, approachIr, 100, 1000000, true, true,
                            40.0 * EWO_PKPH_MM_PER_SEC, 90);
  assert(demand.available && demand.approachRamp);
  assert(near(demand.targetPkph, 40.0));
  assert(demand.pwmTarget == 60 && demand.pwmUpMs == 0);
  assert(near(demand.appliedDownMs, 400.0));

  demand = stop.demand(98, approachIr + EWO_IR_PITCH_UM, 101, 2000000,
                       true, true, 39.5 * EWO_PKPH_MM_PER_SEC, 89);
  assert(demand.newIrObservation && demand.pwmTarget <= 89);

  // Station -5 through 0 is one station-speed hold, not a sequence of
  // intermediate speed or PWM targets.
  stop.noteAcceptedHall(103, approachIr + 50000, EWO_IR_PITCH_UM);
  demand = stop.demand(103, approachIr + 50000, 105, 3000000, true, true,
                       25.0 * EWO_PKPH_MM_PER_SEC, 70);
  assert(demand.available && demand.stationHold && !demand.finalRamp);
  assert(near(demand.targetPkph, EWO_APPROACH_TARGET_PKPH));
  assert(demand.pwmTarget == 60);

  // Station 0 is the only final reference. The target distance is 35 pulses
  // at 9.652 mm/pulse, and the target speed is linear in IR distance.
  const uint64_t finalIr = approachIr + 100000;
  stop.noteAcceptedHall(108, finalIr, EWO_IR_PITCH_UM);
  demand = stop.demand(108, finalIr, 110, 4000000, true, true,
                       25.0 * EWO_PKPH_MM_PER_SEC, 60);
  assert(demand.available && demand.finalRamp);
  assert(near(demand.distanceMm, 0.0));
  assert(near(demand.targetPkph, 5.0));
  assert(demand.pwmTarget == 0 && demand.pwmUpMs == 0);
  assert(near(demand.targetStopMm, 35.0 * 9.652));

  // Ten completed pulses are only 96.52 mm. A fresh stationary Type-5
  // snapshot stops the controller even though the 150/35-pulse endpoint has
  // not been reached and actual PWM is still nonzero.
  demand = stop.demand(108, finalIr + umPerPulse(10), 120, 5000000,
                       true, true, 24.5 * EWO_PKPH_MM_PER_SEC, 59);
  assert(demand.newIrObservation && demand.finalRamp && !demand.stopReached);
  assert(demand.judgment == 1 || demand.judgment == 2 || demand.judgment == 3);
  demand = stop.demand(108, finalIr + umPerPulse(10), 120, 6000000,
                       true, true, 0.0, 30);
  assert(demand.stopReached && demand.pwmTarget == 0);
  assert(near(demand.distanceMm, 96.52));
  assert(stop.phase() == EwoBrakePhase::Stopped);

  // A missing IR observation is visible and keeps only the nominal monotonic
  // final ramp. It is not silently replaced by Hall speed or PWM inference.
  EwoStationStopProfile unavailable = finalAtZero(700000, 72, 60, 5.0);
  demand = unavailable.demand(108, 700000, 72, 2000000, false, false,
                              0.0, 60);
  assert(demand.available && demand.irUnavailable && !demand.stopReached);
  assert(demand.pwmTarget == 0);
  assert(!std::strcmp(demand.reason, "IR_EVIDENCE_UNAVAILABLE_NOMINAL_RAMP"));

  // At exactly 35 pulses the continuous target is zero, but Station +3 and
  // a marker event are not needed. Positive IR speed keeps this in braking;
  // the next stationary snapshot completes the stop.
  EwoStationStopProfile endpoint = finalAtZero(900000, 93, 60, 25.0);
  const uint64_t endpointIr = 900000 + umPerPulse(EWO_FINAL_TARGET_PULSES);
  demand = endpoint.demand(108, endpointIr, 128, 2000000, true, true,
                           2.0 * EWO_PKPH_MM_PER_SEC, 30);
  assert(demand.finalRamp && near(demand.distanceMm, 35.0 * 9.652));
  assert(near(demand.targetPkph, 0.0) && demand.pwmTarget == 0);
  assert(!demand.stopReached);
  demand = endpoint.demand(108, endpointIr, 128, 3000000, true, true,
                           0.0, 30);
  assert(demand.stopReached && endpoint.phase() == EwoBrakePhase::Stopped);

  // A final ramp cannot be entered merely by being past Station 0; its
  // accepted Hall event is required.
  EwoStationStopProfile missingReference;
  missingReference.begin(108, 1, 109, 500000, 51, 60, 5.0, 400.0, 60, 200,
                         1000000);
  demand = missingReference.demand(109, 500000, 51, 1000000, true, true,
                                   5.0 * EWO_PKPH_MM_PER_SEC, 60);
  assert(!demand.available && demand.referenceRequired &&
         !std::strcmp(demand.reason, "STATION_ZERO_HALL_REQUIRED"));

  std::puts("PASS: adaptive approach/hold, Station 0 IR brake, pulse resolution, nominal fallback visibility, physical stop");
}
