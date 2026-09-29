#pragma once

#include <algorithm>
#include <stdint.h>
#include <vector>

#include "../NAVI_EYES_WIDE_OPEN/NaviEvidence.h"
#include "NaviMapAdapter.h"
#include "NaviBootReference.h"
#include "../../reference/NAVI_COHERENCE/IR_ARCHITECTURE_0_4/IrOdometryEpoch.h"

namespace navi_eyes {

enum class EwoEventKind : uint8_t {
  None, Declared, Reversed, HallSupport, TargetConfirmed, MissedMagnet,
  ReferenceReady, SpatialClearance, SpatialCollect, SpatialReady, SpatialEmpty,
  IrDegraded, IrNormal, PwmZeroDisplacement, Reanchored, ObservationLoss,
  SpatialInvalidated, BootReferenceIncomplete
};

struct EwoEvent {
  EwoEventKind kind = EwoEventKind::None;
  uint64_t timestampUs = 0;
  uint32_t hallSerial = 0;
  uint8_t mm = 0;
  uint8_t target = 0;
  int8_t direction = 0;
  uint64_t irUm = 0;
  int16_t median = 0;
  int16_t reference = 0;
  bool degraded = false;
  uint32_t openingSerial = 0;
  uint64_t openingIrUm = 0;
  bool positionReliable = false;
  uint8_t spatialPhase = 0;
  uint64_t consumptionId = 0;
  uint32_t irSequence = 0;
  uint32_t hallQueueDrops = 0, irQueueDrops = 0;
};

// NAVI's working target-only authority. Acquisition delivers each ADC result
// and each decoded type-5 IR snapshot; neither source selects NAVI evidence.
class NaviIntegratedCore {
 public:
  static constexpr uint64_t kIrFreshUs = 1000000;
  static constexpr uint64_t kDegradedGuardUs = 650000;

  void declare(uint8_t mm, int8_t direction, uint64_t nowUs) {
    if (mm >= navi_one::ROUTE_N || (direction != 1 && direction != -1)) return;
    beginDecision(nowUs);
    contextSinceUs_ = nowUs;
    declared_ = true;
    positionReliable_ = true;
    mm_ = mm;
    direction_ = direction;
    reverseTarget_ = false;
    chooseTarget();
    targetOriginUm_ = irApplicable(nowUs) ? latestIr_.nominalUm : 0;
    targetOriginValid_ = irApplicable(nowUs);
    relationshipReliable_ = targetOriginValid_;
    lastAcceptedOpeningUs_ = nowUs;
    resetTargetEvidence();
    spatialPhase_ = SpatialPhase::None;
    setDegraded(!relationshipReliable_ || !targetOriginValid_);
    push(EwoEventKind::Declared);
  }

  void reverse(int8_t direction, uint64_t nowUs) {
    if (!declared_ || (direction != 1 && direction != -1) ||
        direction == direction_) return;
    beginDecision(nowUs);
    contextSinceUs_ = nowUs;
    direction_ = direction;
    // The last passed MM is encountered first when running back over it.
    reverseTarget_ = true;
    chooseTarget();
    targetOriginValid_ = false;  // unsigned IR travel has a new direction frame
    relationshipReliable_ = false;
    lastAcceptedOpeningUs_ = nowUs;
    resetTargetEvidence();
    spatialPhase_ = SpatialPhase::None;
    setDegraded(true);
    push(EwoEventKind::Reversed);
  }

