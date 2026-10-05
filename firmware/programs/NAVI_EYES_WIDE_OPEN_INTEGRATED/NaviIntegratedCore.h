#pragma once

#include <algorithm>
#include <stdint.h>
#include <vector>

#include "../NAVI_EYES_WIDE_OPEN/NaviEvidence.h"
#include "NaviMapAdapter.h"
#include "../../reference/NAVI_COHERENCE/IR_ARCHITECTURE_0_4/IrInstrument.h"

namespace navi_eyes {

static constexpr const char* kPwmZeroMovementWarning =
    "Movement detected while PWM=0. Reposition/verify locomotive and declare position before resuming navigation.";

enum class EwoEventKind : uint8_t {
  None, Declared, Reversed, HallSupport, TargetConfirmed, MissedMagnet,
  ReferenceReady, SpatialClearance, SpatialCollect, SpatialReady, SpatialEmpty,
  IrDistanceHold, IrDistanceReady, PwmZeroDisplacement,
  // NSR1 event 14 is retired; retain the existing IDs of current events.
  ObservationLoss = 15,
  SpatialInvalidated
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
  bool distanceHolding = false;
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

  void declare(uint8_t mm, int8_t direction, uint64_t nowUs) {
    if (mm >= navi_one::ROUTE_N || (direction != 1 && direction != -1)) return;
    beginDecision(nowUs);
    contextSinceUs_ = nowUs;
    declared_ = true;
    positionReliable_ = true;
    mm_ = mm;
    direction_ = direction;
    reverseTarget_ = false;
    firstTargetAfterDeclare_ = true;
    firstTargetSupportAbsent_ = false;
    pwmZeroMovementRequiresDeclaration_ = false;
    chooseTarget();
    // Until the first Hall confirmation, this records movement since declaration,
    // not a known marker position or an expected first-target distance.
    physicalOriginUm_ = irApplicable(nowUs) ? latestIr_.nominalUm : 0;
    expectedCumulativeUm_ = irApplicable(nowUs) ?
        uint64_t(target_.distanceMm) * 1000 : 0;
    targetOriginValid_ = irApplicable(nowUs);
    relationshipReliable_ = targetOriginValid_;
    frameLost_ = false;
    resetTargetEvidence();
    spatialPhase_ = SpatialPhase::None;
    setDistanceHolding(!relationshipReliable_ || !targetOriginValid_);
    push(EwoEventKind::Declared);
  }

