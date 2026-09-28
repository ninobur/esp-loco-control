#include "../SpatialHallReference.h"
#include "../NaviCore.h"

#include <cstdio>

using namespace navi_eyes;
static int failures = 0;
static void check(bool ok, const char* msg) { if (!ok) { ++failures; std::printf("FAIL: %s\n", msg); } }

int main() {
  SpatialHallReference<16, 8> reference(25);
  // The stationary shelf gets many samples but only one spatial bin.
  for (uint32_t i = 0; i < 100; ++i) reference.add(0, 2000);
  for (uint32_t i = 1; i <= 8; ++i) reference.add(i * 25, 1825 + static_cast<int16_t>(i % 3));
  check(reference.ready(), "spatial reference becomes available");
  check(reference.occupiedBins() == 9, "distance, not sample count, defines bins");
  check(reference.candidate() < 1900, "stationary shelf does not dominate moving bins");
  check(reference.candidate() > 1800, "moving ordinary-track level is preserved");

  HallSample sample;
  sample.raw = 1900;
  HallObservation observation;
  observation.samples = &sample;
  observation.sampleCount = 1;
  NaviEvidence evidence;
  evidence.hall = observation;
  evidence.spatialBaselineAvailable = true;
  evidence.spatialBaselineCandidate = reference.candidate();
  evidence.motivePwmZero = true;
  NaviCore navi(40);
  NaviJudgment held = navi.observe(evidence, 7);
  check(held.decision == NaviDecision::Hold, "NAVI sees PWM-zero as context");
  check(held.navMm == 40, "observation cannot advance position upstream");
  evidence.motivePwmZero = false;
  NaviJudgment observed = navi.observe(evidence, 8);
  check(observed.decision == NaviDecision::Observe, "NAVI receives unjudged evidence");
  check(observed.observationSerial == 8, "observation identity is preserved");

  std::printf("%s: %d failures\n", failures ? "FAIL" : "PASS", failures);
  return failures ? 1 : 0;
}
