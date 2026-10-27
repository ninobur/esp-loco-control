// unit_tests.cpp — host unit tests for the portable NAVI waveform code.
// No Arduino dependencies. Covers the recognizer core (synthetic bumps),
// the simple matcher (confirm / discrepancy / duplicate guard / suspend),
// the diagnostic level, and the median-of-5 conditioning helper.
// usage: unit_tests   (exit 0 = all pass)
#include <cstdint>
#include <cstdio>
#include <cmath>

#include "NaviMatchedFilter.h"
#include "NaviWaveformMatcher.h"
#include "NaviDiagnosticLevel.h"
#include "RouteMap.h"

static int failures = 0;
#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      ++failures;                                                          \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
    }                                                                      \
  } while (0)

// One recognized event, built by hand (the recognizer fills these fields).
static NaviRecognizedEvent mkEvent(uint64_t passageUs, int8_t pol, float r = 0.99f,
                                   float a = 180.0f) {
  NaviRecognizedEvent e;
  e.tPassageUs = passageUs;
  e.tRecognizedUs = passageUs + 359000;
  e.r = r;
  e.amplitude = (pol > 0 ? a : -a);
  e.polarity = pol;
  return e;
}

static void testMedianOf5() {
  int16_t a[5] = {5, 1, 4, 2, 3};
  CHECK(medianOf5(a) == 3);
  int16_t b[5] = {10, 20, 30, 40, 50};
  CHECK(medianOf5(b) == 30);
  int16_t c[5] = {50, 40, 30, 20, 10};
  CHECK(medianOf5(c) == 30);
  int16_t d[5] = {7, 7, 7, 7, 7};
  CHECK(medianOf5(d) == 7);
  int16_t e[5] = {-5, 100, 0, -100, 50};
  CHECK(medianOf5(e) == 0);
  std::printf("medianOf5: ok\n");
}

static void testDiagnosticLevel() {
  navi_power::DiagnosticLevel lvl;
  CHECK(!lvl.haveLevel());
  for (int i = 0; i < 50; ++i) lvl.add(1900);
  CHECK(!lvl.haveLevel());  // needs 100 samples
  for (int i = 0; i < 200; ++i) lvl.add(1900);
  CHECK(lvl.haveLevel());
  CHECK(std::fabs(lvl.level() - 1900.0f) < 1.0f);
  for (int i = 0; i < 1500; ++i) lvl.add(2100);  // step up
  CHECK(std::fabs(lvl.level() - 2100.0f) < 1.0f);
  std::printf("DiagnosticLevel: ok\n");
}

static void testMatcher() {
  using namespace navi_power;
  WaveformMatcher m;
  CHECK(m.state() == MatcherState::Unset);
  // Declare at MM44, expecting MM45, CW.
  m.declare(44, 45, +1);
  CHECK(m.state() == MatcherState::Tracking);
  CHECK(m.lastMm() == 44 && m.nextMm() == 45 && m.dir() == 1);
  const int8_t p45 = WaveformMatcher::expectedHallSign(45);
  const int8_t p46 = WaveformMatcher::expectedHallSign(46);
  // MM45, correct polarity, at t = 1 s -> confirm, advance to MM46.
  CHECK(m.onEvent(mkEvent(1000000, p45)) == MatcherOutcome::Confirmed);
  CHECK(m.lastMm() == 45 && m.nextMm() == 46);
  CHECK(m.confirmed() == 1);
  // MM46, WRONG polarity -> discrepancy; position and target unchanged.
  CHECK(m.onEvent(mkEvent(2000000, -p46)) == MatcherOutcome::Discrepancy);
  CHECK(m.lastMm() == 45 && m.nextMm() == 46);
  CHECK(m.discrepancies() == 1);
  // MM46, correct polarity, but only 200 ms after the last advance
  // (t = 1.2 s vs 1.0 s) -> duplicate guard; position unchanged.
  CHECK(m.onEvent(mkEvent(1200000, p46)) == MatcherOutcome::DuplicateGuard);
  CHECK(m.lastMm() == 45 && m.nextMm() == 46);
  CHECK(m.duplicateGuards() == 1);
  // MM46, correct polarity, 500 ms after the last advance -> confirm.
  CHECK(m.onEvent(mkEvent(1500000, p46)) == MatcherOutcome::Confirmed);
  CHECK(m.lastMm() == 46 && m.nextMm() == 47);
  CHECK(m.confirmed() == 2);
  // Observation record: 4 events, in order, with the right outcomes.
  CHECK(m.observationCount() == 4);
  CHECK(m.observation(0).outcome == MatcherOutcome::Confirmed);
  CHECK(m.observation(1).outcome == MatcherOutcome::Discrepancy);
  CHECK(m.observation(2).outcome == MatcherOutcome::DuplicateGuard);
  CHECK(m.observation(3).outcome == MatcherOutcome::Confirmed);
  CHECK(m.observation(0).seq == 1 && m.observation(3).seq == 4);
  CHECK(m.observation(1).expectedMm == 46);
  CHECK(m.observation(1).expectedPolarity == p46);
  // Suspend: no target; an event is recorded as NoClaim, position kept.
  m.suspend();
  CHECK(m.state() == MatcherState::Unset);
  CHECK(m.onEvent(mkEvent(3000000, p46)) == MatcherOutcome::NoClaim);
  CHECK(m.lastMm() == 46);
  std::printf("WaveformMatcher: ok\n");
}

static void testRecognizerSmoke() {
  NaviMatchedFilter f;
  int nev = 0;
  for (int bin = 0; bin < 2000; ++bin) {
    float v = 1900.0f;
    if (bin >= 500 - 60 && bin < 500 + 60)
      v += 170.0f * NAVI_TEMPLATE_CORE[bin - (500 - 60)];
    if (bin >= 1100 - 60 && bin < 1100 + 60)
      v += -180.0f * NAVI_TEMPLATE_CORE[bin - (1100 - 60)];
    for (int k = 0; k < 2; ++k) {
      uint64_t tUs = (uint64_t)((bin * 2 + k) * 1000);
      f.pushTickMedian((int16_t)lroundf(v), tUs);
    }
    while (f.hasEvent()) {
      NaviRecognizedEvent e = f.nextEvent();
      ++nev;
      if (nev == 1) {
        CHECK(e.tPassageUs == 1003000);
        CHECK(e.tRecognizedUs == 1362000);
        CHECK(std::fabs(e.r - 1.0f) < 0.001f);
        CHECK(std::fabs(e.amplitude - 170.0f) < 1.0f);
        CHECK(e.polarity == 1);
      } else if (nev == 2) {
        CHECK(e.tPassageUs == 2203000);
        CHECK(e.tRecognizedUs == 2562000);
        CHECK(std::fabs(e.r + 1.0f) < 0.001f);
        CHECK(std::fabs(e.amplitude + 180.0f) < 1.0f);
        CHECK(e.polarity == -1);
      }
    }
  }
  while (f.hasEvent()) {
    f.nextEvent();
    ++nev;
  }
  CHECK(nev == 2);
  std::printf("recognizer smoke: ok\n");
}

int main() {
  testMedianOf5();
  testDiagnosticLevel();
  testMatcher();
  testRecognizerSmoke();
  if (failures) {
    std::printf("FAILED: %d checks\n", failures);
    return 1;
  }
  std::printf("ALL UNIT TESTS PASSED\n");
  return 0;
}
