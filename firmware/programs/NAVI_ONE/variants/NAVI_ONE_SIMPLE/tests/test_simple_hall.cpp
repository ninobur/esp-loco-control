#include <cassert>
#include <cstdint>
#include <iostream>
#include "SimpleHall.h"
using namespace navi_one;

int main() {
  SimpleHall h;
  uint32_t t = 0;
  auto feed = [&](int16_t raw, uint8_t pwm, uint32_t n,
                  HallEvent wanted = HallEvent::None) {
    unsigned found = 0;
    for (uint32_t i = 0; i < n; ++i) {
      HallDecision d = h.sample(t++, raw, pwm);
      if (d.event == wanted) ++found;
    }
    return found;
  };
  feed(1900, 0, 2000);
  assert(h.ready() && h.baseline() == 1900);
  feed(1900, 90, 100);
  assert(feed(2060, 90, 100, HallEvent::Open) == 1);
  assert(feed(1900, 90, 500, HallEvent::Close) == 1);
  assert(h.accepted() == 0); // no previous marker interval yet
  feed(1900, 90, 100);
  assert(feed(1740, 90, 100, HallEvent::Open) == 1);
  assert(feed(1900, 90, 400, HallEvent::BaselineAccepted) == 1);
  assert(h.baseline() == 1900);

  // A long magnetic shelf, including a location declaration, is one event.
  assert(feed(2060, 90, 100, HallEvent::Open) == 1);
  h.resetFrame();
  assert(feed(2060, 0, 1000, HallEvent::Open) == 0);
  assert(feed(1900, 0, 500, HallEvent::Close) == 1);
  assert(h.baseline() == 1900);

  // First event after a declaration re-establishes cadence.
  assert(feed(1740, 90, 100, HallEvent::Open) == 1);
  assert(feed(1900, 90, 400, HallEvent::Close) == 1);
  // Low PWM during the following collection cannot change the baseline.
  assert(feed(2060, 90, 100, HallEvent::Open) == 1);
  assert(feed(1900, 0, 400, HallEvent::BaselineRejected) == 1);
  assert(h.baseline() == 1900);

  // A clean shifted line updates the next interval's baseline.
  assert(feed(2060, 90, 100, HallEvent::Open) == 1);
  assert(feed(1910, 90, 400, HallEvent::BaselineAccepted) == 1);
  assert(h.baseline() == 1910);

  // A varying collection is rejected and carries that baseline.
  assert(feed(1750, 90, 100, HallEvent::Open) == 1);
  unsigned spreadReject = 0;
  for (int i = 0; i < 400; ++i) {
    HallDecision d = h.sample(t++, (i % 2) ? 1880 : 1920, 90);
    if (d.event == HallEvent::BaselineRejected &&
        d.reject == BaselineReject::Spread) ++spreadReject;
  }
  assert(spreadReject == 1 && h.baseline() == 1910);

  std::cout << "simple Hall detector checks passed\n";
}
