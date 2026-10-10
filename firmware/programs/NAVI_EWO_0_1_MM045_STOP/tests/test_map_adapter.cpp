#include "../NaviMapAdapter.h"

#include <cstdio>

using namespace navi_eyes;

static int failures = 0;
static void check(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::printf("FAIL: %s\n", message);
  }
}

int main() {
  const TargetSpec cw = mappedTargetAfter(0, 1);
  check(cw.sequence == 1 && cw.distanceMm == navi_one::spanMm(0, 1) &&
            cw.direction == 1 &&
            cw.polarity == HallOpeningPolarity::AboveReference,
        "CW map conversion uses next marker, CW span, and North/above mapping");
  const TargetSpec ccw = mappedTargetAfter(0, -1);
  check(ccw.sequence == 170 && ccw.distanceMm == navi_one::spanMm(0, -1) &&
            ccw.direction == 2 &&
            ccw.polarity == HallOpeningPolarity::BelowReference,
        "CCW conversion wraps and uses the reverse span and South/below mapping");
  check(cw.polarity == HallOpeningPolarity::AboveReference,
        "legacy inversion macro has no authority over common measured convention");
  check(mappedTargetAfter(0, 0).polarity == HallOpeningPolarity::Unknown,
        "unset direction cannot create a target");
  std::printf("%s: %d failures\n", failures ? "FAIL" : "PASS", failures);
  return failures ? 1 : 0;
}
