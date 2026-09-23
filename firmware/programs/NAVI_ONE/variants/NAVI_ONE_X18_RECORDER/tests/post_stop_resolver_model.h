#pragma once

// Experimental host-only model. Not included by NAVI_ONE firmware.
// The production integration is deliberately deferred until the field
// replays establish a safe post-stop resolution rule.

#include <stdint.h>

namespace post_stop_model {

struct Candidate {
  uint32_t openedAtMs;
  uint32_t closedAtMs;
  uint16_t peak;
  uint16_t gain;
  uint8_t polarity;
};

enum class Resolution : uint8_t {
  AcceptOrdinary,
  AcceptStrongClose,
  Quarantine,
  ReplaceQuarantined,
};

class Resolver {
 public:
  static constexpr uint32_t kGuardMs = 500;
  static constexpr uint32_t kCompanionMs = 64;
  static constexpr float kStrongRatio = 1.0f;

  void arm() { waitingForAnchor_ = true; active_ = false; quarantined_ = false; }
  void cancel() { waitingForAnchor_ = active_ = quarantined_ = false; }

  void acceptedAnchor(const Candidate& c) {
    lastAcceptedCloseMs_ = c.closedAtMs;
    if (waitingForAnchor_) {
      waitingForAnchor_ = false;
      active_ = true;
    }
  }

  Resolution examine(const Candidate& c, uint8_t expectedPolarity) {
    const uint32_t anchorGap = c.openedAtMs - lastAcceptedCloseMs_;
    if (quarantined_) {
      const uint32_t companionGap = c.openedAtMs - quarantinedCloseMs_;
      // A close companion is evaluated against the unchanged genuine anchor.
      // It may replace the quarantined lobe only when it is the expected pole
      // and its own opening is outside the ordinary guard.
      if (companionGap <= kCompanionMs && c.polarity == expectedPolarity &&
          anchorGap >= kGuardMs) {
        quarantined_ = false;
        active_ = false;
        return Resolution::ReplaceQuarantined;
      }
    }

    if (!active_ || anchorGap >= kGuardMs) {
      quarantined_ = false;
      active_ = false;
      return Resolution::AcceptOrdinary;
    }

    const float ratio = c.gain ? static_cast<float>(c.peak) / c.gain : 0.0f;
    if (c.polarity == expectedPolarity && ratio >= kStrongRatio) {
      quarantined_ = false;
      active_ = false;
      return Resolution::AcceptStrongClose;
    }

    quarantined_ = true;
    quarantinedCloseMs_ = c.closedAtMs;
    return Resolution::Quarantine;
  }

 private:
  bool waitingForAnchor_ = false;
  bool active_ = false;
  bool quarantined_ = false;
  uint32_t lastAcceptedCloseMs_ = 0;
  uint32_t quarantinedCloseMs_ = 0;
};

}  // namespace post_stop_model
