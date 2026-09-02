// Gate 12: a passage may not span a stop (decision 0070), adversarially.
//
// THE RULE UNDER TEST
// -------------------
// A magnetic level is not a magnet event. A partial arc is not a magnet event.
// Only a completed, coherent rise-and-fall waveform establishes a moving
// magnet. A controlled stop is a timeout: the measurement is PAUSED while Toby
// is stationary and RESUMED when moving-magnet morphology returns, and the
// stitched waveform then meets the UNCHANGED recognizer -- amplitude,
// whole-wave signed polarity, Gaussian morphology, clipping, rebound guard.
//
// There is no second, weaker way to establish a magnet. MagnetRecognizer.h and
// WaveformWindow.h are byte-for-byte the firmware's; this gate compiles them
// unmodified.
//
// PWM transitions ARM OBSERVATION. Hall morphology decides whether the
// measurement pauses or resumes. Sections F and G exist to prove that both
// halves of that sentence hold.
//
// WHAT IS REAL HERE, AND WHAT IS NOT
// ----------------------------------
// REAL: HallCapture, MagnetRecognizer, Navigator, StationMachine and RouteMap
//       are the firmware classes. The four records in fixtures_captures.h are
//       the samples the firmware itself published on diag/waveform. Rig below
//       reproduces NAVI_ONE.ino's hallTask(), stationService(), loop() and
//       serviceRamp(); they are meant to be read side by side.
// NOT:  the ADC. A decimated field record is expanded by INTERPOLATING between
//       the stored samples -- the readings between were never transmitted, and
//       they are not median-filtered: the filter would smooth the five stored
//       samples that ARE finding 13's departure crossing. Sections D onwards drive a physical model: a
//       30 mm magnet (15 mm sigma) and Toby's measured fit
//       3.990 x (PWM - 25.1) mm/s, with the throttle scripted exactly as the
//       station machine scripts it.
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

// A RISK THIS GATE DOES NOT CLOSE.
//
// Registered explicitly so that a green run cannot be mistaken for a resolved
// question. Whatever is in here is carried into the field UNANSWERED, and the
// gate says so in its own summary line rather than ending on a bare pass.
struct KnownRisk { std::string what, detail; };
static std::vector<KnownRisk> knownRisks;
static void risk(const std::string& what, const std::string& detail) {
  knownRisks.push_back({ what, detail });
}
static void ok(bool c, const char* what, const std::string& d = "") {
  ++checks;
  if (!c) { ++failures; printf("  *** FAIL  %s %s\n", what, d.c_str()); }
}

static const int16_t IDLE = 1830;      // Toby's measured idle level
static const double  SIGMA_MM = 15.0;  // a 30 mm magnet
static const double  AMP = 210.0;

struct Noise {
  uint32_t s = 12345;
  int next(int amp) { s = s * 1103515245u + 12345u; return (int)((s >> 16) % (uint32_t)(2 * amp + 1)) - amp; }
};

