// Host unit tests for the four new NAVI_POWER_OF_NORMAL headers. No hardware,
// no network, plain C++17.
//   c++ -std=c++17 -O2 -Wall -Wextra -Werror -I<sketch dir> unit.cpp -o t && ./t
#include <cstdio>
#include "NaviObservation.h"
#include "NaviBaseline.h"
#include "NaviCandidate.h"
#include "NaviEncounter.h"
#include "NaviTarget.h"

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

static void test_timing_profile_qualification() {
  TimingPredictor p;
  CHECK(p.noAnchor().result == TimingResult::NoAnchor, "a declaration must not invent a timing opening anchor");
  p.installApprovedProfile({10, +1, 90, 1000, 100});
  CHECK(p.evaluate(10, +1, 90, 90, 1080).result == TimingResult::Matches, "inside an installed band must match");
  CHECK(p.evaluate(10, +1, 90, 90, 1201).result == TimingResult::Mismatches, "outside an installed band must mismatch");
  CHECK(p.evaluate(10, +1, 90, 60, 1000).result == TimingResult::Unprofiled, "a 90 profile must not be applied after a 60-PWM arrival");
  CHECK(p.evaluate(10, -1, 90, 90, 1000).result == TimingResult::Unprofiled, "CW must not be applied to CCW");
  p.installApprovedProfile({10, +1, 60, 1600, 100});
  CHECK(p.evaluate(10, +1, 60, 60, 1600).result == TimingResult::Matches, "60 and 90 profiles must coexist");
  CHECK(p.evaluate(10, +1, 90, 90, 1000).result == TimingResult::Matches, "adding 60 must not replace 90");
}
static void test_timing_anchor_is_last_confirmed_opening() {
  TargetTracker t; t.declare(10, 11, +1);
  CompletedEncounter first{}; first.pol = expectedHallSign(11); first.meetsMinWidth = true; first.openUs = 1000000; first.pwmAtOpen = 90;
  CHECK(t.onEncounter(first) == JudgementResult::Confirmed, "first qualifying encounter establishes the opening anchor");
  CompletedEncounter wrong{}; wrong.pol = int8_t(-expectedHallSign(12)); wrong.meetsMinWidth = true; wrong.openUs = 2000000; wrong.pwmAtOpen = 90;
  CHECK(t.onEncounter(wrong) == JudgementResult::NonQualifying, "wrong polarity remains nonqualifying");
  CompletedEncounter next{}; next.pol = expectedHallSign(12); next.meetsMinWidth = true; next.openUs = 3000000; next.pwmAtOpen = 90;
  CHECK(t.onEncounter(next) == JudgementResult::Confirmed, "next correct polarity confirms while timing is unprofiled");
  CHECK(t.lastTiming().result == TimingResult::Unprofiled, "unmeasured variation remains visible as unprofiled");
  CHECK(t.lastTiming().measuredMs == 2000, "a nonqualifying opening must not reset the interval clock");
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
static void test_encounter_never_times_out() {
  // No MAX_ENCOUNTER_BLOCKS / abandon state in this build (REC-14): an
  // encounter that never observably returns simply stays open. Only the
  // separately-validated arrival boundary may act on "this is taking too
  // long" -- not this class.
  Encounter e;
  CandidateOnset onset; onset.pol = 1; onset.levelAtOpen = 1000;
  e.open(onset, 0);
  CompletedEncounter done; bool completed = false;
  for (uint32_t i = 0; i < 5000 && !completed; ++i) { Block b{}; b.hall = 1200; completed = e.push(b, done); }
  CHECK(!completed, "an encounter that never returns to baseline must stay open indefinitely, not time out");
  CHECK(e.active(), "the encounter must remain active with no timeout mechanism present");
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
// evaluated sample, matching hallTask's 5 native reads per 1 ms tick). This
// stage tracks no reference of its own (2026-10-24: that was an unauthorized
// invention, removed) -- every call is given the live level explicitly, as
// NaviBaseline would supply it. `feed(adc, level)` mimics one 1 ms tick's
// worth of native reads, all equal, exactly as a near-constant real signal
// looks over 1 ms.
static bool feed(CandidateDetector& c, uint16_t adc, float level, uint32_t& serial, CandidateOnset& onset) {
  bool got = false;
  for (uint32_t i = 0; i < CANDIDATE_STRIDE; ++i) {
    const bool r = c.onSample(adc, level, serial, uint64_t(serial) * 200, 90, onset);
    ++serial; got = got || r;
  }
  return got;
}
static void test_candidate_two_consecutive_samples() {
  CandidateDetector c;
  CandidateOnset onset;
  uint32_t serial = 0;
  const float level = 1900;
  CHECK(!feed(c, 1900, level, serial, onset), "quiet samples at the reference level must never trigger an onset");
  CHECK(!feed(c, 1971, level, serial, onset), "a single evaluated crossing sample must not open a candidate by itself");
  const bool got = feed(c, 1972, level, serial, onset);
  CHECK(got, "a second consecutive same-sign EVALUATED crossing sample must open a candidate");
  CHECK(onset.pol == 1, "an above-reference excursion must report positive polarity");
  CHECK(onset.openUs == 1000, "onset must retain the first crossing sample timestamp");
  CHECK(onset.pwmAtOpen == 90, "onset must retain applied PWM context");
  CHECK(onset.levelAtOpen == level, "the reported level must be exactly the live level supplied at open, nothing computed here");
}
static void test_candidate_notices_second_departure_without_being_closed() {
  // There is no open/close lock in this build (2026-10-24: the previous
  // version's global exclusivity caused 184 multi-minute detection
  // blackouts on the real boot-1 corpus). Returning under threshold must by
  // itself make the detector ready to notice the next departure -- nothing
  // external needs to "release" it.
  CandidateDetector c; CandidateOnset onset; uint32_t serial = 0;
  const float level = 1900;
  feed(c, 1972, level, serial, onset);                                    // first evaluated crossing tick
  CHECK(feed(c, 1972, level, serial, onset), "second consecutive tick must open the first departure");
  // Signal returns under threshold (no external close() call exists at all).
  feed(c, 1900, level, serial, onset);
  // A second, later, genuinely separate departure must still be noticed.
  feed(c, 1972, level, serial, onset);
  const bool got = feed(c, 1973, level, serial, onset);
  CHECK(got, "a second departure after a real return to normal must be detected with no external action required");
}
static void test_candidate_follows_external_baseline_updates() {
  // The level is read fresh every call; NaviBaseline's live estimate moving
  // between calls (as it does between magnets) must shift what counts as a
  // departure, without this class doing anything to track that itself.
  CandidateDetector c; CandidateOnset onset; uint32_t serial = 0;
  CHECK(!feed(c, 1972, 1950, serial, onset), "a sample within 70 counts of the CURRENT external level must not cross");
  const bool got = feed(c, 2022, 1950, serial, onset);
  CHECK(!got, "single sample at the new level's threshold must not open by itself");
}
static void test_candidate_single_tick_noise_dip_no_longer_defeats_onset() {
  // A genuine step excursion with a single noisy raw sample dipping back
  // under threshold WITHIN one 1 ms tick must still be recognized, because
  // only one evaluated sample per tick is tested, not every raw 5 kHz read.
  CandidateDetector c; CandidateOnset onset; uint32_t serial = 0;
  const float level = 1900;
  // Tick 1: first evaluated read crosses.
  bool got = c.onSample(1972, level, serial, uint64_t(serial) * 200, 90, onset); ++serial;
  CHECK(!got, "first evaluated crossing sample alone must not open a candidate");
  for (uint32_t i = 1; i < CANDIDATE_STRIDE; ++i) { c.onSample(1800, level, serial, uint64_t(serial) * 200, 90, onset); ++serial; }
  // Tick 2: the evaluated sample crosses again -> onset.
  got = c.onSample(1973, level, serial, uint64_t(serial) * 200, 90, onset); ++serial;
  CHECK(got, "the second tick's evaluated sample must still confirm the onset despite noisy raw reads within either tick");
  for (uint32_t i = 1; i < CANDIDATE_STRIDE; ++i) { c.onSample(1800, level, serial, uint64_t(serial) * 200, 90, onset); ++serial; }
}

// ---- BaselineTracker: the LONG, inter-magnet measurement.
static void test_baseline_boot_collects_stationary_only() {
  BaselineTracker b;
  b.beginBootCollection();
  for (int i = 0; i < 100; ++i) b.onBlock(2500, /*pwmOn=*/true);    // moving, must be excluded at boot
  CHECK(!b.haveLevel(), "PWM>0 blocks must not count toward the boot (stationary) collection");
  for (int i = 0; i < 100; ++i) b.onBlock(1890, /*pwmOn=*/false);   // stationary, included at boot
  for (uint32_t i = 0; i < BASELINE_RECOMPUTE_BLOCKS; ++i) b.onBlock(1890, false);   // force a recompute
  CHECK(b.haveLevel(), "enough stationary samples at boot must produce a live level");
  CHECK(b.level() > 1800 && b.level() < 2000, "the boot level must reflect the stationary samples, not the excluded moving ones");
}
static void test_baseline_steady_state_gates_on_pwm() {
  BaselineTracker b;
  b.beginBootCollection();
  for (uint32_t i = 0; i < BASELINE_RECOMPUTE_BLOCKS; ++i) b.onBlock(1890, false);
  uint32_t n = 0;
  b.finalize(n);                            // boot interval ends; now watching for the next entry point, automatically
  b.onBlock(1890, /*pwmOn=*/true);           // within the entry margin, PWM>0: triggers entry, admitted
  for (int i = 0; i < 200; ++i) b.onBlock(9999, /*pwmOn=*/false);   // stationary, wildly different value: must stay excluded even once collecting
  for (uint32_t i = 0; i < BASELINE_RECOMPUTE_BLOCKS; ++i) b.onBlock(1890, /*pwmOn=*/true);
  CHECK(b.level() > 1800 && b.level() < 2000, "PWM=0 blocks must stay excluded even after collecting has started, however different their value");
}
static void test_baseline_entry_requires_appearing_ordinary() {
  BaselineTracker b;
  b.beginBootCollection();
  for (uint32_t i = 0; i < BASELINE_RECOMPUTE_BLOCKS; ++i) b.onBlock(1890, false);
  uint32_t n = 0; b.finalize(n);
  for (int i = 0; i < 50; ++i) b.onBlock(3000, /*pwmOn=*/true);   // moving, but far from the committed level -- still clearly excursed
  const bool valid = b.finalize(n);
  CHECK(!valid, "blocks far from the committed level must not trigger entry, however many arrive");
  CHECK(n == 0, "nothing should be admitted while still clearly excursed");
}
static void test_baseline_retains_on_zero_evidence() {
  BaselineTracker b;
  b.beginBootCollection();
  for (uint32_t i = 0; i < BASELINE_RECOMPUTE_BLOCKS; ++i) b.onBlock(1890, false);
  uint32_t n = 0;
  CHECK(b.finalize(n), "the boot interval with ample samples must be valid");
  const float before = b.level();
  for (int i = 0; i < 50; ++i) b.onBlock(9999, /*pwmOn=*/false);   // fully stationary the whole interval: gate never opens
  const bool valid = b.finalize(n);
  CHECK(!valid, "an interval with zero gated samples must be reported invalid");
  CHECK(n == 0, "exactly zero samples must have been collected");
  CHECK(b.level() == before, "the previous committed level must be retained untouched when nothing was collected");
}
static void test_baseline_promotes_on_any_nonzero_evidence() {
  // No arbitrary minimum (corrected 2026-10-24): even one collected sample
  // is promoted.
  BaselineTracker b;
  b.beginBootCollection();
  for (uint32_t i = 0; i < BASELINE_RECOMPUTE_BLOCKS; ++i) b.onBlock(1890, false);
  uint32_t n = 0; b.finalize(n);
  b.onBlock(1895, /*pwmOn=*/true);   // exactly one admitted sample
  const bool valid = b.finalize(n);
  CHECK(valid, "even a single collected sample must be promoted -- no arbitrary minimum");
  CHECK(n == 1, "exactly one sample must have been counted");
  CHECK(b.level() == 1895, "the promoted level must be exactly that one sample's value");
}
static void test_baseline_independent_of_encounter_confirmation() {
  // BaselineTracker is fed every block regardless of Encounter's state, and
  // must already be collecting real samples well before Encounter's own
  // RTB_RUN_BLOCKS confirmation completes -- proving no coupling either way.
  BaselineTracker b; Encounter e;
  b.beginBootCollection();
  for (uint32_t i = 0; i < BASELINE_RECOMPUTE_BLOCKS; ++i) b.onBlock(1890, false);
  uint32_t n = 0; b.finalize(n);
  CandidateOnset onset; onset.pol = 1; onset.levelAtOpen = 1890;
  e.open(onset, 0);
  bool confirmed = false;
  for (uint32_t i = 0; i < RTB_RUN_BLOCKS - 2 && !confirmed; ++i) {
    Block blk{}; blk.hall = 1895;
    b.onBlock(blk.hall, true);
    CompletedEncounter done;
    confirmed = e.push(blk, done);
  }
  CHECK(!confirmed, "Encounter must not have confirmed yet with RTB_RUN_BLOCKS-2 settled blocks (sanity check)");
  uint32_t mid = 0; const bool wouldBeValid = b.finalize(mid);
  CHECK(wouldBeValid && mid > 0, "BaselineTracker must have already collected real samples before Encounter's own confirmation completed");
}

// ---- NaviCandidate <-> NaviBaseline integration: the two simultaneous
// measurements must not substitute for each other.
static void test_two_measurements_are_independent() {
  BaselineTracker b; CandidateDetector c; CandidateOnset onset; uint32_t serial = 0;
  b.beginBootCollection();
  for (uint32_t i = 0; i < BASELINE_RECOMPUTE_BLOCKS; ++i) b.onBlock(1890, false);
  uint32_t n = 0; b.finalize(n);
  b.onBlock(1890, /*pwmOn=*/true);   // triggers entry
  for (uint32_t i = 0; i < BASELINE_RECOMPUTE_BLOCKS; ++i) b.onBlock(1890, true);   // ordinary track, steady state
  const float level = b.level();
  feed(c, uint16_t(level + 100), level, serial, onset);
  const bool got = feed(c, uint16_t(level + 100), level, serial, onset);   // the SHORT measurement's detection reads the LONG measurement's live level
  CHECK(got, "detection must use NaviBaseline's live level directly, with no separate copy of its own");
  CHECK(onset.levelAtOpen == level, "the committed level handed to the encounter must be exactly what NaviBaseline was reporting, unaltered by detection");
}

int main() {
  test_target_nonqualifying_keeps_waiting();
  test_target_abandoned_is_no_claim();
  test_target_unset_has_no_claim();
  test_timing_profile_qualification();
  test_timing_anchor_is_last_confirmed_opening();
  test_encounter_completes_on_rtb();
  test_encounter_never_times_out();
  test_encounter_min_width_inert();
  test_candidate_two_consecutive_samples();
  test_candidate_notices_second_departure_without_being_closed();
  test_candidate_follows_external_baseline_updates();
  test_candidate_single_tick_noise_dip_no_longer_defeats_onset();
  test_baseline_boot_collects_stationary_only();
  test_baseline_steady_state_gates_on_pwm();
  test_baseline_entry_requires_appearing_ordinary();
  test_baseline_retains_on_zero_evidence();
  test_baseline_promotes_on_any_nonzero_evidence();
  test_baseline_independent_of_encounter_confirmation();
  test_two_measurements_are_independent();
  if (failures) { printf("%d FAILURE(S)\n", failures); return 1; }
  printf("ALL PASSED\n");
  return 0;
}
