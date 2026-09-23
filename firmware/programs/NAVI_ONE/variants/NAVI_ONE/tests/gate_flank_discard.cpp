// Gate 14: a rising reading is not a false start.
//
// THE DEFECT UNDER TEST
// ---------------------
// HallCapture's discard branch throws away a just-opened passage as a false
// start when, with a stop armed, the field reads as a plateau and the passage
// has shown no progress. The plateau verdict is refreshed once every 25 ms
// from a sixteen-sample window with its extremes trimmed. On a genuine flank
// the one rising sample in that window is the trimmed one, so for up to 25 ms
// after the flank begins the verdict is STALE: it still says "flat". The
// branch fired on every sample in that interval, the passage reopened on the
// next sample, and the pre-roll ring -- which fills only while no passage is
// open -- froze. The surviving record began mid-flank glued to stale pre-roll.
//
// Six Arches CW departures of 2026-09-03 (NAVI_ONE_STATION_CURVES_0_1) carry
// that signature: pre-roll to first-sample jumps of 35, 62, 74, 66, 46 and
// 103 counts, residuals 0.1009 to 0.1372. The 103-count record was refused as
// WRONG_SHAPE and stopped the locomotive at MM110 -> MM111.
//
// THE FIX UNDER TEST
// ------------------
// One added condition: the branch may discard only while the CURRENT reading
// is itself below entryMargin. A passage still reading at or above the
// opening threshold was not opened by an excursion that has gone.
//
// WHAT IS REAL HERE, AND WHAT IS NOT
// ----------------------------------
// REAL: HallCapture and MagnetRecognizer are the firmware's, compiled
//       unmodified. The six records in fixtures_arches_departures.h are the
//       samples the firmware published, verbatim.
// NOT:  the ADC, and the flank the firmware never recorded. Section B rebuilds
//       the missing flank as a straight line at the slope the record itself
//       shows over its first four retained samples. A real flank curves; the
//       gate is about whether the capture keeps what it is given, so a line
//       is sufficient and is declared.
// The baseline is held fixed (mayAdapt false) through the armed section, as
// it is for a locomotive below the adaptation PWM. During a real departure the
// ramp crosses that PWM and the baseline adapts; that is not modelled here.
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "../LL_LocoConfig_9950012.h"
#include "../RouteMap.h"
#include "../HallCapture.h"
#include "../MagnetRecognizer.h"
#include "fixtures_arches_departures.h"
using namespace navi_one;

static int checks = 0, failures = 0;
static void ok(bool c, const char* what, const std::string& d = "") {
  ++checks;
  if (!c) { ++failures; printf("  *** FAIL  %s %s\n", what, d.c_str()); }
}
static const int16_t IDLE = 1830;

struct Rig {
  CaptureConfig    ccfg;
  RecognizerConfig rcfg;
  HallCapture<512> cap{ccfg};
  MagnetRecognizer rec{rcfg};
  uint32_t t = 0;
  bool     fired = false;
  Passage  last{};
  Verdict  verdict{};
  uint32_t firstCrossMs = 0;   // first sample at or above entryMargin, by the stimulus

  void prime() {
    for (int i = 0; i < 3000; ++i, ++t) cap.sample(t, IDLE, true, StopArming::None);
    fired = false; firstCrossMs = 0;
  }
  // One stimulus sample, baseline-relative. Returns true when a passage closed.
  bool feed(int rel, StopArming arm) {
    if (!firstCrossMs && std::abs(rel) >= ccfg.entryMargin) firstCrossMs = t;
    const bool f = cap.sample(t, (int16_t)(IDLE + rel), false, arm);
    ++t;
    if (f && cap.event() == HallEvent::Passage) {
      fired = true; last = cap.passage(); verdict = rec.examine(last);
    }
    return f;
  }
  void rest(int rel, int ms, StopArming arm) { for (int i = 0; i < ms; ++i) feed(rel, arm); }
};

// The largest step between consecutive retained samples over [from, to).
static int maxStep(const int16_t* v, int from, int to) {
  int m = 0;
  for (int i = from + 1; i < to; ++i) { const int d = std::abs((int)v[i] - (int)v[i - 1]); if (d > m) m = d; }
  return m;
}