static double gaussAt(double t, double centre, double sigma, double amp) {
  const double z = (t - centre) / sigma;
  return amp * std::exp(-0.5 * z * z);
}

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

  bool       autoRunning = true;
  StopArming arm = StopArming::None;
  bool       useStations = false;
  int        actualPwm = 0, rampTarget = 0;
  uint16_t   rampStepMs = 0;
  uint32_t   lastRampMs = 0;

  std::vector<std::string> log, events;
  int advances = 0, notMagnets = 0, unresolvedCount = 0, strikes = 0;
  // Experimental field-test build: refusals that stopped the locomotive, and
  // waveform dumps published because of a refusal.
  int stitchedRefusals = 0, dumps = 0;
  uint16_t lastDumpSamples = 0, lastPassageSamples = 0;
  int stitched = 0, abandoned = 0, pauses = 0, resumes = 0;
  uint32_t lastPausedMs = 0;
  float    lastResidual = 0.0f, lastRatio = 0.0f;
  uint16_t lastPeak = 0;
  bool     wasPaused = false;
  int      armedCount = 0, missedCount = 0, dwellBeginCount = 0;
  uint32_t dwellFrom = 0, dwellTo = 0;
  bool     departSeen = false;

  void note(const std::string& s) { if (log.size() < 24) log.push_back(s); }

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
  void withdraw() { autoRunning = false; requestPwm(0, 0, 60); }

  // NAVI_ONE.ino: unresolvedInterruption()
  void unresolvedInterruption() {
    nav.unresolved(); ++unresolvedCount;
    note("UNRESOLVED (a paused measurement never resumed)");
    withdraw();
  }

  // NAVI_ONE.ino: refusedStitched()
  void refusedStitched() {
    nav.unresolved(); ++stitchedRefusals;
    note("STITCHED_REFUSED (advance zero, stop)");
    withdraw();
  }

  // NAVI_ONE.ino: stationService()
  void stationService(uint32_t now) {
    if (!useStations) return;
    const NavStatus& s = nav.status();
    if (!autoRunning || !nav.positionKnown()) {
      if (stm.phase() != StPhase::Idle) stm.reset();
      arm = StopArming::None;
      return;
    }
    const uint8_t cruise = cruisePwmAt(s.navMm, s.navDir, 90);
    StationOrder o = stm.tick(s.navMm, s.navDir, (uint8_t)actualPwm, cruise, now);
    switch (stm.phase()) {
      case StPhase::Ramp:
      case StPhase::Dwell:  arm = StopArming::Decelerating; break;
      case StPhase::Depart: arm = StopArming::Departing;    break;
      default:              arm = StopArming::None;         break;
    }
    if (o.setThrottle) requestPwm((int)o.pwm, 60, o.stepMs);
    if (o.event) {
      events.push_back(std::string(o.event) + "@" + std::to_string(now));
      if (!strcmp(o.event, "ARMED"))       ++armedCount;
      if (!strcmp(o.event, "MISSED"))      ++missedCount;
      if (!strcmp(o.event, "DWELL_BEGIN")) { ++dwellBeginCount; dwellFrom = now; }
      if (!strcmp(o.event, "DEPART"))      { departSeen = true; dwellTo = now; }
    }
  }

  // NAVI_ONE.ino: hallTask() + the judged-queue arm of loop().
  void hallTick(uint32_t now, int16_t raw) {
    if (nav.takeResetRequest()) { rec.reset(); cap.reset(); }
    const bool mayAdapt = actualPwm > NAVI_BASELINE_ADAPT_PWM;
    const bool wasP = cap.paused();
    const bool fired = cap.sample(now, raw, mayAdapt, arm);
    if (cap.paused() && !wasP)  { ++pauses;  note("PAUSE @" + std::to_string(now)); }
    if (!cap.paused() && wasP && cap.open()) { ++resumes; note("RESUME @" + std::to_string(now)); }
    if (!fired) return;

    if (cap.event() == HallEvent::Passage) {
      const Passage& p = cap.passage();
      Verdict v = rec.examine(p);                  // THE UNCHANGED RECOGNIZER
      lastResidual = v.residual; lastRatio = v.amplitudeRatio;
      lastPeak = p.peakCounts; lastPausedMs = cap.pausedMs();
      // NAVI_ONE.ino hallTask(): if (!v.isMagnet) publishWaveformSlot(0, 1).
      // The COMPLETE passage, every sample the recognizer judged, published
      // from the task that still holds it.
      lastPassageSamples = p.sampleCount;
      if (!v.isMagnet) { ++dumps; lastDumpSamples = p.sampleCount; }
      if (cap.pausedMs()) ++stitched;
      char b[220];
      snprintf(b, sizeof(b),
        "%-8s pol %c peak %4u ratio %.3f resid %.4f shape %d guard %d gap %6lu "
        "paused %5lums -> %s",
        cap.pausedMs() ? "STITCHED" : "PASSAGE", poleChar(p.polarity), p.peakCounts,
        (double)v.amplitudeRatio, (double)v.residual, v.shapeTested ? 1 : 0,
        v.guardTested ? 1 : 0, (unsigned long)v.gapMs,
        (unsigned long)cap.pausedMs(), outcomeName(v.outcome));
      note(b);
      Ruling r = nav.judge(p, v);
      switch (r) {
        case Ruling::Advanced:    ++advances; break;
        case Ruling::NotAMagnet:
          ++notMagnets;
          // Experimental field-test build: a refusal that happened AROUND A
          // STOP stops the locomotive -- whether or not the measurement was
          // paused. Conditioning on the pause alone is what let Bamboo run on
          // after a 0.5586 refusal and strike a marker later. An ordinary
          // refusal with no stop near it does not stop him, unchanged.
          if (cap.pausedMs() || p.stopEpisode) refusedStitched();
          break;
        case Ruling::WrongMagnet:
        case Ruling::Contradicted: ++strikes; withdraw(); break;
        default: break;
      }
    } else {
      ++abandoned;
      note("ABANDONED (watchdog)");
      unresolvedInterruption();
    }
  }

  void tick(uint32_t now, int16_t raw) { stationService(now); hallTick(now, raw); serviceRamp(now); }

  void clearTrace() {
    log.clear(); events.clear();
    advances = notMagnets = unresolvedCount = strikes = 0;
    stitched = abandoned = pauses = resumes = 0;
    stitchedRefusals = dumps = 0; lastDumpSamples = lastPassageSamples = 0;
  }
  void report(const std::string& name) const {
    printf("\n  %s\n", name.c_str());
    if (log.empty()) printf("   (nothing happened)\n");
    for (const auto& e : log) printf("   %s\n", e.c_str());
    printf("   advances %d  notMagnet %d  stitched %d  abandoned %d  pauses %d  resumes %d\n",
           advances, notMagnets, stitched, abandoned, pauses, resumes);
    printf("   refusal dumps %d  stop-on-refusal %d\n", dumps, stitchedRefusals);
    printf("   final: AUTO %s  nav %s  mm %u\n",
           autoRunning ? "RUNNING" : "WITHDRAWN",
           navStateName(nav.status().state), nav.status().navMm);
  }
};

// Prime the rig the way a lap does: real passages, through the real capture,
// judged by the real recognizer and navigator.
static void primeLap(Rig& rg, int count, int peak, uint32_t& t, Noise* nz = nullptr) {
  for (int k = 0; k < count; ++k) {
    const uint8_t want = polarityAt(nextMarker(rg.nav.status().navMm, rg.nav.status().navDir));
    const int sign = want ? 1 : -1;
    for (int i = 0; i < 700; ++i, ++t) {
      const double f = gaussAt(i, 350, 108, peak);
      rg.tick(t, (int16_t)(IDLE + sign * (int)(f + 0.5) + (nz ? nz->next(4) : 0)));
    }
    for (int i = 0; i < 900; ++i, ++t) rg.tick(t, (int16_t)(IDLE + (nz ? nz->next(4) : 0)));
  }
}
static void primeCapture(Rig& rg, uint32_t& t) {
  rg.actualPwm = 60; rg.rampTarget = 60;
  for (int i = 0; i < 3000; ++i, ++t) rg.tick(t, IDLE);
}

// ---------------------------------------------------------------------------
// A SCRIPTED STOP over modelled track: one magnet, Toby's measured speed fit,
// and the station machine's own ramp rates. `restOffsetMm` is where the sensor
// comes to rest relative to the magnet's centre -- negative short of it,
// positive past it.
// ---------------------------------------------------------------------------
struct StopRun {
  int    approachPwm, departPwm;
  double restOffsetMm;
  uint32_t dwellMs = 30000;
  int    stallMs = 0;        // wheels turning, locomotive not moving, on departure
  bool   neverMoves = false; // stalled for good
  // THE CONTROL. The identical deceleration, but the ramp stops one count
  // ABOVE the tractive floor, so Toby creeps through the magnet at the same
  // speed he would have stopped at and never stops. Nothing pauses, nothing
  // is stitched, and the ordinary recognizer judges an ordinary passage. It
  // separates "the stop broke the waveform" from "the creep did".
  bool   creepThrough = false;
};

// Where does he actually come to rest? Simulated with the same integer ramp
// the Rig uses, so the magnet can be PLACED relative to the real landing
// instead of an estimate of it. The field does not affect the physics, so one
// dry run settles it exactly.
// Where does he ACTUALLY come to rest? A dry run of the same loop runStop()
// uses, with no field in it -- the field does not affect the physics, so this
// is exact rather than an estimate, and the magnet can be placed relative to
// the real landing.
static double coastMm(int fromPwm, int stepMs) {
  int pwm = fromPwm, target = fromPwm; uint32_t lastRamp = 0;
  double x = -400.0; bool ramping = false;
  for (uint32_t ms = 1; ms < 300000; ++ms) {
    if (pwm > 25) x += 3.990 * (pwm - 25.1) / 1000.0;
    if (!ramping && x >= 0.0) { ramping = true; target = 0; lastRamp = 0; }
    if (pwm != target && ms - lastRamp >= (uint32_t)stepMs) {
      lastRamp = ms; pwm += (target > pwm) ? 1 : -1;
    }
    if (ramping && pwm == 0) return x;
  }
  return x;
}

