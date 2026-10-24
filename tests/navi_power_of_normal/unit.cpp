// Host unit tests for the four new NAVI_POWER_OF_NORMAL headers. No hardware,
// no network, plain C++17.
//   c++ -std=c++17 -O2 -Wall -Wextra -Werror -I<sketch dir> unit.cpp -o t && ./t
#include <cstdio>
#include "NaviObservation.h"
#include "NaviCandidate.h"
#include "NaviEncounter.h"
#include "NaviTarget.h"
#include "NaviArrivalBoundary.h"

static int failures = 0;
#define CHECK(c, ...) do { if (!(c)) { ++failures; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

using namespace navi_power;

// ---- TargetTracker: Proposition 8 -- nonqualifying never stops, never resets.
static void test_target_nonqualifying_keeps_waiting() {
  TargetTracker t;
  t.declare(10, 11, +1);     // ROUTE_POLARITY[11] decides the expected sign
  const int8_t expected = expectedHallSign(11);
  CompletedEncounter wrong{}; wrong.pol = int8_t(-expected); wrong.meetsMinWidth = true; wrong.abandoned = false;
  for (int i = 0; i < 5; ++i) {
    const auto r = t.onEncounter(wrong);
    CHECK(r == JudgementResult::NonQualifying, "wrong-polarity encounter %d should be NonQualifying, not a stop", i);
    CHECK(t.state() == TargetState::Tracking, "state must stay Tracking after a nonqualifying encounter");
    CHECK(t.nextMm() == 11, "target must not change after a nonqualifying encounter");
  }
  CompletedEncounter right{}; right.pol = expected; right.meetsMinWidth = true; right.abandoned = false;
  const auto r = t.onEncounter(right);
  CHECK(r == JudgementResult::Confirmed, "correct-polarity encounter must confirm");
  CHECK(t.lastMm() == 11, "confirmed last must be the previously expected target");
}
static void test_target_abandoned_is_no_claim() {
  TargetTracker t; t.declare(0, 1, +1);
  CompletedEncounter ab{}; ab.abandoned = true; ab.pol = expectedHallSign(1); ab.meetsMinWidth = true;
  CHECK(t.onEncounter(ab) == JudgementResult::NoClaim, "an abandoned encounter is neither confirming nor nonqualifying evidence");
  CHECK(t.nextMm() == 1, "an abandoned encounter must not change the target");
}
static void test_target_unset_has_no_claim() {
  TargetTracker t;   // never declared
  CompletedEncounter e{}; e.pol = 1; e.meetsMinWidth = true;
  CHECK(t.onEncounter(e) == JudgementResult::NoClaim, "an undeclared tracker must never confirm or judge");
}

// ---- Encounter: observed return-to-baseline, not a fitted-window timer.
static void test_encounter_completes_on_rtb() {
  Encounter e;
  CandidateOnset onset; onset.pol = 1; onset.levelAtOpen = 1000;
  e.open(onset, 42);
  CompletedEncounter done;
  bool completed = false;
  // excursion blocks, well above the margin
  for (int i = 0; i < 5 && !completed; ++i) { Block b{}; b.hall = 1200; completed = e.push(b, done); }
  CHECK(!completed, "must not complete while still excursed");
  // return to baseline for RTB_RUN_BLOCKS
  for (uint32_t i = 0; i < RTB_RUN_BLOCKS - 1 && !completed; ++i) { Block b{}; b.hall = 1005; completed = e.push(b, done); }
  CHECK(!completed, "must not complete before RTB_RUN_BLOCKS consecutive settled blocks");
  { Block b{}; b.hall = 1005; completed = e.push(b, done); }
  CHECK(completed, "must complete once RTB_RUN_BLOCKS consecutive settled blocks are observed");
  CHECK(done.openBlockId == 42, "openBlockId must be carried from open()");
  CHECK(!done.abandoned, "a real return-to-baseline is not an abandonment");
  CHECK(done.peakAdc >= 190.0f, "peak excursion (diagnostic) must reflect the 200-count departure observed");
}
static void test_encounter_abandons_on_timeout() {
  Encounter e;
  CandidateOnset onset; onset.pol = 1; onset.levelAtOpen = 1000;
  e.open(onset, 0);
  CompletedEncounter done; bool completed = false;
  for (uint32_t i = 0; i < MAX_ENCOUNTER_BLOCKS && !completed; ++i) { Block b{}; b.hall = 1200; completed = e.push(b, done); }
  CHECK(completed, "an encounter that never returns to baseline must still be decided (abandoned), not left open forever");
  CHECK(done.abandoned, "a timed-out encounter must be marked abandoned");
}
static void test_encounter_min_width_inert() {
  CHECK(MIN_WIDTH_BLOCKS == 0, "the open minimum-width question must not have an invented numeric value");
  Encounter e; CandidateOnset onset; onset.pol = 1; onset.levelAtOpen = 1000; e.open(onset, 0);
  CompletedEncounter done; bool completed = false;
  for (int i = 0; i < 2 && !completed; ++i) { Block b{}; b.hall = 1200; completed = e.push(b, done); }
  for (uint32_t i = 0; i <= RTB_RUN_BLOCKS && !completed; ++i) { Block b{}; b.hall = 1005; completed = e.push(b, done); }
  CHECK(completed && done.meetsMinWidth, "with MIN_WIDTH_BLOCKS==0 nothing may be rejected on width");
}

