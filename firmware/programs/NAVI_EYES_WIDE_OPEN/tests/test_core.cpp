#include "../NaviCore.h"

#include <cstdio>

using namespace navi_eyes;
static int failures = 0;
static void check(bool ok, const char* msg) {
  if (!ok) { ++failures; std::printf("FAIL: %s\n", msg); }
}

static void send(NaviCore& navi, HallSample& sample, uint32_t distanceMm) {
  HallObservation observation{sample.sampleSerial, sample.sampleSerial, 1,
                              &sample};
  observation.irDistanceMm = distanceMm;
  NaviEvidence evidence;
  evidence.hall = observation;
  navi.observe(evidence, sample.sampleSerial);
}

static void establishReference(NaviCore& navi, uint32_t serial = 1) {
  HallSample samples[] = {
      {serial, 3000, 100, 0, 1, IrHealth::Unknown, 0, 1},
      {serial + 1, 3001, 100, 0, 5, IrHealth::Unknown, 0, 1},
      {serial + 2, 3002, 100, 0, 10, IrHealth::Unknown, 0, 1},
  };
  for (HallSample& sample : samples) send(navi, sample, sample.irDistanceMm);
}

static void candidate(NaviCore& navi, uint32_t serial, int16_t first,
                      int16_t second, uint32_t distanceMm) {
  HallSample samples[] = {
      {serial, 4000, first, 0, distanceMm, IrHealth::Unknown, 0, 1},
      {serial + 1, 4001, second, 0, distanceMm, IrHealth::Unknown, 0, 1},
  };
  for (HallSample& sample : samples) send(navi, sample, distanceMm);
}