  void reverse(int8_t direction, uint64_t nowUs) {
    if (!declared_ || (direction != 1 && direction != -1) ||
        direction == direction_) return;
    beginDecision(nowUs);
    // The type-5 counter is cumulative in either travel direction. Retain
    // position relative to mm_ even through repeated reversals before a Hall
    // confirmation: the current target may be mm_ or its mapped neighbor.
    const int8_t oldDirection = direction_;
    const int64_t oldInterval = int64_t(navi_one::spanMm(mm_, oldDirection)) * 1000;
    const int64_t newInterval = int64_t(navi_one::spanMm(mm_, direction)) * 1000;
    const bool coherent = irApplicable(nowUs) && relationshipReliable_ &&
                          targetOriginValid_ && latestIr_.nominalUm >= physicalOriginUm_ &&
                          latestIr_.nominalUm - physicalOriginUm_ <= INT64_MAX / 2 &&
                          expectedCumulativeUm_ <= INT64_MAX / 2;
    const int64_t traveled = coherent ?
        int64_t(latestIr_.nominalUm - physicalOriginUm_) : 0;
    const int64_t remaining = coherent ? int64_t(expectedCumulativeUm_) - traveled : 0;
    const int64_t oldPosition = reverseTarget_ ? -remaining : oldInterval - remaining;
    const int64_t newPosition = -oldPosition;
    // newPosition < 0 means mm_ is ahead in the new direction; otherwise
    // the next mapped marker is ahead. Out-of-interval coordinates are held
    // for operator review rather than inventing a target.
    const bool reversible = coherent &&
        (newPosition < 0 ? -newPosition <= oldInterval : newPosition <= newInterval);
    const bool seekCurrentMm = newPosition < 0;
    const uint64_t distanceToTarget = reversible ? uint64_t(seekCurrentMm ?
        -newPosition : newInterval - newPosition) : 0;
    contextSinceUs_ = nowUs;
    firstTargetAfterDeclare_ = false;
    direction_ = direction;
    reverseTarget_ = seekCurrentMm;
    chooseTarget();
    if (reversible) {
      physicalOriginUm_ = latestIr_.nominalUm;
      expectedCumulativeUm_ = distanceToTarget;
    } else {
      targetOriginValid_ = false;
      relationshipReliable_ = false;
      frameLost_ = true;
    }
    resetTargetEvidence();
    spatialPhase_ = SpatialPhase::None;
    setDistanceHolding(!reversible);
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
    const bool hadIr = haveIr_;
    const auto prior = latestIr_;
    const uint64_t priorReceivedUs = latestIrReceivedUs_;
    // Source and boot identify the measurement stream. Pitch is installed
    // configuration; calibrationId is retained raw wire evidence only.
    const bool sameFrame = hadIr && !sourceChanged && w.bootId == prior.bootId;
    const bool ordered = !hadIr || !sameFrame ||
                         (w.sequence > prior.sequence &&
                          w.capturedUs > prior.capturedUs &&
                          w.completedPulses >= prior.completedPulses);
    const bool validScale = w.bootId && ir_movement::validConfiguredDistance(w);
    const bool discontinuity = hadIr &&
        (!sameFrame || !ordered || receivedUs < latestIrReceivedUs_);
    latestIr_ = w;
    latestIrReceivedUs_ = receivedUs;
    haveIr_ = true;
    interpretedHealth_ = ngr_nav::classifyIrInstrument(w);
    if (!ordered || !validScale || (hadIr && receivedUs < priorReceivedUs)) {
      resetTargetEvidence();
      irContinuity_ = false;
      interpretedHealth_.fault = validScale ? ngr_nav::IrHealthFault::OrderFault :
                                            ngr_nav::IrHealthFault::PacketInvalid;
      interpretedHealth_.readiness = ngr_nav::IrReadiness::Unavailable;
      epochActive_ = false;
      clearSpeedHistory();
      invalidateRelationship();
      return;
    }
    // Health is recorded exactly as classified, but only mathematical frame
    // integrity controls distance applicability. Diagnostic counter changes,
    // low contrast, saturation and reacquisition cannot veto cumulative pulses.
    if (discontinuity) {
      resetTargetEvidence();
      invalidateRelationship();
      clearSpeedHistory();
    }
    if (!epochActive_ || discontinuity) { ++epochId_; epochActive_ = true; }
    irContinuity_ = true;
    addSpeedPoint(w);
    const bool advanced = sameFrame && ordered &&
                          w.completedPulses > prior.completedPulses;
    if (advanced && pwm == 0 && declared_) {
      // Wheel movement is factual, but unsigned handling is not route travel.
      // Do not erase interval/context or disguise an independent frame failure.
      requireDeclarationAfterMovement();
      ++pwmZeroDisplacements_;
      push(EwoEventKind::PwmZeroDisplacement);
      setDistanceHolding(true);
    }
    // IR at PWM=0 remains a fact, but cannot progress references, targets or
    // distance rulings. A zero-displacement dwell leaves a known coordinate alone.
    if (pwm == 0 || pwmZeroMovementRequiresDeclaration_) return;
    if (!irApplicable(decisionUs_)) return;
    setDistanceHolding(!relationshipReliable_ || !targetOriginValid_);
    if (spatialPhase_ != SpatialPhase::None) updateSpatialPhase(w.nominalUm);
    // A packet is not a mapped landmark; only an operator declaration or a
    // Hall+IR confirmation can establish a physical origin.
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
    if (pwmZeroMovementRequiresDeclaration_) return; // only declaration can resume navigation
    // The first native Hall ADC is the provisional boot reference. It is
    // deliberately independent of IR applicability and never blocks startup.
    if (!initialReferenceReady_) {
      if (sample.pwm == 0) return;
      activeReference_ = sample.raw;
      initialReferenceReady_ = true;
      push(EwoEventKind::ReferenceReady);
      return;
    }
    // A declaration/reversal clears incompatible queued evidence as well as
    // the rolling window. Acquisition and its recording remain untouched.
    if (!declared_ || sample.timestampUs < contextSinceUs_) return;
    if (firstTargetAfterDeclare_ && sample.timestampUs == contextSinceUs_) return;
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
    if (!hallSupport_) {
      // Only an observed full window counts as absence; resetTargetEvidence()
      // and the initial false hallSupport_ are not a post-declaration onset.
      if (firstTargetAfterDeclare_) firstTargetSupportAbsent_ = true;
      return;  // target-only shrug, including opposite field
    }
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
    if (!landmark || !landmark->irApplicable) return;
    const bool normal = irApplicable(decisionUs_) &&
                        relationshipReliable_ && targetOriginValid_;
    if (normal) {
      if (latestIr_.nominalUm < physicalOriginUm_) return;
      const uint64_t traveled = latestIr_.nominalUm - physicalOriginUm_;
      const uint64_t expectedInterval = uint64_t(target_.distanceMm) * 1000;
      const uint64_t tolerance = expectedInterval * 15 / 100;
      if (reverseTarget_ && traveled == 0) return;
      if (firstTargetAfterDeclare_) {
        if (!firstTargetSupportAbsent_ || traveled == 0) return;
      } else if (traveled + tolerance < expectedCumulativeUm_ ||
                 traveled > expectedCumulativeUm_ + tolerance) return;
    } else return;  // No measured distance means no MM confirmation.
    confirm(*landmark);
  }

