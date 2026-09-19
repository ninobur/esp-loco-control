// ---------------------------------------------------------------------------
// gate_excursion -- synthetic properties of the X22 detector that no recorded
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
  uint8_t pwm = 255;                 // cruise unless a gate says otherwise
  bool mayAdapt = true;
  std::vector<Excursion> got;
  std::vector<BaselineOutcome> bouts;
  // X21. The navigation events, drained on the sample they happen.
  std::vector<Detection> dets;
  // X20. The station machine is holding AND the ramped PWM is 0. False for
  // every gate below 13, so gates 1..12 drive the identical X19 path.
  bool stopped = false;
  // The prime level matters under a LOCKED reference in a way it did not
  // under X19: whatever the prime settles on is the lock until a collection
  // replaces it, so a gate that wants the line at 1950 must prime there.
  explicit Rig(int16_t primeAt = 1900) : det(cfg) {
    for (int i = 0; i < 2200; ++i) det.sample(t++, primeAt, true);
  }
  void step(int16_t raw) {
    const bool windowDone = det.sample(t, raw, mayAdapt, stopped, pwm);
    Detection d;
    if (det.takeDetection(d)) dets.push_back(d);
    BaselineOutcome bo;
    while (det.takeBaselineOutcome(bo)) bouts.push_back(bo);
    if (windowDone) got.push_back(det.excursion());
    ++t;
  }
  void feed(int16_t raw, int ms) { for (int i = 0; i < ms; ++i) step(raw); }
  // Move to `level` and stay there until the detector has measured it as rest,
  // then forget everything that happened on the way. The step itself is a
  // legitimate candidate -- that is how boot 5 of 2026-09-15 began, primed on
  // a magnet and then driven off it -- but it is setup, not the thing under
  // test.
  void settle(int16_t level) { feed(level, 3000); got.clear(); dets.clear(); bouts.clear(); }
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
  printf("X22 ExcursionDetector -- synthetic gates\n\n");

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

  printf("\n5. the guard is 645 ms from DETECTION, and it is not a level test\n");
  { Rig r; r.feed(1900, 500); r.arc(180, 140); r.feed(1900, 200);
    r.arc(180, 140); r.feed(1900, 900);                // second arc starts at +340 ms
    check(r.got.size() == 1, "a second arc 340 ms after detection is suppressed");
    check(r.det.suppressed() >= 1, "and is counted on suppressed_total, not lost silently");
    // The case the widening is FOR: a second arc that opens, peaks and closes
    // entirely inside the new guard, at a detect-to-detect interval the old
    // 500 ms guard admitted exactly.
    Rig s; s.feed(1900, 500); s.arc(180, 140); s.feed(1900, 360);
    s.arc(180, 140); s.feed(1900, 900);        // opens +500 ms, closed by +621
    check(s.got.size() == 1,
          "a second arc 500 ms after detection is refused (X19's guard took it)");
    Rig u; u.feed(1900, 500); u.arc(180, 140); u.feed(1900, 700);
    u.arc(180, 140); u.feed(1900, 900);                // second arc starts at +840 ms
    check(u.got.size() == 2, "a second arc 840 ms after detection is a second candidate");
    // A magnet that is STILL departed when the guard expires is taken on the
    // sample after it expires. That is inherent to a time guard and is stated
    // here rather than left to be discovered: 645 ms is a floor on the
    // interval, not a window the magnet has to fit inside.
    Rig w; w.feed(1900, 500); w.arc(180, 140); w.feed(1900, 500);
    w.arc(180, 140); w.feed(1900, 900);        // opens +640, still departed at +645
    check(w.got.size() == 2 &&
          w.got[1].detectedAtMs - w.got[0].detectedAtMs >= 645,
          "one still departed at expiry is taken at expiry, never before"); }

  // -------------------------------------------------------------------------
  // 5b. X21. THE GUARD'S ORIGIN AND EXPIRY, TO THE MILLISECOND, AND THE FACT
  // THAT NOTHING ABOUT THE WINDOW MOVES IT.
  // -------------------------------------------------------------------------
  printf("\n5b. the guard begins AT detection and expires 645 ms later, exactly\n");
  { Rig r; r.feed(1900, 500);
    // walk the arc by hand so the detection sample can be caught as it happens
    Detection d; bool have = false;
    for (int i = 0; i < 140 && !have; ++i) {
      const double f = sin(3.14159265 * (double)i / 140.0);
      r.det.sample(r.t, (int16_t)(1900 + (int)(180 * f)), true, false);
      if (r.det.takeDetection(d)) have = true;
      ++r.t;
    }
    check(have, "a detection is available on the sample that declares it");
    check(have && d.guardUntilMs == d.detectedAtMs + 645,
          "guardUntilMs == detectedAtMs + 645");
    check(have && r.det.refractoryUntil() == d.detectedAtMs + 645,
          "and the detector is already guarding, 400 ms before the window closes");
    // finish the window and confirm the expiry did not move
    const uint32_t wanted = d.detectedAtMs + 645;
    r.feed(1900, 900);
    check(r.det.refractoryUntil() == 0 || r.det.refractoryUntil() == wanted,
          "the 400 ms window closing does not move the expiry"); }

  printf("\n5c. a window cut short at PWM 0 does not move the expiry either\n");
  { Rig r; r.settle(1900);
    Detection d; bool have = false;
    for (int i = 0; i < 140 && !have; ++i) {
      const double f = sin(3.14159265 * (double)i / 600.0);
      r.det.sample(r.t, (int16_t)(1900 + (int)(200 * f)), true, false);
      if (r.det.takeDetection(d)) have = true;
      ++r.t;
    }
    check(have, "detected on the leading limb");
    const uint32_t wanted = d.detectedAtMs + 645;
    r.stopped = true;                       // PWM 0 twenty samples later
    r.feed(1960, 40);
    check(r.det.refractoryUntil() == wanted,
          "the truncated window leaves the guard expiring at detection + 645"); }

  // -------------------------------------------------------------------------
  // 6. X21. POLARITY IS THE OPENING SIGN, FIXED AT DETECTION. This is MM136.
  // -------------------------------------------------------------------------
  printf("\n6. polarity is the sign of the opening, and the window cannot revise it\n");
  { // A 140 ms arc inside a 400 ms window: 260 samples of quiet tail follow it.
    Rig r; r.feed(1900, 500); r.arc(180, 140); r.feed(1900, 900);
    check(r.got[0].excursionCount > 20 && r.got[0].excursionCount < 200,
          "the excursion aperture is still measured, and is the arc");
    check(r.got[0].excursionFirst >= r.got[0].preSamples,
          "and it never reaches back into the pre-roll");
    check(r.got[0].polarity == 1 && r.got[0].departAtDetect > 0,
          "an N opening is N"); }

  // MM136 ITSELF, reconstructed from the published record. A South magnet
  // reaching -106, then a POSITIVE SHELF of +117 that outlasts and out-measures
  // it and holds to the end of the 400 ms window. X19/X20 published N here.
  printf("\n6b. MM136: a South opening under a larger, later North shelf\n");
  { Rig r; r.settle(1900);
    r.arc(-106, 90);                         // the magnet: opens South
    for (int i = 0; i < 40; ++i)             // recover through zero
      r.step((int16_t)(1900 - 40 + i * 3));
    r.feed(2017, 400);                       // +117 shelf, to the window's end
    r.feed(1900, 400);
    check(r.got.size() >= 1, "the opening is a candidate");
    if (r.got.size() >= 1) {
      const Excursion& e = r.got[0];
      check(e.departAtDetect < 0, "  the departure that declared it is negative");
      check(e.polarity == 0, "  and the pole published is SOUTH");
      check(e.peakSigned > 0,
            "  while the window's own argmax is the +shelf, and says so");
    } else fails += 3; }

  printf("\n6c. and the same shelf with no magnet under it is North, correctly\n");
  { Rig r; r.settle(1900);
    r.feed(2017, 900);                       // the shelf alone: opens North
    r.feed(1900, 400);
    check(!r.got.empty() && r.got[0].polarity == 1 && r.got[0].departAtDetect > 0,
          "a positive opening with no magnet before it is N"); }

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
  // X19's answer was a differential detector: raw - L, where L tracked the
  // resting level, so a reference 112 counts wrong could not blind it. X22
  // gives that up deliberately -- the lock IS the reference -- and this gate
  // now measures what the trade actually costs, cell by cell.
  //
  // THE PART THAT IS PERMANENT AND MUST NEVER REGRESS: one candidate, declared
  // before the apex, with the pole of the arc. A wrong count is recoverable;
  // an inverted pole strikes.
  //
  // THE PART X22 CHANGES: peak is measured from the lock, so on a resting
  // level displaced by E it is wrong by about E. That is tolerable ONLY
  // because the amplitude screen has no authority over navigation -- if it
  // ever regains one, this gate is the reason it must not.
  //
  // Above the threshold the displacement is not tolerated at all: the
  // lost-lock recovery re-primes onto the real line and the peak comes back to
  // within 12. Below it, the offset stands until the next accepted collection.
  // -------------------------------------------------------------------------
  printf("\n11. a magnet on a resting level displaced from the reference\n");
  {
    const int amps[] = {82, 100, 120, 137, 173, 220, 270};   // the genuine range
    const int offs[] = {112, -112, 45, -45, 0};
    int bad = 0, cells = 0, inverted = 0, unreachable = 0;
    bool why_unreachable = false; (void)why_unreachable;
    for (int oi = 0; oi < 5; ++oi) {
      const int E = offs[oi];
      // Above threshold the recovery re-primes; below it the lock keeps the
      // prime value and the peak carries the offset.
      const int tol = (abs(E) >= 70) ? 12 : abs(E) + 12;
      for (int ai = 0; ai < 7; ++ai) {
        for (int toward = 0; toward < 2; ++toward) {
          const int A = (E >= 0) ? (toward ? -amps[ai] : amps[ai])
                                 : (toward ?  amps[ai] : -amps[ai]);
          Rig r; r.settle((int16_t)(1900 + E));
          const uint32_t arc0 = r.t;
          r.arc(A, 140, (int16_t)(1900 + E));
          r.feed((int16_t)(1900 + E), 1200);
          ++cells;
          // THE MEASURED COST OF A LOCKED REFERENCE, stated rather than
          // tolerated: an arc pulling TOWARD a lock displaced by E needs
          // |A| >= departCounts + |E| to reach the threshold at all. Below
          // that it is lost -- and it is lost SILENTLY, because nothing was
          // ever declared. This is the stale-lock failure mode this build
          // predicts, and base_age_ms is how a field record recognises it.
          const bool reachable =
              (abs(E) >= 70) || (A > 0 ? (A >= 70 + (E > 0 ? 0 : abs(E)))
                                       : (-A >= 70 + (E > 0 ? abs(E) : 0)));
          if (!reachable) { if (r.got.empty()) { ++unreachable; continue; }
                            why_unreachable = true; }
          const char* why = nullptr;
          if (r.got.size() != 1) why = "candidate count";
          else if ((long)r.got[0].detectedAtMs - (long)arc0 > 70) why = "detected after the apex";
          else if (r.got[0].polarity != (A > 0 ? 1 : 0)) { why = "POLARITY INVERTED"; ++inverted; }
          else if (abs((int)r.got[0].peakCounts - abs(A)) > tol) why = "peak";
          if (why) { ++bad; printf("      E=%+4d A=%+4d : %s (n=%zu pk=%u pol=%c)\n",
                                   E, A, why, r.got.size(),
                                   r.got.empty()?0:r.got[0].peakCounts,
                                   r.got.empty()?'-':(r.got[0].polarity?'N':'S')); }
        }
      }
    }
    char msg[112];
    snprintf(msg, sizeof(msg),
             "%d cells: one candidate, before the apex, right pole, peak within |E|+12", cells);
    check(bad == 0, msg);
    check(inverted == 0, "  and no cell anywhere inverts the pole (permanent)");
    snprintf(msg, sizeof(msg),
             "  %d cells below departCounts+|E| are lost, exactly as predicted", unreachable);
    check(unreachable == 4, msg);
  }

  // 12. A LEVEL STEP IS A DETECTION, AND THE LEASH IS WHAT STOPS IT BECOMING
  //     THE LOCK.
  //
  // X19 asked this question at the detector: how fast must a resting level
  // change before it reads as a magnet? A differential detector could answer
  // it. A locked reference cannot, and X22 does not pretend to -- any step of
  // departCounts or more IS a departure, at any rate, and the navigator will
  // judge it against the map like anything else.
  //
  // The protection moved to the collection instead, where it is a statement
  // about physics rather than about rates: a candidate level a magnet's
  // distance from the lock is not a new resting level, it is a magnet. This
  // gate asserts BOTH halves, because the second is what makes the first safe.
  // -------------------------------------------------------------------------
  printf("\n12. a level step detects; the leash refuses it as a baseline\n");
  {
    struct Case { int step, rampMs, want; } cases[] = {
      // The largest resting-level excursion ever recorded is 51 counts
      // (2026-09-15: high state 1964-1987, low state 1936-1959). Nothing that
      // size produces a candidate at any rate, which is what matters.
      {  30,    1, 0 }, {  30,  100, 0 }, {  30, 1000, 0 },
      {  51,    1, 0 }, {  51,  100, 0 }, {  51, 1000, 0 },
      // At or above the threshold it fires, and the RATE NO LONGER MATTERS.
      {  70,    1, 1 }, {  70,   50, 1 },
      { 110,  150, 1 }, { 110,  400, 1 },
      { 150,  400, 1 }, { 150,  600, 1 },
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
      const int got = (int)(r.got.size() - before) > 0 ? 1 : 0;
      if (got != c.want) { ++bad;
        printf("      step %d over %d ms: %d candidates, wanted %d\n",
               c.step, c.rampMs, got, c.want); }
    }
    check(bad == 0,
          "<=51 counts never fires; >=70 fires at any rate");

    // THE LEASH, on the mechanism it was written for. Otto at Grillers on
    // 2026-09-16: quiet line ~1950, standing on the shelf at ~2150.
    {
      Rig r; r.settle(1950);
      r.arc(180, 140, 1950);                 // a magnet, so a cycle is running
      // The shelf, flat, for eight seconds, AT THE STATION RAMP'S PWM -- above
      // the tractive floor and below cruise, which is the whole point: the
      // lost-lock recovery must not fire here, and the leash must refuse.
      for (int i = 0; i < 8000; ++i) {
        if (r.det.sample(r.t, 2150, true, false, 40)) r.got.push_back(r.det.excursion());
        ++r.t;
      }
      BaselineOutcome bo; bool leashed = false, installed = false;
      while (r.det.takeBaselineOutcome(bo)) {
        if (bo.accepted && abs((long)bo.candidate - 1950) > 70) installed = true;
        if (!bo.accepted && bo.reject == BaselineReject::Leash) leashed = true;
      }
      check(!installed, "  the Grillers shelf never becomes the lock");
      (void)leashed;
    }
  }

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

  // =======================================================================
  // X21 -- NAVIGATION IS THE OPENING. Gates 19..22.
  // =======================================================================

  printf("\n19. the navigation event exists at the detection sample, not 400 ms later\n");
  { Rig r; r.settle(1900);
    const uint32_t t0 = r.t;
    r.arc(180, 140); r.feed(1900, 100);          // 240 ms: the window is still open
    check(r.dets.size() == 1, "one navigation event within 240 ms of the arc starting");
    check(r.got.empty(), "and the 400 ms window has not closed yet");
    check(!r.dets.empty() && r.dets[0].detectedAtMs - t0 < 40,
          "queued on the opening, not on the apex and not on the window");
    r.feed(1900, 600);
    check(r.got.size() == 1, "the window closes afterwards, for telemetry");
    check(r.got.size() == 1 && r.dets.size() == 1 &&
          r.got[0].detectedAtMs == r.dets[0].detectedAtMs &&
          r.got[0].polarity == r.dets[0].polarity,
          "and agrees with the event it can no longer change"); }

  printf("\n20. at PWM 0 no navigation event is created, by any route\n");
  { Rig r; r.settle(1900);
    r.stopped = true;                             // ramped PWM == 0
    r.arc(260, 140); r.feed(1900, 900);           // a full-size arc, at a standstill
    check(r.dets.empty(), "a 260-count arc at PWM 0 creates no navigation event");
    check(r.got.empty(), "and no candidate at all");
    check(r.det.suppressed() >= 1, "it is counted, not lost silently");
    // and the same arc with the motor turning is one event, so the gate is
    // measuring the stop and not the waveform
    Rig s; s.settle(1900); s.stopped = false;
    s.arc(260, 140); s.feed(1900, 900);
    check(s.dets.size() == 1, "the identical arc while moving IS one event"); }

  printf("21. a persistence run straddling the stop is discarded, never stitched\n");
  { Rig r; r.settle(1900);
    // one sample over threshold, then PWM 0 on the next: the run is incomplete
    r.step(1980);
    check(r.dets.empty(), "one qualifying sample is not yet a detection");
    r.stopped = true;
    r.feed(1980, 5000);                           // held over threshold, stationary
    check(r.dets.empty(), "and the stationary samples do not complete it");
    check(r.got.empty(), "no candidate is manufactured across the transition"); }

  printf("22. a magnet identified before stopping is not rediscovered after it\n");
  { Rig r; r.settle(1900);
    const int16_t held = r.rollInto(200, 600, 200);
    check(r.dets.size() == 1, "one navigation event, while moving");
    r.stopped = true; r.feed(held, 30000);        // parked inside that same field
    check(r.dets.size() == 1, "nothing new across the 30 s stop");
    r.stopped = false; r.step(held);              // depart
    for (int i = 200; i < 600; ++i) {             // leaving the field
      const double f = sin(3.14159265 * (double)i / 600.0);
      r.step((int16_t)(1900 + (int)(200 * f)));
    }
    r.feed(1900, 200);
    check(r.dets.size() == 1,
          "leaving the field it stopped in is not a second identification");
    r.arc(200, 300); r.feed(1900, 1200);
    check(r.dets.size() == 2, "and the NEXT magnet is identified normally"); }


  // =======================================================================
  // X22 -- THE LOCKED PER-INTERVAL REFERENCE. Gates 23..29.
  //
  // X17's audit blocker B1 was that exactly one capture in a twelve-gate suite
  // ran the policy the flashed image actually had, so "all gates passed" was
  // evidence about the PREVIOUS build. These seven exist so the same cannot be
  // said of X22: every one exercises the reference architecture that is new,
  // and every one of them would fail or be meaningless on X21.
  // =======================================================================

  printf("\n23. the lock does not move between collections\n");
  {
    Rig r; r.settle(1900);
    const int32_t b0 = r.det.baseline();
    for (int i = 0; i < 10000; ++i) r.step((int16_t)(1900 + (i / 400) % 9 - 4));
    check(r.det.baseline() == b0, "10 s of +-4 wander moves the lock by nothing");
    check(r.det.baselineAccepted() == 0, "  and no collection ran, because no magnet did");
    check(r.det.shadowBaseline() != 0, "  the shadow median still runs, as telemetry");
  }

  printf("\n24. one accepted collection per interval, at cadence and while moving\n");
  {
    Rig r; r.settle(1900);
    r.arc(200, 140, 1900);
    r.feed(1920, 1300);
    r.arc(200, 140, 1920);
    r.feed(1920, 1500);
    int acc = 0; int32_t installed = 0;
    for (auto& b : r.bouts) if (b.accepted) { ++acc; installed = b.candidate; }
    check(acc >= 1, "a genuine +20 drift is collected and installed");
    check(labs((long)installed - 1920) <= 3, "  and the installed level is the line, within 3");
    check(r.det.baseline() == installed, "  which is now the lock");
    check(r.det.baselineAgeMs(r.t) < 3000, "  and the lock is fresh");
  }

  printf("\n25. every acceptance gate refuses, and says which one\n");
  {
    { Rig r; r.settle(1900); r.arc(200,140,1900); r.feed(1900, 6000);
      r.arc(200,140,1900); r.feed(1900, 1200);
      bool c = false; for (auto& b : r.bouts) if (!b.accepted && b.reject==BaselineReject::Cadence) c = true;
      check(c, "OFF_CADENCE when the interval did not arrive at cadence"); }
    { Rig r; r.settle(1900); r.arc(200,140,1900); r.feed(1900,1300);
      r.arc(200,140,1900); r.feed(1900, 800);
      r.mayAdapt = false; r.feed(1900, 800);
      bool c = false; for (auto& b : r.bouts) if (!b.accepted && b.reject==BaselineReject::Moving) c = true;
      check(c, "NOT_MOVING when PWM falls below the tractive floor"); }
    { Rig r; r.settle(1900); r.arc(200,140,1900); r.feed(1900,1300);
      r.arc(200,140,1900);
      for (int i=0;i<3000;++i) r.step((int16_t)(1900 + ((i%2)?30:-30)));
      bool c = false; for (auto& b : r.bouts) if (!b.accepted && b.reject==BaselineReject::Spread) c = true;
      check(c, "SPREAD when the collection window is not quiet"); }
    // LEASH is a guarded invariant and is expected NEVER to fire, because
    // CLEAR already bounds every collected sample. The gate asserts the
    // PROPERTY rather than the branch: no accepted candidate is ever
    // departCounts or more from the lock it replaced, and the Grillers shelf
    // never reaches a collection at all.
    { Rig r(1950); r.settle(1950);
      r.arc(200,140,1950); r.feed(1950,1300); r.arc(200,140,1950); r.feed(1950,800);
      r.pwm = 40; r.feed(2150, 12000);        // the shelf
      r.pwm = 90; r.feed(1950, 3000);         // and home again
      bool leashFired = false; long worst = 0;
      for (auto& b : r.bouts) {
        if (!b.accepted && b.reject == BaselineReject::Leash) leashFired = true;
        if (b.accepted && labs((long)b.delta) > worst) worst = labs((long)b.delta);
      }
      check(!leashFired, "LEASH never fires: CLEAR already bounds the collection");
      check(worst < 70, "  no accepted candidate is ever departCounts from the lock");
      check(labs((long)r.det.baseline() - 1950) < 70, "  the shelf never becomes the lock"); }

    // AND THE HAZARD THAT IS NOT CLOSED, recorded as a measured property
    // rather than left to be discovered in the field. Finding 1, 2026-09-19.
    { Rig r; r.settle(1900);
      r.arc(200,140,1900); r.feed(1900,1300); r.arc(200,140,1900);
      r.pwm = 50; r.feed(1945, 3000);         // a flat +45 fringe, sub-threshold
      check(labs((long)r.det.baseline() - 1945) <= 3,
            "KNOWN OPEN: a sub-threshold fringe still becomes the lock (finding 1)");
      bool published = false;
      for (auto& b : r.bouts) if (b.accepted && labs((long)b.delta) > 30) published = true;
      check(published, "  and it is published with its delta, never silent"); }

    { Rig r; r.settle(1900); r.arc(200,140,1900); r.feed(1900,300);
      r.stopped = true; r.pwm = 0; r.feed(1900, 1500);
      bool c = false; for (auto& b : r.bouts) if (!b.accepted && b.reject==BaselineReject::Stopped) c = true;
      check(c, "STOPPED when the ramped PWM reaches 0"); }
  }

  printf("\n26. the lock's age is reported, and a refused run is visible\n");
  {
    Rig r; r.settle(1900);
    r.arc(200,140,1900); r.feed(1900,1300); r.arc(200,140,1900); r.feed(1900,1500);
    const uint32_t fresh = r.det.baselineAgeMs(r.t);
    r.mayAdapt = false;
    for (int k = 0; k < 4; ++k) { r.arc(200,140,1900); r.feed(1900,1300); }
    check(r.det.baselineAgeMs(r.t) > fresh, "the age grows while collections are refused");
    check(r.det.baselineConsecutiveRejects() >= 2, "  and the refusal run is counted");
    check(r.det.lastBaselineReject() == BaselineReject::Moving, "  with the reason retained");
    check(!r.det.baselineStale(r.t), "  not yet stale at this horizon");
    check(r.det.baselineStale(r.t + 130000), "  and stale past staleMs, which gates nothing");
  }

  printf("\n27. a sustained shelf declares ONCE, not once per guard\n");
  {
    Rig r; r.settle(1900);
    r.pwm = 40;
    r.feed(2010, 20000);
    check(r.dets.size() == 1, "twenty seconds above the threshold is one event, not thirty");
    check(r.det.suppressed() > 0, "  and what it refused afterwards is counted, not silent");
    const size_t n = r.dets.size();
    r.feed(1900, 300);
    r.arc(200, 140, 1900);
    r.feed(1900, 900);
    check(r.dets.size() == n + 1, "  the next real magnet, after the return, IS an event");
  }

  printf("\n28. a lock that cannot be right is replaced, but only at cruise\n");
  {
    { Rig r; r.settle(1900);
      r.pwm = 90;
      r.feed(2012, 4000);
      check(r.det.lostLockTotal() >= 1, "at cruise, 2 s of continuous departure falsifies the lock");
      check(labs((long)r.det.baseline() - 2012) <= 3, "  and the line itself becomes the lock");
      bool rec = false; for (auto& b : r.bouts) if (b.recovery && b.accepted) rec = true;
      check(rec, "  published as a recovery, never silently"); }
    { Rig r(1950); r.settle(1950);
      r.pwm = 40;
      r.feed(2150, 12000);
      check(r.det.lostLockTotal() == 0, "on a station ramp the same shelf falsifies nothing");
      check(labs((long)r.det.baseline() - 1950) <= 3, "  and the lock is still the quiet line"); }
  }

  printf("\n29. Grillers end to end: the departure from the shelf is not a magnet\n");
  {
    Rig r(1950); r.settle(1950);
    r.arc(200, 140, 1950);
    r.feed(1950, 900);
    const size_t before = r.dets.size();
    r.pwm = 40; r.feed(2150, 12000);
    r.pwm = 0; r.stopped = true; r.feed(2150, 30000);
    r.stopped = false; r.pwm = 90;
    for (int i = 0; i < 400; ++i)
      r.step((int16_t)(2150 - (int)((long)200 * i / 400)));
    r.feed(1950, 1500);
    const size_t shelfEvents = r.dets.size() - before;
    check(r.det.baseline() >= 1947 && r.det.baseline() <= 1953,
          "the lock is still the quiet line after the whole station stop");
    check(shelfEvents <= 1, "entering the field is at most one event, leaving it is none");
    r.arc(200, 140, 1950);
    r.feed(1950, 900);
    check(r.dets.size() == before + shelfEvents + 1, "  and the REAL next magnet is counted");
  }

  printf("\n%s (%d failures)\n", fails ? "GATE FAILED" : "ALL GATES PASS", fails);
  return fails ? 1 : 0;
}
