// ---------------------------------------------------------------------------
// gate_excursion -- synthetic properties of the X19 detector that no recorded
// corpus can demonstrate, because September 15 contains no instance of them.
// Each one is a claim made in ExcursionDetector.h, checked directly.
// Build: g++ -O2 -std=c++17 -o /tmp/x19gate gate_excursion.cpp && /tmp/x19gate
// ---------------------------------------------------------------------------
#include <cstdio>
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
  Rig() : det(cfg) { for (int i = 0; i < 2200; ++i) det.sample(t++, 1900, true); }
  void feed(int16_t raw, int ms) {
    for (int i = 0; i < ms; ++i) {
      if (det.sample(t, raw, true)) got.push_back(det.excursion());
      ++t;
    }
  }
  // a half-sine arc of the given amplitude and width, on top of `base`
  void arc(int amp, int ms, int16_t base = 1900) {
    for (int i = 0; i < ms; ++i) {
      const double f = sin(3.14159265 * (double)i / (double)ms);
      const int16_t v = (int16_t)(base + (int)(amp * f));
      if (det.sample(t, v, true)) got.push_back(det.excursion());
      ++t;
    }
  }
};

int main() {
  printf("X19 ExcursionDetector -- synthetic gates\n\n");

  printf("1. a persistent displaced level does not manufacture events\n");
  { Rig r; r.feed(1900, 500); r.feed(2010, 60000); r.feed(1900, 2000);
    check(r.got.size() == 1, "a 110-count step held for 60 s yields exactly one candidate");
    check(r.got[0].localRef == 1900, "that candidate's local reference is the pre-step line"); }

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
    check(r.got[0].oriented[100] == 0, "pre-roll samples are the quiet line, L-relative"); }

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

  printf("\n%s (%d failures)\n", fails ? "GATE FAILED" : "ALL GATES PASS", fails);
  return fails ? 1 : 0;
}
