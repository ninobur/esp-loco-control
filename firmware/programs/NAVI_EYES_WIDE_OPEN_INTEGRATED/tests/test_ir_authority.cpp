#include "../NaviIntegratedCore.h"
#include <cassert>
#include <cstdio>

using namespace navi_eyes;

static ir_movement::WireSnapshot ir(uint32_t seq, uint64_t pulses,
                                    uint64_t boot = 42,
                                    uint8_t reason = ir_movement::TRACKING) {
  ir_movement::WireSnapshot w;
  w.bootId = boot;
  w.sequence = seq;
  w.capturedUs = uint64_t(seq) * 100000;
  w.completedPulses = w.observedRises = pulses;
  w.pitchUm = 1000;
  w.nominalUm = pulses * w.pitchUm;
  w.opticalReason = reason;
  return w;
}
static HallSample hall(uint32_t serial, uint64_t t, int16_t raw, uint8_t dir = 1) {
  HallSample h;
  h.sampleSerial = serial;
  h.timestampUs = t;
  h.raw = raw;
  h.direction = dir;
  h.pwm = 40;
  return h;
}
static void field(NaviIntegratedCore& n, uint32_t first, uint64_t t,
                  uint8_t dir) {
  const int16_t raw = n.target().polarity == HallOpeningPolarity::AboveReference
                          ? 200 : 20;
  for (uint32_t i = 0; i < 5; ++i)
    n.observeHall(hall(first + i, t + 1000 * i, raw, dir));
}
static void boot(NaviIntegratedCore& n) {
  n.observeHall(hall(1, 110000, 110));
  n.observeIr(ir(1, 0), 120000, 40);
  n.observeIr(ir(2, 10), 220000, 40);
  assert(n.initialReferenceReady());
}
static void absentSupport(NaviIntegratedCore& n, uint64_t afterUs) {
  for (uint32_t i = 0; i < 5; ++i)
    n.observeHall(hall(10 + i, afterUs + 1000 + i * 1000, 110));
}

