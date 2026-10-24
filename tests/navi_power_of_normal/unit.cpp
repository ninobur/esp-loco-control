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

// ---- CandidateDetector: the two-consecutive-raw-sample onset, with a frozen
// reference (regression test for the level-chasing bug found via replay).
static void test_candidate_two_consecutive_samples() {
  CandidateDetector c;
  CandidateOnset onset;
  bool got = false;
  for (int i = 0; i < 2000 && !got; ++i) got = c.onSample(1900, uint32_t(i), onset);   // settle the reference
  CHECK(!got, "quiet samples near a stable level must never trigger an onset");
  const float settled = c.level();
  CHECK(!c.onSample(1971, 9000, onset), "a single crossing sample must not open a candidate by itself");
  got = c.onSample(1972, 9001, onset);
  CHECK(got, "a second consecutive same-sign crossing sample must open a candidate");
  CHECK(onset.pol == 1, "an above-reference excursion must report positive polarity");
  CHECK(onset.levelAtOpen == settled, "the reference at open must be the level BEFORE the excursion started, not drifted toward it");
}
static void test_candidate_ramp_still_triggers() {
  // A gradual, physically realistic ramp (the case the level-chasing bug missed).
  CandidateDetector c; CandidateOnset onset; bool got = false;
  for (int i = 0; i < 2000 && !got; ++i) got = c.onSample(1900, uint32_t(i), onset);
  for (int step = 0; step < 40 && !got; ++step) got = c.onSample(uint16_t(1900 + step * 3), uint32_t(2000 + step), onset);
  CHECK(got, "a signal that rises gradually (not an instant step) must still be detected once it departs the frozen level by >=70 counts");
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
  test_boundary_unset_never_fires();
  test_boundary_fires_when_set();
  test_hard_gate_present();
  if (failures) { printf("%d FAILURE(S)\n", failures); return 1; }
  printf("ALL PASSED\n");
  return 0;
}
