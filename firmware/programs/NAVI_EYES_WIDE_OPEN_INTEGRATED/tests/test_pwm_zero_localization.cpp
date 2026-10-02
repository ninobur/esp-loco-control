#include "../NaviIntegratedCore.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace navi_eyes;

struct Run {
  NaviIntegratedCore n;
  uint64_t now = 100000, counter = 1000000;
  uint32_t seq = 0, serial = 0;
  uint8_t dir;
  uint8_t mac[6] = {1, 2, 3, 4, 5, 6};
  explicit Run(int8_t direction = -1) : dir(direction > 0 ? 1 : 2) {
    hall(1000); ir(counter);
    n.declare(45, direction, now += 1000);
    window(1000); ir(counter + 106000); window(support());
    assert(n.confirmedCount() == 1 && n.relationshipReliable());
  }
  ir_movement::WireSnapshot packet(uint64_t um) {
    ir_movement::WireSnapshot w;
    w.bootId = 42; w.sequence = ++seq; w.capturedUs = uint64_t(seq) * 100000;
    w.pitchUm = 1; w.completedPulses = w.observedRises = w.nominalUm = um;
    w.opticalReason = ir_movement::TRACKING;
    return w;
  }
  void ir(uint64_t um, uint8_t pwm = 40) {
    counter = um; n.observeIr(packet(um), now += 10000, pwm, mac);
  }
  void hall(int16_t raw, uint8_t pwm = 40) {
    HallSample s;
    s.sampleSerial = ++serial; s.timestampUs = now += 1000;
    s.raw = raw; s.pwm = pwm; s.direction = dir;
    n.observeHall(s);
  }
  void window(int16_t raw, uint8_t pwm = 40) {
    for (unsigned i = 0; i < 5; ++i) hall(raw, pwm);
  }
  int16_t support() const {
    return n.target().polarity == HallOpeningPolarity::AboveReference ? 1100 : 900;
  }
  bool state(const char* s) const { return !std::strcmp(n.irDistanceState(now), s); }
};

static void holdUntilDeclaration() {
  for (int8_t direction : {-1, 1}) {
    Run r(direction);
    const auto mm = r.n.mm(); const auto target = r.n.target();
    const auto reference = r.n.activeReference(); const auto epoch = r.n.irMeasurementEpochId();
    for (unsigned i = 0; i < 28; ++i) {
      r.ir(r.counter + 1000000, 0); r.window(2000, 0);
      assert(r.n.mm() == mm && r.n.target().sequence == target.sequence);
      assert(r.n.target().polarity == target.polarity && r.n.target().distanceMm == target.distanceMm);
      assert(r.n.direction() == direction && r.n.declared() && r.n.positionReliable());
      assert(r.n.activeReference() == reference && r.n.irMeasurementEpochId() == epoch);
      assert(r.n.irApplicable(r.now) && r.n.pwmZeroMovementRequiresDeclaration());
      assert(r.state("PWM_ZERO_MOVEMENT_REDECLARE") && r.n.distanceHolding());
      assert(!r.n.relationshipReliable() && r.n.spatialPhase() == 0);
    }
    assert(r.n.pwmZeroDisplacements() == 28);
    // Later matching onsets/positive IR reports must never recover or substitute.
    for (unsigned i = 0; i < 100; ++i) {
      r.ir(r.counter + 1000000); r.window(1000); r.window(r.support());
      assert(r.n.confirmedCount() == 1 && r.n.missedCount() == 0);
      assert(r.n.mm() == mm && r.n.target().sequence == target.sequence);
      assert(r.n.activeReference() == reference && r.state("PWM_ZERO_MOVEMENT_REDECLARE"));
    }
    r.now += NaviIntegratedCore::kIrFreshUs + 1; r.window(r.support());
    assert(r.state("PWM_ZERO_MOVEMENT_REDECLARE") && !r.n.irApplicable(r.now));
    r.n.declare(255, direction, r.now); // invalid declaration cannot clear the latch
    assert(r.n.pwmZeroMovementRequiresDeclaration());
    r.ir(r.counter);
    r.n.declare(12, 1, r.now += 1000); r.dir = 1;
    assert(!r.n.pwmZeroMovementRequiresDeclaration() && r.n.mm() == 12);
    assert(r.n.target().sequence == 13 && r.n.direction() == 1);
    r.window(1000); r.ir(r.counter + 500000); r.window(r.support());
    assert(r.n.confirmedCount() == 2 && r.n.mm() == 13); // FT2: no upper bound
    const uint64_t origin = r.n.openingIrUm();
    const uint64_t interval = uint64_t(r.n.target().distanceMm) * 1000;
    r.ir(origin + 100000); r.window(1000); r.ir(origin + 200000);
    r.ir(origin + interval - interval * 15 / 100 - 1); r.window(r.support());
    assert(r.n.confirmedCount() == 2);
    r.ir(origin + interval); r.window(r.support());
    assert(r.n.confirmedCount() == 3);
    const uint64_t next = uint64_t(r.n.target().distanceMm) * 1000;
    r.ir(r.n.openingIrUm() + next + next * 15 / 100 + 1);
    assert(r.n.missedCount() == 1); // normal miss authority restored after declaration
  }
}

