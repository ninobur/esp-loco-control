#pragma once

#include <algorithm>
#include <stdint.h>
#include <vector>

#include "NaviEvidence.h"

namespace navi_eyes {

enum class NaviDecision : uint8_t { Observe, Hold, Accept, Reject, Stop };

enum class HallOpeningPolarity : int8_t {
  Unknown = 0,
  AboveReference = 1,
  BelowReference = -1,
};

struct TargetSpec {
  HallOpeningPolarity polarity = HallOpeningPolarity::Unknown;
  uint32_t distanceMm = 0;
  uint32_t sequence = 0;
  uint8_t direction = 0;
};

struct HallOpening {
  uint32_t landmarkObservationSerial = 0;
  uint32_t landmarkIrDistanceMm = 0;
  bool confirmed = false;
  uint32_t observationSerial = 0;
  int32_t departure = 0;
  HallOpeningPolarity polarity = HallOpeningPolarity::Unknown;
};

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

  void configureExpectedTarget(const TargetSpec& target,
                               uint32_t originMm = 0) {
    expectedTarget_ = target;
    targetConfigured_ = target.polarity != HallOpeningPolarity::Unknown &&
                        target.distanceMm != 0;
    targetOriginMm_ = originMm;
    targetConfirmed_ = false;
    targetMissing_ = false;
    resetHallTargetContext();
  }

  void configureSubsequentTarget(const TargetSpec& target) {
    subsequentTarget_ = target;
    subsequentTargetConfigured_ =
        target.polarity != HallOpeningPolarity::Unknown &&
        target.distanceMm != 0;
  }

  // Context transitions clear only Hall evidence that was measured under the
  // previous reference/target context. They do not alter NAVI's position.
  void resetHallTargetContext() {
    rollingHallWindow_.clear();
    hallSupportsTarget_ = false;
    opening_ = HallOpening{};
  }

