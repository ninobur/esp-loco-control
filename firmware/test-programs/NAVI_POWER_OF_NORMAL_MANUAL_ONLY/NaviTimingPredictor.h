#pragma once
// ---------------------------------------------------------------------------
// NaviTimingPredictor — timing as normalized geographic recognition evidence.
//
// Geography supplies exactly one next marker. The 70x2 opening-to-opening
// timing map preserves each directed interval's geographic character as a
// factor. Before it can qualify an encounter, the active locomotive/consist
// establishes its current PWM-90 base pace across MM035–055 and MM115–125.
// The resulting base scales the complete directed survey map. Thus the map is
// sensitive to route geometry but not frozen to the survey battery, consist,
// or mechanical condition.
//
// Hall evidence still establishes a magnetic encounter. Timing neither creates
// confirmation nor overrides wrong polarity. A timing mismatch leaves the same
// target in place for continued observation. Nothing here controls the motor.
// A nonqualifying Hall encounter never resets either the geographic opening
// anchor or the calibration data.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include "RouteMap.h"
#include "NaviTimingProfiles.h"

namespace navi_power {

enum class TimingResult : uint8_t {
  NoAnchor = 0,
  Unprofiled,
  AwaitingCalibration,
  Matches,
  Mismatches,
};

struct TimingEvidence {
  TimingResult result = TimingResult::NoAnchor;
  bool haveAnchor = false;
  bool haveProfile = false;
  bool calibrationReady = false;
  uint16_t calibrationCoveredMm = 0;
  uint16_t calibrationRequiredMm = 0;
  uint8_t fromMm = 0;
  int8_t dir = 0;
  uint8_t sourcePwm = 0;
  uint8_t arrivalPwm = 0;
  uint32_t measuredMs = 0;
  uint32_t expectedMs = 0;
  uint32_t lowerMs = 0;
  uint32_t upperMs = 0;
  // Kept for existing telemetry readers. Qualification uses the asymmetric
  // evidence bounds above, not this derived diagnostic quantity.
  uint32_t toleranceMs = 0;
};

class TimingPredictor {
 public:
  TimingPredictor() { loadApprovedProfiles(); }
  TimingEvidence noAnchor() const { return TimingEvidence{}; }

  // A declaration begins a new directional calibration pass. A completed
  // pass is held stable; it is not rewritten by every target.
  void beginCalibration(int8_t dir) {
    Calibration& c = calibration_[directionSlot(dir)];
    c.weightedTime = 0.0f;
    c.coveredMm = 0;
    c.ready = false;
    for (uint16_t i = 0; i < navi_one::ROUTE_N; ++i) c.seen[i] = false;
  }

  TimingEvidence evaluate(uint8_t fromMm, int8_t dir, uint8_t sourcePwm,
                          uint8_t arrivalPwm, uint32_t measuredMs) const {
    TimingEvidence ev;
    ev.haveAnchor = true; ev.fromMm = fromMm; ev.dir = dir;
    ev.sourcePwm = sourcePwm; ev.arrivalPwm = arrivalPwm; ev.measuredMs = measuredMs;
    const Entry& entry = table_[slot(fromMm, dir)];
    const Profile* profile = findProfile(entry, sourcePwm);
    // A fixed-PWM profile may not be silently applied across a throttle change.
    if (profile == nullptr || arrivalPwm != sourcePwm) {
      ev.result = TimingResult::Unprofiled;
      return ev;
    }
    ev.haveProfile = true;
    const Calibration& c = calibration_[directionSlot(dir)];
    ev.calibrationReady = c.ready;
    ev.calibrationCoveredMm = c.coveredMm;
    ev.calibrationRequiredMm = requiredCalibrationMm_[directionSlot(dir)];
    if (!c.ready) {
      ev.result = TimingResult::AwaitingCalibration;
      return ev;
    }
    ev.expectedMs = scale(profile->factor, c.baseMs());
    ev.lowerMs = scale(profile->lowerFactor, c.baseMs());
    ev.upperMs = scale(profile->upperFactor, c.baseMs());
    const uint32_t lowerDelta = ev.expectedMs > ev.lowerMs ? ev.expectedMs - ev.lowerMs : 0;
    const uint32_t upperDelta = ev.upperMs > ev.expectedMs ? ev.upperMs - ev.expectedMs : 0;
    ev.toleranceMs = lowerDelta > upperDelta ? lowerDelta : upperDelta;
    ev.result = measuredMs >= ev.lowerMs && measuredMs <= ev.upperMs
        ? TimingResult::Matches : TimingResult::Mismatches;
    return ev;
  }

