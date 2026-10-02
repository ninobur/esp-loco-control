#include "../NaviIntegratedCore.h"

#include <cstdio>

using namespace navi_eyes;

static int failures = 0;
static void check(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::printf("FAIL: %s\n", message);
  }
}
static ir_movement::WireSnapshot ir(uint32_t sequence, uint64_t pulses,
                                    uint8_t reason = ir_movement::TRACKING) {
  ir_movement::WireSnapshot w;
  w.bootId = 42;
  w.sequence = sequence * 10;
  w.capturedUs = uint64_t(sequence) * 100000;
  w.completedPulses = pulses;
  w.observedRises = pulses;
  w.pitchUm = 1000;
  w.nominalUm = pulses * 1000;
  w.opticalReason = reason;
  return w;
}
static HallSample hall(uint32_t serial, uint64_t tUs, int16_t raw,
                       uint8_t pwm = 40, uint8_t direction = 1) {
  HallSample h;
  h.sampleSerial = serial;
  h.timestampUs = tUs;
  h.raw = raw;
  h.pwm = pwm;
  h.direction = direction;
  return h;
}
static void provisionalReference(NaviIntegratedCore& n) {
  // Boot reference is exactly one native Hall ADC, with no IR prerequisite.
  n.observeHall(hall(1, 110000, 110, 40));
  n.observeIr(ir(1, 0), 120000, 40);
  n.observeIr(ir(2, 10), 220000, 40);
}
static void absentSupport(NaviIntegratedCore& n, uint64_t afterUs = 500000) {
  // A declaration no longer implies that target support was observed absent.
  for (uint32_t i = 0; i < 5; ++i)
    n.observeHall(hall(2 + i, afterUs + 1000 + i * 1000, 110));
}

