// Gate 8: acquisition under a sustained DC offset, and recovery from one.
//
// Every other gate feeds RECORDED PASSAGES to the recognizer. None of them
// exercises HallCapture acquiring from a live line, which is why the failure of
// 2026-08-31 evening reached the field: it is not a judgement fault at all.
//
// THE MECHANISM, unchanged and still asserted here (sections A-D)
// --------------------------------------------------------------
// A DC offset above entryMargin (38) opens a passage. Closing needs the signal
// back inside exitMargin (25), so an offset of 26 or more holds it open. While
// it is open the reference cannot be maintained, so the offset that holds the
// passage open is the one thing that cannot be measured away. It latches.
//
// What the latch then does depends on POLARITY:
//   opposite-polarity magnet -> cancels the offset, the passage closes, the
//     magnet gets its own passage. Visible as an offset+magnet pair per marker.
//   same-polarity magnet -> deepens the offset, the passage never closes, and
//     the magnet is INVISIBLE. The navigator does not advance. Finding 08.
//
// WHAT IS NEW (sections E-G)
// --------------------------
// The reference is now gated on tractive motion and may migrate under an open
// passage while the locomotive is moving. So the latch is a condition the
// locomotive can DRIVE OUT OF, instead of one only a reboot ever cleared.
// Findings 09 and 10. At rest it still latches, deliberately: at rest the
// median cannot tell an offset from the magnet it is parked on, and adapting
// there is the Bamboo failure of finding 10. Gate 11 covers that side.
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
// moving says whether the locomotive is above its tractive floor throughout.
static Result run(int offset, int magnetSign, bool moving,
                  uint16_t floorMs=NAVI_PASSAGE_FLOOR_MS) {
  CaptureConfig cfg; cfg.floorMs=floorMs; HallCapture<> cap(cfg);
  uint32_t t = 0;
  for (; t < 3000; ++t) cap.sample(t, BASE, moving);
  Result r{0, 0, 0, 0};
  for (int ms = 0; ms < 20000; ++ms, ++t) {
    int v = offset;
    const int phase = ms % 1000;
    if (phase < 150) {
      const double x = (phase - 75) / 28.0;
      v += magnetSign * (int)(220.0 * std::exp(-0.5 * x * x));
      if (phase == 0) ++r.crossed;
    }
    if (cap.sample(t, (int16_t)(BASE + v), moving)) {
      ++r.closed;
      if (cap.passage().peakCounts > 150) ++r.seen;
    }
  }
  r.baseline = cap.baseline();
  return r;
}

