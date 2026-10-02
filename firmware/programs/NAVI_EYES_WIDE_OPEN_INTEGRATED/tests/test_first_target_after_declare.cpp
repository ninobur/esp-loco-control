#include "../NaviIntegratedCore.h"
#include <cassert>
#include <cstdio>

using namespace navi_eyes;

struct Run {
  NaviIntegratedCore n;
  uint64_t now = 100000, declaredAt = 0;
  uint32_t sequence = 0, serial = 0;
  static constexpr uint64_t origin = 1000000;
  uint8_t dir = 2;

  Run(uint8_t mm = 45, int8_t direction = -1) {
    dir = direction > 0 ? 1 : 2;
    sample(1000); // unchanged provisional boot reference
    irAt(origin);
    declare(mm, direction);
  }
  void declare(uint8_t mm, int8_t direction) {
    declaredAt = now += 1000;
    n.declare(mm, direction, now);
  }
  void irAt(uint64_t um) {
    ir_movement::WireSnapshot w;
    w.bootId = 42; w.sequence = ++sequence;
    w.capturedUs = uint64_t(sequence) * 100000;
    // Micrometre fixture scale exercises the exact inclusive/exclusive bounds.
    w.pitchUm = 1; w.completedPulses = w.observedRises = w.nominalUm = um;
    w.opticalReason = ir_movement::TRACKING;
    n.observeIr(w, now += 10000, 40);
  }
  void sample(int16_t raw, uint8_t pwm = 40, uint64_t acquiredAt = 0) {
    HallSample s;
    s.sampleSerial = ++serial; s.timestampUs = acquiredAt ? acquiredAt : now + 1000;
    s.raw = raw; s.pwm = pwm; s.direction = dir;
    n.observeHall(s, now += 1000);
  }
  void window(int16_t raw, uint8_t pwm = 40) {
    for (unsigned i = 0; i < 5; ++i) sample(raw, pwm);
  }
  int16_t support() const {
    return n.target().polarity == HallOpeningPolarity::AboveReference ? 1100 : 900;
  }
  bool event(EwoEventKind kind, uint8_t mm) {
    EwoEvent e; bool found = false;
    while (n.takeEvent(e)) if (e.kind == kind && e.mm == mm) found = true;
    return found;
  }
};

static void fieldCase() {
  // Deterministic reconstruction of David's Otto CCW log case, not an NSR replay.
  Run r;
  assert(r.n.target().sequence == 44 && r.n.target().distanceMm == 300);
  assert(r.n.target().polarity == HallOpeningPolarity::AboveReference);
  r.window(1000);
  r.irAt(Run::origin + 106000);
  const uint32_t leadingSerial = r.serial + 1;
  r.window(r.support());
  assert(r.n.mm() == 44 && r.n.confirmedCount() == 1 && r.n.missedCount() == 0);
  assert(r.n.openingSerial() == leadingSerial &&
         r.n.openingIrUm() == Run::origin + 106000);
  assert(r.event(EwoEventKind::TargetConfirmed, 44));

  const uint64_t secondOrigin = r.n.openingIrUm();
  const uint64_t interval = uint64_t(r.n.target().distanceMm) * 1000;
  assert(r.n.target().sequence == 43);
  r.irAt(secondOrigin + 100000); r.window(1000);
  r.irAt(secondOrigin + 200000); // finish the unchanged spatial cycle
  r.irAt(secondOrigin + interval - interval * 15 / 100 - 1);
  r.window(r.support());
  assert(r.n.confirmedCount() == 1); // first-target exception was cleared
  r.irAt(secondOrigin + interval);
  r.window(r.support());
  assert(r.n.confirmedCount() == 2 && r.n.mm() == 43);
  assert(r.event(EwoEventKind::TargetConfirmed, 43));
}

static void expectedDistanceAndBounds() {
  for (int8_t direction : {-1, 1}) {
    Run r(45, direction);
    const uint8_t target = r.n.target().sequence;
    r.window(1000);
    r.irAt(Run::origin + uint64_t(r.n.target().distanceMm) * 1000);
    r.window(r.support());
    assert(r.n.confirmedCount() == 1 && r.n.mm() == target);
  }
  for (uint64_t travel : {uint64_t(0), uint64_t(1), uint64_t(10000), uint64_t(300000),
                          uint64_t(345000), uint64_t(345001), uint64_t(500000)}) {
    Run r; r.window(1000); r.irAt(Run::origin + travel); r.window(1100);
    assert(r.n.confirmedCount() == (travel > 0 ? 1u : 0u));
    assert(r.n.missedCount() == 0);
  }
}