  void observeIr(const ir_movement::WireSnapshot& w, uint64_t receivedUs,
                 uint8_t pwm, const uint8_t* sourceMac = nullptr,
                 uint64_t judgmentUs = 0) {
    beginDecision(judgmentUs ? judgmentUs : receivedUs);
    ++irObservationCount_;
    bool sourceChanged = false;
    if (sourceMac && haveSource_)
      for (size_t i = 0; i < 6; ++i) sourceChanged |= latestIrMac_[i] != sourceMac[i];
    if (sourceMac)
      for (size_t i = 0; i < 6; ++i) latestIrMac_[i] = sourceMac[i];
    if (sourceMac) haveSource_ = true;
    if (sourceChanged) irEpoch_.sourceChanged();
    const bool hadIr = haveIr_;
    const auto prior = latestIr_;
    const bool sameFrame = hadIr && !sourceChanged && w.bootId == prior.bootId &&
                           w.calibrationId == prior.calibrationId &&
                           w.pitchUm == prior.pitchUm;
    const bool ordered = !hadIr || !sameFrame ||
                         (w.sequence > prior.sequence &&
                          w.capturedUs > prior.capturedUs &&
                          w.completedPulses >= prior.completedPulses);
    const bool discontinuity = hadIr &&
        (!sameFrame || !ordered || receivedUs < latestIrReceivedUs_ ||
         receivedUs - latestIrReceivedUs_ > kIrFreshUs ||
         w.sampleGaps != prior.sampleGaps ||
         w.saturatedSamples != prior.saturatedSamples ||
         w.openAborts != prior.openAborts ||
         w.inferredAdded != prior.inferredAdded ||
         w.inferredRemoved != prior.inferredRemoved);
    latestIr_ = w;
    latestIrReceivedUs_ = receivedUs;
    haveIr_ = true;
    bootReference_.origin(w.completedPulses);
    if (discontinuity) bootReference_.newFrameBeforeCollection(w.completedPulses);
    if (!ordered) {
      resetTargetEvidence();
      irContinuity_ = false;
      interpretedHealth_.healthy = false;
      interpretedHealth_.fault = ngr_nav::IrHealthFault::OrderFault;
      interpretedHealth_.readiness = ngr_nav::IrReadiness::Unavailable;
      interpretedHealth_.detectorReason = w.opticalReason;
      speedAvailable_ = false;
      invalidateRelationship();
      invalidateBoot();
      return;
    }
    const bool priorContinuity = irContinuity_;
    interpretedHealth_ = ngr_nav::classifyIrInstrument(w);
    // Decision 0108: INADEQUATE_CONTRAST describes the optical window. A
    // continuous, unchanged pulse count remains a factual no-change result;
    // it is not automatically a failed measurement or an epoch break. In
    // particular, a PWM-zero station dwell keeps the established IR/MM frame.
    // Positive progression under that diagnostic is not silently qualified.
    const bool continuousNoChange = priorContinuity && hadIr && sameFrame && ordered &&
        w.completedPulses == prior.completedPulses && !discontinuity;
    if (w.opticalReason == ir_movement::INADEQUATE_CONTRAST &&
        continuousNoChange) {
      irContinuity_ = true;
      interpretedHealth_.healthy = true;
      interpretedHealth_.fault = ngr_nav::IrHealthFault::None;
      interpretedHealth_.readiness = ngr_nav::IrReadiness::Ready;
    } else {
      const auto update = irEpoch_.ingest(w);
      irContinuity_ = update.measurementReady && !discontinuity;
    }
    if (discontinuity || !irContinuity_) {
      resetTargetEvidence();
      invalidateRelationship();
      if (bootReference_.started()) invalidateBoot();
    }
    speedAvailable_ = priorContinuity && sameFrame && ordered &&
                      irContinuity_ && !discontinuity;
    if (speedAvailable_)
      speedMmS_ = double(w.completedPulses - prior.completedPulses) *
                  double(w.pitchUm) * 1000.0 / double(w.capturedUs - prior.capturedUs);
    const bool advanced = sameFrame && ordered &&
                          w.completedPulses > prior.completedPulses;
    if (advanced && pwm == 0 && bootReference_.started()) invalidateBoot();
    if (advanced && pwm == 0 && declared_) {
      resetTargetEvidence();
      positionReliable_ = false;
      invalidateRelationship();
      ++pwmZeroDisplacements_;
      push(EwoEventKind::PwmZeroDisplacement);
      setDegraded(true);
    }
    // IR at PWM=0 remains a fact, but cannot progress references, targets or
    // distance rulings. The anomaly above invalidates only the relationship.
    if (pwm == 0) return;
    if (!irApplicable(decisionUs_)) {
      invalidateRelationship();
      return;
    }
    setDegraded(!relationshipReliable_ || !targetOriginValid_);
    if (!initialReferenceReady_) {
      bootReference_.progress(w.completedPulses);
      if (bootReference_.fault()) reportBootFault();
      if (bootReference_.ready()) closeInitialReference();
    }
    if (spatialPhase_ != SpatialPhase::None) updateSpatialPhase(w.nominalUm);
    // A packet arriving after declaration or a degraded confirmation is not
    // itself a mapped landmark. Only declaration with a contemporaneous IR
    // fact, or a later confirmed target, may anchor the target-distance frame.
    evaluateMissing(w.nominalUm);
  }

