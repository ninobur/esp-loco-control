/*
 * NAVI_EYES_WIDE_OPEN — clean-slate observation build.
 *
 * X22R is intentionally absent. Hall and IR are evidence sources. NAVI is
 * the sole navigation authority. This sketch is a hardware integration shell;
 * credentials and motor policy must be supplied by the reviewed target profile.
 */
#include <Arduino.h>

#include "NaviEvidence.h"
#include "NaviCore.h"
#include "SpatialHallReference.h"

using namespace navi_eyes;

static constexpr uint8_t HALL_PIN = 33;
static constexpr uint8_t IR_PIN = 34;
static constexpr uint16_t SPATIAL_BIN_MM = 25;

static SpatialHallReference<> spatialReference(SPATIAL_BIN_MM);
static NaviCore navi(40);
static uint32_t sampleSerial = 0;

// The loop cadence is acquisition scheduling only. It is not a navigation
// interval, timeout, refractory period, or baseline eligibility test.
void setup() {
  Serial.begin(115200);
  pinMode(HALL_PIN, INPUT);
  pinMode(IR_PIN, INPUT);
  Serial.println("READY NAVI_EYES_WIDE_OPEN OBSERVE_ONLY NOT_FIELD_ACCEPTED");
}

void loop() {
  HallSample sample;
  sample.sampleSerial = sampleSerial++;
  sample.timestampUs = micros();
  sample.raw = static_cast<int16_t>(analogRead(HALL_PIN));
  sample.irPulses = 0;  // Replace with the installed IR observer's cumulative fact.
  sample.irDistanceMm = 0;
  sample.irHealth = IrHealth::Unknown;
  sample.pwm = 0;
  sample.direction = 1;

  // This is a physical observation. It is not declared RTB or a magnet.
  spatialReference.add(sample.irDistanceMm, sample.raw);

  HallObservation observation;
  observation.firstSample = sample.sampleSerial;
  observation.lastSample = sample.sampleSerial;
  observation.sampleCount = 1;
  observation.samples = &sample;
  observation.irHealth = sample.irHealth;

  NaviEvidence evidence;
  evidence.hall = observation;
  evidence.spatialBaselineAvailable = spatialReference.ready();
  evidence.spatialBaselineCandidate = spatialReference.candidate();
  evidence.spatialBaselineBins = static_cast<uint16_t>(spatialReference.occupiedBins());
  evidence.motivePwmZero = sample.pwm == 0;
  evidence.operatorMoved = false;

  const NaviJudgment judgment = navi.observe(evidence, sample.sampleSerial);
  Serial.printf("OBS serial=%lu hall=%d ir_pulses=%lu ir_mm=%lu base=%d bins=%u decision=%u reason=%s nav=%u\n",
                static_cast<unsigned long>(sample.sampleSerial), sample.raw,
                static_cast<unsigned long>(sample.irPulses),
                static_cast<unsigned long>(sample.irDistanceMm),
                evidence.spatialBaselineCandidate, evidence.spatialBaselineBins,
                static_cast<unsigned>(judgment.decision), judgment.reason,
                judgment.navMm);
  delay(1);  // scheduler pacing only; no decision depends on this delay.
}