int main() {
  NaviCore navi(40);
  check(!navi.initialHallReferenceAvailable(), "NAVI starts without reference");
  HallSample stationary{1, 900, 1500, 0, 0, IrHealth::Unknown, 0, 1};
  send(navi, stationary, 0);
  check(!navi.initialReferenceCollectionStarted(),
        "stationary evidence does not start initial collection");
  stationary.timestampUs = 999999;
  send(navi, stationary, 0);
  check(!navi.initialReferenceCollectionStarted(),
        "stationary time does not advance collection");

  HallSample initial[] = {
      {10, 1000, 100, 3, 1, IrHealth::AdequateContrast, 0, 1},
      {11, 1001, 300, 4, 4, IrHealth::InadequateContrast, 0, 1},
      {12, 1002, 200, 5, 7, IrHealth::Failed, 0, 0},
      {13, 1003, 250, 6, 10, IrHealth::Unknown, 0, 1},
  };
  for (HallSample& sample : initial) {
    send(navi, sample, sample.irDistanceMm);
    check(navi.lastHallSamples()->sampleSerial == sample.sampleSerial &&
              navi.lastHallSamples()->timestampUs == sample.timestampUs &&
              navi.lastHallSamples()->raw == sample.raw &&
              navi.lastHallSamples()->irPulses == sample.irPulses &&
              navi.lastHallSamples()->irDistanceMm == sample.irDistanceMm &&
              navi.lastHallSamples()->irHealth == sample.irHealth &&
              navi.lastHallSamples()->pwm == sample.pwm &&
              navi.lastHallSamples()->direction == sample.direction,
          "native Hall and factual metadata reach NAVI unchanged");
  }
  check(navi.initialHallReferenceAvailable() && navi.initialHallReference() == 225,
        "NAVI establishes the initial median at 10 mm");

  NaviCore threshold(40);
  establishReference(threshold, 20);
  threshold.configureExpectedTarget(
      {HallOpeningPolarity::AboveReference, 100, 1, 1});
  candidate(threshold, 30, 170, 180, 95);
  check(threshold.opening().candidate && !threshold.expectedTargetConfirmed(),
        "two qualifying samples create only a candidate");
  HallSample below70{32, 4010, 169, 0, 96, IrHealth::Unknown, 0, 1};
  send(threshold, below70, 96);
  check(!threshold.expectedTargetConfirmed(),
        "a sub-threshold third sample cannot confirm the target");

  NaviCore threeOfThree(40);
  establishReference(threeOfThree, 40);
  threeOfThree.configureExpectedTarget(
      {HallOpeningPolarity::AboveReference, 100, 1, 1});
  candidate(threeOfThree, 50, 182, 191, 95);
  HallSample thirdA{52, 4020, 205, 0, 100, IrHealth::Unknown, 0, 1};
  send(threeOfThree, thirdA, 100);
  check(threeOfThree.expectedTargetConfirmed(),
        "3/3 expected polarity confirms Hall evidence");

  NaviCore twoOfThree(40);
  establishReference(twoOfThree, 60);
  twoOfThree.configureExpectedTarget(
      {HallOpeningPolarity::AboveReference, 100, 1, 1});
  candidate(twoOfThree, 70, 182, 191, 95);
  HallSample mixed{72, 4021, 22, 0, 100, IrHealth::Unknown, 0, 1};
  send(twoOfThree, mixed, 100);
  check(twoOfThree.expectedTargetConfirmed(),
        "2/3 expected polarity confirms Hall evidence");

  NaviCore oneOfThree(40);
  establishReference(oneOfThree, 80);
  oneOfThree.configureExpectedTarget(
      {HallOpeningPolarity::AboveReference, 100, 1, 1});
  candidate(oneOfThree, 90, 182, -78, 95);
  HallSample below{92, 4022, -5, 0, 100, IrHealth::Unknown, 0, 1};
  send(oneOfThree, below, 100);
  check(!oneOfThree.expectedTargetConfirmed() && !oneOfThree.opening().confirmed,
        "1/3 expected polarity fails Hall evidence");

  NaviCore wrongDistance(40);
  establishReference(wrongDistance, 100);
  wrongDistance.configureExpectedTarget(
      {HallOpeningPolarity::AboveReference, 100, 1, 1});
  candidate(wrongDistance, 110, 182, 191, 50);
  HallSample wrongDistanceThird{112, 4030, 205, 0, 50, IrHealth::Unknown, 0, 1};
  send(wrongDistance, wrongDistanceThird, 50);
  check(!wrongDistance.expectedTargetConfirmed(),
        "Hall-consistent evidence at wrong IR distance is rejected");
  check(!wrongDistance.spatialReferenceActive(),
        "a rejected target does not start Function 4");
  HallSample stillSeeking{113, 4031, 100, 0, 100, IrHealth::Unknown, 0, 1};
  send(wrongDistance, stillSeeking, 100);
  check(!wrongDistance.expectedTargetMissing(),
        "NAVI continues seeking while target interval remains open");

  NaviCore coherent(40);
  establishReference(coherent, 120);
  coherent.configureExpectedTarget(
      {HallOpeningPolarity::AboveReference, 100, 7, 1});
  candidate(coherent, 130, 182, 191, 90);
  HallSample coherentThird{132, 4040, 205, 0, 100, IrHealth::Unknown, 0, 1};
  send(coherent, coherentThird, 100);
  check(coherent.expectedTargetConfirmed(),
        "coherent Hall and IR evidence confirms expected MM");
  check(coherent.lastConfirmedOpening().landmarkObservationSerial == 130 &&
            coherent.lastConfirmedOpening().landmarkIrDistanceMm == 90,
        "opening landmark is retained from the first qualifying sample");
  check(coherent.spatialReferenceActive() &&
            coherent.spatialReferenceOriginMm() == 90,
        "Function 4 uses the opening landmark, not confirmation distance");
  check(coherent.navMm() == 40,
        "target evidence does not infer or alter an alternative position");

  NaviCore missing(40);
  establishReference(missing, 150);
  missing.configureExpectedTarget(
      {HallOpeningPolarity::AboveReference, 100, 8, 1}, 0);
  missing.configureSubsequentTarget(
      {HallOpeningPolarity::BelowReference, 120, 9, 1});
  HallSample passedInterval{160, 4050, 100, 0, 117, IrHealth::Unknown, 0, 1};
  send(missing, passedInterval, 117);
  check(missing.missingTargetCount() == 1 &&
            missing.expectedTarget().sequence == 9 &&
            missing.expectedTarget().polarity == HallOpeningPolarity::BelowReference &&
            missing.navMm() == 40,
        "passing the interval records missing and advances target context");

  HallSample clearance{200, 5000, 500, 0, 150, IrHealth::Unknown, 0, 1};
  send(coherent, clearance, 150);
  check(coherent.spatialReferenceSampleCount() == 0,
        "0-100 mm clearance does not populate replacement reference");
  HallSample collection[] = {
      {201, 5001, 100, 0, 190, IrHealth::Unknown, 0, 1},
      {202, 5002, 110, 0, 210, IrHealth::Unknown, 0, 1},
      {203, 5003, 120, 0, 240, IrHealth::Unknown, 0, 1},
      {204, 5004, 900, 0, 240, IrHealth::Unknown, 0, 1},
      {205, 5005, 130, 0, 270, IrHealth::Unknown, 0, 1},
      {206, 5006, 140, 0, 290, IrHealth::Unknown, 0, 1},
  };
  send(coherent, collection[0], 190);
  send(coherent, collection[1], 210);
  send(coherent, collection[2], 240);
  send(coherent, collection[3], 240);
  check(coherent.spatialReferenceSampleCount() == 3,
        "100-200 mm collection represents physical locations once");
  send(coherent, collection[4], 270);
  check(coherent.spatialReferenceActive(),
        "reference remains pending before 200 mm");
  send(coherent, collection[5], 290);
  check(!coherent.spatialReferenceActive() && coherent.activeHallReference() == 120,
        "median becomes active at 200 mm");

  std::printf("%s: %d failures\n", failures ? "FAIL" : "PASS", failures);
  return failures ? 1 : 0;
}
