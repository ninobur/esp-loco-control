// Gate 11: a station stop may not teach the baseline the magnet it parks on.
//
// THE FAILURE THIS GATE EXISTS FOR (finding 10, 2026-09-01, Bamboo CW)
// --------------------------------------------------------------------
// Toby rolled to rest at MM159 with the sensor in a magnet's steady field.
// Because the approach is slow the raw level rose GRADUALLY, so no single step
// exceeded entryMargin against the reference of the moment, and the 41-deep
// median simply tracked it: 1853 -> 1899 in about one second, at throttle
// 22 -> 17. It then held 1899 through the whole 30 s dwell. On departure the
// field fell away, the true idle level read 46 counts LOW, a 3,628 ms passage
// opened, swallowed the real MM161 magnet whole and was rejected TOO_SOON, and
// the next magnet disagreed with the still-pending target. STRUCK.
//
// The capture happened during the DECELERATION, complete before the dwell even
// began, so a shorter dwell is no defence. The gate has to be closed through
// the approach ramp.
//
// The exposure band is PWM 25-39: above the tractive floor of 25 (so a naive
// motion gate is open) but slow enough that a 30 mm magnet sits under the
// sensor for longer than the ~525 ms the median needs to move.
#include <cstdio>
#include <cmath>
#include "../HallCapture.h"
using namespace navi_one;

static const int16_t BASE = 1853;      // Toby's measured idle level, 2026-09-01
static const int     FRINGE = 46;      // the shift he actually parked in
static const int     FLOOR = 25;       // NAVI_BASELINE_ADAPT_PWM for this loco
static int checks = 0, failures = 0;
static void ok(bool c, const char* what, const char* d = "") {
  ++checks; if (!c) { ++failures; printf("  FAIL %s %s\n", what, d); }
}

