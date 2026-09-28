#include "../NaviCore.h"

#include <cstdio>

using namespace navi_eyes;
static int failures = 0;
static void check(bool ok, const char* msg) { if (!ok) { ++failures; std::printf("FAIL: %s\n", msg); } }

int main() {
  NaviCore navi(40);
  HallSample samples[] = {
      {17, 1001, 1900, 3, 25, IrHealth::AdequateContrast, 42, 1},
      {18, 1002, 1901, 4, 50, IrHealth::InadequateContrast, 42, 1},
      {19, 1003, 1899, 5, 75, IrHealth::Failed, 43, 0},
  };
  for (const HallSample& sample : samples) {
    HallObservation observation;
    observation.firstSample = sample.sampleSerial;
    observation.lastSample = sample.sampleSerial;
    observation.sampleCount = 1;
    observation.samples = &sample;

    NaviEvidence evidence;
    evidence.hall = observation;
    evidence.motivePwmZero = false;

    NaviJudgment judgment = navi.observe(evidence, sample.sampleSerial);
    check(judgment.observationSerial == sample.sampleSerial,
          "NAVI preserves observation order and serial");
    check(navi.lastHallSampleCount() == 1, "NAVI receives one native sample");
    check(navi.lastHallSamples()->sampleSerial == sample.sampleSerial,
          "NAVI receives the native sample serial unchanged");
    check(navi.lastHallSamples()->timestampUs == sample.timestampUs,
          "NAVI receives the native timestamp unchanged");
    check(navi.lastHallSamples()->raw == sample.raw,
          "NAVI receives the ADC value unchanged");
    check(navi.lastHallSamples()->irPulses == sample.irPulses &&
              navi.lastHallSamples()->irDistanceMm == sample.irDistanceMm &&
              navi.lastHallSamples()->irHealth == sample.irHealth &&
              navi.lastHallSamples()->pwm == sample.pwm &&
              navi.lastHallSamples()->direction == sample.direction,
          "NAVI receives factual metadata unchanged");
  }

  HallSample heldSample{20, 1004, 1900, 6, 100, IrHealth::Unknown, 0, 1};
  HallObservation heldObservation{20, 20, 1, &heldSample};
  NaviEvidence heldEvidence;
  heldEvidence.hall = heldObservation;
  heldEvidence.motivePwmZero = true;
  NaviJudgment held = navi.observe(heldEvidence, heldSample.sampleSerial);
  check(held.decision == NaviDecision::Hold, "NAVI sees PWM-zero as context");
  check(held.navMm == 40, "observation cannot advance position upstream");

  std::printf("%s: %d failures\n", failures ? "FAIL" : "PASS", failures);
  return failures ? 1 : 0;
}
