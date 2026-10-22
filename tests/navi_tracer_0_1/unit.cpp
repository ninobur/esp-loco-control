// Host-side unit tests for NaviTracerCore.h, ported one-to-one from the
// validated Python reference's own synthetic test suite:
//   research/navi_target_acquisition_20261021/tests/test_target_acquisition.py
//   (commit 6efc952, research/navi-target-acquisition-pwm90-20261021)
//
// These are synthetic, not validation against a recording -- that is what
// run_equivalence.sh / equivalence_runner.cpp are for. Nothing here uses real
// recorded data. Deliberately uses the LEGACY 110ms duration
// (MIN_ACQUIRE_DURATION_LEGACY_110MS_S), matching the Python test file's own
// default at the time those tests were written (the Python dataclass default
// was 0.11s) -- NOT the NAVI_TRACER_0_1 field default (the duration floor),
// so that curve widths chosen in these tests behave exactly as the original
// tests intended.
//
// Build: c++ -std=c++17 -O2 -Wall -Wextra -Werror
//   -I ../../firmware/test-programs/NAVI_TRACER_0_1 unit.cpp -o unit && ./unit
#include "NaviTracerCore.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using navi_tracer::TargetAcquisition;
using navi_tracer::Event;
using navi_tracer::EventKind;

static int g_failures = 0;
#define CHECK(cond) do { \
  if (!(cond)) { std::fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failures; } \
} while (0)

struct Sample { double t; double y; int pwm; };

static constexpr double BASELINE_LEVEL = 1800.0;
static constexpr double DT = 0.02;
static constexpr double LEGACY_110MS = navi_tracer::MIN_ACQUIRE_DURATION_LEGACY_110MS_S;

// Deterministic pseudo-noise in [-0.5, 0.5) * amplitude -- not statistically
// matched to the Python reference's numpy RNG (this suite checks logic, not
// bit-identical numbers; that is equivalence_runner.cpp's job).
static double pseudoNoise(uint32_t seed, int i, double amplitude) {
  uint32_t x = (uint32_t)(seed * 2654435761u + (uint32_t)i * 40503u);
  x ^= x >> 13; x *= 0x5bd1e995u; x ^= x >> 15;
  return ((double)(x % 100000) / 100000.0 - 0.5) * amplitude;
}

static std::vector<Sample> quietRun(int n, double level = BASELINE_LEVEL, double noise = 1.0,
                                     double t0 = 0.0, int pwm = 90, uint32_t seed = 0) {
  std::vector<Sample> out;
  for (int i = 0; i < n; ++i) out.push_back({t0 + i * DT, level + pseudoNoise(seed, i, noise * 2), pwm});
  return out;
}
static std::vector<Sample> curveRun(int n, double t0, double amplitude, double width_s = 0.15,
                                     double level = BASELINE_LEVEL, int pwm = 90, uint32_t seed = 1) {
  std::vector<Sample> out;
  const double mu = t0 + n * DT / 2;
  for (int i = 0; i < n; ++i) {
    const double t = t0 + i * DT;
    const double y = level + amplitude * std::exp(-0.5 * std::pow((t - mu) / width_s, 2)) + pseudoNoise(seed, i, 2.0);
    out.push_back({t, y, pwm});
  }
  return out;
}

// Feeds samples; returns every event emitted (in order), mirroring the
// Python test file's `ta.events`. Also fills `completed` with the events
// whose kind == CLOSE_AND_ADVANCE, mirroring `feed()`'s returned list there.
static std::vector<Event> feed(TargetAcquisition& ta, const std::vector<Sample>& obs,
                                std::vector<Event>* allEvents, std::vector<Event>* completed = nullptr) {
  std::vector<Event> local;
  for (const auto& s : obs) {
    Event out[2];
    const int n = ta.observe(s.t, s.y, s.pwm, out);
    for (int i = 0; i < n; ++i) {
      if (allEvents) allEvents->push_back(out[i]);
      local.push_back(out[i]);
      if (completed && out[i].kind == EventKind::CLOSE_AND_ADVANCE) completed->push_back(out[i]);
    }
  }
  return local;
}
static bool hasKind(const std::vector<Event>& evs, EventKind k) {
  for (const auto& e : evs) if (e.kind == k) return true;
  return false;
}

static void test_target_is_deterministic_next_marker() {
  TargetAcquisition ta(45, 1, LEGACY_110MS);
  CHECK(ta.targetMm() == 46);
  CHECK(ta.targetPolarity() == navi_one::polarityAt(46));
}

static void test_baseline_not_trusted_before_minimum_span() {
  TargetAcquisition ta(45, 1, LEGACY_110MS);
  std::vector<Event> all;
  feed(ta, quietRun(5), &all);   // well under MIN_BASELINE_BINS / span
  CHECK(!ta.baselineEstablished());
}

