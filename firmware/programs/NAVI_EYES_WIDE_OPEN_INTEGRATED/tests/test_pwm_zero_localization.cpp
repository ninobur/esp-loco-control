#include "../NaviIntegratedCore.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace navi_eyes;
static constexpr const char* kUnknown = "INTERVAL_KNOWN_IR_POSITION_UNKNOWN";

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
    counter = um;
    n.observeIr(packet(um), now += 10000, pwm, mac);
  }
  void hall(int16_t raw, uint8_t pwm = 40, uint64_t acquired = 0) {
    HallSample s;
    s.sampleSerial = ++serial; s.timestampUs = acquired ? acquired : now + 1000;
    s.raw = raw; s.pwm = pwm; s.direction = dir;
    n.observeHall(s, now += 1000);
  }
  void window(int16_t raw, uint8_t pwm = 40, uint64_t acquired = 0) {
    for (unsigned i = 0; i < 5; ++i) hall(raw, pwm, acquired);
  }
  int16_t support() const {
    return n.target().polarity == HallOpeningPolarity::AboveReference ? 1100 : 900;
  }
  bool state(const char* value) const { return !std::strcmp(n.irDistanceState(now), value); }
  void handling() {
    const auto mm = n.mm(); const auto target = n.target(); const auto direction = n.direction();
    const auto reference = n.activeReference(); const auto epoch = n.irMeasurementEpochId();
    for (unsigned i = 0; i < 28; ++i) {
      ir(counter + 1000000, 0); window(support(), 0);
      assert(n.mm() == mm && n.target().sequence == target.sequence);
      assert(n.target().polarity == target.polarity && n.target().distanceMm == target.distanceMm);
      assert(n.direction() == direction && n.declared() && n.positionReliable());
      assert(n.activeReference() == reference && n.initialReferenceReady());
      assert(n.irApplicable(now) && n.irMeasurementEpochId() == epoch);
      assert(n.confirmedCount() == 1 && n.missedCount() == 0);
      assert(state(kUnknown) && !n.relationshipReliable() && n.distanceHolding());
    }
    assert(n.pwmZeroDisplacements() == 28 && n.spatialPhase() == 0);
  }
  void recover() {
    ir(counter + 500000); // first powered report: discard its mixed delta
    window(1000);
    window(support()); // Hall alone/no post-origin movement cannot confirm
    assert(n.confirmedCount() == 1 && state(kUnknown));
    window(1000);
    ir(counter + 10000);
    window(support());
    assert(n.confirmedCount() == 2 && n.relationshipReliable() && !n.distanceHolding());
    assert(state("HALL_IR_READY") && n.openingIrUm() == counter);
  }
};

static void retainsAndRecovers() {
  for (int8_t dir : {-1, 1}) {
    Run r(dir); r.handling();
    const auto target = r.n.target().sequence;
    r.recover(); assert(r.n.mm() == target);
    const uint64_t origin = r.n.openingIrUm();
    const uint64_t interval = uint64_t(r.n.target().distanceMm) * 1000;
    r.ir(origin + 100000); r.window(1000);
    r.ir(origin + 200000); // unchanged spatial cycle
    r.ir(origin + interval - interval * 15 / 100 - 1); r.window(r.support());
    assert(r.n.confirmedCount() == 2); // normal lower bound restored
    r.ir(origin + interval); r.window(r.support());
    assert(r.n.confirmedCount() == 3 && r.n.missedCount() == 0);
    const uint64_t next = uint64_t(r.n.target().distanceMm) * 1000;
    r.ir(r.n.openingIrUm() + next + next * 15 / 100);
    assert(r.n.missedCount() == 0);
    r.ir(r.counter + 1); assert(r.n.missedCount() == 1); // normal miss authority
  }
}

static void noUnlocalizedMissOrHallOnly() {
  Run r; r.handling();
  const auto mm = r.n.mm(); const auto target = r.n.target().sequence;
  r.window(1000); r.window(r.support()); // not even a powered IR origin yet
  assert(r.n.confirmedCount() == 1);
  r.ir(r.counter); r.window(1000);
  for (unsigned i = 0; i < 5; ++i) {
    r.ir(r.counter + 10000000); r.window(1000);
    assert(r.n.missedCount() == 0 && r.n.mm() == mm && r.n.target().sequence == target);
  }
  r.now += NaviIntegratedCore::kIrFreshUs + 1;
  r.window(r.support()); assert(r.n.confirmedCount() == 1); // stale IR is not Hall authority
  assert(r.state(kUnknown) && !r.n.irApplicable(r.now));
  r.ir(r.counter); r.window(1000); r.window(r.support());
  assert(r.n.confirmedCount() == 2 && r.n.mm() == target); // no upper bound
}

