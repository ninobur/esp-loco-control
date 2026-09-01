// Gate 9: section cruise — the Grillers climb and its ramp-down.
//
// Operator's ruling, 2026-09-01: 110 through the climb CW, then from MM80 shed
// 4 PWM per marker, one count at a time, reaching 90 at MM85 and holding it.
// CCW is untouched: that stretch is downhill.
//
// The whole point of this gate is that the profile is a PURE FUNCTION of
// position and direction. A throttle that depends on where the locomotive
// thinks it is has to be checkable without a locomotive.
#include <cstdio>
#include <initializer_list>
#include "../RouteMap.h"
using namespace navi_one;

static int checks = 0, failures = 0;
static void ok(bool c, const char* what, int mm = -1) {
  ++checks;
  if (!c) { ++failures; if (mm >= 0) printf("  FAIL %s at MM%d\n", what, mm);
            else printf("  FAIL %s\n", what); }
}

int main() {
  const uint8_t BASE = 90;
  printf("gate 9 -- section cruise\n\n");

  printf("A. CW: base cruise before the climb\n");
  for (int mm = 55; mm < 65; ++mm)
    ok(cruisePwmAt((uint8_t)mm, +1, BASE) == BASE, "expected base cruise", mm);

  printf("B. CW: 110 through the climb, MM65..79\n");
  for (int mm = 65; mm < 80; ++mm)
    ok(cruisePwmAt((uint8_t)mm, +1, BASE) == 110, "expected 110 on the grade", mm);

  printf("C. CW: 4 PWM per marker from MM80, reaching base at MM84\n");
  const uint8_t want[5] = {106, 102, 98, 94, 90};
  for (int i = 0; i < 5; ++i) {
    const int mm = 80 + i;
    const uint8_t got = cruisePwmAt((uint8_t)mm, +1, BASE);
    ok(got == want[i], "wrong ramp value", mm);
    if (got != want[i]) printf("       got %u want %u\n", got, want[i]);
  }

  printf("D. CW: base cruise from MM85 onward\n");
  for (int mm = 85; mm < 100; ++mm)
    ok(cruisePwmAt((uint8_t)mm, +1, BASE) == BASE, "expected base cruise", mm);

  printf("E. the ramp only ever descends, and never below base\n");
  {
    uint8_t prev = 255;
    for (int mm = 65; mm <= 90; ++mm) {
      const uint8_t v = cruisePwmAt((uint8_t)mm, +1, BASE);
      ok(v <= prev, "ramp went back up", mm);
      ok(v >= BASE, "ramp fell below base cruise", mm);
      prev = v;
    }
  }

  printf("F. CCW is untouched everywhere\n");
  for (int mm = 0; mm < ROUTE_N; ++mm)
    ok(cruisePwmAt((uint8_t)mm, -1, BASE) == BASE, "CCW disturbed", mm);

  printf("G. an unset direction never raises the throttle\n");
  for (int mm = 0; mm < ROUTE_N; ++mm)
    ok(cruisePwmAt((uint8_t)mm, 0, BASE) == BASE, "unset dir disturbed", mm);

  printf("H. it holds for any base cruise, not just 90\n");
  for (uint8_t b : {60, 80, 90, 100}) {
    ok(cruisePwmAt(70, +1, b) == 110, "grade value should not depend on base");
    ok(cruisePwmAt(90, +1, b) == b,   "off-section should return base");
    // a base above the ramp value must win: never slow the loco below its cruise
    ok(cruisePwmAt(84, +1, b) >= b,   "ramp must not drop below base");
  }
  ok(cruisePwmAt(83, +1, 100) == 100, "base 100 wins over ramp value 94");

  printf("I. marker index wraps safely\n");
  ok(cruisePwmAt((uint8_t)(65 + ROUTE_N), +1, BASE) == 110, "wrapped index");

  printf("\n%d checks, %d failures\n", checks, failures);
  if (failures) { printf("GATE 9 FAILED\n"); return 1; }
  printf("GATE 9 PASSED\n");
  return 0;
}