static void dwellUnchanged() {
  Run r;
  const auto origin = r.n.openingIrUm(); const auto mm = r.n.mm();
  const auto target = r.n.target().sequence; const auto phase = r.n.spatialPhase();
  const auto reference = r.n.activeReference();
  for (unsigned i = 0; i < 20; ++i) { r.ir(r.counter, 0); r.window(2000, 0); }
  assert(r.state("HALL_IR_READY") && r.n.relationshipReliable());
  assert(r.n.openingIrUm() == origin && r.n.mm() == mm && r.n.spatialPhase() == phase);
  assert(r.n.target().sequence == target && r.n.direction() == -1);
  assert(r.n.activeReference() == reference && r.n.pwmZeroDisplacements() == 0);
  assert(!r.n.pwmZeroMovementRequiresDeclaration());
}

static void failuresAndReverse() {
  for (unsigned fault = 0; fault < 5; ++fault) {
    Run r;
    auto w = r.packet(r.counter);
    switch (fault) {
      case 0: ++w.bootId; break;
      case 1: ++r.mac[5]; break;
      case 2: ++w.nominalUm; break;
      case 3: --w.sequence; break;
      case 4: ++w.pitchUm; w.nominalUm = w.completedPulses * w.pitchUm; break;
    }
    r.n.observeIr(w, r.now += 10000, 0, r.mac);
    assert(r.state("FRAME_LOST_REDECLARE") && !r.n.pwmZeroMovementRequiresDeclaration());
    r.ir(r.counter + 10000, 0); // independent fault must not be relabeled as handling alone
    assert(r.state("FRAME_LOST_REDECLARE"));
    r.ir(r.counter + 10000); r.window(1000); r.window(r.support());
    assert(r.n.confirmedCount() == 1 && r.n.missedCount() == 0);
  }
  Run both; both.ir(both.counter + 10000, 0);
  auto reset = both.packet(both.counter); ++reset.bootId;
  both.n.observeIr(reset, both.now += 10000, 40, both.mac);
  assert(both.state("FRAME_LOST_REDECLARE") && both.n.pwmZeroMovementRequiresDeclaration());
  Run r; r.ir(r.counter + 10000, 0);
  r.n.reverse(1, r.now += 1000); r.dir = 1;
  assert(r.n.pwmZeroMovementRequiresDeclaration()); // reversal is not declaration
  r.ir(r.counter + 500000); r.window(1000); r.window(r.support());
  assert(r.n.confirmedCount() == 1 && r.n.missedCount() == 0);
}

int main() {
  holdUntilDeclaration(); dwellUnchanged(); failuresAndReverse();
  std::puts("PASS: PWM-zero movement holds until declaration; no later Hall/IR recovery or miss; dwell, context, frame faults and FT2 preserved");
}
