#include "../NaviCore.h"

#include <cstdio>

using namespace navi_eyes;
static int failures = 0;
static void check(bool ok, const char* msg) {
  if (!ok) { ++failures; std::printf("FAIL: %s\n", msg); }
}

static void send(NaviCore& navi, HallSample& sample, uint32_t distanceMm) {
  sample.irDistanceMm = distanceMm;
  HallObservation observation{sample.sampleSerial, sample.sampleSerial, 1,
                              &sample};
  observation.irDistanceMm = distanceMm;
  NaviEvidence evidence;
  evidence.hall = observation;
  navi.observe(evidence, sample.sampleSerial);
}

static void establishReference(NaviCore& navi, uint32_t serial = 1) {
  HallSample samples[] = {
      {serial, 3000, 100, 3, 1, IrHealth::AdequateContrast, 0, 1},
      {serial + 1, 3001, 100, 4, 5, IrHealth::InadequateContrast, 0, 1},
      {serial + 2, 3002, 100, 5, 10, IrHealth::Failed, 0, 0},
  };
  for (HallSample& sample : samples) send(navi, sample, sample.irDistanceMm);
}

static void sendReadings(NaviCore& navi, uint32_t serial, const int16_t (&raw)[5],
                         uint32_t distanceMm) {
  for (size_t i = 0; i < 5; ++i) {
    HallSample sample{serial + static_cast<uint32_t>(i),
                      4000 + static_cast<uint32_t>(i), raw[i], 0,
                      distanceMm, IrHealth::Unknown, 0, 1};
    send(navi, sample, distanceMm);
  }
}

