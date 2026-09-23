// Grillers regression, 2026-09-10.
//
// The X13 stop was caused by station state arming a pause/stitch path. A
// 139-count passage advanced MM063, then an 80-count fragment 158 ms later was
// stitched, morphology-refused, and allowed to withdraw navigation. Production
// NAVI has no such authority path: acquisition is continuous, and any later
// re-read is an ordinary recognizer refusal that cannot stop or withdraw.
#include <cstdio>
#include "../HallCapture.h"
#include "../Navigator.h"
using namespace navi_one;

static int checks = 0, failures = 0;
static void ck(bool c, const char* what) {
  ++checks;
  if (!c) { ++failures; printf("  FAIL  %s\n", what); }
}

static Passage eventAt(uint32_t opened, uint16_t peak, uint8_t polarity) {
  Passage p;
  p.openedAtMs = opened;
  p.closedAtMs = opened + 140;
  p.peakCounts = peak;
  p.polarity = polarity;
  return p;
}

int main() {
  printf("NAVI_ONE gate 12 — Grillers regression\n");

  printf("\nG1  the observed 158 ms fragment cannot withdraw position\n");
  {
    RecognizerConfig cfg;
    cfg.guardMs = 500;
    cfg.bootstrapGain = 175;                    // Otto's measured starting gain
    MagnetRecognizer rec(cfg);
    Navigator nav;
    nav.declare(62, +1);                       // next physical marker is MM063
    nav.takeResetRequest();

    Passage first = eventAt(1000, 139, polarityAt(63));
    Verdict v1 = rec.examine(first);
    ck(v1.isMagnet, "139-count MM063 passage is admitted");
    ck(nav.judge(first, v1) == Ruling::Advanced, "MM063 advances once");
    ck(nav.status().navMm == 63, "position is MM063");

    Passage reread = eventAt(first.closedAtMs + 158, 80,
                             polarityAt(nextMarker(63, +1)));
    Verdict v2 = rec.examine(reread);
    ck(v2.outcome == Outcome::TooSoon, "158 ms fragment is a re-read");
    ck(nav.judge(reread, v2) == Ruling::NotAMagnet,
       "re-read is discarded before identity");
    ck(nav.status().state == NavState::Declared, "position remains valid");
    ck(nav.status().navMm == 63, "re-read does not advance or withdraw");
    ck(nav.status().refusals == 0, "no identity strike was recorded");
  }

  printf("\nG2  a station dwell inside the field remains one passage\n");
  {
    CaptureConfig cfg;
    HallCapture<> cap(cfg);
    uint32_t t = 0;
    const int16_t base = 1834;
    for (; t < 2500; ++t) cap.sample(t, base, true);
    int closes = 0;
    for (int i = 0; i < 180; ++i, ++t)
      closes += cap.sample(t, base + 140, true) ? 1 : 0;   // arrival
    for (int i = 0; i < 2750; ++i, ++t)
      closes += cap.sample(t, base + 140, false) ? 1 : 0;  // stopped
    for (int i = 0; i < 180; ++i, ++t)
      closes += cap.sample(t, base + 140, true) ? 1 : 0;   // departure
    for (int i = 0; i < 50; ++i, ++t)
      closes += cap.sample(t, base, true) ? 1 : 0;
    ck(closes == 1, "arrival, dwell, and departure close exactly once");
    ck(cap.passage().closedAtMs - cap.passage().openedAtMs > 3000,
       "the passage legitimately spans the dwell");
  }

  printf("\n%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
