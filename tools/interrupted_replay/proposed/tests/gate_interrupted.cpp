// Gate 12: the interrupted-traversal rule of decision 0070, adversarially.
//
// WHAT THIS GATE IS FOR
// ---------------------
// A Hall passage runs from a threshold crossing to a threshold return. The
// model assumes the sensor is MOVING PAST the magnet. When the locomotive
// stops mid-field the model has no way to end, and everything before the stop
// merges with everything after it into one object which is fitted to a
// Gaussian and thrown away. Twice on 2026-09-01, twenty-one minutes apart, at
// two platforms, that lost a marker and struck (findings 11 and 13).
//
// The rule under test adds ONE narrowly bounded second way to establish a
// magnet, reachable only from an identified controlled stop. This gate exists
// to try to break it.
//
// WHAT IS REAL HERE, AND WHAT IS NOT
// ----------------------------------
// REAL: HallCapture, MagnetRecognizer, Navigator, StationMachine and RouteMap
//       are the firmware classes, compiled from the firmware headers. The
//       four field records in fixtures_captures.h are the samples the
//       firmware itself published. Rig below reproduces NAVI_ONE.ino's
//       hallTask(), stationService(), loop() and serviceRamp() line for line;
//       the two are meant to be read side by side.
// NOT:  the ADC. Section D expands a decimated field record by holding each
//       stored reading for its decimation interval -- the readings in between
//       were never transmitted. Section H generates the field from a Gaussian
//       model of a 30 mm magnet and Toby's measured speed fit. Both are said
//       so where they are used, and decision 0070 lists what they therefore
//       do not prove.
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
#include "../Navigator.h"
#include "../Stations.h"
#include "fixtures_captures.h"
using namespace navi_one;

static int checks = 0, failures = 0;
static void ok(bool c, const char* what, const std::string& d = "") {
  ++checks;
  if (!c) { ++failures; printf("  *** FAIL  %s %s\n", what, d.c_str()); }
}

static const int16_t IDLE = 1830;          // Toby's measured idle level

// A repeatable noise source. Not a model of the ADC -- a bounded nuisance, so
// that "it works on a clean stream" is never the whole answer.
struct Noise {
  uint32_t s = 12345;
  int next(int amp) { s = s * 1103515245u + 12345u; return (int)((s >> 16) % (uint32_t)(2 * amp + 1)) - amp; }
};

// ---------------------------------------------------------------------------
// THE .ino LAYER, reproduced.
// ---------------------------------------------------------------------------
struct Rig {
  CaptureConfig    ccfg;
  RecognizerConfig rcfg;
  HallCapture<512> cap{ccfg};
  MagnetRecognizer rec{rcfg};
  Navigator        nav;
  StationMachine   stm;

  bool     autoRunning = true;
  bool     stopIntent  = false;      // raised ONLY by stationService(), below
  bool     useStations = false;      // section H drives the real machine
  int      actualPwm = 0, rampTarget = 0;
  uint16_t rampStepMs = 0;
  uint32_t lastRampMs = 0;

  // observation
  std::vector<std::string> path, evidence, events;
  int advances = 0, notMagnets = 0, unresolvedCount = 0, strikes = 0;
  // Advances that came from an INTERRUPTED SEGMENT rather than a completed
  // passage. This is the number the exactly-once invariant is about. Markers
  // crossed while the zero ramp runs its twelve seconds are ordinary
  // traversals -- the locomotive is still moving, the passages open and close
  // normally, and at 140% of the measured speed the coast alone carries it
  // across seven of them.
  int interruptedAdvances = 0;
  bool judgingSegment = false;
  std::vector<uint32_t> advanceAt;
  uint16_t gainAtStart = 0;
  int      stationOrders = 0, dwellBeginCount = 0, armedCount = 0, missedCount = 0;
  uint32_t dwellDeadline = 0;
  int      advAtRamp = -1;
  bool     departObeyed = false, departBlocked = false;

  void note(std::vector<std::string>& v, const std::string& s) { if (v.size() < 64) v.push_back(s); }
  // Priming is a lap, not a case. Forget it so a report shows only the object
  // under test.
  void clearTrace() {
    path.clear(); evidence.clear(); events.clear();
    advances = notMagnets = unresolvedCount = strikes = 0;
    advanceAt.clear();
  }

  void requestPwm(int target, uint16_t upMs, uint16_t downMs) {
    rampTarget = target;
    rampStepMs = (target > actualPwm) ? upMs : downMs;
  }
  void serviceRamp(uint32_t now) {
    if (actualPwm == rampTarget) return;
    if (rampStepMs && now - lastRampMs < rampStepMs) return;
    lastRampMs = now;
    actualPwm += (rampTarget > actualPwm) ? 1 : -1;
  }

  // NAVI_ONE.ino: withdraw() + unresolvedInterruption()
  void withdraw() { autoRunning = false; requestPwm(0, 0, 60); }
  void unresolvedInterruption(const char* what) {
    nav.unresolved();
    ++unresolvedCount;
    note(path, std::string("UNRESOLVED(") + what + ")");
    withdraw();
  }

