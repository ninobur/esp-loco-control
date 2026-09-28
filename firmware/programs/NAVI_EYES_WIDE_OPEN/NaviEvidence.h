#pragma once

#include <stdint.h>

namespace navi_eyes {

enum class IrHealth : uint8_t { Unknown, AdequateContrast, InadequateContrast, Failed };

struct HallSample {
  uint32_t sampleSerial = 0;
  uint32_t timestampUs = 0;
  int16_t raw = 0;
  uint32_t irPulses = 0;
  uint32_t irDistanceMm = 0;
  IrHealth irHealth = IrHealth::Unknown;
  uint8_t pwm = 0;
  uint8_t direction = 1;
};

struct HallObservation {
  uint32_t firstSample = 0;
  uint32_t lastSample = 0;
  uint16_t sampleCount = 0;
  const HallSample* samples = nullptr;
  uint32_t irDistanceMm = 0;
  IrHealth irHealth = IrHealth::Unknown;
};

// NAVI owns interpretation. This type is intentionally only an evidence
// envelope; it contains no accepted/rejected/count fields.
struct NaviEvidence {
  HallObservation hall;
  int16_t spatialBaselineCandidate = 0;
  bool spatialBaselineAvailable = false;
  uint16_t spatialBaselineBins = 0;
  bool motivePwmZero = false;
  bool operatorMoved = false;
};

}  // namespace navi_eyes