int main() {
  // Function 1 and Function 2 remain unchanged.
  NaviCore initial(40);
  check(!initial.initialHallReferenceAvailable(),
        "NAVI starts without an initial Hall reference");
  HallSample stationary{1, 900, 1500, 0, 0, IrHealth::Unknown, 0, 1};
  send(initial, stationary, 0);
  stationary.timestampUs = 999999;
  send(initial, stationary, 0);
  check(!initial.initialReferenceCollectionStarted(),
        "stationary time does not start the initial collection");

  HallSample factual{10, 1000, 120, 3, 1, IrHealth::AdequateContrast, 7, 1};
  send(initial, factual, 0);
  check(initial.lastHallSamples()->sampleSerial == factual.sampleSerial &&
            initial.lastHallSamples()->timestampUs == factual.timestampUs &&
            initial.lastHallSamples()->raw == factual.raw &&
            initial.lastHallSamples()->irPulses == factual.irPulses &&
            initial.lastHallSamples()->irDistanceMm == factual.irDistanceMm &&
            initial.lastHallSamples()->irHealth == factual.irHealth &&
            initial.lastHallSamples()->pwm == factual.pwm &&
            initial.lastHallSamples()->direction == factual.direction,
        "native Hall and IR facts reach NAVI unchanged");
  HallSample boot[] = {
      {20, 1001, 100, 0, 4, IrHealth::Unknown, 0, 1},
      {21, 1002, 200, 0, 7, IrHealth::Unknown, 0, 1},
      {22, 1003, 300, 0, 10, IrHealth::Unknown, 0, 1},
  };
  for (HallSample& sample : boot) send(initial, sample, sample.irDistanceMm);
  check(initial.initialHallReferenceAvailable() &&
            initial.initialHallReference() == 200,
        "NAVI establishes the Function 2 initial median at 10 mm");

  // Five raw readings are maintained inside NAVI; two outliers do not control
  // the median. The expected target is AboveReference.
  NaviCore positive(40);
  establishReference(positive, 40);
  positive.configureExpectedTarget(
      {HallOpeningPolarity::AboveReference, 100, 1, 1});
  const int16_t positiveRaw[] = {180, 20, 190, 30, 200};
  sendReadings(positive, 50, positiveRaw, 100);
  check(positive.rollingHallSampleCount() == 5 &&
            positive.rollingHallMedian() == 180,
        "NAVI calculates a rolling median of five raw Hall readings");
  check(positive.hallSupportsTarget(),
        "positive median departure of at least 70 supports the target");
  check(positive.expectedTargetConfirmed(),
        "Hall support plus coherent IR and context confirms the target");
  check(positive.lastConfirmedOpening().landmarkObservationSerial == 50,
        "landmark is the earliest expected-direction raw threshold sample");

  NaviCore negative(40);
  establishReference(negative, 70);
  negative.configureExpectedTarget(
      {HallOpeningPolarity::BelowReference, 100, 2, 1});
  const int16_t negativeRaw[] = {20, -80, -90, -100, 30};
  sendReadings(negative, 80, negativeRaw, 100);
  check(negative.rollingHallMedian() == 20 || negative.rollingHallMedian() == -80,
        "negative window median is calculated from the five native readings");
  check(negative.hallSupportsTarget() && negative.expectedTargetConfirmed(),
        "negative median departure of at most -70 supports the target");

  NaviCore opposite(40);
  establishReference(opposite, 100);
  opposite.configureExpectedTarget(
      {HallOpeningPolarity::BelowReference, 100, 3, 1});
  const int16_t oppositeRaw[] = {180, 185, 190, 195, 200};
  sendReadings(opposite, 110, oppositeRaw, 100);
  check(!opposite.hallSupportsTarget() && !opposite.expectedTargetConfirmed(),
        "opposite-polarity median does not support the target");
  check(opposite.rollingHallSampleCount() == 5,
        "two or fewer readings do not create an old candidate state");

  // A Hall-supported window at the wrong physical distance is retained but
  // cannot confirm or begin Function 4.
  NaviCore wrongDistance(40);
  establishReference(wrongDistance, 130);
  wrongDistance.configureExpectedTarget(
      {HallOpeningPolarity::AboveReference, 100, 4, 1});
  sendReadings(wrongDistance, 140, positiveRaw, 50);
  check(wrongDistance.hallSupportsTarget() &&
            !wrongDistance.expectedTargetConfirmed() &&
            !wrongDistance.spatialReferenceActive(),
        "Hall support at the wrong IR distance cannot confirm the target");
  sendReadings(wrongDistance, 150, positiveRaw, 100);
  check(wrongDistance.expectedTargetConfirmed(),
        "NAVI continues seeking and later confirms within the open interval");

  // Inconsistent evidence does not infer another location or alter position.
  check(wrongDistance.navMm() == 40,
        "inconsistent evidence preserves NAVI position/context");

  // Missing-target advancement preserves position and selects the next map
  // target without alternative-position inference.
  NaviCore missing(40);
  establishReference(missing, 180);
  missing.configureExpectedTarget(
      {HallOpeningPolarity::AboveReference, 100, 8, 1}, 0);
  missing.configureSubsequentTarget(
      {HallOpeningPolarity::BelowReference, 120, 9, 1});
  HallSample passed{190, 5000, 100, 0, 117, IrHealth::Unknown, 0, 1};
  send(missing, passed, 117);
  check(missing.missingTargetCount() == 1 && missing.navMm() == 40 &&
            missing.expectedTarget().sequence == 9 &&
            missing.expectedTarget().polarity == HallOpeningPolarity::BelowReference,
        "passing the interval records missing and advances target context");

  // The confirmed target starts Function 4 at the retained landmark, not at
  // the later median-confirmation sample.
  NaviCore spatial(40);
  establishReference(spatial, 220);
  spatial.configureExpectedTarget(
      {HallOpeningPolarity::AboveReference, 100, 10, 1});
  const int16_t landmarkRaw[] = {20, 180, 20, 190, 200};
  sendReadings(spatial, 230, landmarkRaw, 90);
  check(spatial.expectedTargetConfirmed() && spatial.spatialReferenceActive() &&
            spatial.spatialReferenceOriginMm() == 90 &&
            spatial.lastConfirmedOpening().landmarkObservationSerial == 231,
        "Function 4 uses the retained median-window opening landmark");

  HallSample clearance{240, 5100, 500, 0, 150, IrHealth::Unknown, 0, 1};
  send(spatial, clearance, 150);
  check(spatial.spatialReferenceSampleCount() == 0,
        "0-100 mm remains clearance");
  HallSample refSamples[] = {
      {241, 5101, 100, 0, 190, IrHealth::Unknown, 0, 1},
      {242, 5102, 110, 0, 210, IrHealth::Unknown, 0, 1},
      {243, 5103, 120, 0, 240, IrHealth::Unknown, 0, 1},
      {244, 5104, 900, 0, 240, IrHealth::Unknown, 0, 1},
      {245, 5105, 130, 0, 270, IrHealth::Unknown, 0, 1},
      {246, 5106, 140, 0, 290, IrHealth::Unknown, 0, 1},
  };
  for (HallSample& sample : refSamples) send(spatial, sample, sample.irDistanceMm);
  check(!spatial.spatialReferenceActive() && spatial.activeHallReference() == 120,
        "100-200 mm collection completes with its spatial median at 200 mm");
  check(spatial.rollingHallSampleCount() == 0,
        "active-reference replacement clears incompatible Hall window history");

  // Target changes and context/reference transitions clear incompatible Hall
  // history without changing NAVI position.
  NaviCore context(40);
  establishReference(context, 260);
  context.configureExpectedTarget(
      {HallOpeningPolarity::AboveReference, 100, 11, 1});
  HallSample one{270, 6000, 180, 0, 50, IrHealth::Unknown, 0, 1};
  send(context, one, 50);
  check(context.rollingHallSampleCount() == 1, "window retains native history");
  context.configureExpectedTarget(
      {HallOpeningPolarity::BelowReference, 120, 12, 1});
  check(context.rollingHallSampleCount() == 0 && context.navMm() == 40,
        "target-context change clears incompatible rolling history only");
  context.resetHallTargetContext();
  check(context.rollingHallSampleCount() == 0,
        "explicit navigation-context reset clears the rolling window");

  std::printf("%s: %d failures\n", failures ? "FAIL" : "PASS", failures);
  return failures ? 1 : 0;
}