static void test_baseline_estimation_from_quiet_samples() {
  TargetAcquisition ta(45, 1, LEGACY_110MS);
  std::vector<Event> all;
  feed(ta, quietRun(40), &all);  // 0.8 s, > MIN_BASELINE_SPAN_S
  CHECK(ta.baselineEstablished());
  CHECK(std::fabs(ta.baselineLevel() - BASELINE_LEVEL) < 5.0);
}

static void test_correct_polarity_curve_is_acquired_and_closes() {
  TargetAcquisition ta(45, 1, LEGACY_110MS);  // target MM46, polarity South(0)
  CHECK(ta.targetPolarity() == 0);
  std::vector<Event> all; std::vector<Event> completed;
  feed(ta, quietRun(40), &all);
  const int sign = navi_one::polarityAt(ta.targetMm());
  const double amp = sign == 0 ? -140.0 : 140.0;  // South departs below baseline
  auto obs = curveRun(40, 0.8, amp, 0.2);
  auto tail = quietRun(20, BASELINE_LEVEL, 1.0, 0.8 + 40 * DT, 90, 2);
  obs.insert(obs.end(), tail.begin(), tail.end());
  feed(ta, obs, &all, &completed);
  CHECK(hasKind(all, EventKind::OPEN_CANDIDATE));
  CHECK(hasKind(all, EventKind::ACQUIRED));
  CHECK(hasKind(all, EventKind::CLOSE_AND_ADVANCE));
  CHECK(completed.size() == 1);
  CHECK(ta.lastConfirmedMm() == 46);
  CHECK(ta.targetMm() == 47);
}

static void test_wrong_polarity_disturbance_is_preserved_not_advanced() {
  TargetAcquisition ta(45, 1, LEGACY_110MS);  // target South(0): expect departure below
  std::vector<Event> all;
  feed(ta, quietRun(40), &all);
  auto obs = curveRun(40, 0.8, 140.0, 0.2);  // wrong sign: North-shaped
  auto tail = quietRun(20, BASELINE_LEVEL, 1.0, 0.8 + 40 * DT, 90, 3);
  obs.insert(obs.end(), tail.begin(), tail.end());
  feed(ta, obs, &all);
  CHECK(hasKind(all, EventKind::CONTRADICTION));
  CHECK(!hasKind(all, EventKind::CLOSE_AND_ADVANCE));
  CHECK(ta.lastConfirmedMm() == 45);
  CHECK(ta.targetMm() == 46);
}

static void test_short_noise_excursion_below_duration_is_disposed_not_contradiction() {
  TargetAcquisition ta(45, 1, LEGACY_110MS);
  std::vector<Event> all;
  feed(ta, quietRun(40), &all);
  // Correct sign, but far too brief to ever reach the 110ms floor.
  auto obs = curveRun(4, 0.8, -140.0, 0.04);
  auto tail = quietRun(20, BASELINE_LEVEL, 1.0, 0.8 + 4 * DT, 90, 4);
  obs.insert(obs.end(), tail.begin(), tail.end());
  feed(ta, obs, &all);
  CHECK(!hasKind(all, EventKind::ACQUIRED));
  CHECK(!hasKind(all, EventKind::CLOSE_AND_ADVANCE));
  CHECK(ta.targetMm() == 46);
}

static void test_no_advancement_before_closure() {
  TargetAcquisition ta(45, 1, LEGACY_110MS);
  std::vector<Event> all;
  feed(ta, quietRun(40), &all);
  auto obs = curveRun(40, 0.8, -140.0, 0.2);
  feed(ta, obs, &all);  // curve still open -- no quiet return fed yet
  CHECK(ta.lastConfirmedMm() == 45);
  CHECK(ta.candidateOpen());
}

static void test_extended_acquisition_has_no_automatic_timeout() {
  TargetAcquisition ta(45, 1, LEGACY_110MS);
  std::vector<Event> all; std::vector<Event> completed;
  feed(ta, quietRun(40), &all);
  auto obs = curveRun(400, 0.8, -140.0, 2.0);  // 8 s open
  auto tail = quietRun(20, BASELINE_LEVEL, 1.0, 0.8 + 400 * DT, 90, 5);
  obs.insert(obs.end(), tail.begin(), tail.end());
  feed(ta, obs, &all, &completed);
  CHECK(completed.size() == 1);
  CHECK(ta.lastConfirmedMm() == 46);
  CHECK(!hasKind(all, EventKind(99)));  // no LOST/TIMEOUT kind exists in this enum at all
}

static void test_stationary_samples_do_not_invent_progression() {
  TargetAcquisition ta(45, 1, LEGACY_110MS);
  std::vector<Event> all;
  feed(ta, quietRun(40), &all);
  std::vector<Sample> stall;
  for (int i = 0; i < 300; ++i) stall.push_back({0.8 + i * DT, BASELINE_LEVEL + 500.0, 0});
  feed(ta, stall, &all);
  CHECK(ta.targetMm() == 46);
  CHECK(!ta.candidateOpen());
}

