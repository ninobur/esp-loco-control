#pragma once

#include <stdint.h>

#include "NaviEvidence.h"

namespace navi_eyes {

enum class NaviDecision : uint8_t { Observe, Hold, Accept, Reject, Stop };

struct NaviJudgment {
  NaviDecision decision = NaviDecision::Observe;
  uint32_t observationSerial = 0;
  uint16_t navMm = 0;
  const char* reason = "OBSERVE";
};

// The deliberately small authority shell. Physical rules and route context
// belong here when they are actually specified and measured. Acquisition and
// reference modules cannot call these methods or mutate navMm.
class NaviCore {
 public:
  explicit NaviCore(uint16_t initialMm) : navMm_(initialMm) {}

  NaviJudgment observe(const NaviEvidence& evidence, uint32_t serial) {
    if (evidence.hall.samples != nullptr && evidence.hall.sampleCount != 0) {
      lastHallSamples_ = evidence.hall.samples;
      lastHallSampleCount_ = evidence.hall.sampleCount;
    }
    NaviJudgment result;
    result.observationSerial = serial;
    result.navMm = navMm_;
    if (evidence.motivePwmZero) {
      result.decision = NaviDecision::Hold;
      result.reason = "MOTIVE_PWM_ZERO_CONTEXT";
      return result;
    }
    // No inherited X22R gate is silently recreated here. Until NAVI has a
    // route/context rule, the observation remains reconstructible and held.
    result.decision = NaviDecision::Observe;
    result.reason = "UNJUDGED_EVIDENCE";
    return result;
  }

  uint16_t navMm() const { return navMm_; }
  const HallSample* lastHallSamples() const { return lastHallSamples_; }
  uint16_t lastHallSampleCount() const { return lastHallSampleCount_; }

 private:
  uint16_t navMm_;
  const HallSample* lastHallSamples_ = nullptr;
  uint16_t lastHallSampleCount_ = 0;
};

}  // namespace navi_eyes
