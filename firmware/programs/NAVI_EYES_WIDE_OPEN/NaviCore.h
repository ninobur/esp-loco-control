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
  bool candidate = false;
  uint32_t candidateObservationSerial = 0;
  int32_t candidateDeparture = 0;
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
  }

  void configureSubsequentTarget(const TargetSpec& target) {
    subsequentTarget_ = target;
    subsequentTargetConfigured_ =
        target.polarity != HallOpeningPolarity::Unknown &&
        target.distanceMm != 0;
  }

  NaviJudgment observe(const NaviEvidence& evidence, uint32_t serial) {
    if (evidence.hall.samples != nullptr && evidence.hall.sampleCount != 0) {
      lastHallSamples_ = evidence.hall.samples;
      lastHallSampleCount_ = evidence.hall.sampleCount;
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
    } else if (recognizeTargetHallEvidence(evidence.hall)) {
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
  uint8_t qualifyingHallObservationCount() const {
    return qualifyingHallObservationCount_;
  }
  uint8_t confirmationSampleCount() const { return confirmationSampleCount_; }
  const HallOpening& opening() const { return opening_; }
  const HallOpening& lastConfirmedOpening() const { return lastConfirmedOpening_; }
  bool spatialReferenceActive() const { return spatialReferenceActive_; }
  uint32_t spatialReferenceOriginMm() const { return spatialOriginMm_; }
  size_t spatialReferenceSampleCount() const {
    return spatialPopulation_.size();
  }
  bool expectedTargetConfirmed() const { return targetConfirmed_; }
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
  }

  bool recognizeTargetHallEvidence(const HallObservation& observation) {
    if (!initialReferenceAvailable_ || observation.samples == nullptr ||
        observation.sampleCount == 0 || targetMissing_) {
      return false;
    }

    bool confirmed = false;

    for (uint16_t i = 0; i < observation.sampleCount; ++i) {
      const HallSample& sample = observation.samples[i];
      lastHallDeparture_ = static_cast<int32_t>(sample.raw) -
                            static_cast<int32_t>(activeReference_);
      const bool qualifies =
          lastHallDeparture_ >= kHallDepartureThreshold ||
          lastHallDeparture_ <= -kHallDepartureThreshold;

      if (opening_.confirmed) continue;

      if (!qualifies) {
        qualifyingHallObservationCount_ = 0;
        confirmationSampleCount_ = 0;
        targetWindow_.clear();
        opening_.candidate = false;
        continue;
      }

      targetWindow_.push_back({sample.sampleSerial, sample.irDistanceMm,
                               lastHallDeparture_, sample.direction});
      if (targetWindow_.size() > 3) targetWindow_.erase(targetWindow_.begin());
      if (qualifyingHallObservationCount_ < 2) ++qualifyingHallObservationCount_;
      if (qualifyingHallObservationCount_ == 2) {
        opening_.candidate = true;
        opening_.candidateObservationSerial = sample.sampleSerial;
        opening_.candidateDeparture = lastHallDeparture_;
        opening_.landmarkObservationSerial = targetWindow_[0].serial;
        opening_.landmarkIrDistanceMm = targetWindow_[0].irDistanceMm;
      }

      if (!opening_.candidate || targetWindow_.size() < 3 ||
          !targetConfigured_) continue;
      confirmationSampleCount_ = static_cast<uint8_t>(targetWindow_.size());
      if (!hallWindowMatchesTarget() || !distanceMatchesExpectedTarget() ||
          !contextMatchesTarget(sample)) continue;

      opening_.candidate = false;
      opening_.confirmed = true;
      opening_.observationSerial = sample.sampleSerial;
      opening_.departure = lastHallDeparture_;
      opening_.polarity = expectedTarget_.polarity;
      lastConfirmedOpening_ = opening_;
      targetConfirmed_ = true;
      targetMissing_ = false;
      confirmed = true;
    }
    return confirmed;
  }

  bool hallWindowMatchesTarget() const {
    uint8_t matches = 0;
    for (const TargetHallSample& sample : targetWindow_) {
      if ((expectedTarget_.polarity == HallOpeningPolarity::AboveReference &&
           sample.departure >= kHallDepartureThreshold) ||
          (expectedTarget_.polarity == HallOpeningPolarity::BelowReference &&
           sample.departure <= -kHallDepartureThreshold)) ++matches;
    }
    return matches >= 2;
  }

  bool distanceMatchesExpectedTarget() const {
    if (!targetConfigured_ || targetWindow_.empty()) return false;
    const uint32_t current = targetWindow_.back().irDistanceMm;
    if (current < targetOriginMm_) return false;
    const uint64_t expected = expectedTarget_.distanceMm;
    const uint32_t lower = static_cast<uint32_t>(expected * 85 / 100);
    const uint32_t upper = static_cast<uint32_t>((expected * 115 + 99) / 100);
    const uint32_t traveled = current - targetOriginMm_;
    return traveled >= lower && traveled <= upper;
  }

  bool contextMatchesTarget(const HallSample& sample) const {
    return expectedTarget_.direction == 0 ||
           expectedTarget_.direction == sample.direction;
  }

  void evaluateMissingTarget(uint32_t currentDistanceMm) {
    if (!targetConfigured_ || targetConfirmed_ || targetMissing_ ||
        spatialReferenceActive_ || currentDistanceMm < targetOriginMm_) return;
    const uint64_t upper = static_cast<uint64_t>(expectedTarget_.distanceMm) * 115 / 100;
    if (currentDistanceMm - targetOriginMm_ <= upper) return;
    targetMissing_ = true;
    ++missingTargetCount_;
    opening_ = HallOpening{};
    targetWindow_.clear();
    qualifyingHallObservationCount_ = 0;
    confirmationSampleCount_ = 0;
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
    opening_ = HallOpening{};
    qualifyingHallObservationCount_ = 0;
    confirmationSampleCount_ = 0;
    targetWindow_.clear();
    targetConfirmed_ = false;
    targetMissing_ = false;
  }

  struct TargetHallSample {
    uint32_t serial;
    uint32_t irDistanceMm;
    int32_t departure;
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
  uint8_t qualifyingHallObservationCount_ = 0;
  uint8_t confirmationSampleCount_ = 0;
  std::vector<TargetHallSample> targetWindow_;
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