static void test_stop_mid_candidate_preserves_evidence() {
  TargetAcquisition ta(45, 1, LEGACY_110MS);
  std::vector<Event> all;
  feed(ta, quietRun(40), &all);
  auto obs = curveRun(20, 0.8, -140.0, 0.3);
  feed(ta, obs, &all);
  CHECK(ta.candidateOpen());
  std::vector<Sample> stall;
  for (int i = 0; i < 100; ++i) stall.push_back({0.8 + (20 + i) * DT, 9999.0, 0});  // garbage while PWM=0
  feed(ta, stall, &all);
  CHECK(ta.candidateOpen());  // PWM=0 bins must not discard it
  auto obs2 = curveRun(20, 0.8 + 20 * DT, -140.0, 0.3, BASELINE_LEVEL, 90, 9);
  auto tail = quietRun(20, BASELINE_LEVEL, 1.0, 0.8 + 40 * DT, 90, 6);
  obs2.insert(obs2.end(), tail.begin(), tail.end());
  feed(ta, obs2, &all);
  CHECK(ta.lastConfirmedMm() == 46);
}

static void test_contradicting_evidence_is_never_relabeled() {
  TargetAcquisition ta(45, 1, LEGACY_110MS);
  std::vector<Event> all;
  feed(ta, quietRun(40), &all);
  auto obs = curveRun(40, 0.8, 140.0, 0.2);
  auto tail = quietRun(20, BASELINE_LEVEL, 1.0, 0.8 + 40 * DT, 90, 7);
  obs.insert(obs.end(), tail.begin(), tail.end());
  feed(ta, obs, &all);
  bool found = false;
  for (const auto& e : all) if (e.kind == EventKind::CONTRADICTION) { CHECK(e.target_mm == 46); found = true; }
  CHECK(found);
}

static void test_causal_no_future_sample_access() {
  TargetAcquisition ta(45, 1, LEGACY_110MS);
  auto obs = quietRun(40);
  auto curve = curveRun(40, 0.8, -140.0, 0.2);
  obs.insert(obs.end(), curve.begin(), curve.end());
  auto tail = quietRun(20, BASELINE_LEVEL, 1.0, 0.8 + 40 * DT, 90, 8);
  obs.insert(obs.end(), tail.begin(), tail.end());
  double tSeen = 0.0;
  for (const auto& s : obs) {
    Event out[2];
    const int n = ta.observe(s.t, s.y, s.pwm, out);
    tSeen = s.t;
    for (int i = 0; i < n; ++i) CHECK(out[i].t <= tSeen + 1e-9);
  }
}

static void test_frozen_reference_does_not_falsely_close_a_genuine_long_stall() {
  // Regression test for the 2026-10-22 corrective investigation's item 1
  // (Trial 0, MM129, a real 284.58 s open candidate): locks in the frozen
  // (not continuously-extrapolated) baseline reference.
  TargetAcquisition ta(45, 1, LEGACY_110MS);
  std::vector<Event> all; std::vector<Event> completed;
  feed(ta, quietRun(40), &all);
  auto obs = curveRun(20, 0.8, -140.0, 0.3);  // rising departure
  for (int i = 0; i < 5000; ++i)
    obs.push_back({0.8 + (20 + i) * DT, BASELINE_LEVEL - 80.0 + (i * 0.0003), 90});  // 100s plateau, slow creep
  feed(ta, obs, &all, &completed);
  CHECK(completed.empty());
  CHECK(ta.candidateOpen());
  CHECK(ta.lastConfirmedMm() == 45);
}

int main() {
  test_target_is_deterministic_next_marker();
  test_baseline_not_trusted_before_minimum_span();
  test_baseline_estimation_from_quiet_samples();
  test_correct_polarity_curve_is_acquired_and_closes();
  test_wrong_polarity_disturbance_is_preserved_not_advanced();
  test_short_noise_excursion_below_duration_is_disposed_not_contradiction();
  test_no_advancement_before_closure();
  test_extended_acquisition_has_no_automatic_timeout();
  test_stationary_samples_do_not_invent_progression();
  test_stop_mid_candidate_preserves_evidence();
  test_contradicting_evidence_is_never_relabeled();
  test_causal_no_future_sample_access();
  test_frozen_reference_does_not_falsely_close_a_genuine_long_stall();
  if (g_failures) { std::fprintf(stderr, "%d CHECK(S) FAILED\n", g_failures); return 1; }
  std::printf("13 tests, all passing (ported 1:1 from test_target_acquisition.py, commit 6efc952)\n");
  return 0;
}