  // Transport loss is visible, but a cumulative counter can bridge a missing
  // packet. It does not infer a new location or suppress later observations.
  void noteObservationLoss(uint32_t hallDrops, uint32_t irDrops, uint64_t nowUs = 0) {
    beginDecision(nowUs ? nowUs : decisionUs_);
    const bool newIrLoss = irDrops != irLoss_;
    const bool newHallLoss = hallDrops != hallLoss_;
    hallLoss_ = hallDrops;
    irLoss_ = irDrops;
    // A dropped packet does not erase a cumulative counter. Hold while stale;
    // a later same-frame ordered snapshot can bridge the transport gap.
    if (newIrLoss || newHallLoss) {
      resetTargetEvidence();
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
  bool irMeasurementEpochActive() const { return epochActive_; }
  uint64_t irMeasurementEpochId() const { return epochId_; }
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
  bool pwmZeroMovementRequiresDeclaration() const { return pwmZeroMovementRequiresDeclaration_; }
  const char* irDistanceState(uint64_t nowUs) const {
    if (!haveIr_) return "NO_IR_SOURCE";
    if (frameLost_) return "FRAME_LOST_REDECLARE";
    // A physical-movement hold is not an instrument fault. A separate frame
    // failure takes precedence above; neither condition permits Hall recovery.
    if (pwmZeroMovementRequiresDeclaration_) return "PWM_ZERO_MOVEMENT_REDECLARE";
    if (!irApplicable(nowUs)) return "IR_STALE";
    if (!relationshipReliable_ || !targetOriginValid_) return "UNANCHORED_REDECLARE";
    return "HALL_IR_READY";
  }
  bool initialReferenceReady() const { return initialReferenceReady_; }
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
  bool distanceHolding() const { return distanceHolding_; }
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
                           hallMedian_, activeReference_, distanceHolding_,
                           openingSerial_, openingIrUm_, positionReliable_,
                           static_cast<uint8_t>(spatialPhase_), consumptionId_,
                           haveIr_ ? latestIr_.sequence : 0, hallLoss_, irLoss_};
    eventHead_ = next;
  }
  void setDistanceHolding(bool holding) {
    if (distanceHolding_ == holding) return;
    distanceHolding_ = holding;
    push(holding ? EwoEventKind::IrDistanceHold : EwoEventKind::IrDistanceReady);
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
  void clearSpeedHistory() { speedPointCount_ = 0; speedAvailable_ = false; }
  void addSpeedPoint(const ir_movement::WireSnapshot& w) {
    if (speedPointCount_ == kSpeedPointCapacity) {
      for (size_t i = 1; i < speedPointCount_; ++i)
        speedPoints_[i - 1] = speedPoints_[i];
      --speedPointCount_;
    }
    speedPoints_[speedPointCount_++] = {w.capturedUs, w.completedPulses};
    speedAvailable_ = false;
    // A full-second pulse window avoids the 0/1/2-pulse, 100-ms speed steps.
    for (size_t i = speedPointCount_ - 1; i-- > 0;) {
      const uint64_t dt = w.capturedUs - speedPoints_[i].capturedUs;
      if (dt < 900000) continue;
      if (dt > 1500000) break;
      speedMmS_ = double(w.completedPulses - speedPoints_[i].pulses) *
                  double(w.pitchUm) * 1000.0 / double(dt);
      speedAvailable_ = true;
      break;
    }
  }
  void confirm(HallPoint landmark) {
    firstTargetAfterDeclare_ = false;
    setDistanceHolding(false);
    openingSerial_ = landmark.serial;
    openingIrUm_ = landmark.irUm;
    mm_ = static_cast<uint8_t>(target_.sequence);
    positionReliable_ = true;
    reverseTarget_ = false;
    ++confirmedCount_;
    // The field's observed leading boundary, not the later median decision,
    // is the physical landmark for both mapped distance and Function 4.
    physicalOriginUm_ = landmark.irUm;
    expectedCumulativeUm_ = uint64_t(navi_one::spanMm(mm_, direction_)) * 1000;
    targetOriginValid_ = true;
    relationshipReliable_ = true;
    push(EwoEventKind::TargetConfirmed);
    chooseTarget();
    resetTargetEvidence();
    spatialOriginUm_ = landmark.irUm;
    spatialPhase_ = SpatialPhase::Clearance;
    for (auto& occupied : spatialOccupied_) occupied = false;
    spatialCount_ = 0;
    push(EwoEventKind::SpatialClearance);
    setDistanceHolding(!irApplicable(decisionUs_) ||
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
    if (!declared_ || firstTargetAfterDeclare_ || pwmZeroMovementRequiresDeclaration_ || !initialReferenceReady_ ||
        spatialPhase_ != SpatialPhase::None || !relationshipReliable_ ||
        !targetOriginValid_ || !irApplicable(decisionUs_) ||
        nowUm < physicalOriginUm_) return;
    for (unsigned count = 0; count < navi_one::ROUTE_N; ++count) {
      const uint64_t interval = uint64_t(target_.distanceMm) * 1000;
      if (nowUm - physicalOriginUm_ <= expectedCumulativeUm_ + interval * 15 / 100) break;
      expectedCumulativeUm_ += uint64_t(navi_one::spanMm(target_.sequence, direction_)) * 1000;
      mm_ = static_cast<uint8_t>(target_.sequence);
      reverseTarget_ = false;
      ++missedCount_;
      push(EwoEventKind::MissedMagnet);
      chooseTarget();
      resetTargetEvidence();
    }
  }

  void beginDecision(uint64_t nowUs) { decisionUs_ = nowUs; ++consumptionId_; }
  void requireDeclarationAfterMovement() {
    pwmZeroMovementRequiresDeclaration_ = true;
    resetTargetEvidence();
    targetOriginValid_ = relationshipReliable_ = false;
    // A spatial collection measured from the old coordinate cannot continue;
    // the last active Hall reference itself remains authoritative.
    if (spatialPhase_ != SpatialPhase::None) {
      spatialPhase_ = SpatialPhase::None;
      spatialCount_ = 0;
      push(EwoEventKind::SpatialInvalidated);
    }
    setDistanceHolding(true);
  }
  void invalidateRelationship() {
    if (targetOriginValid_ || relationshipReliable_ || spatialPhase_ != SpatialPhase::None)
      resetTargetEvidence();
    targetOriginValid_ = relationshipReliable_ = false;
    frameLost_ = true;
    if (spatialPhase_ != SpatialPhase::None) {
      spatialPhase_ = SpatialPhase::None;
      spatialCount_ = 0;
      push(EwoEventKind::SpatialInvalidated);
    }
    setDistanceHolding(true);
  }
  uint64_t decisionUs_ = 0, consumptionId_ = 0, contextSinceUs_ = 0;
  bool haveSource_ = false, speedAvailable_ = false;
  double speedMmS_ = 0;
  bool declared_ = false, reverseTarget_ = false;
  bool firstTargetAfterDeclare_ = false, firstTargetSupportAbsent_ = false;
  bool positionReliable_ = false;
  uint8_t mm_ = 0;
  int8_t direction_ = 0;
  TargetSpec target_{};
  bool relationshipReliable_ = false, targetOriginValid_ = false;
  bool frameLost_ = false;
  bool pwmZeroMovementRequiresDeclaration_ = false;
  uint64_t physicalOriginUm_ = 0, expectedCumulativeUm_ = 0;
  bool haveIr_ = false, irContinuity_ = false, distanceHolding_ = true;
  ir_movement::WireSnapshot latestIr_{};
  uint8_t latestIrMac_[6]{};
  uint64_t latestIrReceivedUs_ = 0;
  bool epochActive_ = false;
  uint64_t epochId_ = 0;
  struct SpeedPoint { uint64_t capturedUs, pulses; };
  static constexpr size_t kSpeedPointCapacity = 16;
  SpeedPoint speedPoints_[kSpeedPointCapacity]{};
  size_t speedPointCount_ = 0;
  ngr_nav::IrInstrumentState interpretedHealth_{};
  bool haveHall_ = false;
  HallSample lastHall_{};
  uint64_t hallObservationCount_ = 0, irObservationCount_ = 0;
  uint32_t hallLoss_ = 0, irLoss_ = 0, eventLoss_ = 0;
  uint32_t confirmedCount_ = 0, missedCount_ = 0, pwmZeroDisplacements_ = 0;
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