  void observeHall(const HallSample& sample) {
    observeHall(sample, sample.timestampUs); // synchronous host callers
  }
  void observeHall(const HallSample& sample, uint64_t judgmentUs) {
    beginDecision(judgmentUs);
    ++hallObservationCount_;
    lastHall_ = sample;
    haveHall_ = true;
    if (sample.pwm == 0) return;  // still delivered and retained
    if (!irApplicable(decisionUs_)) {
      invalidateRelationship();
      if (bootReference_.started()) invalidateBoot();
    }
    if (!initialReferenceReady_) {
      if (irApplicable(decisionUs_)) bootReference_.observe(sample.raw);
      if (bootReference_.fault()) reportBootFault();
      return;
    }
    // A declaration/reversal clears incompatible queued evidence as well as
    // the rolling window. Acquisition and its recording remain untouched.
    if (!declared_ || sample.timestampUs < contextSinceUs_) return;
    if (spatialPhase_ != SpatialPhase::None &&
        irApplicable(decisionUs_) && relationshipReliable_) {
      collectSpatial(sample.raw);
      return;
    }
    HallPoint point{sample.sampleSerial, sample.timestampUs, sample.raw,
                    haveIr_ ? latestIr_.nominalUm : 0,
                    irApplicable(decisionUs_)};
    hallWindow_.push_back(point);
    if (hallWindow_.size() > 5) hallWindow_.erase(hallWindow_.begin());
    if (hallWindow_.size() < 5) return;
    int16_t ordered[5];
    for (size_t i = 0; i < 5; ++i) ordered[i] = hallWindow_[i].raw;
    std::sort(ordered, ordered + 5);
    hallMedian_ = ordered[2];
    const int32_t departure = int32_t(hallMedian_) - activeReference_;
    const bool priorSupport = hallSupport_;
    hallSupport_ = (target_.polarity == HallOpeningPolarity::AboveReference &&
                    departure >= 70) ||
                   (target_.polarity == HallOpeningPolarity::BelowReference &&
                    departure <= -70);
    if (!hallSupport_) return;  // target-only shrug, including opposite field
    if (!priorSupport) push(EwoEventKind::HallSupport);
    if (sample.direction != (direction_ > 0 ? 1 : 2)) return;
    const HallPoint* landmark = nullptr;
    for (const HallPoint& p : hallWindow_) {
      const int32_t d = int32_t(p.raw) - activeReference_;
      if ((target_.polarity == HallOpeningPolarity::AboveReference && d >= 70) ||
          (target_.polarity == HallOpeningPolarity::BelowReference && d <= -70)) {
        landmark = &p;
        break;
      }
    }
    if (!landmark) return;
    const bool normal = irApplicable(decisionUs_) &&
                        relationshipReliable_ && targetOriginValid_;
    if (normal) {
      if (latestIr_.nominalUm < targetOriginUm_) return;
      const uint64_t traveled = latestIr_.nominalUm - targetOriginUm_;
      const uint64_t expected = uint64_t(target_.distanceMm) * 1000;
      if (traveled * 100 < expected * 85 ||
          traveled * 100 > expected * 115) return;
    } else if (sample.timestampUs < lastAcceptedOpeningUs_ ||
               sample.timestampUs - lastAcceptedOpeningUs_ < kDegradedGuardUs) {
      setDegraded(true);
      return;
    }
    confirm(*landmark, sample, !normal);
  }

  // A transport gap is a fact. NAVI invalidates only the IR distance relation;
  // it does not infer a new location or suppress future observations.
  void noteObservationLoss(uint32_t hallDrops, uint32_t irDrops, uint64_t nowUs = 0) {
    beginDecision(nowUs ? nowUs : decisionUs_);
    const bool newIrLoss = irDrops != irLoss_;
    const bool newHallLoss = hallDrops != hallLoss_;
    hallLoss_ = hallDrops;
    irLoss_ = irDrops;
    if (newIrLoss) {
      irContinuity_ = false;
      speedAvailable_ = false;
      invalidateRelationship();
    }
    if (newIrLoss || newHallLoss) {
      resetTargetEvidence();
      if (bootReference_.started()) invalidateBoot();
    }
    push(EwoEventKind::ObservationLoss);
  }