// ---- CandidateDetector: the two-consecutive-sample onset at the
// established rule's own 1 kHz cadence (CANDIDATE_STRIDE native calls per
// evaluated sample, matching hallTask's 5 native reads per 1 ms tick), with
// a frozen reference (regression test for the level-chasing bug found via
// replay). `feed(adc)` mimics one 1 ms tick's worth of native reads, all
// equal, exactly as a near-constant real signal looks over 1 ms.
static bool feed(CandidateDetector& c, uint16_t adc, uint32_t& serial, CandidateOnset& onset) {
  bool got = false;
  for (uint32_t i = 0; i < CANDIDATE_STRIDE; ++i) { const bool r = c.onSample(adc, serial++, onset); got = got || r; }
  return got;
}
static void test_candidate_two_consecutive_samples() {
  CandidateDetector c;
  CandidateOnset onset;
  uint32_t serial = 0;
  bool got = false;
  for (int i = 0; i < 2000 && !got; ++i) got = feed(c, 1900, serial, onset);   // settle the reference
  CHECK(!got, "quiet samples near a stable level must never trigger an onset");
  const float settled = c.level();
  CHECK(!feed(c, 1971, serial, onset), "a single evaluated crossing sample must not open a candidate by itself");
  got = feed(c, 1972, serial, onset);
  CHECK(got, "a second consecutive same-sign EVALUATED crossing sample must open a candidate");
  CHECK(onset.pol == 1, "an above-reference excursion must report positive polarity");
  CHECK(onset.levelAtOpen == settled, "the reference at open must be the level BEFORE the excursion started, not drifted toward it");
}
static void test_candidate_ramp_still_triggers() {
  // A gradual, physically realistic ramp (the case the level-chasing bug missed).
  CandidateDetector c; CandidateOnset onset; uint32_t serial = 0; bool got = false;
  for (int i = 0; i < 2000 && !got; ++i) got = feed(c, 1900, serial, onset);
  for (int step = 0; step < 40 && !got; ++step) got = feed(c, uint16_t(1900 + step * 3), serial, onset);
  CHECK(got, "a signal that rises gradually (not an instant step) must still be detected once it departs the frozen level by >=70 counts");
}
static void test_candidate_single_tick_noise_dip_no_longer_defeats_onset() {
  // The case the 5 kHz-vs-1 kHz mismatch caused: a genuine step excursion
  // with a single noisy raw sample dipping back under threshold WITHIN one
  // 1 ms tick must still be recognized, because only one evaluated sample
  // per tick is tested, not every raw 5 kHz read.
  CandidateDetector c; CandidateOnset onset; uint32_t serial = 0; bool got = false;
  for (int i = 0; i < 2000 && !got; ++i) got = feed(c, 1900, serial, onset);
  CHECK(!got, "settling phase must not itself trigger");
  // Tick 1: first evaluated read crosses; the OTHER 4 raw reads in this same
  // tick are irrelevant to detection now (they were not, before the fix).
  got = c.onSample(1972, serial++, onset);
  CHECK(!got, "first evaluated crossing sample alone must not open a candidate");
  for (uint32_t i = 1; i < CANDIDATE_STRIDE; ++i) c.onSample(1800, serial++, onset);   // noisy/irrelevant raw reads, same tick
  // Tick 2: the evaluated sample crosses again -> onset.
  got = c.onSample(1973, serial++, onset);
  CHECK(got, "the second tick's evaluated sample must still confirm the onset despite noisy raw reads within either tick");
  for (uint32_t i = 1; i < CANDIDATE_STRIDE; ++i) c.onSample(1800, serial++, onset);
}

// ---- ArrivalBoundary: fails open when unset; the hard AUTO gate is present.
static void test_boundary_unset_never_fires() {
  boundaryTableInit();
  ArrivalBoundary b;
  b.startInterval(5, 0, 90);
  CHECK(!b.exceeded(10'000'000), "an interval with no fitted boundary must never stop AUTO, however long elapsed");
}
static void test_boundary_fires_when_set() {
  boundaryTableInit();
  BOUNDARY_TABLE_MS[7] = 5000;
  ArrivalBoundary b;
  b.startInterval(7, 0, 90);
  for (int i = 0; i < 100; ++i) b.samplePace(90);     // steady at the reference pace
  CHECK(!b.exceeded(4000), "must not fire before its own boundary at reference pace");
  CHECK(b.exceeded(6000), "must fire once elapsed pace-adjusted time exceeds its boundary");
}
static void test_hard_gate_present() {
  CHECK(ARRIVAL_BOUNDARY_VALIDATED == false, "AUTO must remain hard-refused until the arrival boundary is validated against recorded evidence");
}

int main() {
  test_target_nonqualifying_keeps_waiting();
  test_target_abandoned_is_no_claim();
  test_target_unset_has_no_claim();
  test_encounter_completes_on_rtb();
  test_encounter_abandons_on_timeout();
  test_encounter_min_width_inert();
  test_candidate_two_consecutive_samples();
  test_candidate_ramp_still_triggers();
  test_candidate_single_tick_noise_dip_no_longer_defeats_onset();
  test_boundary_unset_never_fires();
  test_boundary_fires_when_set();
  test_hard_gate_present();
  if (failures) { printf("%d FAILURE(S)\n", failures); return 1; }
  printf("ALL PASSED\n");
  return 0;
}
