#pragma once

#include <algorithm>
#include <stdint.h>
#include <vector>

#include "../NAVI_EYES_WIDE_OPEN/NaviEvidence.h"
#include "NaviMapAdapter.h"
#include "NaviDartEvidence.h"
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
    resetDartContext();
    contextSinceUs_ = nowUs;
    declared_ = true;
    positionReliable_ = true;
    mm_ = mm;
    direction_ = direction;
    reverseTarget_ = false;
    firstTargetAfterDeclare_ = true;
    physicalPositionAnchored_ = false;
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
    resetDartContext();
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
    retainIr(w, receivedUs);
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
    // Spatial reference progression follows Hall acquisition time below,
    // not a newer packet drained ahead of queued ADC observations.
    // A packet is not a mapped landmark; only an operator declaration or a
    // Hall+IR confirmation can establish a physical origin.
    // No Hall-free geographic advancement. Missing-target stopping is unapproved.
  }

  void observeHall(const HallSample& sample) { observeHall(sample, sample.timestampUs); }
  void observeHall(const HallSample& sample, uint64_t judgmentUs) {
    beginDecision(judgmentUs);
    ++hallObservationCount_;
    lastHall_ = sample; haveHall_ = true;
    if (!initialReferenceReady_) {
      activeReference_ = sample.raw; initialReferenceReady_ = true;
      ++referenceVersion_; push(EwoEventKind::ReferenceReady);
    }
    // Serial 1 starts the acquisition task's first five-conversion batch.
    // A gap cannot silently join partial batches or two qualifying ticks.
    const unsigned slot = sample.sampleSerial ? (sample.sampleSerial - 1) % 5 : 5;
    if (slot == 0) { batchCount_ = 0; batchFirst_ = sample; }
    if (slot >= 5 || slot != batchCount_ ||
        (slot && sample.sampleSerial != batchLastSerial_ + 1)) {
      batchCount_ = 0; tickPole_ = 0; detectorLatched_ = false;
      ++incompleteBatches_;
      if(declared_) comparisonIncomplete_=true;
      return;
    }
    batch_[batchCount_++] = sample.raw; batchLastSerial_ = sample.sampleSerial;
    // Preserve spatial-reference mechanism, but never return before detection.
    const DartIr sampleIr=irAt(sample.timestampUs);
    if (sample.pwm && spatialPhase_ != SpatialPhase::None &&
        sampleIr.applicable && relationshipReliable_) {
      updateSpatialPhase(sampleIr.um);
      collectSpatial(sample.raw,sampleIr.um);
    }
    if (batchCount_ != 5) return;
    batchCount_ = 0;
    int16_t values[5]; for (unsigned i=0;i<5;++i) values[i]=batch_[i];
    std::sort(values, values+5); hallMedian_=values[2];
    processedHallUs_=sample.timestampUs;
    const int32_t departure=int32_t(hallMedian_)-activeReference_;
    const int8_t pole=departure>=70 ? 1 : departure<=-70 ? -1 : 0;
    hallSupport_=pole==(target_.polarity==HallOpeningPolarity::AboveReference?1:-1);
    if (!pole) { tickPole_=0; detectorLatched_=false; return; }
    if (pole!=tickPole_) {
      tickPole_=pole; firstTickUs_=sample.timestampUs; detectorLatched_=false; return;
    }
    if (detectorLatched_) return;
    detectorLatched_=true;
    DartOpening o{};
    o.id=++nextOpeningId_; o.serial=sample.sampleSerial; o.timeUs=sample.timestampUs;
    o.firstTickUs=firstTickUs_; o.referenceVersion=referenceVersion_;
    o.median=hallMedian_; o.reference=activeReference_; o.pole=pole;
    o.pwm=sample.pwm; o.direction=sample.direction; o.ir=irAt(o.timeUs);
    recordDart(o,DartReason::Opening);
    if (!declared_ || !o.pwm || pwmZeroMovementRequiresDeclaration_ ||
        o.timeUs<=contextSinceUs_ || o.direction!=(direction_>0?1:2)) {
      recordDart(o,DartReason::Inapplicable); return;
    }
    if (pendingCount_==kPendingCapacity) {
      comparisonIncomplete_=true; ++pendingOverflows_;
      recordDart(o,DartReason::Incomplete); return;
    }
    if(hallSupport_) push(EwoEventKind::HallSupport);
    pending_[pendingCount_++]=o;
    if(pendingCount_>pendingHighWater_) pendingHighWater_=pendingCount_;
    evaluateOpening(o);
  }

  // Caller supplies only a fully processed native acquisition frontier. This
  // is separate from packet ingestion, so a queued Hall tie gets considered.
  void completeComparison(uint64_t throughUs, uint64_t judgmentUs) {
    beginDecision(judgmentUs);
    if (throughUs>processedHallUs_) throughUs=processedHallUs_;
    DartIr progress=irAt(throughUs);
    if (!bestValid_ || !lastHall_.pwm || comparisonIncomplete_ || !canCompare() ||
        !progress.applicable || progress.epoch!=best_.ir.epoch ||
        progress.um<physicalOriginUm_) return;
    if (progress.um-physicalOriginUm_>=boundaryUm_) selectBest(DartReason::Boundary);
  }
  uint64_t processedHallUs() const { return processedHallUs_; }
  bool takeDartRecord(DartRecord& r) {
    if (diagTail_==diagHead_) return false;
    r=diagnostics_[diagTail_]; diagTail_=(diagTail_+1)%kDiagCapacity; return true;
  }
  uint32_t diagnosticDrops() const { return diagnosticDrops_; }
  uint32_t openingCount() const { return nextOpeningId_; }
  uint32_t anchorMismatchCount() const { return anchorMismatches_; }
  uint32_t incompleteBatches() const { return incompleteBatches_; }
  uint32_t pendingOverflows() const { return pendingOverflows_; }
  bool comparisonIncomplete() const { return comparisonIncomplete_; }
  bool candidateAvailable() const { return bestValid_; }
  uint64_t candidateBoundaryUm() const { return boundaryUm_; }
  uint64_t candidateCoordinateUm() const { return best_.ir.um; }
  bool inferredAnchor() const { return inferredAnchor_; }
  int64_t cumulativeResidualUm() const { return cumulativeResidualUm_; }
  size_t pendingHighWater() const { return pendingHighWater_; }
  void noteOperatingContext(uint8_t context, uint64_t departureUs=0) {
    if (context!=operatingContext_) intervalContextChanged_=true;
    operatingContext_=context;
    if (departureUs) departureUs_=departureUs;
  }
  const DartTiming& timing(size_t index) const { return timing_[index%kTimingCapacity]; }
  size_t timingCount() const { return timingCount_; }

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
    if (newHallLoss) {
      comparisonIncomplete_=true;
      tickPole_=0; detectorLatched_=false; batchCount_=0;
    }
    (void)newIrLoss; // same-frame cumulative packets can bridge packet loss
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
  bool physicalPositionAnchored() const { return physicalPositionAnchored_; }
  // Read-only physical offset from mm_, signed in the current travel direction.
  // This exposes the same relationship used by reverse(); it does not advance
  // NAVI or establish a second position estimate. An interval declaration is
  // not an exact Hall anchor, including after reversing before the first Hall.
  bool physicalOffsetUm(uint64_t nowUs, int64_t& offsetUm) const {
    if (!declared_ || !physicalPositionAnchored_ || !positionReliable_ ||
        pwmZeroMovementRequiresDeclaration_ || !relationshipReliable_ ||
        !targetOriginValid_ || !irApplicable(nowUs) ||
        latestIr_.nominalUm < physicalOriginUm_ ||
        latestIr_.nominalUm - physicalOriginUm_ > INT64_MAX / 2 ||
        expectedCumulativeUm_ > INT64_MAX / 2) return false;
    const int64_t traveled = int64_t(latestIr_.nominalUm - physicalOriginUm_);
    const int64_t remaining = int64_t(expectedCumulativeUm_) - traveled;
    offsetUm = reverseTarget_ ? -remaining :
        int64_t(navi_one::spanMm(mm_, direction_)) * 1000 - remaining;
    return true;
  }
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
    bestValid_=false; tieValid_=false; pendingCount_=0;
    // Detector onset state is independent of target selection.
    hallSupport_ = false;
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
      ++referenceVersion_; tickPole_=0; detectorLatched_=false;
      push(EwoEventKind::SpatialReady);
    } else {
      spatialPhase_ = SpatialPhase::None;
      push(EwoEventKind::SpatialEmpty);
    }
    // An empty interval cannot produce a median; keep the prior reference and
    // report the missing population rather than fabricate one.
  }
  void collectSpatial(int16_t raw,uint64_t observedUm) {
    if (spatialPhase_ != SpatialPhase::Collect || observedUm < spatialOriginUm_) return;
    const uint64_t distance = observedUm - spatialOriginUm_;
    if (distance < 100000 || distance >= 200000) return;
    const size_t index = static_cast<size_t>(distance / 1000) - 100;
    if (!spatialOccupied_[index]) {
      spatialOccupied_[index] = true;
      spatialValues_[index] = raw;
      ++spatialCount_;
    }
  }
  static constexpr size_t kIrHistoryCapacity=32, kPendingCapacity=32,
      kDiagCapacity=16, kTimingCapacity=16;
  DartIr irHistory_[kIrHistoryCapacity]{};
  size_t irHistoryHead_=0, irHistoryCount_=0;
  DartOpening pending_[kPendingCapacity]{}, best_{}, tie_{};
  size_t pendingCount_=0, pendingHighWater_=0;
  bool bestValid_=false, tieValid_=false, inferredAnchor_=false, comparisonIncomplete_=false;
  bool replayingPending_=false;
  uint64_t bestErrorUm_=0, boundaryUm_=0, processedHallUs_=0;
  uint32_t nextOpeningId_=0, generation_=0, anchorMismatches_=0, pendingOverflows_=0;
  uint32_t referenceVersion_=0, incompleteBatches_=0;
  int16_t batch_[5]{}; unsigned batchCount_=0; HallSample batchFirst_{};
  uint32_t batchLastSerial_=0; int8_t tickPole_=0; bool detectorLatched_=false;
  uint64_t firstTickUs_=0;
  DartRecord diagnostics_[kDiagCapacity]{}; size_t diagHead_=0, diagTail_=0;
  uint32_t diagnosticDrops_=0;
  int64_t cumulativeResidualUm_=0, previousResidualUm_=0;
  DartOpening priorSelection_{};
  bool priorSelectionValid_=false, intervalContextChanged_=false;
  uint8_t operatingContext_=0; uint64_t departureUs_=0;
  DartTiming timing_[kTimingCapacity]{}; size_t timingHead_=0, timingCount_=0;

  void resetDartContext() {
    pendingCount_=0; bestValid_=tieValid_=false; comparisonIncomplete_=false;
    bestErrorUm_=boundaryUm_=0;
    priorSelectionValid_=false; cumulativeResidualUm_=previousResidualUm_=0;
    intervalContextChanged_=true; ++generation_;
  }
  void retainIr(const ir_movement::WireSnapshot& w,uint64_t receivedUs) {
    DartIr v{}; v.um=w.nominalUm; v.receivedUs=receivedUs; v.capturedUs=w.capturedUs;
    v.pulses=w.completedPulses; v.sequence=w.sequence; v.epoch=epochId_; v.boot=w.bootId; v.applicable=true;
    irHistory_[irHistoryHead_]=v;
    irHistoryHead_=(irHistoryHead_+1)%kIrHistoryCapacity;
    if(irHistoryCount_<kIrHistoryCapacity) ++irHistoryCount_;
  }
  DartIr irAt(uint64_t t) const {
    for(size_t i=0;i<irHistoryCount_;++i) {
      DartIr v=irHistory_[(irHistoryHead_+kIrHistoryCapacity-1-i)%kIrHistoryCapacity];
      if(v.receivedUs<=t) {
        v.applicable=v.epoch==epochId_ && irContinuity_ && t-v.receivedUs<=kIrFreshUs;
        return v;
      }
    }
    return {};
  }
  bool canCompare() const {
    return declared_ && !pwmZeroMovementRequiresDeclaration_ && targetOriginValid_ &&
      relationshipReliable_ && irApplicable(decisionUs_) && !comparisonIncomplete_;
  }
  void enqueueDiagnostic(const DartRecord& r) {
    const size_t next=(diagHead_+1)%kDiagCapacity;
    if(next==diagTail_) { ++diagnosticDrops_; return; }
    diagnostics_[diagHead_]=r; diagHead_=next;
  }
  void recordDart(const DartOpening& o,DartReason reason) {
    DartRecord r{}; r.opening=o; r.reason=reason; r.target=target_.sequence;
    r.decisionUs=decisionUs_; r.generation=generation_; r.anchorUm=physicalOriginUm_;
    r.errorUm=bestErrorUm_; r.boundaryUm=boundaryUm_; r.partnerId=tieValid_?tie_.id:0;
    r.context=operatingContext_; r.cumulativeUm=cumulativeResidualUm_;
    enqueueDiagnostic(r);
  }
  void evaluateOpening(const DartOpening& o) {
    if(comparisonIncomplete_) return;
    if(o.pole!=(target_.polarity==HallOpeningPolarity::AboveReference?1:-1)) {
      recordDart(o,DartReason::WrongPolarity); return;
    }
    if(firstTargetAfterDeclare_) {
      best_=o; bestValid_=true; tieValid_=false;
      selectBest(DartReason::Selected); return;
    }
    if(!canCompare() || !o.ir.applicable || o.ir.epoch!=epochId_ ||
        o.ir.um<physicalOriginUm_) { recordDart(o,DartReason::NoCoordinate); return; }
    const uint64_t x=o.ir.um-physicalOriginUm_, d=expectedCumulativeUm_;
    // Crossing strictly beyond the previous boundary cannot retrospectively
    // extend that search. selectBest replays retained later observations.
    if(bestValid_ && x>boundaryUm_) { selectBest(DartReason::Boundary); return; }
    const uint64_t error=x>d?x-d:d-x;
    if(!bestValid_ || error<bestErrorUm_) {
      const bool replacement=bestValid_;
      if(replacement) recordDart(best_,DartReason::Eliminated);
      best_=o; bestValid_=true; tieValid_=false; bestErrorUm_=error;
      if(d>UINT64_MAX-error) { comparisonIncomplete_=true; recordDart(o,DartReason::Incomplete); return; }
      boundaryUm_=d+error;
      recordDart(o,replacement?DartReason::Replaced:DartReason::Candidate);
      if(x>=d) selectBest(DartReason::Selected);
    } else if(error==bestErrorUm_ && o.ir.um!=best_.ir.um) {
      tie_=o; tieValid_=true; recordDart(o,DartReason::Tie); selectBest(DartReason::Selected);
    } else recordDart(o,DartReason::Eliminated);
  }
  void selectBest(DartReason reason) {
    if(!bestValid_ || comparisonIncomplete_) return;
    const DartOpening chosen=best_; const bool inferred=tieValid_;
    const uint64_t selected=tieValid_ ? best_.ir.um/2+tie_.ir.um/2+
      ((best_.ir.um%2+tie_.ir.um%2)/2) : best_.ir.um;
    const uint64_t consumedThrough=tieValid_?tie_.timeUs:chosen.timeUs;
    const uint8_t previousMm=mm_;
    DartRecord r{}; r.opening=chosen; r.partnerId=tieValid_?tie_.id:0;
    r.inferred=inferred; r.reason=reason; r.target=target_.sequence;
    r.generation=generation_; r.previousMm=previousMm; r.decisionUs=decisionUs_;
    r.errorUm=bestErrorUm_; r.boundaryUm=boundaryUm_; r.anchorUm=selected;
    r.context=operatingContext_; r.departureUs=departureUs_; r.priorResidualUm=previousResidualUm_;
    const bool measured=chosen.ir.applicable && chosen.ir.epoch==epochId_;
    if(!firstTargetAfterDeclare_ && targetOriginValid_ && measured && selected>=physicalOriginUm_ &&
        selected-physicalOriginUm_<=INT64_MAX && expectedCumulativeUm_<=INT64_MAX) {
      r.residualValid=true;
      r.residualUm=int64_t(selected-physicalOriginUm_)-int64_t(expectedCumulativeUm_);
      // Diagnostic accumulation cannot affect the coordinate or comparator.
      if((r.residualUm>0 && cumulativeResidualUm_>INT64_MAX-r.residualUm) ||
         (r.residualUm<0 && cumulativeResidualUm_<INT64_MIN-r.residualUm)) cumulativeResidualUm_=0;
      cumulativeResidualUm_+=r.residualUm; previousResidualUm_=r.residualUm;
    }
    r.cumulativeUm=cumulativeResidualUm_;
    if(priorSelectionValid_ && !inferred && chosen.timeUs>=priorSelection_.timeUs) {
      r.intervalUs=chosen.timeUs-priorSelection_.timeUs;
      r.intervalValid=!intervalContextChanged_;
      DartTiming t{}; t.fromUs=priorSelection_.timeUs; t.toUs=chosen.timeUs;
      t.fromId=priorSelection_.id; t.toId=chosen.id; t.fromMm=previousMm;
      t.toMm=target_.sequence; t.direction=direction_; t.context=operatingContext_;
      t.departureUs=departureUs_; t.valid=r.intervalValid;
      timing_[timingHead_]=t; timingHead_=(timingHead_+1)%kTimingCapacity;
      if(timingCount_<kTimingCapacity) ++timingCount_;
    }
    firstTargetAfterDeclare_=false; physicalPositionAnchored_=true;
    openingSerial_=chosen.serial; openingIrUm_=selected; physicalOriginUm_=selected;
    inferredAnchor_=inferred; mm_=uint8_t(target_.sequence); positionReliable_=true;
    reverseTarget_=false; ++confirmedCount_;
    targetOriginValid_=relationshipReliable_=measured;
    expectedCumulativeUm_=uint64_t(navi_one::spanMm(mm_,direction_))*1000;
    if(physicalOriginUm_!=r.anchorUm || openingIrUm_!=r.anchorUm) {
      ++anchorMismatches_; r.reason=DartReason::AnchorMismatch;
    }
    enqueueDiagnostic(r); push(EwoEventKind::TargetConfirmed);
    priorSelection_=chosen; priorSelectionValid_=!inferred;
    intervalContextChanged_=false;
    chooseTarget(); ++generation_; bestValid_=tieValid_=false;
    spatialOriginUm_=selected;
    spatialPhase_=measured?SpatialPhase::Clearance:SpatialPhase::None;
    for(auto& occupied:spatialOccupied_) occupied=false;
    spatialCount_=0;
    if(measured) push(EwoEventKind::SpatialClearance);
    setDistanceHolding(!measured || !irApplicable(decisionUs_));
    size_t kept=0;
    for(size_t i=0;i<pendingCount_;++i)
      if(pending_[i].timeUs>consumedThrough) pending_[kept++]=pending_[i];
    pendingCount_=kept;
    // Bounded iterative replay: a backlog must not grow the loop task stack
    // by recursively selecting one target per retained opening.
    if(replayingPending_) return;
    replayingPending_=true;
    size_t i=0;
    while(i<pendingCount_) {
      const uint32_t generation=generation_;
      const DartOpening later=pending_[i]; evaluateOpening(later);
      if(generation_!=generation) i=0; else ++i;
    }
    replayingPending_=false;
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
  bool physicalPositionAnchored_ = false;
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