  bool takeEvent(EwoEvent& out) {
    if (eventTail_ == eventHead_) return false;
    out = events_[eventTail_];
    eventTail_ = (eventTail_ + 1) % kEventCapacity;
    return true;
  }

  bool irApplicable(uint64_t nowUs) const {
    return haveIr_ && irContinuity_ && nowUs >= latestIrReceivedUs_ &&
           nowUs - latestIrReceivedUs_ <= kIrFreshUs;
  }
  bool irMeasurementEpochActive() const { return irEpoch_.epochActive(); }
  uint64_t irMeasurementEpochId() const { return irEpoch_.epochId(); }
  uint8_t irHealthFault() const {
    return static_cast<uint8_t>(interpretedHealth_.fault);
  }
  uint8_t irReadiness() const {
    return static_cast<uint8_t>(interpretedHealth_.readiness);
  }
  bool declared() const { return declared_; }
  bool positionReliable() const { return positionReliable_; }
  uint8_t mm() const { return mm_; }
  int8_t direction() const { return direction_; }
  TargetSpec target() const { return target_; }
  bool relationshipReliable() const { return relationshipReliable_; }
  bool initialReferenceReady() const { return initialReferenceReady_; }
  bool storageReady() const { return bootReference_.storageReady(); }
  bool initialCollectionStarted() const { return bootReference_.started(); }
  bool bootReferenceIncomplete() const { return bootReference_.fault(); }
  uint8_t bootReferencePositions() const { return bootReference_.positions(); }
  uint64_t consumptionId() const { return consumptionId_; }
  bool irSpeedAvailable(uint64_t nowUs) const { return speedAvailable_ && irApplicable(nowUs); }
  double irSpeedMmS() const { return speedMmS_; }
  int16_t activeReference() const { return activeReference_; }
  int16_t hallMedian() const { return hallMedian_; }
  bool hallSupport() const { return hallSupport_; }
  uint32_t lastHallSerial() const { return haveHall_ ? lastHall_.sampleSerial : 0; }
  HallSample lastHall() const { return lastHall_; }
  ir_movement::WireSnapshot latestIr() const { return latestIr_; }
  const uint8_t* latestIrMac() const { return latestIrMac_; }
  uint64_t latestIrReceivedUs() const { return latestIrReceivedUs_; }
  uint64_t hallObservationCount() const { return hallObservationCount_; }
  uint64_t irObservationCount() const { return irObservationCount_; }
  uint32_t missedCount() const { return missedCount_; }
  uint32_t confirmedCount() const { return confirmedCount_; }
  uint32_t pwmZeroDisplacements() const { return pwmZeroDisplacements_; }
  uint32_t hallLoss() const { return hallLoss_; }
  uint32_t irLoss() const { return irLoss_; }
  uint32_t eventLoss() const { return eventLoss_; }
  uint64_t spatialOriginUm() const { return spatialOriginUm_; }
  uint8_t spatialPhase() const { return static_cast<uint8_t>(spatialPhase_); }
  bool degraded() const { return degraded_; }
  uint32_t openingSerial() const { return openingSerial_; }
  uint64_t openingIrUm() const { return openingIrUm_; }

 private:
  struct HallPoint {
    uint32_t serial;
    uint64_t timestampUs;
    int16_t raw;
    uint64_t irUm;
    bool irApplicable;
  };
  enum class SpatialPhase : uint8_t { None, Clearance, Collect };
  static constexpr size_t kEventCapacity = 64;