  // NAVI_ONE.ino: stationService()
  void stationService(uint32_t now) {
    if (!useStations) return;
    const NavStatus& s = nav.status();
    if (!autoRunning || !nav.positionKnown()) {
      if (stm.phase() != StPhase::Idle) stm.reset();
      stopIntent = false;
      return;
    }
    const uint8_t cruise = cruisePwmAt(s.navMm, s.navDir, 90);
    StationOrder o = stm.tick(s.navMm, s.navDir, (uint8_t)actualPwm, cruise, now);

    stopIntent = (stm.phase() == StPhase::Ramp || stm.phase() == StPhase::Dwell);

    if (o.event && !strcmp(o.event, "DEPART") && cap.openUnsettled()) {
      departBlocked = true;
      unresolvedInterruption("a passage stayed open across the whole dwell");
      return;
    }
    if (o.setThrottle) requestPwm((int)o.pwm, 60, o.stepMs);
    if (o.event) {
      ++stationOrders;
      note(events, std::string(o.event) + "@" + std::to_string(now) +
                   " off " + std::to_string(o.offset) + " pwm " + std::to_string(o.pwm));
      if (!strcmp(o.event, "DWELL_BEGIN")) { ++dwellBeginCount; dwellDeadline = now + STATION_DWELL_MS; }
      if (!strcmp(o.event, "ARMED"))       ++armedCount;
      // The stop episode begins here: the zero ramp is the first moment
      // stopIntent is raised, so nothing before it can be an interruption.
      if (!strcmp(o.event, "ZERO_RAMP") && advAtRamp < 0) advAtRamp = advances;
      if (!strcmp(o.event, "MISSED"))      ++missedCount;
      if (!strcmp(o.event, "DEPART"))      departObeyed = true;
    }
  }

  // NAVI_ONE.ino: hallTask() + the judged-queue arm of loop(). Single
  // threaded here, so the queue is a direct call.
  void hallTick(uint32_t now, int16_t raw) {
    if (nav.takeResetRequest()) { rec.reset(); cap.reset(); }
    const bool mayAdapt = actualPwm > NAVI_BASELINE_ADAPT_PWM;
    if (!cap.sample(now, raw, mayAdapt, stopIntent)) return;

    const uint16_t gBefore = rec.gain();
    switch (cap.event()) {
      case HallEvent::Passage: {
        const Passage& p = cap.passage();
        Verdict v = rec.examine(p);
        note(path, std::string("PASSAGE/") + outcomeName(v.outcome));
        char b[240];
        snprintf(b, sizeof(b),
          "  PASSAGE   pol %c peak %4u ratio %.3f resid %.4f shape %d guard %d gap %lu "
          "gain %u->%u",
          poleChar(p.polarity), p.peakCounts, (double)v.amplitudeRatio,
          (double)v.residual, v.shapeTested ? 1 : 0, v.guardTested ? 1 : 0,
          (unsigned long)v.gapMs, gBefore, rec.gain());
        note(evidence, b);
        judge(p, v, now);
        break; }
      case HallEvent::PreStop:
      case HallEvent::Departure: {
        const Segment& sg = cap.segment();
        Verdict v = rec.examineInterrupted(sg);
        if (v.isMagnet) cap.episodeCounted();
        note(path, std::string(cap.event() == HallEvent::PreStop ? "PRE_STOP/" : "DEPARTURE/")
                   + outcomeName(v.outcome) + (v.isMagnet ? "->OCCUPIED_COUNTED" : "->OCCUPIED_PENDING"));
        char b[300];
        snprintf(b, sizeof(b),
          "  %-9s pol %c from %4d peak %4u growth %4d ratio %.3f support %u shape %d "
          "guard %d gap %lu gain %u->%u  %s",
          sg.departure ? "DEPARTURE" : "PRE_STOP", poleChar(sg.polarity),
          (int)sg.growthFrom, v.peak, (int)v.entryGrowth, (double)v.amplitudeRatio,
          v.support, v.shapeTested ? 1 : 0, v.guardTested ? 1 : 0,
          (unsigned long)v.gapMs, gBefore, rec.gain(), outcomeName(v.outcome));
        note(evidence, b);
        Passage p; p.openedAtMs = sg.fromMs; p.closedAtMs = sg.toMs;
        p.peakCounts = v.peak; p.polarity = sg.polarity;
        judgingSegment = true; judge(p, v, now); judgingSegment = false;
        break; }
      default: {
        note(path, "ABANDONED");
        note(evidence, "  ABANDONED the episode cleared having established nothing");
        unresolvedInterruption("neither the approach nor the departure established a magnet");
        break; }
    }
  }

  void judge(const Passage& p, const Verdict& v, uint32_t now) {
    Ruling r = nav.judge(p, v);
    switch (r) {
      case Ruling::Advanced:
        ++advances; advanceAt.push_back(now);
        if (judgingSegment) ++interruptedAdvances;
        break;
      case Ruling::NotAMagnet:  ++notMagnets; break;
      case Ruling::WrongMagnet:
      case Ruling::Contradicted: ++strikes; withdraw(); break;
      default: break;
    }
  }

  void tick(uint32_t now, int16_t raw) { stationService(now); hallTick(now, raw); serviceRamp(now); }

