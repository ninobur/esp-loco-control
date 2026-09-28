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

  NaviCore noReference(40);
  HallSample noReferenceSample{30, 2000, 170, 0, 0, IrHealth::Unknown, 0, 1};
  HallObservation noReferenceObservation{30, 30, 1, &noReferenceSample};
  NaviEvidence noReferenceEvidence;
  noReferenceEvidence.hall = noReferenceObservation;
  noReference.observe(noReferenceEvidence, noReferenceSample.sampleSerial);
  check(!noReference.opening().candidate && !noReference.opening().confirmed,
        "no opening is recognized before a Hall reference exists");

  auto send = [](NaviCore& navi, HallSample& sample) {
    HallObservation observation{sample.sampleSerial, sample.sampleSerial, 1,
                                &sample};
    observation.irDistanceMm = 10;
    NaviEvidence evidence;
    evidence.hall = observation;
    navi.observe(evidence, sample.sampleSerial);
  };
  auto establishReference = [&send](NaviCore& navi, uint32_t serial) {
    HallSample samples[] = {
        {serial, 3000, 100, 0, 1, IrHealth::Unknown, 0, 1},
        {serial + 1, 3001, 100, 0, 5, IrHealth::Unknown, 0, 1},
        {serial + 2, 3002, 100, 0, 10, IrHealth::Unknown, 0, 1},
    };
    for (HallSample& sample : samples) send(navi, sample);
  };

  NaviCore openingNavi(40);
  establishReference(openingNavi, 40);
  check(openingNavi.initialHallReference() == 100,
        "opening tests have an established Hall reference");

  HallSample below70{50, 3010, 169, 0, 10, IrHealth::Unknown, 0, 1};
  send(openingNavi, below70);
  check(openingNavi.lastHallDeparture() == 69 &&
            openingNavi.qualifyingHallObservationCount() == 0,
        "departure below 70 is nonqualifying");

  HallSample exactly70{51, 3011, 170, 0, 10, IrHealth::Unknown, 0, 1};
  send(openingNavi, exactly70);
  check(openingNavi.qualifyingHallObservationCount() == 1 &&
            !openingNavi.opening().candidate,
        "exactly 70 qualifies but one observation does not create a candidate");

  HallSample reset{52, 3012, 169, 0, 10, IrHealth::Unknown, 0, 1};
  send(openingNavi, reset);
  check(openingNavi.qualifyingHallObservationCount() == 0,
        "nonqualifying observation resets routine qualification");

  HallSample candidateFirst{53, 3013, 182, 0, 10, IrHealth::Unknown, 0, 1};
  HallSample candidateSecond{54, 3014, 191, 0, 10, IrHealth::Unknown, 0, 1};
  send(openingNavi, candidateFirst);
  send(openingNavi, candidateSecond);
  check(openingNavi.opening().candidate &&
            !openingNavi.opening().confirmed &&
            openingNavi.opening().candidateObservationSerial ==
                candidateSecond.sampleSerial &&
            openingNavi.opening().polarity == HallOpeningPolarity::Unknown,
        "70x2 creates only an unpolarized candidate");
  check(openingNavi.confirmationSampleCount() == 0,
        "the two candidate-opening samples are excluded from confirmation");

  HallSample rising[] = {
      {55, 3015, 205, 0, 10, IrHealth::Unknown, 0, 1},
      {56, 3016, 215, 0, 10, IrHealth::Unknown, 0, 1},
      {57, 3017, 225, 0, 10, IrHealth::Unknown, 0, 1},
  };
  send(openingNavi, rising[0]);
  check(openingNavi.confirmationSampleCount() == 1 &&
            !openingNavi.opening().confirmed,
        "first new qualifying sample starts confirmation");
  send(openingNavi, rising[1]);
  check(openingNavi.confirmationSampleCount() == 2 &&
            !openingNavi.opening().confirmed,
        "second new rising sample does not yet confirm");
  send(openingNavi, rising[2]);
  check(openingNavi.opening().confirmed &&
            openingNavi.opening().observationSerial == rising[2].sampleSerial &&
            openingNavi.opening().polarity == HallOpeningPolarity::AboveReference,
        "three new rising samples confirm the magnet and polarity");

  HallSample laterOpposite{58, 3018, 20, 0, 10, IrHealth::Unknown, 0, 1};
  send(openingNavi, laterOpposite);
  check(openingNavi.lastHallSamples()->sampleSerial == laterOpposite.sampleSerial &&
            openingNavi.lastHallSamples()->raw == laterOpposite.raw,
        "Hall observations remain delivered after confirmation");
  check(openingNavi.opening().polarity == HallOpeningPolarity::AboveReference,
        "later opposite Hall behavior cannot overwrite confirmed polarity");

  NaviCore fallingNavi(40);
  establishReference(fallingNavi, 70);
  HallSample fallingCandidate[] = {
      {80, 3030, 182, 0, 10, IrHealth::Unknown, 0, 1},
      {81, 3031, 191, 0, 10, IrHealth::Unknown, 0, 1},
  };
  send(fallingNavi, fallingCandidate[0]);
  send(fallingNavi, fallingCandidate[1]);
  HallSample falling[] = {
      {82, 3032, 18, 0, 10, IrHealth::Unknown, 0, 1},
      {83, 3033, 8, 0, 10, IrHealth::Unknown, 0, 1},
      {84, 3034, -2, 0, 10, IrHealth::Unknown, 0, 1},
  };
  for (HallSample& sample : falling) send(fallingNavi, sample);
  check(fallingNavi.opening().confirmed &&
            fallingNavi.opening().polarity == HallOpeningPolarity::BelowReference,
        "three new falling samples confirm falling polarity");

  NaviCore transientNavi(40);
  establishReference(transientNavi, 90);
  HallSample transientCandidate[] = {
      {100, 3040, 182, 0, 10, IrHealth::Unknown, 0, 1},
      {101, 3041, 191, 0, 10, IrHealth::Unknown, 0, 1},
  };
  send(transientNavi, transientCandidate[0]);
  send(transientNavi, transientCandidate[1]);
  HallSample lowSamples[] = {
      {102, 3042, 40, 0, 10, IrHealth::Unknown, 0, 1},
      {103, 3043, 50, 0, 10, IrHealth::Unknown, 0, 1},
      {104, 3044, 40, 0, 10, IrHealth::Unknown, 0, 1},
  };
  for (HallSample& sample : lowSamples) send(transientNavi, sample);
  check(!transientNavi.opening().candidate &&
            !transientNavi.opening().confirmed &&
            transientNavi.transientSampleCount() == 0,
        "three consecutive low samples abandon the candidate as transient");

  HallSample routineAfterTransient[] = {
      {105, 3045, 170, 0, 10, IrHealth::Unknown, 0, 1},
      {106, 3046, 180, 0, 10, IrHealth::Unknown, 0, 1},
  };
  send(transientNavi, routineAfterTransient[0]);
  send(transientNavi, routineAfterTransient[1]);
  check(transientNavi.opening().candidate &&
            !transientNavi.opening().confirmed,
        "transient abandonment returns to routine 70x2 detection");

  NaviCore equivocalNavi(40);
  establishReference(equivocalNavi, 110);
  HallSample equivocalCandidate[] = {
      {120, 3050, 182, 0, 10, IrHealth::Unknown, 0, 1},
      {121, 3051, 191, 0, 10, IrHealth::Unknown, 0, 1},
  };
  send(equivocalNavi, equivocalCandidate[0]);
  send(equivocalNavi, equivocalCandidate[1]);
  HallSample brokenMonotonic[] = {
      {122, 3052, 205, 0, 10, IrHealth::Unknown, 0, 1},
      {123, 3053, 195, 0, 10, IrHealth::Unknown, 0, 1},
  };
  send(equivocalNavi, brokenMonotonic[0]);
  send(equivocalNavi, brokenMonotonic[1]);
  check(equivocalNavi.opening().candidate,
        "a broken high pattern keeps the candidate active");
  check(!equivocalNavi.opening().confirmed,
        "a broken high pattern does not confirm the magnet");
  check(equivocalNavi.confirmationSampleCount() == 1,
        "a broken high pattern restarts the high sequence");
  check(equivocalNavi.transientSampleCount() == 0,
        "a broken high pattern is not counted as a negative");

  std::printf("%s: %d failures\n", failures ? "FAIL" : "PASS", failures);
  return failures ? 1 : 0;
}