int main() {
  printf("gate 14: a rising reading is not a false start\n");
  printf("   entryMargin %d  settleStepMs %u  settleWindowMs %u  settleSpan %u\n\n",
         (int)CaptureConfig{}.entryMargin, (unsigned)CaptureConfig{}.settleStepMs,
         (unsigned)CaptureConfig{}.settleWindowMs, (unsigned)CaptureConfig{}.settleSpan);

  // -------------------------------------------------------------------------
  printf("A. a synthetic flank opens while the plateau verdict is stale\n");
  // -------------------------------------------------------------------------
  {
    static const double SLOPES[] = { 1.0, 2.5, 3.25, 5.0 };
    for (double slope : SLOPES) {
      Rig rg; rg.prime();
      // Stationary, armed for departure, long enough for the plateau window
      // to fill and read flat: the verdict is now true and will be stale the
      // moment the field moves.
      rg.rest(0, 1200, StopArming::Departing);
      const uint32_t discardsBefore = rg.cap.discards();
      // A straight flank from the baseline to a 210-count peak, then a
      // Gaussian fall so the passage closes normally.
      const int peak = 210;
      const int riseMs = (int)std::lround(peak / slope);
      for (int i = 1; i <= riseMs && !rg.fired; ++i) rg.feed((int)std::lround(i * slope), StopArming::Departing);
      for (int i = 0; i < 400 && !rg.fired; ++i) {
        const double z = i / 30.0;
        rg.feed((int)std::lround(peak * std::exp(-0.5 * z * z)), StopArming::Departing);
      }
      rg.rest(0, 100, StopArming::Departing);
      const int discards = (int)(rg.cap.discards() - discardsBefore);
      char tag[96]; snprintf(tag, sizeof tag, "(slope %.2f counts/ms)", slope);
      ok(rg.fired, "the flank closes as a passage", tag);
      if (!rg.fired) continue;
      const Passage& p = rg.last;
      const int pre = p.preSamples;
      const int jump = std::abs((int)p.oriented[pre] - (int)p.oriented[pre - 1]);
      const int flank = maxStep(p.oriented, 0, pre + 20);
      const int allow = (int)std::ceil(slope) + 2;
      printf("   slope %.2f/ms  opened %+ld ms from the crossing  discards %d  pre %d  "
             "pre->flank jump %3d  max step in first 20 %3d  n %3u  resid %.4f\n",
             slope, (long)p.openedAtMs - (long)rg.firstCrossMs, discards, pre, jump, flank,
             (unsigned)p.sampleCount, (double)rg.verdict.residual);
      ok(discards == 0,                       "no sample of a rising flank is discarded", tag);
      ok(p.openedAtMs == rg.firstCrossMs,      "the passage opens on the entry crossing", tag);
      ok(pre == 12,                            "the pre-roll is complete", tag);
      ok(jump <= allow,                        "pre-roll joins the flank without a step", tag);
      ok(flank <= allow,                       "the rising flank is continuous", tag);
    }
  }

  // -------------------------------------------------------------------------
  printf("\nB. the six Arches CW departures, flank restored, through the capture\n");
  printf("   the record as published is fed first (control); then the same record\n");
  printf("   with its missing flank rebuilt at the slope of its first retained samples\n\n");
  // -------------------------------------------------------------------------
  for (int k = 0; k < ARCHES_DEPARTURE_COUNT; ++k) {
    const ArchesDeparture& d = *ARCHES_DEPARTURES[k];
    const int pre = 12;
    const int restLevel = d.v[pre - 1];
    const double slope = (d.v[pre + 4] - d.v[pre]) / 4.0;
    const int gap = d.v[pre] - d.v[pre - 1];
    const int missing = (int)std::lround(gap / slope) - 1;
    char tag[64]; snprintf(tag, sizeof tag, "(seq %d)", d.seq);

    // CONTROL: the amputated record itself. Rest at the pre-roll level long
    // enough for the plateau verdict to be stale, then straight into the
    // first retained sample -- the step the firmware recorded. The recognizer
    // here must agree with the recognizer in the field.
    Rig c; c.prime();
    c.rest(restLevel, 1200, StopArming::Departing);
    for (int i = pre; i < d.n && !c.fired; ++i) c.feed(d.v[i], StopArming::Departing);
    c.rest(0, 100, StopArming::Departing);
    ok(c.fired, "control: the published record closes as a passage", tag);
    const double controlResid = c.fired ? c.verdict.residual : -1.0;
    if (c.fired)
      ok(std::fabs(controlResid - d.residual) < 0.006,
         "control: the harness judges the published record at the published residual", tag);

    // RESTORED: the same record with `missing` samples of straight flank
    // between the last pre-roll sample and the first retained one.
    Rig r; r.prime();
    r.rest(restLevel, 1200, StopArming::Departing);
    const uint32_t discardsBefore = r.cap.discards();
    for (int i = 1; i <= missing && !r.fired; ++i)
      r.feed(restLevel + (int)std::lround(i * slope), StopArming::Departing);
    for (int i = pre; i < d.n && !r.fired; ++i) r.feed(d.v[i], StopArming::Departing);
    r.rest(0, 100, StopArming::Departing);
    const int discards = (int)(r.cap.discards() - discardsBefore);
    ok(r.fired, "restored: the record closes as a passage", tag);
    if (!r.fired) continue;
    const Passage& p = r.last;
    const int jump  = std::abs((int)p.oriented[p.preSamples] - (int)p.oriented[p.preSamples - 1]);
    const int flank = maxStep(p.oriented, 0, p.preSamples + missing + 4);
    const int allow = (int)std::ceil(slope) + 2;
    printf("   seq %4d  published jump %3d resid %.4f %-8s | control resid %.4f | "
           "restored: %2d samples at %.2f/ms  discards %d  opened %+ld ms  jump %2d  max step %2d  n %3u  resid %.4f %s\n",
           d.seq, gap, (double)d.residual, d.accepted ? "accepted" : "REFUSED",
           controlResid, missing, slope, discards, (long)p.openedAtMs - (long)r.firstCrossMs,
           jump, flank, (unsigned)p.sampleCount, (double)r.verdict.residual,
           r.verdict.isMagnet ? "accepted" : "REFUSED");
    ok(discards == 0,                   "restored: nothing is discarded", tag);
    ok(p.openedAtMs == r.firstCrossMs,   "restored: the passage opens on the entry crossing", tag);
    ok(jump <= allow,                    "restored: no discard-created step at the pre-roll boundary", tag);
    ok(flank <= allow,                   "restored: the rising flank is continuous", tag);
    ok(r.verdict.residual < d.residual,  "restored: the whole flank fits better than the amputated one", tag);
  }

  // -------------------------------------------------------------------------
  printf("\nC. what the discard is FOR still happens, and what it must not touch is untouched\n");
  // -------------------------------------------------------------------------
  {
    // Bamboo CCW, 2026-09-02 12:29: at rest in a 30-count fringe, one sample
    // of artifact over the threshold. The opening is a false start and must
    // still be discarded, not left open through the dwell.
    Rig rg; rg.prime();
    rg.rest(30, 1200, StopArming::Decelerating);
    const uint32_t before = rg.cap.discards();
    rg.feed(45, StopArming::Decelerating);
    rg.rest(30, 600, StopArming::Decelerating);
    printf("   Bamboo: fringe 30, one-sample artifact 45 -> discards %u  open %d  passage %d\n",
           (unsigned)(rg.cap.discards() - before), rg.cap.open() ? 1 : 0, rg.fired ? 1 : 0);
    ok(rg.cap.discards() > before, "a one-sample artifact opening in a fringe is still discarded");
    ok(!rg.cap.open(),             "and nothing stays open through the dwell");
    ok(!rg.fired,                  "and no passage reaches the recognizer");
  }
  {
    // Arches CCW, 2026-09-02 14:14: at rest INSIDE a magnet at 88 counts,
    // the passage opened by the field itself. The guard already leaves this
    // passage open (the field accounts for the opening); the fix must not
    // close or discard it. Known fault, deliberately untouched.
    Rig rg; rg.prime();
    const uint32_t before = rg.cap.discards();
    rg.rest(88, 5000, StopArming::Decelerating);
    printf("   Arches CCW: rest inside a magnet at 88 -> discards %u  open %d  passage %d\n",
           (unsigned)(rg.cap.discards() - before), rg.cap.open() ? 1 : 0, rg.fired ? 1 : 0);
    ok(rg.cap.open(),  "resting inside a real field stays open through the dwell (the known fault, unchanged)");
    ok(!rg.fired,      "and nothing is handed to the recognizer while stationary");
  }
  {
    // An excursion that opened and went while stationary: reading rises to 60
    // over 20 ms, falls back to 10 and stays. Below entryMargin and flat
    // again: the opening was a false start and is discarded.
    Rig rg; rg.prime();
    rg.rest(10, 1200, StopArming::Departing);
    const uint32_t before = rg.cap.discards();
    for (int i = 1; i <= 20; ++i) rg.feed(10 + i * 3, StopArming::Departing);
    // fall back below exitMargin? No -- to 30, above exit (25), below entry (38)
    for (int i = 20; i >= 0; --i) rg.feed(30 + i, StopArming::Departing);
    rg.rest(30, 800, StopArming::Departing);
    printf("   excursion gone: rose to 70, settled at 30 -> discards %u  open %d  passage %d\n",
           (unsigned)(rg.cap.discards() - before), rg.cap.open() ? 1 : 0, rg.fired ? 1 : 0);
    ok(rg.cap.discards() > before, "an excursion that has gone is still discarded once the reading is back below entryMargin");
    ok(!rg.cap.open(),             "and the passage does not stay open");
  }

  printf("\n%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
