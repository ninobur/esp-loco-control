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

struct HallOpening {
  bool recognized = false;
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
    recognizeOpening(evidence.hall);
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
  bool initialReferenceCollectionStarted() const { return collectionStarted_; }
  size_t initialReferenceSampleCount() const { return collection_.size(); }
  uint32_t lastObservedIrDistanceMm() const { return lastIrDistanceMm_; }
  int32_t lastHallDeparture() const { return lastHallDeparture_; }
  uint8_t qualifyingHallObservationCount() const {
    return qualifyingHallObservationCount_;
  }
  const HallOpening& opening() const { return opening_; }

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
    initialReferenceAvailable_ = true;
  }

  void recognizeOpening(const HallObservation& observation) {
    if (!initialReferenceAvailable_ || observation.samples == nullptr ||
        observation.sampleCount == 0) {
      return;
    }

    for (uint16_t i = 0; i < observation.sampleCount; ++i) {
      const HallSample& sample = observation.samples[i];
      lastHallDeparture_ = static_cast<int32_t>(sample.raw) -
                            static_cast<int32_t>(initialReference_);
      const bool qualifies =
          lastHallDeparture_ >= kHallDepartureThreshold ||
          lastHallDeparture_ <= -kHallDepartureThreshold;

      if (!qualifies) {
        qualifyingHallObservationCount_ = 0;
        continue;
      }

      if (qualifyingHallObservationCount_ < 2) {
        ++qualifyingHallObservationCount_;
      }
      if (!opening_.recognized && qualifyingHallObservationCount_ == 2) {
        opening_.recognized = true;
        opening_.observationSerial = sample.sampleSerial;
        opening_.departure = lastHallDeparture_;
        opening_.polarity = lastHallDeparture_ > 0
                                ? HallOpeningPolarity::AboveReference
                                : HallOpeningPolarity::BelowReference;
      }
    }
  }

  uint16_t navMm_;
  const HallSample* lastHallSamples_ = nullptr;
  uint16_t lastHallSampleCount_ = 0;
  uint32_t lastIrDistanceMm_ = 0;
  bool collectionStarted_ = false;
  std::vector<int16_t> collection_;
  bool initialReferenceAvailable_ = false;
  int16_t initialReference_ = 0;
  int32_t lastHallDeparture_ = 0;
  uint8_t qualifyingHallObservationCount_ = 0;
  HallOpening opening_;
};

}  // namespace navi_eyes