int main() {
  // Otto's MM13->12->11 failure: one sustained Hall field, unchanged IR,
  // repeated observations more than 650 ms apart. No timer can move MM.
  NaviIntegratedCore sustained;
  boot(sustained);
  sustained.declare(13, -1, 500000);
  for (uint32_t seq = 5; seq < 35; ++seq) {
    auto w = ir(seq, 10, 42, ir_movement::INADEQUATE_CONTRAST);
    w.openAborts = seq - 4;  // the deployed detector's stationary diagnostic
    sustained.observeIr(w, uint64_t(seq) * 100000 + 10000, 40);
    field(sustained, 100 + seq * 5, uint64_t(seq) * 100000 + 20000, 2);
  }
  assert(sustained.mm() == 13 && sustained.target().sequence == 12);
  assert(sustained.confirmedCount() == 0 && sustained.missedCount() == 0);
  assert(sustained.relationshipReliable() && sustained.positionReliable());
  assert(sustained.irApplicable(3410000));
  assert(sustained.irHealthFault() ==
         uint8_t(ngr_nav::IrHealthFault::InadequateContrast));

  NaviIntegratedCore reversal;
  boot(reversal);
  reversal.declare(0, 1, 500000);
  absentSupport(reversal, 500000);
  reversal.observeIr(ir(6, 340), 610000, 40);
  field(reversal, 200, 620000, 1);
  assert(reversal.mm() == 1 && reversal.confirmedCount() == 1);
  reversal.observeIr(ir(7, 540), 710000, 40);
  reversal.reverse(-1, 720000);
  assert(reversal.relationshipReliable() && reversal.target().sequence == 1);
  field(reversal, 300, 730000, 2);  // no reversed travel yet
  assert(reversal.confirmedCount() == 1);
  reversal.observeIr(ir(8, 740), 810000, 40);
  field(reversal, 400, 820000, 2);
  assert(reversal.mm() == 1 && reversal.confirmedCount() == 2);
  assert(reversal.relationshipReliable());

  NaviIntegratedCore doubleReverse;
  boot(doubleReverse);
  doubleReverse.declare(0, 1, 500000);
  absentSupport(doubleReverse, 500000);
  doubleReverse.observeIr(ir(6, 340), 610000, 40);
  field(doubleReverse, 350, 620000, 1);
  doubleReverse.observeIr(ir(7, 540), 710000, 40);
  doubleReverse.reverse(-1, 720000);
  doubleReverse.observeIr(ir(8, 640), 810000, 40);  // halfway back to MM1
  doubleReverse.reverse(1, 820000);
  assert(doubleReverse.relationshipReliable() &&
         doubleReverse.target().sequence == 2 && doubleReverse.mm() == 1);
  doubleReverse.observeIr(ir(9, 880), 910000, 40);
  field(doubleReverse, 380, 920000, 1);
  assert(doubleReverse.confirmedCount() == 2 && doubleReverse.mm() == 2);

  NaviIntegratedCore reverseAfterMiss;
  boot(reverseAfterMiss);
  reverseAfterMiss.declare(0, 1, 500000);
  reverseAfterMiss.observeIr(ir(6, 400), 610000, 40);
  reverseAfterMiss.observeIr(ir(7, 740), 710000, 40);
  assert(reverseAfterMiss.missedCount() == 2 && reverseAfterMiss.mm() == 2);
  reverseAfterMiss.reverse(-1, 720000);
  assert(reverseAfterMiss.relationshipReliable() &&
         reverseAfterMiss.target().sequence == 2);
  reverseAfterMiss.observeIr(ir(8, 800), 810000, 40);
  field(reverseAfterMiss, 450, 820000, 2);
  assert(reverseAfterMiss.confirmedCount() == 1 && reverseAfterMiss.mm() == 2);

  NaviIntegratedCore reset;
  boot(reset);
  reset.declare(0, 1, 500000);
  absentSupport(reset, 500000);
  reset.observeIr(ir(6, 340), 610000, 40);
  field(reset, 500, 620000, 1);
  assert(reset.mm() == 1);
  reset.observeIr(ir(1, 0, 43), 710000, 40);  // genuine counter/frame reset
  assert(!reset.relationshipReliable() && reset.positionReliable());
  assert(reset.mm() == 1 && reset.target().sequence == 2);
  reset.observeIr(ir(2, 340, 43), 810000, 40);
  field(reset, 600, 820000, 1);
  assert(reset.confirmedCount() == 1 && reset.missedCount() == 0 &&
         reset.mm() == 1 && reset.target().sequence == 2);
  reset.declare(1, 1, 900000);  // operator-owned recovery
  absentSupport(reset, 900000);
  assert(reset.relationshipReliable() && reset.target().sequence == 2);
  reset.observeIr(ir(3, 680, 43), 1010000, 40);
  field(reset, 700, 1020000, 1);
  assert(reset.confirmedCount() == 2 && reset.mm() == 2);

  NaviIntegratedCore speed;
  boot(speed);
  for (uint32_t seq = 3; seq <= 12; ++seq)
    speed.observeIr(ir(seq, 10 + 2 * (seq - 2)), uint64_t(seq) * 100000 + 20000, 40);
  assert(speed.irSpeedAvailable(1220000));
  assert(speed.irSpeedMmS() > 19.9 && speed.irSpeedMmS() < 20.1);
  for (uint32_t seq = 13; seq <= 24; ++seq)
    speed.observeIr(ir(seq, 30, 42, ir_movement::INADEQUATE_CONTRAST),
                    uint64_t(seq) * 100000 + 20000, 0);
  assert(speed.irSpeedAvailable(2420000) && speed.irSpeedMmS() == 0);

  std::puts("PASS: stationary field, single/double reversal including missed MM, frame reset/redeclaration, 1s IR speed");
}
