// ---------------------------------------------------------------------------
// gate_excursion -- synthetic properties of the X20 detector that no recorded
// corpus can demonstrate, because September 15 contains no instance of them.
// Each one is a claim made in ExcursionDetector.h, checked directly.
// Build: g++ -O2 -std=c++17 -o /tmp/x19gate gate_excursion.cpp && /tmp/x19gate
// ---------------------------------------------------------------------------
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include "../ExcursionDetector.h"
using namespace navi_one;

static int fails = 0;
static void check(bool ok, const char* what) {
  printf("  %-72s %s\n", what, ok ? "ok" : "FAIL");
  if (!ok) ++fails;
}

struct Rig {
  DetectorConfig cfg;
  ExcursionDetector<> det;
  uint32_t t = 0;
  std::vector<Excursion> got;
  // X20. The station machine is holding AND the ramped PWM is 0. False for
  // every gate below 13, so gates 1..12 drive the identical X19 path.
  bool stopped = false;
  Rig() : det(cfg) { for (int i = 0; i < 2200; ++i) det.sample(t++, 1900, true); }
  void step(int16_t raw) {
    if (det.sample(t, raw, true, stopped)) got.push_back(det.excursion());
    ++t;
  }
  void feed(int16_t raw, int ms) { for (int i = 0; i < ms; ++i) step(raw); }
  // Move to `level` and stay there until the detector has measured it as rest,
  // then forget everything that happened on the way. The step itself is a
  // legitimate candidate -- that is how boot 5 of 2026-09-15 began, primed on
  // a magnet and then driven off it -- but it is setup, not the thing under
  // test.
  void settle(int16_t level) { feed(level, 3000); got.clear(); }
  // a half-sine arc of the given amplitude and width, on top of `base`
  void arc(int amp, int ms, int16_t base = 1900) {
    for (int i = 0; i < ms; ++i) {
      const double f = sin(3.14159265 * (double)i / (double)ms);
      step((int16_t)(base + (int)(amp * f)));
    }
  }
  // Run up the leading limb of an arc of total width `ms` and STOP THERE,
  // holding the level the sensor reached -- a locomotive whose zero ramp
  // expired with the magnet still under it. Returns the held raw value.
  int16_t rollInto(int amp, int ms, int intoMs, int16_t base = 1900) {
    int16_t v = base;
    for (int i = 0; i < intoMs; ++i) {
      const double f = sin(3.14159265 * (double)i / (double)ms);
      v = (int16_t)(base + (int)(amp * f));
      step(v);
    }
    return v;
  }
};

// The Arches dwell of 2026-09-15, 18:58. Otto stands at PWM 0 with the sensor
// about 100 counts inside a magnet's fringe, and the signal wanders back
// toward the line three times across the 30 s stop. Under X19 each of those
// wanders is a 70-count departure from a rest the detector has re-measured
// onto the displaced level, and each one advanced the map by a marker.
static void archesDwell(Rig& r) {
  r.feed(2002, 15000);
  for (int i = 0; i < 3; ++i) { r.feed(1900, 3); r.feed(2002, 900); }
  r.feed(2002, 12000);
}

