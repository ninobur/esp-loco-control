#pragma once
// ---------------------------------------------------------------------------
// NaviTimingPredictor — timing as geographic recognition evidence.
//
// Geography supplies exactly one next marker. From that last CONFIRMED
// marker, movement history predicts when that marker's 70x2 opening should
// appear. Hall evidence still establishes that a magnetic excursion occurred;
// timing neither creates confirmation nor overrides wrong polarity. A timing
// mismatch leaves the same target in place for continued observation. Nothing
// here controls the motor or requests a stop.
//
// The clock is native-opening to native-opening: the first sample of the
// confirmed 70x2 pair to the first sample of the candidate 70x2 pair. It is
// never derived from delayed 20 ms tracing blocks, and a nonqualifying Hall
// observation never resets it.
//
// Profiles are keyed by (from marker, direction, fixed operating PWM). They
// must be installed from field evidence in NaviTimingProfiles.h. Until then
// timing is measured, recorded, and reported as UNPROFILED; it is not silently
// converted into a guessed rejection rule.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include "RouteMap.h"
#include "NaviTimingProfiles.h"

namespace navi_power {

enum class TimingResult : uint8_t { NoAnchor = 0, Unprofiled, Matches, Mismatches };

struct TimingEvidence {
  TimingResult result = TimingResult::NoAnchor;
  bool haveAnchor = false;
  bool haveProfile = false;
  uint8_t fromMm = 0;
  int8_t dir = 0;
  uint8_t sourcePwm = 0;
  uint8_t arrivalPwm = 0;
  uint32_t measuredMs = 0;
  uint32_t expectedMs = 0;
  uint32_t toleranceMs = 0;
};

class TimingPredictor {
 public:
  TimingPredictor() { loadApprovedProfiles(); }
  TimingEvidence noAnchor() const { return TimingEvidence{}; }

  TimingEvidence evaluate(uint8_t fromMm, int8_t dir, uint8_t sourcePwm,
                          uint8_t arrivalPwm, uint32_t measuredMs) const {
    TimingEvidence ev;
    ev.haveAnchor = true; ev.fromMm = fromMm; ev.dir = dir;
    ev.sourcePwm = sourcePwm; ev.arrivalPwm = arrivalPwm; ev.measuredMs = measuredMs;
    const Entry& entry = table_[slot(fromMm, dir)];
    const Profile* profile = findProfile(entry, sourcePwm);
    // A calibrated fixed-PWM traversal is comparable only when both endpoints
    // were observed at that PWM. A throttle change is useful telemetry, not a
    // reason to apply the wrong profile.
    if (profile == nullptr || arrivalPwm != sourcePwm) {
      ev.result = TimingResult::Unprofiled;
      return ev;
    }
    ev.haveProfile = true;
    ev.expectedMs = profile->expectedMs;
    ev.toleranceMs = profile->toleranceMs;
    const uint32_t delta = measuredMs > profile->expectedMs
        ? measuredMs - profile->expectedMs : profile->expectedMs - measuredMs;
    ev.result = delta <= profile->toleranceMs ? TimingResult::Matches : TimingResult::Mismatches;
    return ev;
  }

  // Only a verified geographic advance contributes a timing observation.
  // These are diagnostic records, not self-trained acceptance profiles.
  void observeConfirmed(uint8_t fromMm, int8_t dir, uint8_t sourcePwm,
                        uint8_t arrivalPwm, uint32_t measuredMs) {
    Entry& entry = table_[slot(fromMm, dir)];
    entry.haveObservation = true; entry.lastObservedMs = measuredMs;
    entry.lastSourcePwm = sourcePwm; entry.lastArrivalPwm = arrivalPwm;
  }

  // Used by setup-time profile loading and host tests. Firmware never learns
  // an acceptance band while it is running.
  void installApprovedProfile(const TimingProfileDefinition& definition) {
    if (definition.dir == 0 || definition.toleranceMs == 0 || definition.expectedMs == 0) return;
    Entry& entry = table_[slot(definition.fromMm, definition.dir)];
    Profile* profile = findProfile(entry, definition.pwm);
    if (profile == nullptr) profile = firstFreeProfile(entry);
    if (profile == nullptr) return;  // never overwrite another PWM profile
    profile->have = true; profile->pwm = definition.pwm;
    profile->expectedMs = definition.expectedMs; profile->toleranceMs = definition.toleranceMs;
  }

 private:
  static constexpr uint8_t MAX_PWM_PROFILES_PER_INTERVAL = 4;
  struct Profile { bool have = false; uint8_t pwm = 0; uint32_t expectedMs = 0; uint32_t toleranceMs = 0; };
  struct Entry {
    Profile profiles[MAX_PWM_PROFILES_PER_INTERVAL];
    bool haveObservation = false;
    uint32_t lastObservedMs = 0;
    uint8_t lastSourcePwm = 0, lastArrivalPwm = 0;
  };
  Entry table_[navi_one::ROUTE_N * 2];

  static uint16_t slot(uint8_t fromMm, int8_t dir) { return uint16_t(fromMm) * 2 + (dir > 0 ? 0 : 1); }
  static const Profile* findProfile(const Entry& entry, uint8_t pwm) {
    for (uint8_t i = 0; i < MAX_PWM_PROFILES_PER_INTERVAL; ++i)
      if (entry.profiles[i].have && entry.profiles[i].pwm == pwm) return &entry.profiles[i];
    return nullptr;
  }
  static Profile* findProfile(Entry& entry, uint8_t pwm) {
    for (uint8_t i = 0; i < MAX_PWM_PROFILES_PER_INTERVAL; ++i)
      if (entry.profiles[i].have && entry.profiles[i].pwm == pwm) return &entry.profiles[i];
    return nullptr;
  }
  static Profile* firstFreeProfile(Entry& entry) {
    for (uint8_t i = 0; i < MAX_PWM_PROFILES_PER_INTERVAL; ++i)
      if (!entry.profiles[i].have) return &entry.profiles[i];
    return nullptr;
  }
  void loadApprovedProfiles() {
    for (uint16_t i = 0; i < TIMING_PROFILE_COUNT; ++i) installApprovedProfile(TIMING_PROFILES[i]);
  }
};

}  // namespace navi_power