int main() {
  NaviIntegratedCore n;
  check(!n.initialReferenceReady(), "boot has no Hall reference");
  provisionalReference(n);
  check(n.initialReferenceReady() && n.activeReference() == 110,
        "one native Hall ADC establishes the provisional boot reference immediately");
  check(n.hallObservationCount() == 1 && n.lastHall().raw == 110 &&
            n.lastHallSerial() == 1,
        "all native Hall readings remain unchanged and in order");
  n.declare(0, 1, 500000);
  absentSupport(n);
  check(n.target().sequence == 1 && n.target().distanceMm == 330,
        "declared route context chooses the known CW target");
  n.observeIr(ir(5, 340), 600000, 40);
  const int16_t raw[] = {180, 20, 190, 30, 200};
  for (uint32_t i = 0; i < 5; ++i)
    n.observeHall(hall(10 + i, 610000 + i * 1000, raw[i]));
  bool confirmedEventWithMedian = false;
  EwoEvent event;
  while (n.takeEvent(event))
    if (event.kind == EwoEventKind::TargetConfirmed && event.median == 180)
      confirmedEventWithMedian = true;
  check(confirmedEventWithMedian && n.confirmedCount() == 1 && n.mm() == 1,
        "inside NAVI, median five plus mapped polarity and IR window confirms target");
  check(n.openingSerial() == 10 && n.openingIrUm() == 340000 &&
            n.spatialPhase() == 1,
        "first threshold reading in qualifying population anchors clearance");
  NaviIntegratedCore boundary;
  provisionalReference(boundary);
  boundary.declare(0, 1, 500000);
  absentSupport(boundary);
  boundary.observeIr(ir(5, 340), 600000, 40);
  boundary.observeHall(hall(500, 610000, 190));
  boundary.observeHall(hall(501, 611000, 190));
  boundary.observeIr(ir(6, 342), 612000, 40);
  for (uint32_t i = 0; i < 3; ++i)
    boundary.observeHall(hall(502 + i, 613000 + i * 1000, 190));
  check(boundary.confirmedCount() == 1 && boundary.openingSerial() == 500 &&
            boundary.openingIrUm() == 340000,
        "IR update inside median window does not move the observed leading boundary");
  boundary.observeIr(ir(7, 732), 700000, 40);
  check(boundary.missedCount() == 1,
        "next target distance uses the leading boundary, not the later median point");
  n.observeIr(ir(6, 439), 700000, 40);
  n.observeHall(hall(15, 710000, 900));
  check(n.activeReference() == 110 && n.spatialPhase() == 1,
        "clearance does not collect or replace reference");
  n.observeIr(ir(7, 440), 800000, 40);
  n.observeHall(hall(16, 810000, 120));
  n.observeHall(hall(17, 811000, 900));
  n.observeIr(ir(8, 490), 900000, 40);
  n.observeHall(hall(18, 910000, 130));
  n.observeIr(ir(9, 539), 1000000, 40);
  n.observeHall(hall(19, 1010000, 140));
  n.observeIr(ir(10, 540), 1100000, 40);
  check(n.activeReference() == 130 && n.spatialPhase() == 0,
        "per-location 100-200 mm median replaces reference at 200 mm");
  for (uint32_t i = 0; i < 5; ++i)
    n.observeHall(hall(40 + i, 2200000 + i * 1000, 0));
  check(n.confirmedCount() == 1 && n.mm() == 1,
        "without fresh IR, Hall cannot confirm another target");
  for (uint32_t i = 0; i < 5; ++i)
    n.observeHall(hall(50 + i, 3000000 + i * 1000, 0));
  check(n.confirmedCount() == 1 && n.mm() == 1 && n.missedCount() == 0,
        "elapsed time cannot manufacture Hall confirmation or IR misses");

  NaviIntegratedCore stationary;
  provisionalReference(stationary);
  stationary.declare(0, 1, 500000);
  const uint64_t priorIrCount = stationary.irObservationCount();
  stationary.observeIr(ir(5, 10), 600000, 0);
  stationary.observeHall(hall(20, 610000, 200, 0));
  for (uint32_t i = 0; i < 5; ++i)
    stationary.observeHall(hall(21 + i, 611000 + i * 1000, 200, 0));
  check(stationary.irObservationCount() == priorIrCount + 1 &&
            stationary.hallObservationCount() == 7 &&
            stationary.relationshipReliable(),
        "stationary PWM-zero observations remain visible without invalidating IR relationship");
  const uint64_t epochAtStop = stationary.irMeasurementEpochId();
  stationary.observeIr(ir(6, 10, ir_movement::INADEQUATE_CONTRAST), 650000, 0);
  check(stationary.irApplicable(650000) && stationary.irMeasurementEpochActive() &&
            stationary.irMeasurementEpochId() == epochAtStop &&
            stationary.relationshipReliable() && !stationary.distanceHolding() &&
            stationary.irHealthFault() == static_cast<uint8_t>(ngr_nav::IrHealthFault::InadequateContrast) &&
            stationary.latestIr().opticalReason == ir_movement::INADEQUATE_CONTRAST,
        "PWM-zero inadequate contrast with no measured change preserves epoch, IR/MM relation and raw diagnostic");
  NaviIntegratedCore longDwell;
  provisionalReference(longDwell);
  longDwell.declare(0, 1, 500000);
  const uint64_t dwellEpoch = longDwell.irMeasurementEpochId();
  longDwell.observeIr(ir(5, 10, ir_movement::INADEQUATE_CONTRAST), 600000, 0);
  longDwell.observeIr(ir(6, 10, ir_movement::INADEQUATE_CONTRAST), 700000, 0);
  longDwell.observeIr(ir(7, 11), 800000, 40);
  check(longDwell.irMeasurementEpochId() == dwellEpoch &&
            longDwell.relationshipReliable() && longDwell.irApplicable(800000) &&
            !longDwell.distanceHolding(),
        "repeated stationary contrast diagnostics do not break continuity on restart");
  stationary.observeIr(ir(7, 11), 700000, 0);
  check(!stationary.relationshipReliable() &&
            stationary.pwmZeroDisplacements() == 1 &&
            stationary.positionReliable() && stationary.mm() == 0,
        "unexpected PWM-zero IR travel loses within-interval coordinate but retains known MM");
  stationary.observeIr(ir(8, 340), 800000, 40);
  const int16_t sustained[] = {190, 191, 192, 193, 194};
  for (uint32_t i = 0; i < 5; ++i)
    stationary.observeHall(hall(30 + i, 1200000 + i * 1000, sustained[i]));
  check(stationary.confirmedCount() == 0 && !stationary.relationshipReliable() &&
            stationary.positionReliable() && stationary.mm() == 0,
        "Hall without post-resume IR progression and observed absence cannot re-anchor");

  NaviIntegratedCore shrug;
  provisionalReference(shrug);
  shrug.declare(0, 1, 500000);
  shrug.observeIr(ir(5, 340), 600000, 40);
  for (uint32_t i = 0; i < 5; ++i)
    shrug.observeHall(hall(30 + i, 610000 + i * 1000, 0));
  check(shrug.confirmedCount() == 0 && shrug.mm() == 0,
        "opposite Hall median shrugs without alternate-position inference");
  shrug.observeIr(ir(6, 400), 700000, 40);
  check(shrug.missedCount() == 0 && shrug.target().sequence == 1 &&
            shrug.mm() == 0,
        "first target holds despite IR passage and opposite Hall support");

  NaviIntegratedCore consecutiveMisses;
  provisionalReference(consecutiveMisses);
  // Establish MM0 at an actual Hall landmark before testing normal misses.
  consecutiveMisses.declare(navi_one::nextMarker(0, -1), 1, 250000);
  absentSupport(consecutiveMisses, 250000);
  consecutiveMisses.observeIr(ir(3, 11), 300000, 40);
  const int16_t firstRaw = consecutiveMisses.target().polarity ==
      HallOpeningPolarity::AboveReference ? 200 : 20;
  for (uint32_t i = 0; i < 5; ++i)
    consecutiveMisses.observeHall(hall(10 + i, 310000 + i * 1000, firstRaw));
  check(consecutiveMisses.mm() == 0 && consecutiveMisses.confirmedCount() == 1,
        "normal miss fixture starts from confirmed MM0");
  consecutiveMisses.observeIr(ir(4, 211), 400000, 40);
  // Target 1 (330 mm) and target 2 (340 mm) are missed from the same
  // physical origin. Target 3 is then accepted at the cumulative 990 mm
  // position, with tolerance based only on target 3's 330-mm interval.
  consecutiveMisses.observeIr(ir(5, 400), 600000, 40);
  consecutiveMisses.observeIr(ir(6, 740), 700000, 40);
  check(consecutiveMisses.missedCount() == 2 && consecutiveMisses.mm() == 2 &&
            consecutiveMisses.target().sequence == 3,
        "consecutive Missed Magnets advance mapped target without moving physical origin");
  consecutiveMisses.observeIr(ir(7, 1001), 800000, 40);
  for (uint32_t i = 0; i < 5; ++i)
    consecutiveMisses.observeHall(hall(80 + i, 810000 + i * 1000, 20));
  check(consecutiveMisses.confirmedCount() == 2 && consecutiveMisses.mm() == 3,
        "cumulative expected distance uses local tolerance for the current interval");

  NaviIntegratedCore noIr;
  provisionalReference(noIr);
  noIr.declare(0, 1, 500000);
  noIr.observeHall(hall(50, 2000000, 190));
  check(noIr.missedCount() == 0,
        "absence of IR cannot manufacture missed-magnet progression");
  noIr.reverse(-1, 2100000);
  check(noIr.target().sequence == 0 && noIr.direction() == -1,
        "reversal seeks last passed MM in new route direction");

  NaviIntegratedCore stale;
  provisionalReference(stale);
  stale.declare(0, 1, 500000);
  absentSupport(stale);
  stale.observeIr(ir(5, 340), 600000, 40);
  for (uint32_t i = 0; i < 5; ++i)
    stale.observeHall(hall(60 + i, 610000 + i * 1000, 190));
  check(stale.spatialPhase() == 1, "confirmed target starts clearance before IR outage");
  for (uint32_t i = 0; i < 5; ++i)
    stale.observeHall(hall(70 + i, 2200000 + i * 1000, 0));
  check(stale.confirmedCount() == 1 && stale.mm() == 1,
        "stale IR holds target even after prior spatial collection");

  NaviIntegratedCore emptySpatial;
  provisionalReference(emptySpatial);
  emptySpatial.declare(0, 1, 500000);
  absentSupport(emptySpatial);
  emptySpatial.observeIr(ir(5, 340), 600000, 40);
  for (uint32_t i = 0; i < 5; ++i)
    emptySpatial.observeHall(hall(75 + i, 610000 + i * 1000, 190));
  emptySpatial.observeIr(ir(6, 540), 700000, 40);
  bool emptyReported = false;
  while (emptySpatial.takeEvent(event))
    if (event.kind == EwoEventKind::SpatialEmpty) emptyReported = true;
  check(emptyReported && emptySpatial.spatialPhase() == 0 &&
            emptySpatial.activeReference() == 110,
        "empty 100-200 mm interval is reported without inventing a median");

  NaviIntegratedCore ccw;
  provisionalReference(ccw);
  ccw.declare(1, -1, 500000);
  absentSupport(ccw);
  check(ccw.target().sequence == 0 && ccw.target().direction == 2,
        "CCW declaration selects correct target context");
  ccw.observeIr(ir(5, 340), 600000, 40);
  for (uint32_t i = 0; i < 5; ++i)
    ccw.observeHall(hall(80 + i, 610000 + i * 1000, 190, 40, 1));
  check(ccw.confirmedCount() == 0,
        "wrong route direction cannot confirm even coherent Hall and IR evidence");
  for (uint32_t i = 0; i < 5; ++i)
    ccw.observeHall(hall(90 + i, 620000 + i * 1000, 190, 40, 2));
  check(ccw.confirmedCount() == 1 && ccw.mm() == 0,
        "correct CCW direction confirms the known target");

  NaviIntegratedCore health;
  provisionalReference(health);
  health.declare(0, 1, 500000);
  health.observeIr(ir(5, 10, ir_movement::SIGNAL_STALE), 600000, 40);
  check(health.irApplicable(700000),
        "fresh healthy no-change IR remains an applicable measurement of zero travel");
  check(!health.irApplicable(1600001), "one-second IR freshness is NAVI-owned");
  health.observeIr(ir(6, 10, ir_movement::INADEQUATE_CONTRAST), 700000, 40);
  check(health.irApplicable(700000),
        "inadequate optical contrast alone does not invalidate measured no-change");
  health.observeIr(ir(7, 11, ir_movement::INADEQUATE_CONTRAST), 800000, 40);
  check(health.irApplicable(800000) &&
            health.irHealthFault() == static_cast<uint8_t>(ngr_nav::IrHealthFault::InadequateContrast),
        "diagnostic remains visible without vetoing coherent cumulative pulses");

  NaviIntegratedCore brokenAtStop;
  provisionalReference(brokenAtStop);
  brokenAtStop.declare(0, 1, 500000);
  auto gapAtStop = ir(5, 10);
  ++gapAtStop.sampleGaps;
  brokenAtStop.observeIr(gapAtStop, 600000, 0);
  check(brokenAtStop.relationshipReliable() &&
            brokenAtStop.latestIr().sampleGaps == 1,
        "diagnostic sample-gap count is visible without erasing cumulative distance");

  NaviIntegratedCore latest;
  provisionalReference(latest);
  latest.declare(0, 1, 500000);
  absentSupport(latest);
  const uint8_t sourceMac[6] = {2, 3, 4, 5, 6, 7};
  latest.observeIr(ir(5, 200), 600000, 40, sourceMac);
  latest.observeIr(ir(6, 340), 700000, 40, sourceMac);
  check(latest.latestIrMac()[0] == 2 && latest.latestIrMac()[5] == 7,
        "source MAC provenance accompanies IR reports into NAVI");
  for (uint32_t i = 0; i < 5; ++i)
    latest.observeHall(hall(100 + i, 710000 + i * 1000, 190));
  check(latest.confirmedCount() == 1,
        "Hall uses the latest received IR fact, not a stale earlier packet");

  NaviIntegratedCore losses;
  provisionalReference(losses);
  losses.declare(0, 1, 500000);
  losses.noteObservationLoss(2, 3);
  check(losses.hallLoss() == 2 && losses.irLoss() == 3 &&
            losses.declared() && losses.mm() == 0 && losses.relationshipReliable(),
        "packet loss is visible and cumulative counter can bridge it");

  NaviIntegratedCore redeclared;
  provisionalReference(redeclared);
  redeclared.declare(0, 1, 500000);
  redeclared.observeIr(ir(5, 340), 600000, 40);
  for (uint32_t i = 0; i < 4; ++i)
    redeclared.observeHall(hall(110 + i, 610000 + i * 1000, 190));
  redeclared.declare(50, -1, 700000);
  redeclared.observeHall(hall(120, 710000, 0, 40, 2));
  check(redeclared.mm() == 50 && redeclared.target().sequence == 49 &&
            redeclared.confirmedCount() == 0,
        "redeclaration clears incompatible Hall window and installs new context");

  std::printf("%s: %d failures\n", failures ? "FAIL" : "PASS", failures);
  return failures ? 1 : 0;
}