  NaviJudgment observe(const NaviEvidence& evidence, uint32_t serial) {
    if (evidence.hall.samples != nullptr && evidence.hall.sampleCount != 0) {
      lastHallSamples_ = evidence.hall.samples;
      lastHallSampleCount_ = evidence.hall.sampleCount;
      for (uint16_t i = 0; i < evidence.hall.sampleCount; ++i)
        appendHallSample(evidence.hall.samples[i]);
    }
    // NAVI compares the cumulative IR measurement itself. Hall observations
    // continue to be collected even when the IR value repeats between updates.
    const bool irDistanceProgressed =
        evidence.hall.irDistanceMm > lastIrDistanceMm_;
    if (!collectionStarted_ && irDistanceProgressed) {
      collectionStarted_ = true;
    }
    collectInitialReference(evidence.hall);
    if (spatialReferenceActive_) {
      updateSpatialReference(evidence.hall);
    } else if (recognizeTargetHallEvidence()) {
      beginSpatialReference(opening_.landmarkIrDistanceMm);
    }

    evaluateMissingTarget(evidence.hall.irDistanceMm);
    lastIrDistanceMm_ = evidence.hall.irDistanceMm;
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
  int16_t activeHallReference() const { return activeReference_; }
  bool initialReferenceCollectionStarted() const { return collectionStarted_; }
  size_t initialReferenceSampleCount() const { return collection_.size(); }
  uint32_t lastObservedIrDistanceMm() const { return lastIrDistanceMm_; }
  int32_t lastHallDeparture() const { return lastHallDeparture_; }
  const HallOpening& opening() const { return opening_; }
  const HallOpening& lastConfirmedOpening() const { return lastConfirmedOpening_; }
  bool spatialReferenceActive() const { return spatialReferenceActive_; }
  uint32_t spatialReferenceOriginMm() const { return spatialOriginMm_; }
  size_t spatialReferenceSampleCount() const {
    return spatialPopulation_.size();
  }
  bool expectedTargetConfirmed() const { return targetConfirmed_; }
  bool hallSupportsTarget() const { return hallSupportsTarget_; }
  int16_t rollingHallMedian() const { return rollingHallMedian_; }
  size_t rollingHallSampleCount() const { return rollingHallWindow_.size(); }
  bool expectedTargetMissing() const { return targetMissing_; }
  uint32_t missingTargetCount() const { return missingTargetCount_; }
  uint32_t targetOriginMm() const { return targetOriginMm_; }
  TargetSpec expectedTarget() const { return expectedTarget_; }

 private:
  static constexpr uint32_t kInitialReferenceTravelMm = 10;
  static constexpr int32_t kHallDepartureThreshold = 70;

  void collectInitialReference(const HallObservation& observation) {
    if (initialReferenceAvailable_ || observation.samples == nullptr ||
        observation.sampleCount == 0 || !collectionStarted_) {
      return;
    }

    if (observation.irDistanceMm <= kInitialReferenceTravelMm) {
      for (uint16_t i = 0; i < observation.sampleCount; ++i) {
        collection_.push_back(observation.samples[i].raw);
      }
    }

    if (observation.irDistanceMm < kInitialReferenceTravelMm ||
        collection_.empty()) {
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
    activeReference_ = initialReference_;
    initialReferenceAvailable_ = true;
    resetHallTargetContext();
  }

  void appendHallSample(const HallSample& sample) {
    rollingHallWindow_.push_back(
        {sample.sampleSerial, sample.irDistanceMm, sample.raw, sample.direction});
    if (rollingHallWindow_.size() > 5) rollingHallWindow_.erase(rollingHallWindow_.begin());
  }

  bool recognizeTargetHallEvidence() {
    if (!initialReferenceAvailable_ || rollingHallWindow_.size() < 5 ||
        targetMissing_) {
      return false;
    }

    std::vector<int16_t> ordered;
    ordered.reserve(rollingHallWindow_.size());
    for (const HallWindowSample& sample : rollingHallWindow_)
      ordered.push_back(sample.raw);
    std::sort(ordered.begin(), ordered.end());
    rollingHallMedian_ = ordered[ordered.size() / 2];
    const int32_t medianDeparture =
        static_cast<int32_t>(rollingHallMedian_) -
        static_cast<int32_t>(activeReference_);
    lastHallDeparture_ = medianDeparture;
    hallSupportsTarget_ =
        (expectedTarget_.polarity == HallOpeningPolarity::AboveReference &&
         medianDeparture >= kHallDepartureThreshold) ||
        (expectedTarget_.polarity == HallOpeningPolarity::BelowReference &&
         medianDeparture <= -kHallDepartureThreshold);
    if (!hallSupportsTarget_ || !targetConfigured_ || opening_.confirmed)
      return false;

    const HallWindowSample* landmark = nullptr;
    for (const HallWindowSample& sample : rollingHallWindow_) {
      const int32_t departure = static_cast<int32_t>(sample.raw) -
                                static_cast<int32_t>(activeReference_);
      const bool expectedDirection =
          (expectedTarget_.polarity == HallOpeningPolarity::AboveReference &&
           departure >= kHallDepartureThreshold) ||
          (expectedTarget_.polarity == HallOpeningPolarity::BelowReference &&
           departure <= -kHallDepartureThreshold);
      if (expectedDirection) {
        landmark = &sample;
        break;
      }
    }
    if (landmark == nullptr) return false;
    opening_.landmarkObservationSerial = landmark->serial;
    opening_.landmarkIrDistanceMm = landmark->irDistanceMm;
    const HallWindowSample& latest = rollingHallWindow_.back();
    if (!distanceMatchesExpectedTarget(latest.irDistanceMm) ||
        !contextMatchesTarget(latest.direction))
      return false;

    opening_.confirmed = true;
    opening_.observationSerial = latest.serial;
    opening_.departure = medianDeparture;
    opening_.polarity = expectedTarget_.polarity;
    lastConfirmedOpening_ = opening_;
    targetConfirmed_ = true;
    targetMissing_ = false;
    return true;
  }

  bool distanceMatchesExpectedTarget(uint32_t current) const {
    if (!targetConfigured_) return false;
    if (current < targetOriginMm_) return false;
    const uint64_t expected = expectedTarget_.distanceMm;
    const uint32_t lower = static_cast<uint32_t>(expected * 85 / 100);
    const uint32_t upper = static_cast<uint32_t>((expected * 115 + 99) / 100);
    const uint32_t traveled = current - targetOriginMm_;
    return traveled >= lower && traveled <= upper;
  }

  bool contextMatchesTarget(uint8_t direction) const {
    return expectedTarget_.direction == 0 ||
           expectedTarget_.direction == direction;
  }

  void evaluateMissingTarget(uint32_t currentDistanceMm) {
    if (!targetConfigured_ || targetConfirmed_ || targetMissing_ ||
        spatialReferenceActive_ || currentDistanceMm < targetOriginMm_) return;
    const uint64_t upper = static_cast<uint64_t>(expectedTarget_.distanceMm) * 115 / 100;
    if (currentDistanceMm - targetOriginMm_ <= upper) return;
    targetMissing_ = true;
    ++missingTargetCount_;
    opening_ = HallOpening{};
    rollingHallWindow_.clear();
    hallSupportsTarget_ = false;
    if (subsequentTargetConfigured_) {
      targetOriginMm_ += expectedTarget_.distanceMm;
      expectedTarget_ = subsequentTarget_;
      targetMissing_ = false;
    }
  }

  void beginSpatialReference(uint32_t originMm) {
    spatialReferenceActive_ = true;
    spatialOriginMm_ = originMm;
    spatialPopulation_.clear();
  }

  void updateSpatialReference(const HallObservation& observation) {
    if (observation.irDistanceMm < spatialOriginMm_) return;
    const uint32_t traveledMm = observation.irDistanceMm - spatialOriginMm_;

    if (traveledMm >= 100 && traveledMm <= 200 &&
        observation.samples != nullptr && observation.sampleCount != 0) {
      for (uint16_t i = 0; i < observation.sampleCount; ++i) {
        bool represented = false;
        for (const SpatialHallSample& selected : spatialPopulation_) {
          if (selected.distanceMm == observation.irDistanceMm) {
            represented = true;
            break;
          }
        }
        if (!represented) {
          spatialPopulation_.push_back(
              {observation.irDistanceMm, observation.samples[i].raw});
        }
      }
    }

    if (traveledMm < 200 || spatialPopulation_.empty()) return;

    std::vector<int16_t> ordered;
    ordered.reserve(spatialPopulation_.size());
    for (const SpatialHallSample& selected : spatialPopulation_) {
      ordered.push_back(selected.raw);
    }
    std::sort(ordered.begin(), ordered.end());
    const size_t middle = ordered.size() / 2;
    if (ordered.size() % 2 != 0) {
      activeReference_ = ordered[middle];
    } else {
      const int32_t lower = ordered[middle - 1];
      const int32_t upper = ordered[middle];
      activeReference_ = static_cast<int16_t>((lower + upper) / 2);
    }
    spatialReferenceActive_ = false;
    resetOpeningRecognition();
  }

  void resetOpeningRecognition() {
    resetHallTargetContext();
    targetConfirmed_ = false;
    targetMissing_ = false;
  }

  struct HallWindowSample {
    uint32_t serial;
    uint32_t irDistanceMm;
    int16_t raw;
    uint8_t direction;
  };

  struct SpatialHallSample {
    uint32_t distanceMm;
    int16_t raw;
  };

  uint16_t navMm_;
  const HallSample* lastHallSamples_ = nullptr;
  uint16_t lastHallSampleCount_ = 0;
  uint32_t lastIrDistanceMm_ = 0;
  bool collectionStarted_ = false;
  std::vector<int16_t> collection_;
  bool initialReferenceAvailable_ = false;
  int16_t initialReference_ = 0;
  int16_t activeReference_ = 0;
  int32_t lastHallDeparture_ = 0;
  std::vector<HallWindowSample> rollingHallWindow_;
  int16_t rollingHallMedian_ = 0;
  bool hallSupportsTarget_ = false;
  HallOpening opening_;
  HallOpening lastConfirmedOpening_;
  TargetSpec expectedTarget_;
  TargetSpec subsequentTarget_;
  bool targetConfigured_ = false;
  bool subsequentTargetConfigured_ = false;
  bool targetConfirmed_ = false;
  bool targetMissing_ = false;
  uint32_t missingTargetCount_ = 0;
  uint32_t targetOriginMm_ = 0;
  bool spatialReferenceActive_ = false;
  uint32_t spatialOriginMm_ = 0;
  std::vector<SpatialHallSample> spatialPopulation_;
};

}  // namespace navi_eyes
