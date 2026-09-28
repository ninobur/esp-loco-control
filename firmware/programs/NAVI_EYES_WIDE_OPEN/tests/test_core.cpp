#include "../NaviCore.h"

#include <cstdio>

using namespace navi_eyes;
static int failures = 0;
static void check(bool ok, const char* msg) { if (!ok) { ++failures; std::printf("FAIL: %s\n", msg); } }

int main() {
  NaviCore navi(40);
  check(!navi.initialHallReferenceAvailable(),
        "NAVI starts without an initial Hall reference");

  HallSample stationarySample{1, 900, 1500, 0, 0, IrHealth::Unknown, 0, 1};
  HallObservation stationaryObservation{1, 1, 1, &stationarySample};
  NaviEvidence stationaryEvidence;
  stationaryEvidence.hall = stationaryObservation;
  navi.observe(stationaryEvidence, 1);
  check(!navi.initialHallReferenceAvailable(),
        "stationary Hall observation does not establish a reference");
  check(!navi.initialReferenceCollectionStarted(),
        "stationary Hall observation does not start collection");
  stationarySample.timestampUs = 999999;
  navi.observe(stationaryEvidence, 2);
  check(!navi.initialReferenceCollectionStarted(),
        "stationary elapsed time does not start collection");

  HallSample samples[] = {
      {17, 1001, 100, 3, 1, IrHealth::AdequateContrast, 0, 1},
      {18, 1002, 300, 4, 1, IrHealth::InadequateContrast, 0, 1},
      {19, 1003, 200, 5, 4, IrHealth::Failed, 0, 0},
      {20, 1004, 250, 6, 4, IrHealth::Unknown, 0, 1},
      {21, 1005, 400, 7, 10, IrHealth::Unknown, 0, 1},
  };
  const uint32_t travelMm[] = {1, 1, 4, 4, 10};
  for (size_t i = 0; i < 5; ++i) {
    const HallSample& sample = samples[i];
    HallObservation observation;
    observation.firstSample = sample.sampleSerial;
    observation.lastSample = sample.sampleSerial;
    observation.sampleCount = 1;
    observation.samples = &sample;
    observation.irDistanceMm = travelMm[i];

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
    check(navi.lastObservedIrDistanceMm() == travelMm[i],
          "NAVI receives the cumulative IR distance unchanged");

    if (i < 4) {
      check(!navi.initialHallReferenceAvailable(),
            "initial reference remains unavailable before 10 mm");
    }
  }

  check(navi.initialReferenceCollectionStarted(),
        "IR movement starts the initial-reference collection");
  check(navi.initialReferenceSampleCount() == 5,
        "all native Hall observations in the 10 mm interval are collected");
  check(navi.initialHallReferenceAvailable(),
        "10 mm of IR-measured travel completes the initial reference");
  check(navi.initialHallReference() == 250,
        "initial reference is the median of collected Hall observations");

  HallSample heldSample{22, 1006, 1900, 8, 11, IrHealth::Unknown, 0, 1};
  HallObservation heldObservation{22, 22, 1, &heldSample};
  heldObservation.irDistanceMm = 11;
  NaviEvidence heldEvidence;
  heldEvidence.hall = heldObservation;
  heldEvidence.motivePwmZero = true;
  NaviJudgment held = navi.observe(heldEvidence, heldSample.sampleSerial);
  check(held.decision == NaviDecision::Hold, "NAVI sees PWM-zero as context");
  check(held.navMm == 40, "observation cannot advance position upstream");
  check(navi.initialHallReference() == 250,
        "subsequent Hall observations do not change the initial reference");
  check(navi.initialReferenceSampleCount() == 5,
        "subsequent Hall observations do not extend the completed collection");

  HallSample stationaryAfterReference{23, 1007, 500, 9, 11, IrHealth::Unknown, 0, 1};
  HallObservation stationaryAfterObservation{23, 23, 1, &stationaryAfterReference};
  NaviEvidence stationaryAfterEvidence;
  stationaryAfterEvidence.hall = stationaryAfterObservation;
  navi.observe(stationaryAfterEvidence, stationaryAfterReference.sampleSerial);
  check(navi.initialHallReference() == 250,
        "stationary time does not change the established reference");

  std::printf("%s: %d failures\n", failures ? "FAIL" : "PASS", failures);
  return failures ? 1 : 0;
}
