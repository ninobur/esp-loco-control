#include <cassert>
#include <cstdint>
#include <iostream>

#include "../HypothesisNavigator.h"

using namespace navi_hypothesis;
using namespace navi_one;

static Observation observation(uint32_t atMs, uint8_t opening,
                               uint8_t window, bool windowValid = true,
                               bool moving = true) {
  Observation value;
  value.atMs = atMs;
  value.openingPolarity = opening;
  value.windowPolarity = window;
  value.windowValid = windowValid;
  value.motionPermitsAdvance = moving;
  return value;
}

static bool hasPosition(const HypothesisNavigator& navigator, uint8_t mm) {
  for (uint8_t i = 0; i < navigator.count(); ++i)
    if (navigator.hypothesis(i).mm == mm) return true;
  return false;
}

static uint8_t findDifferentNextPair() {
  for (uint16_t mm = 0; mm < ROUTE_N; ++mm) {
    const uint8_t one = nextMarker(mm, 1);
    const uint8_t two = nextMarker(one, 1);
    if (polarityAt(one) != polarityAt(two)) return static_cast<uint8_t>(mm);
  }
  assert(false);
  return 0;
}

int main() {
  HypothesisNavigator clean;
  clean.declare(0, 1, 1000);
  uint32_t now = 1400;
  for (uint8_t mm = 1; mm <= 5; ++mm, now += 400) {
    const uint8_t pole = polarityAt(mm);
    assert(clean.observe(observation(now, pole, pole)) == Result::Tracking);
    assert(clean.positionCertain() && clean.position() == mm);
  }

  // A physically impossible event is retained as false and cannot advance.
  HypothesisNavigator tooSoon;
  tooSoon.declare(0, 1, 1000);
  const uint8_t mm1Pole = polarityAt(1);
  assert(tooSoon.observe(observation(1100, mm1Pole, mm1Pole)) == Result::Tracking);
  assert(tooSoon.positionCertain() && tooSoon.position() == 0);
  assert(tooSoon.hypothesis(0).causes & CAUSE_FALSE_OBSERVATION);
  assert(tooSoon.observe(observation(1500, mm1Pole, mm1Pole)) == Result::Tracking);
  assert(tooSoon.position() == 1);

  // Stationary evidence is visible to the caller but cannot change hypotheses.
  HypothesisNavigator stationary;
  stationary.declare(0, 1, 1000);
  assert(stationary.observe(observation(1500, mm1Pole, mm1Pole, true, false)) == Result::Ignored);
  assert(stationary.positionCertain() && stationary.position() == 0);

  // Contradictory polarity forks "bad polarity" and "false observation".
  // Later clean route evidence must resolve without an operator declaration.
  HypothesisNavigator polarityConflict;
  const uint8_t start = findDifferentNextPair();
  polarityConflict.declare(start, 1, 1000);
  const uint8_t next = nextMarker(start, 1);
  const uint8_t wrong = 1 - polarityAt(next);
  assert(polarityConflict.observe(observation(1400, wrong, wrong)) == Result::Ambiguous);
  assert(hasPosition(polarityConflict, start));
  assert(hasPosition(polarityConflict, next));
  uint8_t truth = next;
  now = 1800;
  for (uint8_t i = 0; i < 12 && !polarityConflict.positionCertain(); ++i, now += 400) {
    truth = nextMarker(truth, 1);
    const uint8_t pole = polarityAt(truth);
    polarityConflict.observe(observation(now, pole, pole));
  }
  assert(polarityConflict.positionCertain());
  assert(polarityConflict.position() == truth);

  // If the next marker was missed and the following polarity contradicts the
  // expected next marker, a skip-one branch is retained and later resolved.
  HypothesisNavigator missed;
  missed.declare(start, 1, 1000);
  const uint8_t skipped = nextMarker(next, 1);
  const uint8_t skippedPole = polarityAt(skipped);
  assert(skippedPole != polarityAt(next));
  assert(missed.observe(observation(1800, skippedPole, skippedPole)) == Result::Ambiguous);
  assert(hasPosition(missed, skipped));
  truth = skipped;
  now = 2200;
  for (uint8_t i = 0; i < 12 && !missed.positionCertain(); ++i, now += 400) {
    truth = nextMarker(truth, 1);
    const uint8_t pole = polarityAt(truth);
    missed.observe(observation(now, pole, pole));
  }
  assert(missed.positionCertain());
  assert(missed.position() == truth);

  // Opening/window disagreement preserves both a route interpretation and a
  // false-observation interpretation instead of assigning authority to either.
  HypothesisNavigator disagreement;
  disagreement.declare(start, 1, 1000);
  assert(disagreement.observe(observation(1400, 0, 1)) == Result::Ambiguous);
  assert(hasPosition(disagreement, start));
  assert(hasPosition(disagreement, next));

  std::cout << "NAVI_HYPOTHESIS_SHADOW checks passed\n";
}