int main() {
  printf("X19 ExcursionDetector -- synthetic gates\n\n");

  printf("1. a persistent displaced level does not manufacture events\n");
  { Rig r; r.feed(1900, 500);
    // 600, not 300: a candidate is emitted when its 400 ms window closes, not
    // when it is detected, so a shorter feed measures nothing.
    r.feed(2010, 600);  const size_t afterStep = r.got.size();
    r.feed(2010, 60000); const size_t afterDwell = r.got.size();
    r.feed(1900, 2000);
    check(afterStep == 1, "a 110-count step is one candidate");
    check(afterDwell == afterStep,
          "and 60 SECONDS at that level add nothing -- persistence never repeats");
    check(r.got[0].localRef == 1900, "the step's local reference is the pre-step line");
    // The instantaneous return is a second candidate, and under a detector
    // with no global reference it has to be: after 800 ms at 2010 the detector
    // has MEASURED 2010 to be rest, and it has nothing that could tell it 1900
    // is more entitled to be home. That is the price of taking the reference
    // out of the event path, it is bounded, and gate 12 measures the bound.
    check(r.got.size() == 2, "the instantaneous RETURN is a second candidate (see gate 12)"); }

  printf("\n2. a decaying tail is not a new arrival\n");
  { Rig r; r.feed(1900, 500);
    for (int v = 2050; v >= 1900; --v) r.feed((int16_t)v, 8);   // 150 counts over 1.2 s
    r.feed(1900, 1000);
    check(r.got.size() == 1, "one candidate on the rise, none on the 1.2 s decay"); }

  printf("\n3. BOTH POLES detect, including on a shelf of the opposite sign\n");
  { Rig r; r.feed(1900, 500); r.arc(180, 140);  r.feed(1900, 900);
    check(r.got.size() == 1 && r.got[0].polarity == 1, "a positive arc is N");
    Rig s; s.feed(1900, 500); s.arc(-180, 140); s.feed(1900, 900);
    check(s.got.size() == 1 && s.got[0].polarity == 0, "a negative arc is S");
    // the case the 2026-09-15 corpus does not contain at all
    Rig u; u.feed(1900, 500); u.feed(1990, 3000);     // a persistent N shelf
    u.arc(-260, 140, 1990); u.feed(1990, 1200);
    check(u.got.size() == 2, "an S magnet on a 90-count N shelf is a second candidate");
    check(u.got.size() == 2 && u.got[1].polarity == 0, "and it is identified as S"); }

  printf("\n4. the global reference cancels\n");
  { Rig a; a.feed(1900, 500); a.arc(180, 140); a.feed(1900, 900);
    Rig b; b.feed(1900, 500); b.feed(2012, 3000);      // reference now 112 counts wrong
    b.arc(180, 140, 2012); b.feed(2012, 1200);
    check(b.got.size() == 2, "a magnet on a 112-count offset still produces a candidate");
    check(b.got.size() == 2 && a.got.size() == 1 &&
          abs((int)b.got[1].peakCounts - (int)a.got[0].peakCounts) <= 2,
          "and its measured peak matches the un-offset case within 2 counts"); }

  printf("\n5. the refractory is 500 ms from DETECTION, and it is not a level test\n");
  { Rig r; r.feed(1900, 500); r.arc(180, 140); r.feed(1900, 200);
    r.arc(180, 140); r.feed(1900, 900);                // second arc starts at +340 ms
    check(r.got.size() == 1, "a second arc 340 ms after detection is suppressed");
    check(r.det.suppressed() >= 1, "and is counted on suppressed_total, not lost silently");
    Rig s; s.feed(1900, 500); s.arc(180, 140); s.feed(1900, 500);
    s.arc(180, 140); s.feed(1900, 900);                // second arc starts at +640 ms
    check(s.got.size() == 2, "a second arc 640 ms after detection is a second candidate"); }

  printf("\n6. polarity comes from the excursion, not from the whole window\n");
  { // A 140 ms arc inside a 400 ms window: 260 samples of quiet tail follow it.
    Rig r; r.feed(1900, 500); r.arc(180, 140); r.feed(1900, 900);
    check(r.got[0].excursionCount > 20 && r.got[0].excursionCount < 200,
          "the polarity aperture is the arc, not the 400-sample window");
    check(r.got[0].excursionFirst >= r.got[0].preSamples,
          "and it never reaches back into the pre-roll"); }

  printf("\n7. the record carries 512 ms of pre-roll at 1 kHz, undecimated\n");
  { Rig r; r.feed(1900, 2000); r.arc(180, 140); r.feed(1900, 900);
    check(r.got[0].preSamples == 512, "preSamples == 512");
    check(r.got[0].sampleCount == 512 + 1 + 400, "sampleCount == pre + detect + window");
    // L is the trailing sample nearest the measured resting level, so on a
    // quiet line the pre-roll reads zero to within the noise of that level,
    // not exactly zero. One bin is the resolution the histogram works at.
    check(r.got[0].oriented[100] > -8 && r.got[0].oriented[100] < 8,
          "pre-roll samples are the quiet line, L-relative, to within one bin"); }

  printf("\n8. no duration floor: a short event IS a candidate\n");
  { Rig r; r.feed(1900, 500); r.arc(180, 30); r.feed(1900, 1000);
    check(r.got.size() == 1, "a 30 ms arc produces a candidate (X18 refused these)");
    check(r.got.size() == 1 && r.got[0].widthCaliperMs < 82,
          "and reports a width below 82 ms, so the exposure is visible in telemetry"); }

  printf("\n9. a single-sample transient does not\n");
  { Rig r; r.feed(1900, 500);
    for (int i = 0; i < 5; ++i) { r.feed(2100, 1); r.feed(1900, 400); }
    check(r.got.size() == 0, "five 1 ms spikes of 200 counts produce no candidate"); }

  printf("\n10. a declaration mid-magnet does not re-emit that magnet\n");
  { Rig r; r.feed(1900, 500);
    // halfway across a 300 ms arc, the frame ends
    for (int i = 0; i < 150; ++i) {
      const double f = sin(3.14159265 * (double)i / 300.0);
      if (r.det.sample(r.t, (int16_t)(1900 + (int)(200 * f)), true))
        r.got.push_back(r.det.excursion());
      ++r.t;
    }
    const size_t before = r.got.size();
    r.det.reset();
    for (int i = 150; i < 300; ++i) {
      const double f = sin(3.14159265 * (double)i / 300.0);
      if (r.det.sample(r.t, (int16_t)(1900 + (int)(200 * f)), true))
        r.got.push_back(r.det.excursion());
      ++r.t;
    }
    r.feed(1900, 1200);
    check(r.got.size() == before,
          "the rest of the arc yields no candidate under the new frame"); }

  // -------------------------------------------------------------------------
  // 11. THE COUNTEREXAMPLE. Operator's challenge, 2026-09-15.
  //
  // raw - L cancels the reference algebraically, but the first implementation
  // used baseline_ to CHOOSE L, so the reference still held detection
  // authority. Measured on that code, with the reference 112 counts off the
  // resting level and a magnet pulling the signal back toward it:
  //
  //     amplitude   82   100   120   137   173
  //     detected   +134  +127  +125  +127  +130     the arc ends at +140
  //     peak        82   100   112   112   113      clamped at |E|, not A
  //     polarity     N     N     N     N     N      every one of them is S
  //
  // Five of the seven amplitudes in the genuine range, detected on the wrong
  // edge with an inverted pole. A wrong count is recoverable; an inverted pole
  // strikes. This gate is permanent.
  // -------------------------------------------------------------------------
  printf("\n11. a magnet on a resting level displaced from the reference\n");
  {
    const int amps[] = {82, 100, 120, 137, 173, 220, 270};   // the genuine range
    const int offs[] = {112, -112, 45, -45, 0};
    int bad = 0, cells = 0;
    for (int oi = 0; oi < 5; ++oi) {
      const int E = offs[oi];
      for (int ai = 0; ai < 7; ++ai) {
        for (int toward = 0; toward < 2; ++toward) {
          // `toward` pulls the signal back toward the erroneous reference
          const int A = (E >= 0) ? (toward ? -amps[ai] : amps[ai])
                                 : (toward ?  amps[ai] : -amps[ai]);
          Rig r; r.settle((int16_t)(1900 + E));
          const uint32_t arc0 = r.t;
          r.arc(A, 140, (int16_t)(1900 + E));
          r.feed((int16_t)(1900 + E), 1200);
          ++cells;
          const char* why = nullptr;
          if (r.got.size() != 1) why = "candidate count";
          else if ((long)r.got[0].detectedAtMs - (long)arc0 > 70) why = "detected after the apex";
          else if (r.got[0].polarity != (A > 0 ? 1 : 0)) why = "POLARITY INVERTED";
          else if (abs((int)r.got[0].peakCounts - abs(A)) > 12) why = "peak";
          if (why) { ++bad; printf("      E=%+4d A=%+4d : %s (n=%zu pk=%u pol=%c)\n",
                                   E, A, why, r.got.size(),
                                   r.got.empty()?0:r.got[0].peakCounts,
                                   r.got.empty()?'-':(r.got[0].polarity?'N':'S')); }
        }
      }
    }
    char msg[96];
    snprintf(msg, sizeof(msg),
             "%d cells: one candidate, before the apex, right pole, peak within 12", cells);
    check(bad == 0, msg);
  }

  // -------------------------------------------------------------------------
  // 12. THE BOUND on gate 1's second candidate. Measured, not asserted.
  // A resting level that CHANGES produces a candidate only if it changes fast.
  // -------------------------------------------------------------------------
  printf("\n12. how fast a resting level must change before it reads as a magnet\n");
  {
    struct Case { int step, rampMs, want; } cases[] = {
      // The largest resting-level excursion ever recorded is 51 counts
      // (2026-09-15: high state 1964-1987, low state 1936-1959). Nothing that
      // size can produce a candidate at any rate, which is what matters.
      {  30,    1, 0 }, {  30,  100, 0 }, {  30, 1000, 0 },
      {  51,    1, 0 }, {  51,  100, 0 }, {  51, 1000, 0 },
      {  70,    1, 1 }, {  70,   50, 0 },
      { 110,  150, 1 }, { 110,  400, 0 },
      { 150,  400, 1 }, { 150,  600, 0 },
    };
    int bad = 0;
    for (auto& c : cases) {
      Rig r; r.feed((int16_t)(1900 + c.step), 30000);
      const size_t before = r.got.size();
      for (int i = 0; i < c.rampMs; ++i) {
        const int16_t v = (int16_t)(1900 + c.step - (int)((long)c.step * i / c.rampMs));
        if (r.det.sample(r.t, v, true)) r.got.push_back(r.det.excursion());
        ++r.t;
      }
      r.feed(1900, 2000);
      const int got = (int)(r.got.size() - before);
      if (got != c.want) { ++bad;
        printf("      step %d over %d ms: %d candidates, wanted %d\n",
               c.step, c.rampMs, got, c.want); }
    }
    check(bad == 0,
          "<=51 counts never fires; 110 needs <400 ms; 150 needs <600 ms");
  }

  // =======================================================================
  // X20 -- THE STATION DWELL. Gates 13..18.
  // =======================================================================

  printf("\n13. parked in a fringe field declares nothing (Arches, 2026-09-15)\n");
  { Rig a; a.settle(1900); a.stopped = false; archesDwell(a);   // X19's path
    Rig b; b.settle(1900); b.stopped = true;  archesDwell(b);   // X20's
    check(a.got.size() >= 3,
          "X19 manufactures candidates from a parked fringe field");
    check(b.got.empty(),
          "X20 declares nothing across the whole 30 s dwell"); }

  printf("14. the zero ramp is NOT a dwell: deceleration keeps full detection\n");
  { Rig r; r.settle(1900); r.stopped = false;   // twelve seconds of ramp, PWM > 0
    r.feed(1900, 4000); r.arc(200, 300); r.feed(1900, 1200);
    check(r.got.size() == 1, "a magnet crossed during the ramp is one candidate");
    check(r.got.empty() || r.got[0].polarity == 1, "and keeps its pole"); }

  printf("15. PWM reaches 0 mid-window: the measurement ends, it is not stitched\n");
  for (int sign = 1; sign >= -1; sign -= 2) {
    Rig r; r.settle(1900);
    const int16_t held = r.rollInto(200 * sign, 600, 200);
    r.stopped = true;                         // the ramp expires, sensor in field
    r.feed(held, 30000);
    const bool one = r.got.size() == 1;
    check(one, sign > 0 ? "N: exactly one candidate" : "S: exactly one candidate");
    if (one) {
      const Excursion& e = r.got[0];
      check(e.stoppedShort, "  the record says it ended at PWM 0");
      check(e.measuredMs < r.cfg.windowMs,
            "  and is shorter than the 400 ms window it never completed");
      check(e.polarity == (sign > 0 ? 1 : 0),
            "  the pole comes out of the moving samples, correctly");
      check(e.windowEndMs - e.detectedAtMs == e.measuredMs,
            "  no stationary sample was appended to the moving ones");
    } else { fails += 4; }
  }

  printf("16. stopped AT REST: armed on departure, with no blind time\n");
  { Rig r; r.settle(1900);
    r.stopped = true; r.feed(1900, 30000);
    check(r.det.dwellActive() && !r.det.dwellDisplaced(), "classified REST");
    check(r.got.empty(), "nothing declared during the dwell");
    r.stopped = false; r.step(1900);          // DEPART
    check(r.det.armedForNew(), "armed at the departure sample itself");
    check(!r.det.refractory(), "and no refractory was armed to carry across it");
    r.arc(200, 300); r.feed(1900, 1200);
    check(r.got.size() == 1, "the next legitimate magnet is counted"); }

  printf("17. stopped IN THE OLD FIELD: leaving it is not a new magnet\n");
  { Rig r; r.settle(1900);
    const int16_t held = r.rollInto(200, 600, 200);
    r.stopped = true; r.feed(held, 30000);
    const size_t atStop = r.got.size();
    check(r.det.dwellDisplaced(), "classified IN_OLD_FIELD");
    check(atStop == 1, "the magnet encountered while moving was counted, once");
    r.stopped = false; r.step(held);          // DEPART
    check(!r.det.armedForNew(), "not armed: the old field is still under the sensor");
    // leaving it -- the far limb of the same arc, back down to the line
    for (int i = 200; i < 600; ++i) {
      const double f = sin(3.14159265 * (double)i / 600.0);
      r.step((int16_t)(1900 + (int)(200 * f)));
    }
    r.feed(1900, 200);
    check(r.got.size() == atStop, "leaving the old field declares nothing");
    check(r.det.armedForNew(), "OLD_FIELD_CLEAR armed it as the line returned");
    check(!r.det.refractory(), "with no refractory carried across the stop");
    r.arc(200, 300); r.feed(1900, 1200);
    check(r.got.size() == atStop + 1, "and the NEXT magnet is counted"); }

  printf("18. X19's counterexample to 17: the same departure, unguarded\n");
  { Rig r; r.settle(1900);
    const int16_t held = r.rollInto(200, 600, 200);
    r.stopped = false;                        // X19 knows nothing of the stop
    r.feed(held, 30000);
    const size_t atStop = r.got.size();
    for (int i = 200; i < 600; ++i) {
      const double f = sin(3.14159265 * (double)i / 600.0);
      r.step((int16_t)(1900 + (int)(200 * f)));
    }
    r.feed(1900, 400);
    check(r.got.size() > atStop,
          "X19 reads the departure from a parked field as a fresh magnet");
    bool inverted = false;
    for (size_t i = atStop; i < r.got.size(); ++i) if (!r.got[i].polarity) inverted = true;
    check(inverted, "and gives it the opposite pole -- the strike at Arches"); }

  printf("\n%s (%d failures)\n", fails ? "GATE FAILED" : "ALL GATES PASS", fails);
  return fails ? 1 : 0;
}
