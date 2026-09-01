// Gate 8: acquisition under a sustained offset.
//
// Every other gate feeds RECORDED PASSAGES to the recognizer. None of them
// exercises HallCapture acquiring from a live line, which is why the failure of
// 2026-08-31 evening reached the field: it is not a judgement fault at all.
//
// updateBaseline() will not sample while a passage is open, and the baseline is
// a median of 41 samples at 25 ms, so it needs 1,025 ms of passage-free time to
// re-reference. A DC offset above entryMargin (38) opens a passage that cannot
// close, because closing needs the signal below exitMargin (25). The offset
// holding the passage open is therefore the one thing that cannot be measured
// away. It latches.
//
// What the latch then does depends on POLARITY:
//   opposite-polarity magnet -> cancels the offset, the passage closes, the
//     magnet gets its own passage. Visible as an offset+magnet pair per marker.
//   same-polarity magnet -> deepens the offset, the passage never closes, and
//     the magnet is INVISIBLE: HallCapture cannot open a passage while one is
//     open. The navigator does not advance. Finding 08.
//
// This gate records the behaviour as it stands. It does not assert a fix.
#include <cstdio>
#include <cmath>
#include <initializer_list>
#include "../HallCapture.h"
using namespace navi_one;

static const int16_t BASE = 1834;
static int checks = 0, failures = 0;
static void ok(bool c, const char* what, const char* d = "") {
  ++checks; if (!c) { ++failures; printf("  FAIL %s %s\n", what, d); }
}

struct Result { int crossed, closed, seen; int32_t baseline; };

// 20 s of running: one 150 ms, 220-count magnet per second, on a DC offset.
static Result run(int offset, int magnetSign) {
  CaptureConfig cfg; HallCapture<> cap(cfg);
  uint32_t t = 0;
  for (; t < 3000; ++t) cap.sample(t, BASE);
  Result r{0, 0, 0, 0};
  for (int ms = 0; ms < 20000; ++ms, ++t) {
    int v = offset;
    const int phase = ms % 1000;
    if (phase < 150) {
      const double x = (phase - 75) / 28.0;
      v += magnetSign * (int)(220.0 * std::exp(-0.5 * x * x));
      if (phase == 0) ++r.crossed;
    }
    if (cap.sample(t, (int16_t)(BASE + v))) {
      ++r.closed;
      if (cap.passage().peakCounts > 150) ++r.seen;
    }
  }
  r.baseline = cap.baseline();
  return r;
}

int main() {
  printf("gate 8 -- acquisition under a sustained DC offset\n\n");

  printf("A. no offset: the control\n");
  {
    Result r = run(0, +1);
    printf("   crossed=%d closed=%d seen=%d baseline=%d\n", r.crossed, r.closed, r.seen, (int)r.baseline);
    ok(r.seen == r.crossed, "every magnet produces its own passage");
    ok(r.baseline == BASE, "baseline undisturbed");
  }

  printf("B. -50 count offset, OPPOSITE-polarity magnets\n");
  {
    Result r = run(-50, +1);
    printf("   crossed=%d closed=%d seen=%d baseline=%d (a correct reference would read %d)\n",
           r.crossed, r.closed, r.seen, (int)r.baseline, BASE - 50);
    ok(r.seen == r.crossed, "magnets still seen -- each one closes the stalled passage");
    ok(r.closed > r.crossed, "an extra passage per marker: the offset itself");
    ok(r.baseline == BASE, "BASELINE IS FROZEN -- it never re-references to the offset");
  }

  printf("C. -50 count offset, SAME-polarity magnets  <-- the field failure\n");
  {
    Result r = run(-50, -1);
    printf("   crossed=%d closed=%d seen=%d baseline=%d\n", r.crossed, r.closed, r.seen, (int)r.baseline);
    ok(r.crossed == 20, "twenty magnets were crossed");
    ok(r.seen == 0, "NONE of them was seen -- every marker swallowed");
    ok(r.closed == 0, "the passage never closed in twenty seconds");
    ok(r.baseline == BASE, "baseline frozen throughout");
  }

  // The threshold that matters is NOT entryMargin. The magnet itself opens the
  // passage; the offset then only has to hold the signal above exitMargin (25)
  // for the passage never to close. So the bar for a latch is 25 counts, not
  // 38 -- a third lower than the figure finding 08 first assumed, and a much
  // easier condition to meet in the field.
  printf("\nD. the latch threshold is exitMargin (25), not entryMargin (38)\n");
  for (int off : {-10, -20, -24, -26, -30, -37, -45, -60}) {
    Result r = run(off, -1);
    printf("   offset %-4d -> magnets seen %2d/20  %s\n", off, r.seen,
           r.seen == 0 ? "LATCHED" : (r.seen == 20 ? "clean" : "partial"));
    if (off >= -24) ok(r.seen == 20, "at or below exitMargin 25: no latch");
    if (off <= -26) ok(r.seen == 0, "above exitMargin 25: latches, even well below entryMargin 38");
  }

  printf("\nE. a reboot clears the latch; nothing else does\n");
  {
    // Identical 20 s run either way. The only difference is whether the offset
    // was present while the baseline primed -- i.e. whether the locomotive was
    // rebooted after the offset appeared. Priming adopts whatever DC level is
    // there as the new zero, so a reboot re-references the sensor and there is
    // nothing left to latch. This is why the four stops of 2026-08-31 evening
    // were ONE persistent condition rather than four events, and why the flash
    // that appeared to fix them fixed them by rebooting.
    CaptureConfig cfg; HallCapture<> cap(cfg);
    uint32_t t = 0;
    const int OFF = -50;
    for (; t < 3000; ++t) cap.sample(t, (int16_t)(BASE + OFF));   // primed WITH it
    int crossed = 0, seen = 0;
    for (int ms = 0; ms < 20000; ++ms, ++t) {
      int v = OFF; const int ph = ms % 1000;
      if (ph < 150) { const double x = (ph - 75) / 28.0;
                      v -= (int)(220.0 * std::exp(-0.5 * x * x));
                      if (!ph) ++crossed; }
      if (cap.sample(t, (int16_t)(BASE + v)) && cap.passage().peakCounts > 150) ++seen;
    }
    printf("   primed WITH the offset: baseline=%d seen=%d/%d\n",
           (int)cap.baseline(), seen, crossed);
    ok(cap.baseline() == BASE + OFF, "priming adopts the offset as the new zero");
    ok(seen == crossed, "REBOOT CLEARS THE LATCH -- every magnet seen again");
  }

  printf("\n%d checks, %d failures\n", checks, failures);
  if (failures) { printf("GATE 8 FAILED\n"); return 1; }
  printf("GATE 8 PASSED (behaviour recorded; this gate asserts no fix)\n");
  return 0;
}