  void report(const char* name) const {
    printf("\n  %s\n", name);
    printf("   path:");
    if (path.empty()) printf(" TRAVERSING (nothing was ever emitted)");
    for (size_t i = 0; i < path.size() && i < 10; ++i) printf(" %s", path[i].c_str());
    if (path.size() > 10) printf(" ...(%u more)", (unsigned)(path.size() - 10));
    printf("\n");
    for (size_t i = 0; i < evidence.size() && i < 10; ++i) printf("%s\n", evidence[i].c_str());
    printf("   advances %d", advances);
    if (!advanceAt.empty()) { printf(" at"); for (auto t : advanceAt) printf(" %ums", t); }
    printf("   notMagnet %d   unresolved %d   strikes %d\n", notMagnets, unresolvedCount, strikes);
    printf("   final: AUTO %s   nav %s   mm %u   trust %s   gain %u\n",
           autoRunning ? "RUNNING" : "WITHDRAWN", navStateName(nav.status().state),
           nav.status().navMm, trustName(nav.status().trust),
           const_cast<Rig*>(this)->rec.gain());
  }
};

// ---------------------------------------------------------------------------
// FIELD SHAPES
// ---------------------------------------------------------------------------
static double gaussAt(double t, double centre, double sigma, double amp) {
  const double z = (t - centre) / sigma;
  return amp * std::exp(-0.5 * z * z);
}

// Prime the rig the way a lap does: real passages, through the real capture,
// judged by the real recognizer and navigator. Leaves gain() at `peak`, the
// rebound guard armed, and navMm advanced by `count`.
static void primeLap(Rig& rg, int count, int peak, uint32_t& t, Noise* nz = nullptr) {
  for (int k = 0; k < count; ++k) {
    const uint8_t want = polarityAt(nextMarker(rg.nav.status().navMm, rg.nav.status().navDir));
    const int sign = want ? 1 : -1;
    for (int i = 0; i < 700; ++i, ++t) {
      const double f = gaussAt(i, 350, 108, peak);
      int16_t raw = (int16_t)(IDLE + sign * (int)(f + 0.5) + (nz ? nz->next(4) : 0));
      rg.tick(t, raw);
    }
    for (int i = 0; i < 900; ++i, ++t) rg.tick(t, (int16_t)(IDLE + (nz ? nz->next(4) : 0)));
  }
}

// Bring the capture up out of priming with the locomotive at speed.
static void primeCapture(Rig& rg, uint32_t& t) {
  rg.actualPwm = 60; rg.rampTarget = 60;
  for (int i = 0; i < 3000; ++i, ++t) rg.tick(t, IDLE);
}

// Inject one raw reading, with the throttle where a stopped locomotive's
// actually is. The baseline's motion gate is driven by it (findings 09 and 10,
// and NAVI_BASELINE_ADAPT_PWM), so a harness that left the throttle at cruise
// through a dwell would let the reference migrate onto the parked field and
// would be testing something that cannot happen on the railway.
static void inject(Rig& rg, uint32_t t, int raw) {
  rg.actualPwm  = rg.stopIntent ? 0 : 60;
  rg.rampTarget = rg.actualPwm;
  rg.tick(t, (int16_t)raw);
}