  static int16_t median(std::vector<int16_t>& values) {
    std::sort(values.begin(), values.end());
    const size_t mid = values.size() / 2;
    if (values.size() & 1) return values[mid];
    return int16_t((int32_t(values[mid - 1]) + values[mid]) / 2);
  }
  void push(EwoEventKind kind) {
    const size_t next = (eventHead_ + 1) % kEventCapacity;
    if (next == eventTail_) { ++eventLoss_; return; }
    events_[eventHead_] = {kind, decisionUs_,
                           haveHall_ ? lastHall_.sampleSerial : 0,
                           mm_, static_cast<uint8_t>(target_.sequence),
                           direction_, haveIr_ ? latestIr_.nominalUm : 0,
                           hallMedian_, activeReference_, degraded_,
                           openingSerial_, openingIrUm_, positionReliable_,
                           static_cast<uint8_t>(spatialPhase_), consumptionId_,
                           haveIr_ ? latestIr_.sequence : 0, hallLoss_, irLoss_};
    eventHead_ = next;
  }
  void setDegraded(bool degraded) {
    if (degraded_ == degraded) return;
    degraded_ = degraded;
    push(degraded ? EwoEventKind::IrDegraded : EwoEventKind::IrNormal);
  }
  void chooseTarget() {
    if (reverseTarget_) {
      target_ = mappedTargetAfter(navi_one::nextMarker(mm_, -direction_),
                                  direction_);
      target_.sequence = mm_;
    } else target_ = mappedTargetAfter(mm_, direction_);
  }
  void resetTargetEvidence() {
    hallWindow_.clear();
    hallSupport_ = false;
    hallMedian_ = 0;
  }
  void closeInitialReference() {
    if (initialReferenceReady_ || !bootReference_.ready()) return;
    activeReference_ = bootReference_.reference();
    initialReferenceReady_ = true;
    resetTargetEvidence();
    push(EwoEventKind::ReferenceReady);
  }
  void confirm(HallPoint landmark, const HallSample&,
               bool degraded) {
    setDegraded(degraded);
    openingSerial_ = landmark.serial;
    openingIrUm_ = landmark.irApplicable ? landmark.irUm : 0;
    lastAcceptedOpeningUs_ = landmark.timestampUs;
    mm_ = static_cast<uint8_t>(target_.sequence);
    positionReliable_ = true;
    reverseTarget_ = false;
    ++confirmedCount_;
    const bool boundaryMeasured = landmark.irApplicable &&
                                  irApplicable(decisionUs_);
    const bool reanchored = !relationshipReliable_ && boundaryMeasured;
    if (boundaryMeasured) {
      // The field's observed leading boundary, not the later median decision,
      // is the physical landmark for both mapped distance and Function 4.
      targetOriginUm_ = landmark.irUm;
      targetOriginValid_ = true;
      relationshipReliable_ = true;
    } else {
      targetOriginValid_ = false;
      relationshipReliable_ = false;
    }
    push(EwoEventKind::TargetConfirmed);
    if (reanchored) push(EwoEventKind::Reanchored);
    chooseTarget();
    resetTargetEvidence();
    if (boundaryMeasured) {
      spatialOriginUm_ = landmark.irUm;
      spatialPhase_ = SpatialPhase::Clearance;
      for (auto& occupied : spatialOccupied_) occupied = false;
      spatialCount_ = 0;
      push(EwoEventKind::SpatialClearance);
    } else spatialPhase_ = SpatialPhase::None;
    setDegraded(!irApplicable(decisionUs_) ||
                !relationshipReliable_ || !targetOriginValid_);
  }
  void updateSpatialPhase(uint64_t nowUm) {
    if (nowUm < spatialOriginUm_) return;
    const uint64_t traveled = nowUm - spatialOriginUm_;
    if (traveled >= 100000 && spatialPhase_ == SpatialPhase::Clearance) {
      spatialPhase_ = SpatialPhase::Collect;
      push(EwoEventKind::SpatialCollect);
    }
    if (traveled < 200000 || spatialPhase_ != SpatialPhase::Collect) return;
    if (spatialCount_) {
      std::vector<int16_t> represented;
      represented.reserve(spatialCount_);
      for (size_t i = 0; i < 101; ++i)
        if (spatialOccupied_[i]) represented.push_back(spatialValues_[i]);
      activeReference_ = median(represented);
      spatialPhase_ = SpatialPhase::None;
      resetTargetEvidence();
      push(EwoEventKind::SpatialReady);
    } else {
      spatialPhase_ = SpatialPhase::None;
      resetTargetEvidence();
      push(EwoEventKind::SpatialEmpty);
    }
    // An empty interval cannot produce a median; keep the prior reference and
    // report the missing population rather than fabricate one.
  }
  void collectSpatial(int16_t raw) {
    if (spatialPhase_ != SpatialPhase::Collect || !haveIr_ ||
        latestIr_.nominalUm < spatialOriginUm_) return;
    const uint64_t distance = latestIr_.nominalUm - spatialOriginUm_;
    if (distance < 100000 || distance >= 200000) return;
    const size_t index = static_cast<size_t>(distance / 1000) - 100;
    if (!spatialOccupied_[index]) {
      spatialOccupied_[index] = true;
      spatialValues_[index] = raw;
      ++spatialCount_;
    }
  }
  void evaluateMissing(uint64_t nowUm) {
    if (!declared_ || !initialReferenceReady_ ||
        spatialPhase_ != SpatialPhase::None || !relationshipReliable_ ||
        !targetOriginValid_ || !irApplicable(decisionUs_) ||
        nowUm < targetOriginUm_) return;
    for (unsigned count = 0; count < navi_one::ROUTE_N; ++count) {
      const uint64_t expected = uint64_t(target_.distanceMm) * 1000;
      if ((nowUm - targetOriginUm_) * 100 <= expected * 115) break;
      targetOriginUm_ += expected;
      mm_ = static_cast<uint8_t>(target_.sequence);
      reverseTarget_ = false;
      ++missedCount_;
      push(EwoEventKind::MissedMagnet);
      chooseTarget();
      resetTargetEvidence();
    }
  }