int main() {
  printf("gate 11 -- a station stop cannot teach the baseline the magnet\n\n");

  printf("A. the Bamboo profile, marker for marker\n");
  {
    CaptureConfig cfg; HallCapture<> cap(cfg);
    uint32_t t = 0;
    for (; t < 3000; ++t) cap.sample(t, BASE, true);          // primed at speed
    ok(cap.baseline() == BASE, "primed on the true idle level");

    // Deceleration: throttle 60 -> 0 over 12 s, and across the last 2 s the
    // sensor slides into the platform magnet's fringe. Exactly the shape of the
    // field record: gradual, so no single step trips the 38-count entry margin.
    int pwm = 60;
    for (int ms = 0; ms < 12000; ++ms, ++t) {
      pwm = 60 - (ms * 60) / 12000;
      const int into = ms - 10000;                            // last 2 s
      const int v = into <= 0 ? 0 : (FRINGE * into) / 2000;
      cap.sample(t, (int16_t)(BASE + v), pwm > FLOOR);
    }
    printf("   after the approach ramp: baseline=%d (field showed 1899)\n", (int)cap.baseline());
    ok(cap.baseline() == BASE, "THE REFERENCE DID NOT FOLLOW IT DOWN THE RAMP");

    // The 30 s dwell, parked in the fringe, throttle 0.
    for (int ms = 0; ms < 30000; ++ms, ++t) cap.sample(t, (int16_t)(BASE + FRINGE), false);
    ok(cap.baseline() == BASE, "and did not learn it across the 30 s dwell");

    // Departure: throttle ramps 0 -> 90 at 200 ms per count. The locomotive
    // starts to roll once it is above the floor and pulls clear of the fringe
    // over about a second. Note the migration guard runs from the moment motion
    // permission ARRIVES, not from the passage opening -- the parked passage is
    // already 32 s old here, and without that the reference would be free to
    // walk straight onto the fringe the instant the throttle crossed 25.
    bool moved = false;
    for (int ms = 0; ms < 8000; ++ms, ++t) {
      const int pwm2 = (ms / 200 < 90) ? ms / 200 : 90;
      int v = FRINGE;
      if (pwm2 > FLOOR) {
        const int rolling = ms - (FLOOR + 1) * 200;
        v = (rolling >= 1000) ? 0 : FRINGE - (FRINGE * rolling) / 1000;
      }
      cap.sample(t, (int16_t)(BASE + v), pwm2 > FLOOR);
      if (cap.baseline() != BASE) moved = true;
    }
    printf("   after departure:          baseline=%d\n", (int)cap.baseline());
    ok(!moved, "the reference never moved at any point in the whole stop");
    ok(cap.baseline() == BASE, "the reference is still the true idle level");
  }

  printf("\nB. a magnet crossed slowly through PWM 25-39 stays ONE passage\n");
  {
    // At ~30 mm effective width and 3.990 x (PWM - 25.1) mm/s, PWM 30 puts a
    // magnet under the sensor for about 1.5 s -- three times the ~525 ms the
    // median needs. It must not be split, truncated, or have its judged shape
    // pulled about by a reference moving underneath it.
    auto crossAt = [](int durMs, bool moving, uint16_t& n, uint16_t& peak,
                      uint16_t& closes, uint16_t& dec) {
      CaptureConfig cfg; HallCapture<> cap(cfg);
      uint32_t t = 0;
      for (; t < 3000; ++t) cap.sample(t, BASE, moving);
      closes = 0; n = 0; peak = 0; dec = 1;
      for (int ms = 0; ms < durMs + 2000; ++ms, ++t) {
        const double x = (ms - durMs / 2.0) / (durMs / 5.2);
        const int v = (int)(220.0 * std::exp(-0.5 * x * x));
        if (cap.sample(t, (int16_t)(BASE + v), moving)) {
          ++closes; n = cap.passage().sampleCount;
          peak = cap.passage().peakCounts; dec = cap.passage().decimation;
        }
      }
    };
    uint16_t nFast, pFast, cFast, dFast, nSlow, pSlow, cSlow, dSlow;
    crossAt(150,  true, nFast, pFast, cFast, dFast);   // at cruise
    crossAt(1500, true, nSlow, pSlow, cSlow, dSlow);   // PWM ~30
    printf("   150 ms crossing:  passages=%u peak=%u\n", cFast, pFast);
    printf("   1500 ms crossing: passages=%u peak=%u\n", cSlow, pSlow);
    ok(cFast == 1, "the fast crossing is one passage");
    ok(cSlow == 1, "THE SLOW CROSSING IS ALSO ONE PASSAGE -- not split");
    ok(pSlow >= pFast - 2 && pSlow <= pFast + 2, "and reaches the same peak -- not truncated");
  }

  printf("\nC. the same crossing at 3 s, well past the migration guard\n");
  {
    // Longer than openMigrateMs, so the live reference IS free to move under it.
    // The recording is entry-referenced, so the judged peak must be untouched.
    CaptureConfig cfg; HallCapture<> cap(cfg);
    uint32_t t = 0;
    for (; t < 3000; ++t) cap.sample(t, BASE, true);
    uint16_t peak = 0, closes = 0;
    for (int ms = 0; ms < 5000; ++ms, ++t) {
      const double x = (ms - 1500.0) / (3000 / 5.2);
      const int v = (int)(220.0 * std::exp(-0.5 * x * x));
      if (cap.sample(t, (int16_t)(BASE + v), true)) { ++closes; peak = cap.passage().peakCounts; }
    }
    printf("   3000 ms crossing: passages=%u peak=%u\n", closes, peak);
    ok(closes >= 1, "it closes");
    ok(peak >= 215, "and the judged peak is the true 220, not a migrated remnant");
  }

  printf("\nD. the floor is 25, and 22 is on the wrong side of it\n");
  {
    // QUORUM used MOTOR_DEAD_ZONE_PWM 20. The Bamboo capture began at throttle
    // 22, which a gate of 20 lets through and a gate of 25 refuses. This is why
    // the constant is taken from Toby's own PWM/speed fit and not borrowed.
    ok(!(22 > FLOOR), "throttle 22 does NOT permit adaptation");
    ok(!(25 > FLOOR), "throttle 25, the floor itself, does not either");
    ok(  26 > FLOOR,  "throttle 26 does");
    ok(  22 > 20,     "a borrowed floor of 20 would have permitted the capture");
  }

  printf("\nE. the counter-test: with the gate held open, the capture happens\n");
  {
    // The same profile with mayAdapt forced true throughout -- which is what
    // 0.6 did, having no motion gate at all. If this did NOT reproduce the
    // field number, section A would be asserting nothing.
    CaptureConfig cfg; HallCapture<> cap(cfg);
    uint32_t t = 0;
    for (; t < 3000; ++t) cap.sample(t, BASE, true);
    for (int ms = 0; ms < 12000; ++ms, ++t) {
      const int into = ms - 10000;
      const int v = into <= 0 ? 0 : (FRINGE * into) / 2000;
      cap.sample(t, (int16_t)(BASE + v), true);          // <- gate held open
    }
    // 0.6 had no gate at all, so it went on adapting while parked. Three
    // seconds of that is all it takes.
    for (int ms = 0; ms < 3000; ++ms, ++t) cap.sample(t, (int16_t)(BASE + FRINGE), true);
    printf("   ungated, parked three seconds:     baseline=%d (field showed %d)\n",
           (int)cap.baseline(), BASE + FRINGE);
    ok(cap.baseline() == BASE + FRINGE,
       "UNGATED the reference becomes the magnet exactly, as it did at Bamboo");
    ok(cap.baseline() - BASE > 38,
       "and the error exceeds entryMargin, so departure opens a spurious passage");
  }

  printf("\n%d checks, %d failures\n", checks, failures);
  if (failures) { printf("GATE 11 FAILED\n"); return 1; }
  printf("GATE 11 PASSED\n");
  return 0;
}