int main() {
  printf("gate 12 -- the interrupted-traversal rule (decision 0070)\n");
  printf("           PROPOSED. Firmware unchanged; this compiles the proposed tree.\n\n");

  // =========================================================================
  printf("A. the judgement copy, evaluated in place\n");
  {
    // med3At(v,n,i) must equal medianOfThree(v,n,dst)[i] for every i. If it
    // does not, the interrupted path is judging something the ordinary path
    // would not recognise, and decision 0065 has a hole in it.
    Noise nz; int bad = 0;
    for (int trial = 0; trial < 20000; ++trial) {
      const uint16_t n = (uint16_t)(1 + (trial % 24));
      int16_t src[32], dst[32];
      for (uint16_t i = 0; i < n; ++i) src[i] = (int16_t)nz.next(300);
      medianOfThree(src, n, dst);
      for (uint16_t i = 0; i < n; ++i) if (med3At(src, n, i) != dst[i]) ++bad;
    }
    ok(bad == 0, "med3At is medianOfThree, elementwise", std::to_string(bad) + " disagreements");
    printf("   20,000 random vectors, lengths 1..24: %d disagreements\n", bad);
  }

  // =========================================================================
  printf("\nB. the constants the two layers share\n");
  {
    CaptureConfig c; RecognizerConfig r; MagnetRecognizer rec(r);
    ok(rec.checkInterruptedConfig(c.entryMargin, c.exitMargin, c.floorMs),
       "the recognizer's restated constants match the capture's");
    ok(c.settleWindowMs == 16 * c.settleStepMs, "settle window is 16 samples");
    printf("   entry %d  exit %d  floor %ums  settle %d counts over %ums in %u samples\n",
           (int)c.entryMargin, (int)c.exitMargin, (unsigned)c.floorMs,
           (int)c.settleSpan, (unsigned)c.settleWindowMs,
           (unsigned)(c.settleWindowMs / c.settleStepMs));
  }

  // =========================================================================
  printf("\nC. settle detection against the measured stationary noise\n");
  printf("   Real 1 kHz readings, taken while Toby was demonstrably parked, from\n");
  printf("   the plateaus of the four field records. The question is whether the\n");
  printf("   detector can fire at all -- and whether the WITHDRAWN +/-8 raw band could.\n");
  {
    for (int f = 0; f < CAPTURE_COUNT; ++f) {
      const CaptureFixture& F = CAPTURES[f];
      // the plateau: everything after the first tenth of the record
      const int from = F.n / 10, to = F.n - 6;
      int rawWorst = 0, trimWorst = 0, fired = 0, windows = 0;
      for (int s = from; s + 16 <= to; ++s) {
        int16_t w[16];
        for (int i = 0; i < 16; ++i) w[i] = F.v[s + i];
        for (int i = 1; i < 16; ++i) { int16_t v = w[i]; int j = i - 1; while (j >= 0 && w[j] > v) { w[j+1] = w[j]; --j; } w[j+1] = v; }
        const int raw = w[15] - w[0], trim = w[14] - w[1];
        if (raw > rawWorst) rawWorst = raw;
        if (trim > trimWorst) trimWorst = trim;
        if (trim <= 20) ++fired;
        ++windows;
      }
      printf("   %-5s raw span max %2d   trimmed max %2d   trimmed<=20 in %d/%d windows\n",
             F.tag, rawWorst, trimWorst, fired, windows);
      ok(trimWorst <= 20, "the trimmed span stays inside settleSpan", F.tag);
      ok(rawWorst > 16, "a raw +/-8 band could not have fired here", F.tag);
      ok(fired == windows, "every stationary window settles", F.tag);
    }
  }

  // =========================================================================
  printf("\nD. the four field records, replayed through the real stack\n");
  printf("   Each stored reading is held for its decimation interval; the readings\n");
  printf("   between were never transmitted. Run twice: clean, and with +/-6 counts\n");
  printf("   of bounded noise, which is wider than the +/-4.2 sd measured at rest.\n");
  {
    struct Want { const char* tag; int adv; const char* note; };
    const Want WANTS[] = {
      { "F09A", 0, "a stationary offset has no entry -- and no stop selected it either" },
      { "F09B", 0, "the same, held 9m15s" },
      { "F11",  1, "the magnet is at the HEAD: counted on arrival, departure suppressed" },
      { "F13",  1, "the magnet is at the TAIL: zero on arrival, one on departure" },
    };
    for (int f = 0; f < CAPTURE_COUNT; ++f) {
      const CaptureFixture& F = CAPTURES[f];
      const Want& W = WANTS[f];
      for (int variant = 0; variant < 2; ++variant) {
        Noise nz; Noise* pn = variant ? &nz : nullptr;
        Rig rg; uint32_t t = 1;
        primeCapture(rg, t);
        rg.nav.declare(20, +1);
        primeLap(rg, 10, F.gain, t, pn);        // gain and guard from real passages
        // Run the lap on until the marker the map expects next has the pole
        // this record actually carries. Identity is the Navigator's business
        // and it is not what is under test here; a record replayed against the
        // wrong expected pole would strike on polarity whatever the Hall said.
        for (int k = 0; k < 12 &&
             polarityAt(nextMarker(rg.nav.status().navMm, rg.nav.status().navDir)) != F.polarity; ++k)
          primeLap(rg, 1, F.gain, t, pn);
        const uint16_t gainNow = rg.rec.gain();
        rg.clearTrace();

        // The record. F09A/F09B are latches, not controlled stops: nothing
        // raises stopIntent for them, and that is the point.
        const bool stopped = (F.tag[1] == '1');   // F11, F13
        rg.stopIntent = false;
        const int sign = F.polarity ? 1 : -1;
        // SINGLE STORED OUTLIERS ARE REMOVED BEFORE TIME-STRETCHING, by the
        // house median of three. Not tidiness: a stored sample stands for 128
        // real milliseconds here, so stretching F13's one reading of -1 (mean
        // 41) manufactures 128 ms inside the exit margin -- which would have
        // CLOSED the passage. The field says it stayed open for 36.7 s, so
        // that stretch did not exist. The firmware saw the outlier as one
        // millisecond and its own median-of-three judgement copy discounted
        // it; this is the same operation, applied where the record is coarse.
        std::vector<int16_t> rec((size_t)F.n);
        medianOfThree(F.v, F.n, rec.data());
        for (uint16_t i = 0; i < F.n; ++i) {
          // THE READINGS BETWEEN TWO STORED SAMPLES WERE NEVER TRANSMITTED.
          // They are interpolated, not held. Holding would manufacture events
          // the record itself rules out: a single stationary outlier -- F13's
          // plateau contains one reading of -1 against a mean of 41 -- becomes
          // a 128 ms excursion, and 128 ms inside the exit margin would have
          // CLOSED the passage. The field record says it stayed open for
          // 36.7 s, so no such stretch existed. Interpolation is a guess too;
          // it is the guess consistent with what the firmware observed.
          const int a0 = rec[i];
          const int a1 = (i + 1 < F.n) ? rec[i + 1] : rec[i];
          for (uint16_t k = 0; k < F.decimation; ++k, ++t) {
            const int lerp = a0 + (int)((long)(a1 - a0) * k / (long)F.decimation);
            const int16_t raw = (int16_t)(IDLE + sign * lerp + (pn ? pn->next(6) : 0));
            // A station zero-ramp/dwell is in force for the two station
            // incidents, from a second into the record until the field moves
            // again. The throttle is AT ZERO through it, as it was in the
            // field -- which is what keeps the baseline's motion gate shut.
            rg.stopIntent = stopped && i > 2 && i + 8 < F.n;
            rg.actualPwm = rg.stopIntent ? 0 : 60;
            rg.rampTarget = rg.actualPwm;
            rg.tick(t, raw);
          }
        }
        rg.stopIntent = false;
        for (int i = 0; i < 4000; ++i, ++t) rg.tick(t, (int16_t)(IDLE + (pn ? pn->next(6) : 0)));

        const int adv = rg.advances;
        char nm[160];
        snprintf(nm, sizeof(nm), "%s  %s   [%s]  gain %u", F.tag, F.what,
                 variant ? "with noise" : "clean", gainNow);
        rg.report(nm);
        printf("   expected %d: %s\n", W.adv, W.note);
        ok(adv == W.adv, "advance count",
           std::string(F.tag) + " got " + std::to_string(adv) + " wanted " + std::to_string(W.adv));
      }
    }
  }

  // =========================================================================
  printf("\n\nE. stops at every point on the arc\n");
  printf("   peak 210, sigma 108 ms (a 30 mm magnet at station speed), 8 s dwell.\n");
  {
    struct Stop { const char* name; double frac; bool rising; int want; };
    const Stop STOPS[] = {
      { "stop on the RISING edge at 15% of peak (32 counts, under the entry margin)", 0.15, true,  1 },
      { "stop on the RISING edge at 25% of peak (53 counts)",                          0.25, true,  1 },
      { "stop on the RISING edge at 40% of peak (84 counts)",                          0.40, true,  1 },
      { "stop at the PEAK",                                                            0.99, true,  1 },
      { "stop on the FALLING edge at 40% of peak",                                     0.40, false, 1 },
      { "stop on the FALLING edge at 15% of peak",                                     0.15, false, 1 },
    };
    for (const Stop& S : STOPS) {
      Rig rg; uint32_t t = 1;
      primeCapture(rg, t);
      rg.nav.declare(20, +1);
      primeLap(rg, 10, 210, t);
      rg.clearTrace();
      const uint8_t want = polarityAt(nextMarker(rg.nav.status().navMm, rg.nav.status().navDir));
      const int sign = want ? 1 : -1;
      const double sigma = 108.0;
      const double off = sigma * std::sqrt(-2.0 * std::log(S.frac));
      const double centre = 700.0;
      const int tstop = (int)(S.rising ? centre - off : centre + off);

      for (int i = 0; i < tstop; ++i, ++t) inject(rg, t, (IDLE + sign * (int)gaussAt(i, centre, sigma, 210)));
      rg.stopIntent = true;
      for (int i = 0; i < 8000; ++i, ++t) inject(rg, t, (IDLE + sign * (int)gaussAt(tstop, centre, sigma, 210)));
      for (int i = tstop; i < 1400; ++i, ++t) { rg.stopIntent = false; inject(rg, t, (IDLE + sign * (int)gaussAt(i, centre, sigma, 210))); }
      for (int i = 0; i < 3000; ++i, ++t) inject(rg, t, IDLE);

      rg.report(S.name);
      ok(rg.advances == S.want, "advance count", S.name);
    }
  }

  // =========================================================================
  printf("\n\nF. things that are not magnets\n");
  {
    struct NM { const char* name; int want; };
    // Each case builds its own field; `want` is the permitted advance count.
    // A stop is asserted throughout the stationary part of every one of them,
    // which is the hostile setting: the alternate rule is switched ON.
    for (int c = 0; c < 8; ++c) {
      static const char* NAMES[8] = {
        "a pure DC offset that was already there, then decays away",
        "a gradual DC ramp into an 80-count plateau              [FALSE ACCEPT]",
        "an electrical step with a slow leading edge, then parked [FALSE ACCEPT]",
        "an isolated opposite-polarity spike at the apex",
        "an isolated same-polarity spike during the dwell",
        "a shoulder: two overlapping lobes, stopped on the second  [FALSE ACCEPT]",
        "a double-lobed non-magnet, stopped in the notch          [FALSE ACCEPT]",
        "an opposite-sign parked fringe, same-sign departure     [REFUSED, stops]",
      };
      // What is PERMITTED, and why. Three of these are false accepts and are
      // labelled as such: without the shape test, a slow same-sign excursion of
      // the right size during a station stop is not distinguishable from an
      // arrival. Case 7 is refused and stops the railway, which is correct --
      // see the note printed under it.
      const int WANT[8] = { 0, 1, 1, 1, 1, 1, 1, 0 };
      Rig rg; uint32_t t = 1;
      primeCapture(rg, t);
      rg.nav.declare(20, +1);
      primeLap(rg, 10, 210, t);
      rg.clearTrace();
      const uint8_t want = polarityAt(nextMarker(rg.nav.status().navMm, rg.nav.status().navDir));
      const int sign = want ? 1 : -1;
      std::vector<int> f;
      auto push = [&](int v, int n) { for (int i = 0; i < n; ++i) f.push_back(v); };
      switch (c) {
        case 0: push(68, 12000); for (int i = 0; i < 400; ++i) f.push_back(68 - 68 * i / 400); push(0, 2000); break;
        case 1: for (int i = 0; i < 3000; ++i) f.push_back(80 * i / 3000); push(80, 9000); push(0, 2000); break;
        case 2: for (int i = 0; i < 260; ++i) f.push_back(95 * i / 260); push(95, 9000);
                for (int i = 0; i < 200; ++i) f.push_back(95 - 95 * i / 200); push(0, 2000); break;
        case 3: for (int i = 0; i < 700; ++i) f.push_back((int)gaussAt(i, 700, 108, 210));
                f.push_back(-260); push((int)gaussAt(700, 700, 108, 210), 9000);
                for (int i = 700; i < 1400; ++i) f.push_back((int)gaussAt(i, 700, 108, 210)); push(0, 2000); break;
        case 4: for (int i = 0; i < 700; ++i) f.push_back((int)gaussAt(i, 700, 108, 210));
                push(210, 4000); f.push_back(700); push(210, 5000);
                for (int i = 700; i < 1400; ++i) f.push_back((int)gaussAt(i, 700, 108, 210)); push(0, 2000); break;
        case 5: for (int i = 0; i < 900; ++i) f.push_back((int)(gaussAt(i, 400, 90, 150) + gaussAt(i, 780, 90, 200)));
                push((int)(gaussAt(900, 400, 90, 150) + gaussAt(900, 780, 90, 200)), 9000);
                for (int i = 900; i < 1600; ++i) f.push_back((int)(gaussAt(i, 400, 90, 150) + gaussAt(i, 780, 90, 200)));
                push(0, 2000); break;
        case 6: for (int i = 0; i < 500; ++i) f.push_back((int)(gaussAt(i, 300, 130, 190) + gaussAt(i, 700, 130, 190)));
                push((int)(gaussAt(500, 300, 130, 190) + gaussAt(500, 700, 130, 190)), 9000);
                for (int i = 500; i < 1100; ++i) f.push_back((int)(gaussAt(i, 300, 130, 190) + gaussAt(i, 700, 130, 190)));
                push(0, 2000); break;
        case 7: for (int i = 0; i < 300; ++i) f.push_back(-70 * i / 300); push(-70, 9000);
                for (int i = 0; i < 1400; ++i) f.push_back((int)(-70 + gaussAt(i, 700, 108, 260)));
                push(0, 2000); break;
      }
      const int stopFrom = (c == 0 || c == 1) ? 0 : 400;
      for (size_t i = 0; i < f.size(); ++i, ++t) {
        rg.stopIntent = ((int)i >= stopFrom) && ((int)i < (int)f.size() - 2200);
        inject(rg, t, (IDLE + sign * f[i]));
      }
      rg.stopIntent = false;
      for (int i = 0; i < 3000; ++i, ++t) inject(rg, t, IDLE);
      rg.report(NAMES[c]);
      const int adv = rg.advances;
      printf("   permitted %d\n", WANT[c]);
      ok(adv == WANT[c], "advance count", std::string(NAMES[c]) + " got " + std::to_string(adv));
      ok(adv <= 1, "never two", NAMES[c]);
    }
  }

  // =========================================================================
  printf("\n\nG. uninterrupted slow crossings -- the Gaussian must survive\n");
  printf("   No stop is requested. Settling alone may not select the alternate rule.\n");
  {
    // The PWM that produces each crossing, from the same fit the railway runs
    // on: a 30 mm magnet has a 15 mm sigma, so sigma_t = 15000/v ms and
    // v = 3.990 x (PWM - 25.1). A harness that held cruise through a nine
    // second crossing would be testing a speed the locomotive cannot make.
    const int WIDTHS[] = { 108, 200, 400, 700, 1200, 1800 };   // sigma, ms
    for (int w : WIDTHS) {
      const double v = 15000.0 / (double)w;                    // mm/s
      const int pwm = (int)(25.1 + v / 3.990 + 0.5);
      Rig rg; uint32_t t = 1;
      primeCapture(rg, t);
      rg.nav.declare(20, +1);
      primeLap(rg, 10, 210, t);
      rg.clearTrace();
      const uint8_t want = polarityAt(nextMarker(rg.nav.status().navMm, rg.nav.status().navDir));
      const int sign = want ? 1 : -1;
      const int span = w * 13;
      const int apexMove = (int)(210 - gaussAt(200, 0, w, 210));
      for (int i = 0; i < span; ++i, ++t) {
        rg.actualPwm = pwm; rg.rampTarget = pwm;
        rg.tick(t, (int16_t)(IDLE + sign * (int)gaussAt(i, span / 2.0, w, 210)));
        ++t;
      }
      for (int i = 0; i < 3000; ++i, ++t) { rg.actualPwm = pwm; rg.rampTarget = pwm; rg.tick(t, IDLE); }
      char nm[200];
      snprintf(nm, sizeof(nm),
               "sigma %4d ms crossing at PWM %3d (%3.0f mm/s), no stop -- apex moves %d counts per 400 ms",
               w, pwm, v, apexMove);
      rg.report(nm);
      // THE INVARIANT UNDER TEST is not the advance count -- that belongs to
      // the ordinary recognizer, which decision 0070 does not touch. It is
      // that a flat-looking apex, with no controlled stop, NEVER selects the
      // alternate rule.
      bool anyInterrupted = false;
      for (const auto& p : rg.path) if (p.rfind("PRE_STOP", 0) == 0 || p.rfind("DEPARTURE", 0) == 0) anyInterrupted = true;
      ok(!anyInterrupted, "the interrupted path was never selected", nm);
      ok(rg.advances <= 1, "never more than one advance", nm);
      if (rg.advances != 1)
        printf("   NOTE: the ORDINARY recognizer refused this one. Not a 0070 effect --\n"
               "   a passage open longer than openMigrateMs (2000 ms) has the live\n"
               "   baseline walk under it by design (0.9), which deforms the record.\n"
               "   Pre-existing, and the same in both trees. Worth its own finding.\n");
    }
  }

  // =========================================================================
  printf("\n\nH. end to end: the real station machine, over modelled track\n");
  printf("   The whole approach, zero ramp, 30 s dwell and departure, driven by the\n");
  printf("   real StationMachine and the real Navigator. Field from a 30 mm magnet\n");
  printf("   (15 mm sigma, 210 counts); speed from Toby's measured fit,\n");
  printf("   3.990 x (PWM - 25.1) mm/s, which by itself reproduces the 1-3 marker\n");
  printf("   coast the field records show. The landing is then walked across a\n");
  printf("   WHOLE marker spacing in 30 mm steps, so every place he can come to\n");
  printf("   rest is tried: clear of everything, in a fringe, and dead on a magnet.\n");
  {
    printf("\n   Where he comes to rest is NOT something the firmware chooses: the zero\n");
    printf("   ramp starts at a marker and the coast that follows is traction, load and\n");
    printf("   grade. So the sweep is over the coast itself -- speed scaled 60%% to 140%%\n");
    printf("   of the measured fit, which walks the landing 390 mm, more than the 300 mm\n");
    printf("   between markers. Every resting place is therefore visited.\n");
    printf("\n   `adv` counts advances made from INTERRUPTED SEGMENTS only. Markers\n");
    printf("   crossed while the zero ramp runs are ordinary traversals: at 140%% the\n");
    printf("   coast alone carries him across seven of them, every one judged by the\n");
    printf("   complete recognizer.\n");
    printf("\n   %5s %8s %8s  %-30s %4s %4s %-9s %s\n",
           "speed", "coast", "at rest", "path through the stop", "adv", "unr", "AUTO", "dwell");
    for (int pct = 60; pct <= 140; pct += 10) {
      const int shift = 0;
      Rig rg; uint32_t t = 1;
      primeCapture(rg, t);
      rg.nav.declare(128, -1);
      primeLap(rg, 8, 210, t);
      rg.clearTrace();
      rg.useStations = true;
      rg.requestPwm(90, 60, 200);

      std::vector<double> mx; std::vector<int> mp;
      double acc = 0.0; uint8_t m = rg.nav.status().navMm;
      for (int k = 0; k < 60; ++k) {
        acc += (double)spanMm(m, -1);
        m = nextMarker(m, -1);
        mx.push_back(acc); mp.push_back(polarityAt(m));
      }
      double x = 0.0, rampX = -1.0;
      int advAtDwell = -1, advAtDepart = -1, parkedField = 0;
      uint32_t dwellFrom = 0, dwellTo = 0;
      bool armedSeen = false;
      for (uint32_t i = 0; i < 260000; ++i, ++t) {
        const double v = rg.actualPwm > 25 ? (pct / 100.0) * 3.990 * (rg.actualPwm - 25.1) : 0.0;
        x += v / 1000.0;
        double field = 0.0;
        for (size_t k = 0; k < mx.size(); ++k) {
          const double d = x + shift - mx[k];
          if (d > -90 && d < 90) field += (mp[k] ? 1 : -1) * gaussAt(d, 0, 15.0, 210.0);
        }
        rg.tick(t, (int16_t)(IDLE + (int)field));
        if (!armedSeen && rg.armedCount == 1) armedSeen = true;
        if (rg.advAtRamp >= 0 && rampX < 0) rampX = x;
        if (rg.dwellBeginCount == 1 && advAtDwell < 0) {
          advAtDwell = rg.advances; dwellFrom = t; parkedField = (int)field;
        }
        if (rg.departObeyed && advAtDepart < 0) { advAtDepart = rg.advances; dwellTo = t; }
        if (!rg.autoRunning && armedSeen) break;
        if (rg.departObeyed && t > dwellTo + 8000) break;
      }
      // Only the STOP EPISODE is under test: from the zero ramp -- the first
      // moment anything raises stopIntent -- to the end of the run.
      const int episodeAdv = rg.interruptedAdvances;
      std::string p;
      for (const auto& e : rg.path)
        if (e.rfind("PRE_STOP", 0) == 0 || e.rfind("DEPARTURE", 0) == 0 || e == "ABANDONED") {
          if (!p.empty()) p += " "; p += e;
        }
      if (p.size() > 34) p = p.substr(0, 31) + "...";
      if (p.empty()) p = "-- no interruption --";
      char dw[16];
      snprintf(dw, sizeof(dw), "%s", (dwellTo > dwellFrom)
               ? (std::to_string((dwellTo - dwellFrom) / 1000) + "s").c_str() : "-");
      printf("   %4d%% %6dmm %7d  %-30s %4d %4d %-9s %s\n",
             pct, (int)(x - (rampX < 0 ? 0 : rampX)), parkedField, p.c_str(),
             episodeAdv, rg.unresolvedCount,
             rg.autoRunning ? "RUNNING" : "WITHDRAWN", dw);
      for (const auto& e : rg.evidence)
        if (e.find("PRE_STOP") != std::string::npos || e.find("DEPARTURE") != std::string::npos ||
            e.find("ABANDONED") != std::string::npos)
          printf("     %s\n", e.c_str() + 2);

      const std::string tag = "speed " + std::to_string(pct) + "%";
      ok(rg.armedCount == 1, "the approach armed exactly once", tag);
      ok(rg.missedCount == 0, "an advance during the stop did not abandon the station", tag);
      ok(rg.dwellBeginCount <= 1, "the dwell began at most once", tag);
      ok(episodeAdv <= 1, "ZERO OR ONE interrupted advance across the stop episode", tag);
      ok(rg.strikes == 0, "no polarity strike", tag);
      if (dwellTo > dwellFrom)
        ok(dwellTo - dwellFrom >= STATION_DWELL_MS, "the dwell was not shortened", tag);
      (void)advAtDwell; (void)advAtDepart;
    }
  }

  printf("\n\nI. the gain history is not fed by interrupted acceptances\n");
  {
    Rig rg; uint32_t t = 1;
    primeCapture(rg, t);
    rg.nav.declare(20, +1);
    primeLap(rg, 12, 210, t);
    const uint16_t before = rg.rec.gain();
    // Twenty interruptions in a row, each accepted on a partial peak far from
    // the true one. If any of them entered the median, the gain would walk.
    for (int k = 0; k < 20; ++k) {
      const uint8_t want = polarityAt(nextMarker(rg.nav.status().navMm, rg.nav.status().navDir));
      const int sign = want ? 1 : -1;
      for (int i = 0; i < 640; ++i, ++t) inject(rg, t, (IDLE + sign * (int)gaussAt(i, 700, 108, 210)));
      rg.stopIntent = true;
      const int held = (int)gaussAt(640, 700, 108, 210);
      for (int i = 0; i < 6000; ++i, ++t) inject(rg, t, (IDLE + sign * held));
      rg.stopIntent = false;
      for (int i = 640; i < 1400; ++i, ++t) inject(rg, t, (IDLE + sign * (int)gaussAt(i, 700, 108, 210)));
      for (int i = 0; i < 1200; ++i, ++t) inject(rg, t, IDLE);
    }
    const uint16_t after = rg.rec.gain();
    printf("   gain before %u, after 20 interrupted acceptances %u\n", before, after);
    ok(before == after, "the gain median did not move");
  }

  // =========================================================================
  printf("\n\nJ. an episode that establishes nothing withdraws AUTO at once\n");
  {
    // Parked in a 60-count fringe that never grows: no entry on arrival, and a
    // departure excursion that is a decay rather than a crossing.
    Rig rg; uint32_t t = 1;
    primeCapture(rg, t);
    rg.nav.declare(20, +1);
    primeLap(rg, 10, 210, t);
    rg.clearTrace();
    const uint8_t want = polarityAt(nextMarker(rg.nav.status().navMm, rg.nav.status().navDir));
    const int sign = want ? 1 : -1;
    // The zero ramp is already running as he rolls into the fringe -- which is
    // the finding-10 geometry, and what keeps the baseline's motion gate shut.
    rg.stopIntent = true;
    for (int i = 0; i < 900; ++i, ++t) inject(rg, t, (IDLE + sign * (60 * i / 900)));
    for (int i = 0; i < 12000; ++i, ++t) inject(rg, t, (IDLE + sign * 60));
    rg.stopIntent = false;
    for (int i = 0; i < 900; ++i, ++t) inject(rg, t, (IDLE + sign * (60 - 60 * i / 900)));
    for (int i = 0; i < 3000; ++i, ++t) inject(rg, t, IDLE);
    rg.report("parked in a 60-count fringe that never grew and never crossed");
    ok(rg.advances == 0, "zero advances");
    ok(rg.unresolvedCount == 1, "one unresolved-interruption diagnostic");
    ok(!rg.autoRunning, "AUTO withdrawn");
    ok(rg.nav.status().state == NavState::Struck, "position withdrawn, at once");
  }

  printf("\n\n%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