  void beginDecision(uint64_t nowUs) { decisionUs_ = nowUs; ++consumptionId_; }
  void invalidateRelationship() {
    if (targetOriginValid_ || relationshipReliable_ || spatialPhase_ != SpatialPhase::None)
      resetTargetEvidence();
    targetOriginValid_ = relationshipReliable_ = false;
    if (spatialPhase_ != SpatialPhase::None) {
      spatialPhase_ = SpatialPhase::None;
      spatialCount_ = 0;
      push(EwoEventKind::SpatialInvalidated);
    }
    setDegraded(true);
  }
  void reportBootFault() {
    if (!bootFaultReported_) { bootFaultReported_ = true; push(EwoEventKind::BootReferenceIncomplete); }
  }
  void invalidateBoot() { bootReference_.invalidate(); if (bootReference_.fault()) reportBootFault(); }
  uint64_t decisionUs_ = 0, consumptionId_ = 0, contextSinceUs_ = 0;
  bool haveSource_ = false, speedAvailable_ = false, bootFaultReported_ = false;
  double speedMmS_ = 0;
  bool declared_ = false, reverseTarget_ = false;
  bool positionReliable_ = false;
  uint8_t mm_ = 0;
  int8_t direction_ = 0;
  TargetSpec target_{};
  bool relationshipReliable_ = false, targetOriginValid_ = false;
  uint64_t targetOriginUm_ = 0, lastAcceptedOpeningUs_ = 0;
  bool haveIr_ = false, irContinuity_ = false, degraded_ = true;
  ir_movement::WireSnapshot latestIr_{};
  uint8_t latestIrMac_[6]{};
  uint64_t latestIrReceivedUs_ = 0;
  ngr_nav::IrOdometryEpoch irEpoch_;
  ngr_nav::IrInstrumentState interpretedHealth_{};
  bool haveHall_ = false;
  HallSample lastHall_{};
  uint64_t hallObservationCount_ = 0, irObservationCount_ = 0;
  uint32_t hallLoss_ = 0, irLoss_ = 0, eventLoss_ = 0;
  uint32_t confirmedCount_ = 0, missedCount_ = 0, pwmZeroDisplacements_ = 0;
  BootReference bootReference_;
  bool initialReferenceReady_ = false;
  int16_t activeReference_ = 0, hallMedian_ = 0;
  bool hallSupport_ = false;
  std::vector<HallPoint> hallWindow_;
  uint32_t openingSerial_ = 0;
  uint64_t openingIrUm_ = 0, spatialOriginUm_ = 0;
  SpatialPhase spatialPhase_ = SpatialPhase::None;
  bool spatialOccupied_[101]{};
  int16_t spatialValues_[101]{};
  size_t spatialCount_ = 0;
  EwoEvent events_[kEventCapacity]{};
  size_t eventHead_ = 0, eventTail_ = 0;
};

}  // namespace navi_eyes