  // Only a verified geographic advance may add a calibration observation.
  // It contributes once, and only if it is a designated PWM-90
  // standardization interval. The directed intervals total 9000 mm.
  void observeConfirmed(uint8_t fromMm, int8_t dir, uint8_t sourcePwm,
                        uint8_t arrivalPwm, uint32_t measuredMs) {
    if (sourcePwm != arrivalPwm) return;
    Entry& entry = table_[slot(fromMm, dir)];
    const Profile* profile = findProfile(entry, sourcePwm);
    if (profile == nullptr || profile->calibrationWeightMm == 0) return;
    Calibration& c = calibration_[directionSlot(dir)];
    if (c.ready || c.seen[fromMm]) return;
    c.seen[fromMm] = true;
    c.weightedTime += float(measuredMs) * float(profile->calibrationWeightMm);
    c.coveredMm = uint16_t(c.coveredMm + profile->calibrationWeightMm);
    c.ready = c.coveredMm == requiredCalibrationMm_[directionSlot(dir)];
  }

  // Host-test seam: production firmware derives its base only through the
  // designated standardization intervals above.
  void setCalibrationBaseForTest(int8_t dir, float baseMs) {
    Calibration& c = calibration_[directionSlot(dir)];
    c.weightedTime = baseMs * float(requiredCalibrationMm_[directionSlot(dir)]);
    c.coveredMm = requiredCalibrationMm_[directionSlot(dir)];
    c.ready = true;
  }

 private:
  static constexpr uint8_t MAX_PWM_PROFILES_PER_INTERVAL = 4;
  struct Profile {
    bool have = false;
    uint8_t pwm = 0;
    float factor = 0.0f, lowerFactor = 0.0f, upperFactor = 0.0f;
    uint16_t calibrationWeightMm = 0;
  };
  struct Entry { Profile profiles[MAX_PWM_PROFILES_PER_INTERVAL]; };
  struct Calibration {
    bool seen[navi_one::ROUTE_N]{};
    float weightedTime = 0.0f;
    uint16_t coveredMm = 0;
    bool ready = false;
    float baseMs() const { return coveredMm ? weightedTime / float(coveredMm) : 0.0f; }
  };
  Entry table_[navi_one::ROUTE_N * 2];
  Calibration calibration_[2];
  uint16_t requiredCalibrationMm_[2]{};

  static uint16_t slot(uint8_t fromMm, int8_t dir) { return uint16_t(fromMm) * 2 + (dir > 0 ? 0 : 1); }
  static uint8_t directionSlot(int8_t dir) { return dir > 0 ? 0 : 1; }
  static uint32_t scale(float factor, float baseMs) {
    const float value = factor * baseMs;
    return value <= 0.0f ? 0U : value >= float(UINT32_MAX) ? UINT32_MAX : uint32_t(value + 0.5f);
  }
  static const Profile* findProfile(const Entry& entry, uint8_t pwm) {
    for (uint8_t i = 0; i < MAX_PWM_PROFILES_PER_INTERVAL; ++i)
      if (entry.profiles[i].have && entry.profiles[i].pwm == pwm) return &entry.profiles[i];
    return nullptr;
  }
  static Profile* firstFreeProfile(Entry& entry) {
    for (uint8_t i = 0; i < MAX_PWM_PROFILES_PER_INTERVAL; ++i)
      if (!entry.profiles[i].have) return &entry.profiles[i];
    return nullptr;
  }
  void installApprovedProfile(const TimingProfileDefinition& definition) {
    if (definition.dir == 0 || definition.pwm == 0 || definition.factor <= 0.0f ||
        definition.lowerFactor <= 0.0f || definition.upperFactor < definition.lowerFactor) return;
    Entry& entry = table_[slot(definition.fromMm, definition.dir)];
    Profile* profile = firstFreeProfile(entry);
    if (profile == nullptr) return;
    profile->have = true; profile->pwm = definition.pwm;
    profile->factor = definition.factor; profile->lowerFactor = definition.lowerFactor;
    profile->upperFactor = definition.upperFactor;
    profile->calibrationWeightMm = definition.calibrationWeightMm;
    if (definition.calibrationWeightMm)
      requiredCalibrationMm_[directionSlot(definition.dir)] = uint16_t(
          requiredCalibrationMm_[directionSlot(definition.dir)] + definition.calibrationWeightMm);
  }
  void loadApprovedProfiles() {
    for (uint16_t i = 0; i < TIMING_PROFILE_COUNT; ++i) installApprovedProfile(TIMING_PROFILES[i]);
  }
};

}  // namespace navi_power