static void parkedInField() {
  Run r;
  r.window(1100, 0); // non-actionable observations cannot qualify an onset
  r.window(1100); // first full actionable window already supports the target
  assert(r.n.confirmedCount() == 0 && r.n.hallSupport());
  r.irAt(Run::origin + 106000);
  r.window(1100);
  assert(r.n.confirmedCount() == 0); // movement alone cannot qualify old support
  r.window(1000); assert(!r.n.hallSupport());
  r.irAt(Run::origin + 116000);
  r.window(1100);
  assert(r.n.confirmedCount() == 1 && r.n.mm() == 44);
}

static void noFirstMiss() {
  Run r; r.window(1000);
  for (uint64_t travel : {uint64_t(345000), uint64_t(345001), uint64_t(500000),
                          uint64_t(10000000), uint64_t(1000000000)}) {
    r.irAt(Run::origin + travel); r.window(1000);
    assert(r.n.missedCount() == 0 && r.n.mm() == 45 && r.n.target().sequence == 44);
    assert(!r.event(EwoEventKind::MissedMagnet, 44));
  }
  // Known limit: the next matching onset is still labelled the first target,
  // even if physical magnets were not detected. No distance ceiling remains.
  r.window(1100);
  assert(r.n.confirmedCount() == 1 && r.n.mm() == 44);
  assert(r.n.openingIrUm() == Run::origin + 1000000000);
}

static void secondMissStillNormal() {
  Run r; r.window(1000); r.irAt(Run::origin + 500000); r.window(1100);
  const uint64_t physicalOrigin = r.n.openingIrUm();
  const uint64_t interval = uint64_t(r.n.target().distanceMm) * 1000;
  r.irAt(physicalOrigin + 200000); // complete spatial cycle
  r.irAt(physicalOrigin + interval + interval * 15 / 100);
  assert(r.n.missedCount() == 0 && r.n.target().sequence == 43);
  r.irAt(physicalOrigin + interval + interval * 15 / 100 + 1);
  assert(r.n.missedCount() == 1 && r.n.mm() == 43 && r.n.target().sequence == 42);
  assert(r.event(EwoEventKind::MissedMagnet, 43));
}

static void wrongPolarityAndContext() {
  Run r; r.window(1000); r.irAt(Run::origin + 106000); r.window(900);
  assert(r.n.confirmedCount() == 0 && r.n.mm() == 45 && !r.n.hallSupport());
  r.window(1100); assert(r.n.confirmedCount() == 1);

  Run queued;
  for (unsigned i = 0; i < 5; ++i) queued.sample(1000, 40, queued.declaredAt - 1);
  for (unsigned i = 0; i < 5; ++i) queued.sample(1000, 40, queued.declaredAt);
  queued.irAt(Run::origin + 106000); queued.window(1100);
  assert(queued.n.confirmedCount() == 0); // no actual absence strictly after declaration
  queued.window(1000); queued.window(1100);
  assert(queued.n.confirmedCount() == 1);

  Run redeclared; redeclared.window(1000); redeclared.declare(45, -1);
  redeclared.irAt(Run::origin + 106000); redeclared.window(1100);
  assert(redeclared.n.confirmedCount() == 0); // earlier declaration's absence is not reused
}

static void reverseClearsException() {
  Run r; r.window(1000); r.irAt(Run::origin + 106000);
  r.n.reverse(1, r.now += 1000); r.dir = 1;
  assert(r.n.target().sequence == 45);
  r.irAt(Run::origin + 116000); r.window(r.support());
  assert(r.n.confirmedCount() == 0); // no relaxed lower bound after reverse
  r.irAt(Run::origin + 212000); r.window(r.support());
  assert(r.n.confirmedCount() == 1 && r.n.mm() == 45);
  // This intentionally preserves the assumed-origin reversal math, not actual
  // position within the operator-declared interval (known limit).
}

int main() {
  fieldCase(); expectedDistanceAndBounds(); parkedInField(); noFirstMiss();
  secondMissStillNormal();
  wrongPolarityAndContext(); reverseClearsException();
  std::puts("PASS: FT2 first target at 10/106/300/500 mm, onset safety, no first miss, normal second window/miss, polarity, context, reversal");
}
