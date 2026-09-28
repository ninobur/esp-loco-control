#pragma once

#include <algorithm>
#include <stdint.h>
#include <vector>

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
    collectInitialReference(evidence.hall);
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
  bool initialHallReferenceAvailable() const { return initialReferenceAvailable_; }
  int16_t initialHallReference() const { return initialReference_; }
  bool initialReferenceCollectionStarted() const { return collectionStarted_; }
  size_t initialReferenceSampleCount() const { return collection_.size(); }

 private:
  static constexpr uint32_t kInitialReferenceTravelMm = 10;

  void collectInitialReference(const HallObservation& observation) {
    if (initialReferenceAvailable_ || observation.samples == nullptr ||
        observation.sampleCount == 0 || !observation.irAdvanced) {
      return;
    }

    if (!collectionStarted_) {
      collectionStarted_ = true;
    }

    for (uint16_t i = 0; i < observation.sampleCount; ++i) {
      collection_.push_back(observation.samples[i].raw);
    }

    if (observation.irDistanceMm < kInitialReferenceTravelMm) {
      return;
    }

    std::vector<int16_t> ordered = collection_;
    std::sort(ordered.begin(), ordered.end());
    const size_t middle = ordered.size() / 2;
    if (ordered.size() % 2 != 0) {
      initialReference_ = ordered[middle];
    } else {
      const int32_t lower = ordered[middle - 1];
      const int32_t upper = ordered[middle];
      initialReference_ = static_cast<int16_t>((lower + upper) / 2);
    }
    initialReferenceAvailable_ = true;
  }

  uint16_t navMm_;
  const HallSample* lastHallSamples_ = nullptr;
  uint16_t lastHallSampleCount_ = 0;
  bool collectionStarted_ = false;
  std::vector<int16_t> collection_;
  bool initialReferenceAvailable_ = false;
  int16_t initialReference_ = 0;
};

}  // namespace navi_eyes