static void onsetAndContext() {
  Run r; r.handling();
  const uint64_t beforeResume = r.now;
  r.ir(r.counter);
  r.window(1000, 40, beforeResume); // old queued Hall absence cannot qualify
  r.window(1000, 40, r.now - 5000); // timestamp at the resume report is also excluded
  r.window(r.support());
  r.ir(r.counter + 10000); r.window(r.support());
  assert(r.n.confirmedCount() == 1); // parked target-polarity field
  r.window(1000);
  r.window(r.support() == 1100 ? 900 : 1100);
  assert(r.n.confirmedCount() == 1); // wrong polarity
  const auto dir = r.dir; r.dir = dir == 1 ? 2 : 1;
  r.window(r.support()); assert(r.n.confirmedCount() == 1); // wrong direction
  r.dir = dir; r.window(1000); r.window(r.support());
  assert(r.n.confirmedCount() == 2);

  Run stoppedAgain; stoppedAgain.handling(); stoppedAgain.ir(stoppedAgain.counter);
  stoppedAgain.window(1000);
  stoppedAgain.ir(stoppedAgain.counter, 0); // no displacement, but ends pending powered origin
  stoppedAgain.ir(stoppedAgain.counter + 1000000);
  stoppedAgain.window(stoppedAgain.support());
  assert(stoppedAgain.n.confirmedCount() == 1);
  stoppedAgain.window(1000); stoppedAgain.ir(stoppedAgain.counter + 10000);
  stoppedAgain.window(stoppedAgain.support()); assert(stoppedAgain.n.confirmedCount() == 2);
}

static void dwellAndRedeclare() {
  Run r;
  const auto origin = r.n.openingIrUm(); const auto mm = r.n.mm();
  const auto phase = r.n.spatialPhase(); const auto reference = r.n.activeReference();
  for (unsigned i = 0; i < 20; ++i) { r.ir(r.counter, 0); r.window(2000, 0); }
  assert(r.state("HALL_IR_READY") && r.n.relationshipReliable());
  assert(r.n.openingIrUm() == origin && r.n.mm() == mm && r.n.spatialPhase() == phase);
  assert(r.n.activeReference() == reference && r.n.pwmZeroDisplacements() == 0);
  r.handling(); r.n.declare(12, 1, r.now += 1000); r.dir = 1;
  assert(r.n.mm() == 12 && r.n.target().sequence == 13 && r.n.direction() == 1);
  r.window(1000); r.ir(r.counter + 500000); r.window(r.support());
  assert(r.n.confirmedCount() == 2 && r.n.mm() == 13); // FT2 unchanged
}

static void independentFrameFailures() {
  for (unsigned fault = 0; fault < 5; ++fault) {
    Run r; r.handling();
    auto w = r.packet(r.counter);
    switch (fault) {
      case 0: ++w.bootId; break;
      case 1: ++r.mac[5]; break;
      case 2: ++w.nominalUm; break; // invalid scale
      case 3: --w.sequence; break; // duplicate order
      case 4: ++w.pitchUm; w.nominalUm = w.completedPulses * w.pitchUm; break;
    }
    r.n.observeIr(w, r.now += 10000, 0, r.mac);
    assert(r.state("FRAME_LOST_REDECLARE"));
    r.ir(r.counter + 10000, 0); // never downgrade frame failure to recoverable localization
    assert(r.state("FRAME_LOST_REDECLARE"));
    r.ir(r.counter + 10000); r.window(1000);
    r.ir(r.counter + 10000); r.window(r.support());
    assert(r.n.confirmedCount() == 1 && r.n.missedCount() == 0);
    assert(r.state("FRAME_LOST_REDECLARE"));
  }
  Run reversed; reversed.handling();
  reversed.n.reverse(1, reversed.now += 1000); reversed.dir = 1;
  assert(reversed.state("FRAME_LOST_REDECLARE")); // unchanged reversal limitation, documented
  reversed.ir(reversed.counter + 10000); reversed.window(1000);
  reversed.ir(reversed.counter + 10000); reversed.window(reversed.support());
  assert(reversed.n.confirmedCount() == 1);
}

int main() {
  retainsAndRecovers(); noUnlocalizedMissOrHallOnly(); onsetAndContext();
  dwellAndRedeclare(); independentFrameFailures();
  std::puts("PASS: PWM-zero retains interval/context/reference; powered Hall+IR recovery; no Hall-only/miss; dwell, redeclaration, genuine failures distinct");
}