static double gRestX = 0, gRestField = 0;
static void runStop(Rig& rg, uint32_t& t, const StopRun& R, int sign, int cap_ms = 200000) {
  const double coast = coastMm(R.approachPwm, STATION_STOP_STEP_MS);
  // Start far enough back that the passage opens and closes cleanly. The ramp
  // trigger is one step early, which the dry run above has already accounted
  // for; what is left is rounding.
  double x = R.restOffsetMm - coast - 400.0;
  rg.actualPwm = R.approachPwm; rg.rampTarget = R.approachPwm;
  rg.arm = StopArming::None;
  enum { RUN, RAMP, DWELL, DEPART } ph = RUN;
  uint32_t phMs = 0;
  for (int i = 0; i < cap_ms; ++i, ++t) {
    const double v = rg.actualPwm > 25 ? 3.990 * (rg.actualPwm - 25.1) : 0.0;
    const bool moving = !(ph == DEPART && R.stallMs && (t - phMs) < (uint32_t)R.stallMs)
                        && !(ph == DEPART && R.neverMoves);
    if (moving) x += v / 1000.0;
    rg.tick(t, (int16_t)(IDLE + sign * (int)gaussAt(x, 0, SIGMA_MM, AMP)));
    switch (ph) {
      case RUN:
        if (x >= R.restOffsetMm - coast) {
          ph = RAMP; phMs = t;
          rg.arm = R.creepThrough ? StopArming::None : StopArming::Decelerating;
          rg.requestPwm(R.creepThrough ? 26 : 0, 60, STATION_STOP_STEP_MS);
        }
        break;
      case RAMP:
        if (R.creepThrough) { if (x > 400.0) return; break; }
        if (rg.actualPwm == 0) {
          ph = DWELL; phMs = t;
          gRestX = x; gRestField = gaussAt(x, 0, SIGMA_MM, AMP);
        }
        break;
      case DWELL:
        if (t - phMs >= R.dwellMs) {
          ph = DEPART; phMs = t; rg.arm = StopArming::Departing;
          rg.requestPwm(R.departPwm, 60, STATION_DEPART_STEP_MS);
        }
        break;
      case DEPART:
        if (x > 400.0) { rg.arm = StopArming::None; return; }
        break;
    }
  }
}