int main() {
  printf("gate 8 -- acquisition under a sustained DC offset, and recovery\n\n");

  printf("A. no offset: the control\n");
  {
    Result r = run(0, +1, true);
    printf("   crossed=%d closed=%d seen=%d baseline=%d\n", r.crossed, r.closed, r.seen, (int)r.baseline);
    ok(r.seen == r.crossed, "every magnet produces its own passage");
    ok(r.baseline == BASE, "baseline undisturbed by 20 magnets at speed");
  }

  printf("B. -50 count offset, OPPOSITE-polarity magnets, AT REST\n");
  {
    Result r = run(-50, +1, false);
    printf("   crossed=%d closed=%d seen=%d baseline=%d\n", r.crossed, r.closed, r.seen, (int)r.baseline);
    ok(r.seen == r.crossed, "magnets still seen -- each one closes the stalled passage");
    ok(r.closed > r.crossed, "an extra passage per marker: the offset itself");
    ok(r.baseline == BASE, "baseline frozen -- it never re-references to the offset");
  }

  printf("C. -50 count offset, SAME-polarity magnets, AT REST  <-- the field failure\n");
  {
    Result r = run(-50, -1, false);
    printf("   crossed=%d closed=%d seen=%d baseline=%d\n", r.crossed, r.closed, r.seen, (int)r.baseline);
    ok(r.crossed == 20, "twenty magnets were crossed");
    ok(r.seen == 0, "NONE of them was seen -- every marker swallowed");
    ok(r.closed == 0, "the passage never closed in twenty seconds");
    ok(r.baseline == BASE, "baseline frozen throughout");
  }

  // The threshold that matters is NOT entryMargin. The magnet itself opens the
  // passage; the offset then only has to hold the signal above exitMargin (25)
  // for the passage never to close. So the bar for a latch is 25 counts, not
  // 38 -- a third lower than the figure finding 08 first assumed.
  printf("\nD. the latch threshold is exitMargin (25), not entryMargin (38)\n");
  for (int off : {-10, -20, -24, -26, -30, -37, -45, -60}) {
    Result r = run(off, -1, false);
    printf("   offset %-4d -> magnets seen %2d/20  %s\n", off, r.seen,
           r.seen == 0 ? "LATCHED" : (r.seen == 20 ? "clean" : "partial"));
    if (off >= -24) ok(r.seen == 20, "at or below exitMargin 25: no latch");
    if (off <= -26) ok(r.seen == 0, "above exitMargin 25: latches at rest");
  }

  printf("\nE. THE SAME OFFSETS, MOVING: the locomotive drives out of every one\n");
  for (int off : {-26, -30, -45, -60, -90, +45, +90}) {
    Result r = run(off, off < 0 ? -1 : +1, true);
    printf("   offset %-4d -> seen %2d/20  final baseline %d (true line %d)\n",
           off, r.seen, (int)r.baseline, BASE + off);
    ok(r.baseline == BASE + off, "the reference walks onto the offset");
    ok(r.seen >= 17, "and markers are seen again within about 3 s");
  }

  printf("\nF. a bad startup baseline recovers during motion\n");
  {
    // Finding 09. On 2026-09-01 the 2 s prime window overlapped a transient and
    // the median settled about 70 counts below the true idle level. That is
    // above entryMargin, so a passage opened on the first sample after priming
    // and could never close: reset() preserves the baseline, and the median
    // needs ~525 ms of CLOSED line while the offset re-opens a passage after
    // one 25 ms interval. It held for 72 minutes across two declarations.
    const int BAD = -70;
    CaptureConfig cfg; HallCapture<> cap(cfg);
    uint32_t t = 0;
    for (; t < 3000; ++t) cap.sample(t, (int16_t)(BASE + BAD), false);   // poisoned prime
    ok(cap.baseline() == BASE + BAD, "the prime adopted the wrong level");

    // The line returns to its true level and the locomotive sits there.
    for (int i = 0; i < 10000; ++i, ++t) cap.sample(t, BASE, false);
    ok(cap.baseline() == BASE + BAD, "AT REST it is never corrected -- 10 s, no change");

    // A declaration does not help either: reset() keeps the baseline by design.
    cap.reset();
    for (int i = 0; i < 5000; ++i, ++t) cap.sample(t, BASE, false);
    ok(cap.baseline() == BASE + BAD, "a declaration does not clear it either");

    // Now drive.
    int crossed = 0, seen = 0;
    uint32_t recoveredAt = 0;
    for (int ms = 0; ms < 20000; ++ms, ++t) {
      int v = 0; const int ph = ms % 1000;
      if (ph < 150) { const double x = (ph - 75) / 28.0;
                      v += (int)(220.0 * std::exp(-0.5 * x * x));
                      if (!ph) ++crossed; }
      if (cap.sample(t, (int16_t)(BASE + v), true) && cap.passage().peakCounts > 150) ++seen;
      if (!recoveredAt && cap.baseline() == BASE) recoveredAt = (uint32_t)ms;
    }
    printf("   drove off: baseline=%d recovered after %u ms, seen %d/%d\n",
           (int)cap.baseline(), (unsigned)recoveredAt, seen, crossed);
    ok(cap.baseline() == BASE, "MOVING, the reference walks back to the true idle level");
    ok(recoveredAt > 0 && recoveredAt < 4000, "within four seconds of moving off");
    ok(seen >= crossed - 3, "and the markers come back");
  }

  printf("\nG. migration never moves the RECORDING out from under a passage\n");
  {
    // The live reference may walk onto a stale offset; the stored samples are
    // measured against the baseline captured at passage open and must not move
    // with it. Decisions 0064 and 0065 depend on that curve being untouched.
    const int OFF = -60;
    CaptureConfig cfg; HallCapture<> cap(cfg);
    uint32_t t = 0;
    for (; t < 3000; ++t) cap.sample(t, BASE, true);
    bool closed = false; int32_t entry = 0;
    // A flat offset, held long enough that the reference migrates and closes it.
    for (int i = 0; i < 6000 && !closed; ++i, ++t) {
      if (cap.sample(t, (int16_t)(BASE + OFF), true)) closed = true;
      if (i == 0) entry = cap.entryBaseline();
    }
    ok(closed, "the offset passage does close once the reference migrates");
    ok(entry == BASE, "the passage was opened against the pre-migration reference");
    const Passage& p = cap.passage();
    // Oriented at close: a negative offset is negated, so every stored sample
    // reads +60 -- the TRUE depth, not a shrinking one. The property under test
    // is that the tail, recorded long AFTER the live reference had migrated
    // onto the offset, is identical to the head.
    bool tailFull = true;
    for (uint16_t i = (uint16_t)(p.sampleCount * 3 / 4); i < p.sampleCount; ++i)
      if (p.oriented[i] != -OFF) tailFull = false;
    ok(tailFull, "the last quarter still carries the full offset depth");
    ok(p.oriented[p.sampleCount - 1] == -OFF, "the final sample has not decayed");
    ok(p.peakCounts == (uint16_t)(-OFF), "and the judged peak is the true depth");

    // Every in-passage sample carries it, allowing for one boundary sample:
    // push() halves preAt_ when it decimates, so after three halvings the
    // pre-roll marker can under-report by a sample. That is pre-existing and
    // cosmetic -- fitResidual() searches upward from preSamples for the arch
    // above 20% of peak, and a near-zero pre-roll sample is below that -- but
    // it is real and it is recorded here rather than papered over.
    int notFull = 0;
    for (uint16_t i = p.preSamples; i < p.sampleCount; ++i)
      if (p.oriented[i] != -OFF) ++notFull;
    ok(notFull <= 1, "at most the one decimation boundary sample differs");
  }

  printf("\nH. diagnostic sweep: floor versus visibility in case B\n");
  for (uint16_t floor : {40, 50, 60, 70, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90}) {
    Result r = run(-50, +1, false, floor);
    printf("   floor %2u ms -> crossed=%d closed=%d seen=%d\n",
           (unsigned)floor, r.crossed, r.closed, r.seen);
  }

  printf("\nI. fixed-startup field policy leaves the rolling median in shadow only\n");
  {
    CaptureConfig cfg; cfg.fixedAfterPrime = true; HallCapture<> cap(cfg);
    uint32_t t = 0;
    for (; t < 3000; ++t) cap.sample(t, BASE, true);
    ok(cap.baseline() == BASE, "startup baseline is established normally");
    ok(cap.shadowBaseline() == BASE, "shadow begins at the startup baseline");

    // A sustained, sub-entry shift is exactly the quantity this field build
    // must measure. It moves the old adaptive reference, now shadow-only,
    // without moving the reference used by passage acquisition.
    for (int i = 0; i < 3000; ++i, ++t) cap.sample(t, BASE + 30, true);
    ok(cap.shadowBaseline() == BASE + 30, "shadow median follows the shifted line");
    ok(cap.baseline() == BASE, "authoritative baseline remains fixed");
    ok(!cap.open(), "a sub-entry shift does not invent a passage");

    for (int i = 0; i < 3000; ++i, ++t) cap.sample(t, BASE, true);
    ok(cap.shadowBaseline() == BASE, "shadow median follows the line home");
    ok(cap.baseline() == BASE, "authoritative baseline remains fixed after return");
  }

  printf("\n%d checks, %d failures\n", checks, failures);
  if (failures) { printf("GATE 8 FAILED\n"); return 1; }
  printf("GATE 8 PASSED\n");
  return 0;
}