int main() {
  printf("gate 12 -- a passage may not span a stop (decision 0070)\n");
  printf("           EXPERIMENTAL FIELD-TEST BUILD -- NOT field-accepted NAVI_ONE 1.0.\n");
  printf("           MagnetRecognizer.h and WaveformWindow.h are unmodified: there is\n");
  printf("           no second recognizer here to test, and no ceiling was moved.\n");
  printf("           MagnetRecognizer.h and WaveformWindow.h are the firmware's,\n");
  printf("           byte for byte. There is no second recognizer here to test.\n\n");

  // =========================================================================
  printf("A. the constants\n");
  {
    CaptureConfig c;
    ok(c.settleWindowMs == 16 * c.settleStepMs, "the progression window is 16 samples");
    ok(c.resumeMove == c.exitMargin, "resumeMove is exitMargin, not a new number");
    printf("   entry %d  exit %d  floor %ums  |  progression: span<=%d over %ums\n",
           (int)c.entryMargin, (int)c.exitMargin, (unsigned)c.floorMs,
           (int)c.settleSpan, (unsigned)c.settleWindowMs);
    printf("   resume: %d counts, %u of %u steps agreeing, over %ums\n",
           (int)c.resumeMove, (unsigned)c.resumeAgree,
           (unsigned)(c.resumeWindowMs / c.settleStepMs - 1), (unsigned)c.resumeWindowMs);
    printf("   watchdogs: pause %lus, resume %lus (wall clock, never paused)\n",
           (unsigned long)(c.pauseMaxMs / 1000), (unsigned long)(c.resumeMaxMs / 1000));
  }

  // =========================================================================
  printf("\nB. loss of progression, against the MEASURED stationary noise\n");
  printf("   Real 1 kHz readings taken while Toby was demonstrably parked, from\n");
  printf("   the plateaus of the four field records. The question is whether the\n");
  printf("   detector can fire at all -- and whether a raw +/-8 band could.\n");
  {
    for (int f = 0; f < CAPTURE_COUNT; ++f) {
      const CaptureFixture& F = CAPTURES[f];
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
      printf("   %-5s raw span max %2d   trimmed max %2d   plateau in %d/%d windows\n",
             F.tag, rawWorst, trimWorst, fired, windows);
      ok(trimWorst <= 20, "the trimmed span stays inside settleSpan", F.tag);
      ok(rawWorst > 16, "a raw +/-8 band could not have fired here", F.tag);
      ok(fired == windows, "every stationary window reads as a plateau", F.tag);
    }
  }

  // =========================================================================
  printf("\n\nC. the two station incidents, replayed through the real stack\n");
  printf("   Each stored reading stands for its decimation interval and is\n");
  printf("   interpolated across it; the readings between were never transmitted.\n");
  printf("   Run clean, and again with +/-3 counts of EXTRA noise -- only 3,\n");
  printf("   because the stored readings already carry this sensor's own noise\n");
  printf("   (sd 3.0 to 4.2, section B) and a second full helping on top would\n");
  printf("   be testing a line twice as noisy as the one Toby runs on.\n");
  {
    struct Want { const char* tag; int adv; int stitched; const char* note; };
    const Want WANTS[] = {
      { "F09A", 0, 0, "a latch, not a stop: nothing arms, nothing pauses" },
      { "F09B", 0, 0, "the same, held 9m15s" },
      { "F11",  1, 1, "parked ON MM59: one stitched rise-and-fall, accepted" },
      { "F13",  1, 1, "parked in MM106's fringe: one stitched arc, accepted" },
    };
    for (int f = 0; f < CAPTURE_COUNT; ++f) {
      const CaptureFixture& F = CAPTURES[f];
      const Want& W = WANTS[f];
      // The clean run's own residual, carried into the noisy run's report. It
      // used to be a literal 0.1271; the departure-side retention of
      // 2026-09-02 moved it, and a risk register that quotes a stale number is
      // worse than none.
      float cleanResid = 0.0f;
      for (int variant = 0; variant < 2; ++variant) {
        Noise nz; Noise* pn = variant ? &nz : nullptr;
        Rig rg; uint32_t t = 1;
        primeCapture(rg, t);
        rg.nav.declare(20, +1);
        primeLap(rg, 10, F.gain, t, pn);
        for (int k = 0; k < 12 &&
             polarityAt(nextMarker(rg.nav.status().navMm, rg.nav.status().navDir)) != F.polarity; ++k)
          primeLap(rg, 1, F.gain, t, pn);
        const uint16_t gainNow = rg.rec.gain();
        rg.clearTrace();

        const bool stopped = (F.tag[1] == '1');       // F11, F13
        const int sign = F.polarity ? 1 : -1;
        // Interpolated, NOT median-filtered. The old design needed the filter
        // because a stored outlier stretched to 128 ms could close a passage;
        // the pause path never evaluates the exit rule, the plateau test is
        // trimmed, and the resume window is longer than one stored interval,
        // so none of those is reachable now -- and filtering would smooth the
        // five stored samples that ARE finding 13's departure crossing.
        std::vector<int16_t> recm(F.v, F.v + F.n);
        for (uint16_t i = 0; i < F.n; ++i) {
          const int a0 = recm[i];
          // The record stops at the exit threshold, not at zero: the passage
          // closed just after its last stored sample. Interpolating the last
          // interval toward zero reconstructs that; holding the last value
          // would append a flat tail the field never had.
          const int a1 = (i + 1 < F.n) ? recm[i + 1] : 0;
          // The zero ramp and dwell run through the body of the record; the
          // departure order comes before the locomotive actually moves.
          if (!stopped)                      rg.arm = StopArming::None;
          else if (i * 100 < (uint32_t)F.n * 85) rg.arm = StopArming::Decelerating;
          else                               rg.arm = StopArming::Departing;
          rg.actualPwm = (rg.arm == StopArming::Decelerating) ? 0 : 60;
          rg.rampTarget = rg.actualPwm;
          for (uint16_t k = 0; k < F.decimation; ++k, ++t) {
            const int lerp = a0 + (int)((long)(a1 - a0) * k / (long)F.decimation);
            rg.tick(t, (int16_t)(IDLE + sign * lerp + (pn ? pn->next(3) : 0)));
          }
        }
        rg.arm = StopArming::None; rg.actualPwm = 60; rg.rampTarget = 60;
        for (int i = 0; i < 6000; ++i, ++t) rg.tick(t, (int16_t)(IDLE + (pn ? pn->next(3) : 0)));

        char nm[200];
        snprintf(nm, sizeof(nm), "%s  %s  [%s]  gain %u", F.tag, F.what,
                 variant ? "with noise" : "clean", gainNow);
        rg.report(nm);
        printf("   wanted: %d advance, %d stitched -- %s\n", W.adv, W.stitched, W.note);
        ok(rg.stitched == W.stitched, "stitched-waveform count", F.tag);
        ok(rg.strikes == 0, "no strike", F.tag);
        ok(rg.advances <= W.adv, "never more than the wanted advance", F.tag);
        if (variant == 0) {
          cleanResid = rg.lastResidual;
          ok(rg.advances == W.adv, "advance count",
             std::string(F.tag) + " got " + std::to_string(rg.advances));
        } else if (rg.advances != W.adv) {
          // NOT a pass, and NOT a failure of the invariant: a MEASUREMENT.
          // What IS asserted here is that the refusal is SAFE and VISIBLE --
          // the four things the field-test build owes the operator when the
          // ceiling goes against it.
          ok(rg.advances == 0, "it advanced NOTHING", F.tag);
          ok(rg.stitchedRefusals == 1, "the refusal stopped the locomotive", F.tag);
          ok(!rg.autoRunning, "AUTO withdrawn on the refusal itself", F.tag);
          ok(rg.nav.status().state == NavState::Struck, "position withdrawn", F.tag);
          ok(rg.dumps >= 1, "the refused waveform was published", F.tag);
          ok(rg.lastDumpSamples == rg.lastPassageSamples && rg.lastDumpSamples > 0,
             "and published COMPLETE -- every sample the recognizer judged",
             std::string(F.tag) + " dumped " + std::to_string(rg.lastDumpSamples) +
             " of " + std::to_string(rg.lastPassageSamples));
          printf("   *** NOTE: with noise this one lands at residual %.4f against the\n"
                 "   0.13 ceiling -- REFUSED. Clean it is %.4f, accepted. Finding 13 sits\n"
                 "   on the line, and which side it falls is decided by three counts of\n"
                 "   noise. Nothing is miscounted: the complete stitched waveform is\n"
                 "   published, nothing advances, and the locomotive stops there and\n"
                 "   then rather than carrying a lost marker to a later strike.\n"
                 "   THE CEILING IS NOT MOVED TO ANSWER THIS. The field answers it.\n",
                 (double)rg.lastResidual, (double)cleanResid);
        }
        // Registered whichever way the noise happened to fall. The margin is
        // the risk; this run is one sample of it, not a verdict on it.
        if (variant == 1 && !strcmp(F.tag, "F13")) {
          char d[400];
          snprintf(d, sizeof(d),
            "clean %.4f ACCEPTED, +/-3 counts of noise %.4f %s, ceiling 0.130. "
            "Margin %.4f -- about %.0f%% of the ceiling. This gate reproduces "
            "the record; it does not measure the real line. What answers it is "
            "an undecimated capture of a stop-and-go at Arches under this build.",
            (double)cleanResid, (double)rg.lastResidual,
            rg.advances == W.adv ? "still accepted" : "REFUSED",
            0.130 - cleanResid, (0.130 - cleanResid) / 0.130 * 100.0);
          risk("Finding 13's stitched waveform sits on the 0.13 shape ceiling", d);
        }
      }
    }
  }

  // =========================================================================
  printf("\n\nD. stops around the arc, at three approach and three departure speeds\n");
  printf("   One magnet, 210 counts, 15 mm sigma. Toby decelerates on the real\n");
  printf("   200 ms-a-count ramp, sits for 30 s, and leaves on the real 200 ms\n");
  printf("   ramp to the departure throttle. `rest` is where the sensor stops,\n");
  printf("   relative to the magnet's centre.\n");
  {
    printf("\n   The right-hand pair is THE CONTROL: the identical deceleration with\n");
    printf("   the ramp stopping one count above the tractive floor, so Toby creeps\n");
    printf("   through at the same speed and never stops. Nothing pauses there and\n");
    printf("   the ordinary recognizer judges an ordinary passage.\n");
    printf("\n   %-26s %5s %6s %5s %9s %7s %3s  |  %7s %3s\n",
           "stop point", "appr", "restmm", "field", "paused", "resid", "adv", "resid", "adv");
    struct P { const char* name; double off; };
    const P POINTS[] = {
      { "rising edge, 15% of peak",  -28.5 },
      { "rising edge, 25% of peak",  -24.9 },
      { "rising edge, 40% of peak",  -20.2 },
      { "the peak",                    0.0 },
      { "falling edge, 40% of peak",  20.2 },
      { "falling edge, 15% of peak",  28.5 },
    };
    const int APPR[] = { 45, 60, 75 };
    const int DEP[]  = { 60, 90, 110 };
    int refusedStops = 0, refusedCreeps = 0;
    for (const P& p : POINTS) {
      for (int si = 0; si < 3; ++si) {
        Rig rg; uint32_t t = 1;
        primeCapture(rg, t);
        rg.nav.declare(20, +1);
        primeLap(rg, 10, (int)AMP, t);
        rg.clearTrace();
        const uint8_t want = polarityAt(nextMarker(rg.nav.status().navMm, rg.nav.status().navDir));
        StopRun R{ APPR[si], DEP[si], p.off };
        runStop(rg, t, R, want ? 1 : -1);
        for (int i = 0; i < 3000; ++i, ++t) rg.tick(t, IDLE);
        // THE CONTROL: identical creep, no stop.
        Rig cg; uint32_t ct = 1;
        primeCapture(cg, ct);
        cg.nav.declare(20, +1);
        primeLap(cg, 10, (int)AMP, ct);
        cg.clearTrace();
        const uint8_t cwant = polarityAt(nextMarker(cg.nav.status().navMm, cg.nav.status().navDir));
        StopRun C{ APPR[si], DEP[si], p.off }; C.creepThrough = true;
        runStop(cg, ct, C, cwant ? 1 : -1, 400000);
        for (int i = 0; i < 3000; ++i, ++ct) cg.tick(ct, IDLE);
        printf("   %-26s %5d %+6.1f %5.0f %7lums %7.4f %3d  |  %7.4f %3d\n",
               p.name, APPR[si], gRestX, gRestField,
               (unsigned long)rg.lastPausedMs,
               (double)rg.lastResidual, rg.advances,
               (double)cg.lastResidual, cg.advances);
        const std::string tag = std::string(p.name) + " appr " + std::to_string(APPR[si]);
        ok(rg.advances <= 1, "at most one advance", tag);
        ok(rg.strikes == 0, "no strike", tag);
        // The stop must not make the waveform WORSE than the same creep does
        // without one. That is the claim the design has to earn; whether the
        // creep itself is recognisable is the ordinary recognizer's business
        // and is not decided here.
        ok(rg.advances >= cg.advances, "the stop cost nothing the creep did not", tag);
        if (rg.advances == 0) ++refusedStops;
        if (cg.advances == 0) ++refusedCreeps;
      }
    }
    printf("\n   %d of 18 stops refused on shape; %d of the 18 identical creeps,\n"
           "   with no stop in them at all, refused too. The stop is not what\n"
           "   breaks these waveforms -- crossing a magnet at a crawl is.\n",
           refusedStops, refusedCreeps);
  }

  // =========================================================================
  printf("\n\nE. artifacts stay refused -- the four false accepts of the first design\n");
  printf("   Each is presented DURING a controlled stop, which is the hostile\n");
  printf("   setting: the pause logic is armed throughout.\n");
  {
    static const char* NAMES[4] = {
      "a gradual DC ramp into an 80-count plateau",
      "an electrical step with a slow leading edge, then parked",
      "a shoulder: two overlapping lobes, stopped on the second",
      "a double-lobed non-magnet, stopped in the notch",
    };
    for (int c = 0; c < 4; ++c) {
      Rig rg; uint32_t t = 1;
      primeCapture(rg, t);
      rg.nav.declare(20, +1);
      primeLap(rg, 10, (int)AMP, t);
      rg.clearTrace();
      const uint8_t want = polarityAt(nextMarker(rg.nav.status().navMm, rg.nav.status().navDir));
      const int sign = want ? 1 : -1;
      std::vector<int> f;
      auto push = [&](int v, int n) { for (int i = 0; i < n; ++i) f.push_back(v); };
      switch (c) {
        case 0: for (int i = 0; i < 3000; ++i) f.push_back(80 * i / 3000); push(80, 12000);
                for (int i = 0; i < 3000; ++i) f.push_back(80 - 80 * i / 3000); push(0, 3000); break;
        case 1: for (int i = 0; i < 260; ++i) f.push_back(95 * i / 260); push(95, 12000);
                for (int i = 0; i < 200; ++i) f.push_back(95 - 95 * i / 200); push(0, 3000); break;
        case 2: for (int i = 0; i < 900; ++i) f.push_back((int)(gaussAt(i, 400, 90, 150) + gaussAt(i, 780, 90, 200)));
                push((int)(gaussAt(900, 400, 90, 150) + gaussAt(900, 780, 90, 200)), 12000);
                for (int i = 900; i < 1600; ++i) f.push_back((int)(gaussAt(i, 400, 90, 150) + gaussAt(i, 780, 90, 200)));
                push(0, 3000); break;
        case 3: for (int i = 0; i < 500; ++i) f.push_back((int)(gaussAt(i, 300, 130, 190) + gaussAt(i, 700, 130, 190)));
                push((int)(gaussAt(500, 300, 130, 190) + gaussAt(500, 700, 130, 190)), 12000);
                for (int i = 500; i < 1100; ++i) f.push_back((int)(gaussAt(i, 300, 130, 190) + gaussAt(i, 700, 130, 190)));
                push(0, 3000); break;
      }
      for (size_t i = 0; i < f.size(); ++i, ++t) {
        const size_t tail = f.size() - 3200;
        rg.arm = (i < 300) ? StopArming::None
               : (i < tail ? StopArming::Decelerating : StopArming::Departing);
        rg.actualPwm = (rg.arm == StopArming::Decelerating) ? 0 : 60;
        rg.rampTarget = rg.actualPwm;
        rg.tick(t, (int16_t)(IDLE + sign * f[i]));
      }
      rg.arm = StopArming::None;
      for (int i = 0; i < 3000; ++i, ++t) rg.tick(t, IDLE);
      rg.report(NAMES[c]);
      ok(rg.advances == 0, "REFUSED -- zero advances", NAMES[c]);
    }
  }

  // =========================================================================
  printf("\n\nF. the sentinels arm observation and decide nothing\n");
  {
    // F1: falling PWM alone does not pause while the field keeps moving.
    {
      Rig rg; uint32_t t = 1;
      primeCapture(rg, t);
      rg.nav.declare(20, +1);
      primeLap(rg, 10, (int)AMP, t);
      rg.clearTrace();
      const uint8_t want = polarityAt(nextMarker(rg.nav.status().navMm, rg.nav.status().navDir));
      // Decelerating is armed for the WHOLE crossing, and Toby coasts through
      // it: the throttle is at zero from the first sample.
      rg.arm = StopArming::Decelerating; rg.actualPwm = 0; rg.rampTarget = 0;
      for (int i = 0; i < 900; ++i, ++t) rg.tick(t, (int16_t)(IDLE + (want ? 1 : -1) * (int)gaussAt(i, 450, 108, AMP)));
      rg.arm = StopArming::None;
      for (int i = 0; i < 3000; ++i, ++t) rg.tick(t, IDLE);
      rg.report("falling PWM armed, throttle at ZERO, but the field keeps moving");
      ok(rg.pauses == 0, "no pause: the Hall was still progressing");
      ok(rg.advances == 1, "the crossing completed and advanced once");
      ok(rg.stitched == 0, "and it was never stitched");
    }
    // F2: rising PWM alone does not resume while Toby is stationary.
    // F3: ...nor while he stalls, then finally moves.
    for (int variant = 0; variant < 3; ++variant) {
      static const char* WHAT[3] = {
        "rising PWM armed for 12 s while Toby does not move at all",
        "rising PWM armed, wheels spinning: isolated spikes and steps in the field",
        "rising PWM armed, a 12 s stall, and then he actually moves",
      };
      Rig rg; uint32_t t = 1;
      primeCapture(rg, t);
      rg.nav.declare(20, +1);
      primeLap(rg, 10, (int)AMP, t);
      rg.clearTrace();
      const uint8_t want = polarityAt(nextMarker(rg.nav.status().navMm, rg.nav.status().navDir));
      StopRun R{ 60, 90, 0.0 };
      R.stallMs = 12000;
      R.neverMoves = (variant != 2);
      Noise nz;
      // Drive the stop, then hold with the requested nuisance on the line.
      const double coast = coastMm(60, STATION_STOP_STEP_MS);
      double x = -coast - 400.0;
      rg.actualPwm = 60; rg.rampTarget = 60; rg.arm = StopArming::None;
      enum { RUN, RAMP, DWELL, DEPART } ph = RUN;
      uint32_t phMs = 0;
      int resumesAtStall = -1;
      for (int i = 0; i < 140000; ++i, ++t) {
        const double v = rg.actualPwm > 25 ? 3.990 * (rg.actualPwm - 25.1) : 0.0;
        const bool stalled = (ph == DEPART) && (variant != 2 || (t - phMs) < 12000u);
        if (!stalled) x += v / 1000.0;
        int f = (int)gaussAt(x, 0, SIGMA_MM, AMP);
        if (stalled && variant == 1) {
          // WHEELS TURNING, LOCOMOTIVE STILL. The sensor has not moved, so the
          // field has not moved: what the line carries is the motor's noise
          // and the occasional artifact. Isolated spikes, one reversal, a step
          // that arrives and sits there, and disorganised variation -- the
          // operator's list, none of which is a magnet continuing.
          if ((i % 2000) == 0) f += 120;                       // isolated spike
          else if ((i % 3300) == 0) f -= 90;                   // isolated reversal
          if ((t - phMs) > 6000u) f += 30;                     // a step that stays
          f += nz.next(10);                                    // motor hash
        }
        rg.tick(t, (int16_t)(IDLE + (want ? 1 : -1) * f));
        if (ph == DEPART && resumesAtStall < 0 && (t - phMs) >= 12000u) resumesAtStall = rg.resumes;
        switch (ph) {
          case RUN:   if (x >= -coast) { ph = RAMP; phMs = t; rg.arm = StopArming::Decelerating; rg.requestPwm(0, 60, STATION_STOP_STEP_MS); } break;
          case RAMP:  if (rg.actualPwm == 0) { ph = DWELL; phMs = t; } break;
          case DWELL: if (t - phMs >= 30000u) { ph = DEPART; phMs = t; rg.arm = StopArming::Departing; rg.requestPwm(90, 60, STATION_DEPART_STEP_MS); } break;
          case DEPART: if (x > 400.0) { rg.arm = StopArming::None; i = 140000; } break;
        }
      }
      for (int i = 0; i < 3000; ++i, ++t) { rg.arm = StopArming::None; rg.tick(t, IDLE); }
      rg.report(WHAT[variant]);
      ok(rg.pauses >= 1, "the measurement paused", WHAT[variant]);
      ok(resumesAtStall == 0, "NOTHING resumed it during the stall", WHAT[variant]);
      ok(rg.advances <= 1, "at most one advance", WHAT[variant]);
      ok(rg.strikes == 0, "no strike", WHAT[variant]);
      if (variant == 2 && rg.advances == 0)
        printf("   NOTE: the stitched waveform was REFUSED on shape (resid %.4f). Toby\n"
               "   broke free after twelve seconds, by which time the throttle had\n"
               "   ramped the whole way, so the second half of the arc is travelled far\n"
               "   faster than the first. The recognizer is right to object; nothing is\n"
               "   miscounted; the marker is lost. See decision 0070, limitations.\n",
               (double)rg.lastResidual);
      if (variant == 2) {
        ok(rg.resumes == 1, "it resumed when he actually moved", WHAT[variant]);
      } else {
        ok(rg.resumes == 0, "it never resumed", WHAT[variant]);
        ok(rg.abandoned == 1, "the wall-clock watchdog ended it", WHAT[variant]);
        ok(!rg.autoRunning, "AUTO withdrawn", WHAT[variant]);
        ok(rg.nav.status().state == NavState::Struck, "position withdrawn", WHAT[variant]);
      }
    }
  }

  // =========================================================================
  printf("\n\nG. uninterrupted crossings -- nothing may pause without an arming\n");
  {
    const int WIDTHS[] = { 108, 200, 400, 700 };
    for (int w : WIDTHS) {
      const double v = 15000.0 / (double)w;
      const int pwm = (int)(25.1 + v / 3.990 + 0.5);
      Rig rg; uint32_t t = 1;
      primeCapture(rg, t);
      rg.nav.declare(20, +1);
      primeLap(rg, 10, (int)AMP, t);
      rg.clearTrace();
      const uint8_t want = polarityAt(nextMarker(rg.nav.status().navMm, rg.nav.status().navDir));
      const int span = w * 13;
      for (int i = 0; i < span; ++i, ++t) {
        rg.actualPwm = pwm; rg.rampTarget = pwm;
        rg.tick(t, (int16_t)(IDLE + (want ? 1 : -1) * (int)gaussAt(i, span / 2.0, w, AMP)));
      }
      for (int i = 0; i < 3000; ++i, ++t) { rg.actualPwm = pwm; rg.rampTarget = pwm; rg.tick(t, IDLE); }
      char nm[160];
      snprintf(nm, sizeof(nm), "sigma %4d ms at PWM %3d (%3.0f mm/s), nothing armed", w, pwm, v);
      rg.report(nm);
      ok(rg.pauses == 0, "never paused", nm);
      ok(rg.stitched == 0, "never stitched", nm);
      ok(rg.advances == 1, "exactly one advance", nm);
    }
  }

  // =========================================================================
  printf("\n\nH. end to end: the real station machine, over modelled track\n");
  printf("   The coast is swept 60%% to 140%% of the measured fit, which walks the\n");
  printf("   landing 390 mm -- more than the 300 mm between markers -- so every\n");
  printf("   resting place is visited.\n");
  {
    printf("\n   %5s %8s %6s %6s %6s %5s %5s %-9s %s\n",
           "speed", "at rest", "pause", "resume", "stitch", "abn", "adv", "AUTO", "dwell");
    for (int pct = 60; pct <= 140; pct += 10) {
      Rig rg; uint32_t t = 1;
      primeCapture(rg, t);
      rg.nav.declare(128, -1);
      primeLap(rg, 8, (int)AMP, t);
      rg.clearTrace();
      rg.useStations = true;
      rg.requestPwm(90, 60, 200);

      std::vector<double> mx; std::vector<int> mp;
      double acc = 0.0; uint8_t m = rg.nav.status().navMm;
      for (int k = 0; k < 60; ++k) {
        acc += (double)spanMm(m, -1); m = nextMarker(m, -1);
        mx.push_back(acc); mp.push_back(polarityAt(m));
      }
      double x = 0.0, rampX = -1.0;
      int parked = 0; bool armedSeen = false;
      for (uint32_t i = 0; i < 300000; ++i, ++t) {
        const double v = rg.actualPwm > 25 ? (pct / 100.0) * 3.990 * (rg.actualPwm - 25.1) : 0.0;
        x += v / 1000.0;
        double field = 0.0;
        for (size_t k = 0; k < mx.size(); ++k) {
          const double d = x - mx[k];
          if (d > -90 && d < 90) field += (mp[k] ? 1 : -1) * gaussAt(d, 0, SIGMA_MM, AMP);
        }
        rg.tick(t, (int16_t)(IDLE + (int)field));
        if (!armedSeen && rg.armedCount == 1) { armedSeen = true; rampX = x; rg.clearTrace(); }
        if (rg.dwellBeginCount == 1 && !parked) parked = (int)field ? (int)field : -1;
        if (!rg.autoRunning && armedSeen) break;
        if (rg.departSeen && t > rg.dwellTo + 20000) break;
      }
      printf("   %4d%% %8d %6d %6d %6d %5d %5d %-9s %lus\n",
             pct, parked == -1 ? 0 : parked, rg.pauses, rg.resumes,
             rg.stitched, rg.abandoned, rg.advances,
             rg.autoRunning ? "RUNNING" : "WITHDRAWN",
             (unsigned long)((rg.dwellTo > rg.dwellFrom) ? (rg.dwellTo - rg.dwellFrom) / 1000 : 0));
      (void)rampX;
      const std::string tag = "speed " + std::to_string(pct) + "%";
      ok(rg.armedCount == 1, "the approach armed exactly once", tag);
      ok(rg.missedCount == 0, "the station was not abandoned", tag);
      ok(rg.dwellBeginCount <= 1, "the dwell began at most once", tag);
      ok(rg.stitched <= 1, "at most ONE stitched waveform per stop episode", tag);
      ok(rg.strikes == 0, "no polarity strike", tag);
      if (rg.dwellTo > rg.dwellFrom)
        ok(rg.dwellTo - rg.dwellFrom >= STATION_DWELL_MS, "the dwell was not shortened", tag);
    }
  }

  // =========================================================================
  printf("\n\nI. a passage may not open while the locomotive is standing still\n");
  printf("   Bamboo CCW, 2026-09-02 12:29:07, reconstructed. Toby came to rest\n");
  printf("   with about 30 counts of a neighbouring field on the sensor -- FIVE\n");
  printf("   counts above the 25 that would have closed a passage, eight below\n");
  printf("   the 38 that opens one. One artifact sample carried him over the\n");
  printf("   opening threshold. The passage could then neither close (the field\n");
  printf("   never fell far enough) nor pause (it had shown no progress since it\n");
  printf("   opened), so it ran for 35.8 SECONDS, decimated to 128 ms a sample\n");
  printf("   holding a flat line, and recorded the real departure magnet in\n");
  printf("   eight samples. Residual 0.5586, refused, marker lost -- and the\n");
  printf("   strike came a marker later, three markers from where Toby was.\n");
  {
    struct Case { const char* name; bool artifact; int want; };
    const Case CASES[] = {
      { "no artifact: nothing opens early, the departure is judged alone", false, 1 },
      { "THE FIELD CASE: one artifact sample opens a passage at rest",     true,  1 },
    };
    for (const Case& C : CASES) {
      Rig rg; uint32_t t = 1;
      rg.useStations = false;
      primeCapture(rg, t);
      rg.nav.declare(157, -1);
      primeLap(rg, 6, 213, t);
      rg.clearTrace();
      const int sign =
        polarityAt(nextMarker(rg.nav.status().navMm, rg.nav.status().navDir)) ? 1 : -1;
      Noise nz;
      // Rolled to a stand inside a neighbouring fringe; the field settles at 30.
      rg.arm = StopArming::Decelerating; rg.actualPwm = 0; rg.rampTarget = 0;
      for (int i = 0; i < 1200; ++i, ++t)
        rg.tick(t, (int16_t)(IDLE + sign * (30 + nz.next(4))));
      // The artifact: ONE sample over entryMargin. Decision 0065's population,
      // and the same one that put a -5 and a +93 in the records of that day.
      if (C.artifact) { rg.tick(t, (int16_t)(IDLE + sign * 46)); ++t; }
      for (int i = 0; i < 30000; ++i, ++t)          // the dwell
        rg.tick(t, (int16_t)(IDLE + sign * (30 + nz.next(4))));
      rg.arm = StopArming::Departing; rg.actualPwm = 60; rg.rampTarget = 90;
      for (int i = 0; i < 900; ++i, ++t) {          // the real departure magnet
        const double f = 30.0 + gaussAt(i, 450, 120, 190);
        rg.tick(t, (int16_t)(IDLE + sign * (int)(f + 0.5) + nz.next(4)));
      }
      for (int i = 0; i < 1500; ++i, ++t)
        rg.tick(t, (int16_t)(IDLE + sign * (30 + nz.next(4))));
      rg.arm = StopArming::None;
      for (int i = 0; i < 3000; ++i, ++t) rg.tick(t, (int16_t)(IDLE + nz.next(4)));

      rg.report(C.name);
      ok(rg.advances == C.want, "the departure magnet is counted, once", C.name);
      ok(rg.strikes == 0, "no strike", C.name);
      ok(rg.autoRunning, "AUTO still running", C.name);
      ok(rg.nav.status().state == NavState::Declared, "position held", C.name);
      // The invariant itself, stated as the operator stated it: stationary time
      // during a controlled stop never belongs in a moving-magnet waveform.
      ok(rg.lastPassageSamples < 4000,
         "no passage carried the dwell",
         std::string(C.name) + " judged " + std::to_string(rg.lastPassageSamples) +
         " samples");
    }
    // 1.0X6 shipped the discard WITHOUT the field guard and this is what it
    // cost: Arches CCW 2026-09-02 14:14:40. Toby rests INSIDE a magnet, not in
    // a fringe -- 88 counts, 44% of MM106's gain. The discard fired on that
    // passage every sample, and it reopened every sample because the field was
    // still there, so the pre-roll froze and the arrival never reached the
    // record. What the recognizer got was twelve stale pre-roll samples and a
    // bare falling side. Residual 0.2706.
    printf("\n   AND THE OTHER HALF OF THE RULE: resting INSIDE a field is not a\n");
    printf("   false opening. There is a magnet there and the passage is its own.\n");
    for (double frac : { 0.44, 0.60 }) {
      const double xr = -SIGMA_MM * sqrt(2.0 * log(1.0 / frac));
      Rig rg; uint32_t t = 1;
      rg.useStations = false;
      primeCapture(rg, t); rg.nav.declare(108, -1); primeLap(rg, 6, 201, t);
      rg.clearTrace();
      const int sign =
        polarityAt(nextMarker(rg.nav.status().navMm, rg.nav.status().navDir)) ? 1 : -1;
      double x = -70.0; int ph = 0; uint32_t at = 0;
      rg.arm = StopArming::Decelerating; rg.actualPwm = 0; rg.rampTarget = 0;
      for (int i = 0; i < 80000; ++i, ++t) {
        double v = 0.0;
        if (ph == 0) { v = 40.0; if (x >= xr) { ph = 1; at = t; } }
        else if (ph == 1) { if (t - at >= 30000u) { ph = 2; at = t;
              rg.arm = StopArming::Departing; rg.actualPwm = 60; rg.rampTarget = 90; } }
        else { const double e = (double)(t - at); v = 150.0 * (e < 600.0 ? e / 600.0 : 1.0); }
        x += v / 1000.0;
        rg.tick(t, (int16_t)(IDLE + sign * (int)gaussAt(x, 0, SIGMA_MM, 201.0)));
        if (rg.advances || rg.notMagnets) break;
        if (x > 400.0) break;
      }
      for (int i = 0; i < 3000; ++i, ++t) rg.tick(t, IDLE);
      char nm[120];
      snprintf(nm, sizeof(nm), "rolls in and rests at %.0f%% of peak, INSIDE the field",
               frac * 100);
      rg.report(nm);
      ok(rg.advances == 1, "the magnet it is parked on is still counted", nm);
      ok(rg.pauses >= 1, "the dwell was PAUSED out, not discarded", nm);
      ok(rg.strikes == 0, "no strike", nm);
      ok(rg.autoRunning, "AUTO still running", nm);
    }
    printf("\n   The artifact case and the clean case now reach the same verdict\n"
           "   by the same road: the stationary opening is discarded, and the\n"
           "   departure arc is judged as the ordinary passage it is.\n");
  }

  printf("\n\n%d checks, %d failures\n", checks, failures);
  if (!knownRisks.empty()) {
    printf("\n");
    printf("  ======================================================================\n");
    printf("   %u KNOWN RISK%s CARRIED INTO THE FIELD -- THIS GATE DOES NOT CLOSE %s\n",
           (unsigned)knownRisks.size(), knownRisks.size() == 1 ? "" : "S",
           knownRisks.size() == 1 ? "IT" : "THEM");
    printf("  ======================================================================\n");
    for (size_t i = 0; i < knownRisks.size(); ++i) {
      printf("   %u. %s\n", (unsigned)(i + 1), knownRisks[i].what.c_str());
      // wrap the detail at ~68 columns
      const std::string& d = knownRisks[i].detail;
      size_t at = 0;
      while (at < d.size()) {
        size_t take = d.size() - at < 68 ? d.size() - at : 68;
        if (at + take < d.size()) {
          size_t sp = d.rfind(' ', at + take);
          if (sp != std::string::npos && sp > at) take = sp - at;
        }
        printf("      %s\n", d.substr(at, take).c_str());
        at += take; while (at < d.size() && d[at] == ' ') ++at;
      }
    }
    printf("\n   A green run of this gate is NOT a clearance. It says the design\n"
           "   behaves as specified INCLUDING when the ceiling goes against it:\n"
           "   nothing advances, the whole waveform is published, and Toby stops.\n"
           "   Whether the ceiling goes against it on real track is unmeasured.\n");
  }
  return failures ? 1 : 0;
}
