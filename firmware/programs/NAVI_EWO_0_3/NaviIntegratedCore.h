#pragma once

#include <algorithm>
#include <stdint.h>
#include <vector>

#include "../NAVI_EYES_WIDE_OPEN/NaviEvidence.h"
#include "NaviMapAdapter.h"
#include "NaviIrEvidence.h"
#include "NaviPulseObservation.h"
#include "NaviEvidenceBridge.h"
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
  SpatialInvalidated, EncounterObserved, GeographicContradiction,
  DnaResolved, IrTimingQuestionable, IrObservabilityGap, IrCredibilityRestored
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
  uint64_t hallOpenedUs=0,hallDetectedUs=0;
  int8_t openingPolarity=0;
  bool openingIrValid=false;
};

// NAVI's sequential journey authority. Acquisition delivers each ADC result
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
    evidenceBridge_.resetForDeclarationOrReversal();
    pendingCount_=0; dnaChecks_=0;
    irWindowQuestionable_=false; reanchorNeeded_=false;
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
    const bool seekCurrentMm = newPosition < 0 && !evidenceBridge_.inField();
    const uint64_t distanceToTarget = reversible ? uint64_t(seekCurrentMm ?
        -newPosition : newInterval - newPosition) : 0;
    contextSinceUs_ = nowUs;
    firstTargetAfterDeclare_ = !physicalPositionAnchored_;
    firstTargetSupportAbsent_ = false;
    direction_ = direction;
    reverseTarget_ = !physicalPositionAnchored_ ? !reverseTarget_ :
        coherent ? seekCurrentMm : !evidenceBridge_.inField();
    chooseTarget();
    if (reversible || !physicalPositionAnchored_) {
      physicalOriginUm_ = latestIr_.nominalUm;
      expectedCumulativeUm_ = distanceToTarget;
    } else {
      // Stale/questionable distance is not a loss of established journey.
      // Retain the last verified MM and direction; the next qualified field
      // can identify its current-MM return or adjacent marker. True frame loss
      // retains its existing operator-declaration latch.
      targetOriginValid_ = false;
      relationshipReliable_ = false;
    }
    resetTargetEvidence();
    evidenceBridge_.reverse();
    pendingCount_=0; pulseEvidence_.reset(); irPulseObservable_=false;
    spatialPhase_ = SpatialPhase::None;
    setDistanceHolding(!reversible && physicalPositionAnchored_);
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
      irBoundaryCount_=0; pulseEvidence_.reset(); irPulseObservable_=false;
      irWindowQuestionable_=true; reanchorNeeded_=true;
      resetTargetEvidence();
      invalidateRelationship();
      clearSpeedHistory();
    }
    if (!epochActive_ || discontinuity) { ++epochId_; epochActive_ = true; }
    irContinuity_ = true;
    addIrBoundary(w,receivedUs);
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
    // IR packets alone never assign a marker. Missed Hall identities require
    // retained magnetic sequence evidence, not distance-based progression.
  }

  void observeHall(const HallSample& sample) {
    observeHall(sample, sample.timestampUs); // synchronous host callers
  }
  void observeHall(const HallSample& sample, uint64_t judgmentUs) {
    beginDecision(judgmentUs);
    ++hallObservationCount_;
    lastHall_=sample; haveHall_=true;
    if (!sample.pwm || pwmZeroMovementRequiresDeclaration_) {
      evidenceBridge_.breakSamples(); return;
    }
    if (!initialReferenceReady_) {
      activeReference_=sample.raw; initialReferenceReady_=true;
      push(EwoEventKind::ReferenceReady); return;
    }
    if (!declared_ || sample.timestampUs<=contextSinceUs_) return;
    const int delta=int(sample.raw)-activeReference_;
    addHallDiagnostic(sample.raw); // median-of-five remains diagnostic only
    hallSupport_=target_.polarity==HallOpeningPolarity::AboveReference ?
                  delta>=70 : delta<=-70;
    if (firstTargetAfterDeclare_ && delta<70 && delta>-70)
      firstTargetSupportAbsent_=true;
    // Do not feed a magnetic excursion into reference collection. Observation
    // still runs throughout clearance/collection, including stalled IR.
    if (spatialPhase_!=SpatialPhase::None && delta<70 && delta>-70 &&
        irApplicable(decisionUs_) && relationshipReliable_)
      collectSpatial(sample.raw);

    const IrBoundary boundary=irBoundaryAt(sample.timestampUs);
    ngr_evidence::IdentityEvidence identity{};
    identity.expectedMm=static_cast<uint8_t>(target_.sequence);
    identity.expectedPolarity=target_.polarity==HallOpeningPolarity::AboveReference ?
        ngr_evidence::Polarity::North : ngr_evidence::Polarity::South;
    identity.expectedDirection=direction_;
    identity.irCredible=boundary.valid && relationshipReliable_ &&
                        targetOriginValid_ && irMagnetVetoAvailable(sample.timestampUs);
    if(identity.irCredible && boundary.um>=physicalOriginUm_) {
      const uint64_t traveled=boundary.um-physicalOriginUm_;
      const uint64_t tolerance=uint64_t(target_.distanceMm)*150;
      identity.irInsideWindow=traveled+tolerance>=expectedCumulativeUm_ &&
                             traveled<=expectedCumulativeUm_+tolerance;
    }
    // A between-marker declaration has no measured marker origin. Retain its
    // established first-target behavior; do not invent a mapped distance.
    if(firstTargetAfterDeclare_)identity.irInsideWindow=true;
    const int8_t sensedDir=sample.direction==1?1:sample.direction==2?-1:0;
    const uint64_t separation=reverseTarget_ ? expectedCumulativeUm_*85/100 : uint64_t(target_.distanceMm)*850;
    const auto ruling=evidenceBridge_.observe(sample.raw,activeReference_,
        sample.timestampUs,sample.sampleSerial,boundary.um,sensedDir,identity,
        identity.irCredible,separation,boundary.valid);
    if(!ruling.newEncounter)return;
    auto encounter=ruling.encounter;
    lastEncounter_=encounter;
    openingSerial_=encounter.serial; openingIrUm_=encounter.irUm;
    push(EwoEventKind::EncounterObserved);
    ++dnaChecks_; // every encounter, including ordinary IR agreement
    checkDnaJourney();
    // Preserve observation and do not allow the wrong journey to move MM.
    if(ruling.directionConflict)return;
    if(firstTargetAfterDeclare_ && (!firstTargetSupportAbsent_ ||
       (boundary.valid && boundary.um<=physicalOriginUm_)))return;
    if(frameLost_)return; // genuine cumulative-stream loss still needs declaration
    if(reverseTarget_ && boundary.valid && boundary.um<=physicalOriginUm_)return;
    if(pendingCount_ || !positionReliable_ || !ruling.expectedMagnet) {
      evidenceBridge_.markContradiction();
      if(!pendingCount_) { pendingTarget_=target_; pendingCount_=1; }
      else ++pendingCount_;
      positionReliable_=false;
      push(EwoEventKind::GeographicContradiction);
      resolvePending(); return;
    }
    HallPoint landmark{encounter.serial,encounter.openedUs,sample.raw,
                       encounter.irUm,encounter.irValid};
    confirm(landmark);
    const size_t size=evidenceBridge_.history().size();
    evidenceBridge_.recordConfirmed(size-1,mm_);
    checkDnaJourney();
  }

  // Called only after the unchanged Type-6 parser validates wire/provenance.
  void observePulse(const navi_pulse::State& state,uint8_t pwm,int8_t direction,
                    uint64_t judgmentUs=0) {
    beginDecision(judgmentUs?judgmentUs:state.receivedUs);
    const bool same=haveIr_ && state.event.bootId==latestIr_.bootId &&
        (!haveSource_ || !std::memcmp(state.mac,latestIrMac_,6));
    if(state.receivedUs<contextSinceUs_ ||
       (physicalPositionAnchored_ && state.receivedUs<anchorOpenedUs_))return;
    lastPulseReceivedUs_=state.receivedUs;
    if(!same || direction!=direction_) {pulseEvidence_.reset();irPulseObservable_=false;return;}
    if(!state.eventValid) {
      pulseEvidence_.reset(); irPulseObservable_=false;
      irWindowQuestionable_=true;
      push(EwoEventKind::IrObservabilityGap); return;
    }
    const auto assessment=pulseEvidence_.observe(state.event.completedPulses,
                                                state.event.completedUs,pwm);
    lastPulseAssessment_=assessment;
    // A new PWM population, a gap, or a clock fault cannot inherit the previous
    // population's credibility. These are observability losses, not pulse losses.
    if(assessment.finding==ngr_evidence::PulseFinding::Baseline ||
       assessment.finding==ngr_evidence::PulseFinding::ObservationGap ||
       assessment.finding==ngr_evidence::PulseFinding::ClockError)
      irPulseObservable_=false;
    if(assessment.finding==ngr_evidence::PulseFinding::TimingAnomaly ||
       assessment.questionable) {
      // Timing alone cannot distinguish wheel-speed variation from an optical
      // cycle that was never counted. Record the question without granting it
      // missed-pulse authority. Raw timestamps/counts remain unchanged.
      ++irTimingQuestions_; push(EwoEventKind::IrTimingQuestionable);
    }
    const bool continuous=assessment.finding==ngr_evidence::PulseFinding::Normal ||
                          assessment.finding==ngr_evidence::PulseFinding::TimingAnomaly ||
                          assessment.finding==ngr_evidence::PulseFinding::BracketedMissingCompatible;
    if(!reanchorNeeded_ && continuous &&
              irWindowQuestionable_) {
      irWindowQuestionable_=false;
      push(EwoEventKind::IrCredibilityRestored);
    }
    if(continuous && !reanchorNeeded_)
      irPulseObservable_=true;
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
      if(newHallLoss)evidenceBridge_.breakSamples();
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
  bool geographicJudgmentPending()const{return pendingCount_!=0;}
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
    if (irWindowQuestionable_) return "IR_TIMING_QUESTIONABLE";
    if (!irMagnetVetoAvailable(nowUs)) return "IR_TIMING_UNOBSERVED";
    return "HALL_IR_READY";
  }
  const ngr_evidence::DnaHistory& dnaHistory()const{return evidenceBridge_.history();}
  ngr_evidence::DnaCheck dnaCheck()const{return dnaCheck_;}
  uint32_t geographicRevision()const{return geographicRevision_;}
  uint32_t dnaChecks()const{return dnaChecks_;}
  uint32_t irTimingQuestions()const{return irTimingQuestions_;}
  // The caller must supply the observation/decision time being evaluated.
  // decisionUs_ is the last consumed input, not a clock: using it by default
  // would let a read-only query retain authority indefinitely during silence.
  // Continuous detected pulses are not proof that no optical cycle was missed;
  // see IR_VALIDITY_CORRECTION.md for the unresolved physical limitation.
  bool irMagnetVetoAvailable(uint64_t nowUs)const{
    return irPulseObservable_ && !irWindowQuestionable_ && irApplicable(nowUs) &&
           nowUs>=lastPulseReceivedUs_ && nowUs-lastPulseReceivedUs_<=kIrFreshUs;
  }
  ngr_evidence::PulseAssessment pulseAssessment()const{return lastPulseAssessment_;}
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
    events_[eventHead_].hallOpenedUs=lastEncounter_.openedUs;
    events_[eventHead_].hallDetectedUs=lastEncounter_.detectedUs;
    events_[eventHead_].openingPolarity=static_cast<int8_t>(lastEncounter_.polarity);
    events_[eventHead_].openingIrValid=lastEncounter_.irValid;
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
    diagnosticCount_=0;
    hallSupport_ = false;
    hallMedian_ = 0;
  }
  void addHallDiagnostic(int16_t raw) {
    if(diagnosticCount_==5){for(unsigned i=1;i<5;++i)diagnosticHall_[i-1]=diagnosticHall_[i];--diagnosticCount_;}
    diagnosticHall_[diagnosticCount_++]=raw;
    if(diagnosticCount_==5){
      int16_t sorted[5];for(unsigned i=0;i<5;++i)sorted[i]=diagnosticHall_[i];
      std::sort(sorted,sorted+5);hallMedian_=sorted[2];
    }
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
    physicalPositionAnchored_ = true;
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
    anchorOpenedUs_=landmark.timestampUs;
    expectedCumulativeUm_ = uint64_t(navi_one::spanMm(mm_, direction_)) * 1000;
    targetOriginValid_=landmark.irApplicable;
    relationshipReliable_=landmark.irApplicable;
    reanchorNeeded_=false; // a fresh verified geographic boundary ends old span
    if(!landmark.irApplicable)irWindowQuestionable_=true;
    push(EwoEventKind::TargetConfirmed);
    chooseTarget();
    resetTargetEvidence();
    spatialOriginUm_ = landmark.irUm;
    spatialPhase_=landmark.irApplicable ? SpatialPhase::Clearance : SpatialPhase::None;
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
  void checkDnaJourney() {
    dnaCheck_=evidenceBridge_.verify([](uint8_t mm){return mapPole(mm);});
    const auto& h=evidenceBridge_.history();
    for(size_t i=1;i<h.size();++i) {
      const auto& prior=h.at(i-1);const auto& current=h.at(i);
      if(!prior.identified || !current.identified)continue;
      const uint8_t next=navi_one::nextMarker(prior.mm,current.direction);
      const bool sequential=current.direction==prior.direction ? current.mm==next :
                            current.mm==prior.mm || current.mm==next;
      if(!sequential)++dnaCheck_.contradictions;
    }
    if(dnaCheck_.contradictions)positionReliable_=false;
  }
  static ngr_evidence::Polarity mapPole(uint8_t mm) {
    return navi_one::polarityAt(mm)?ngr_evidence::Polarity::North:ngr_evidence::Polarity::South;
  }
  // Two evidence interpretations of the single leading contradiction: a real
  // sequential encounter with bad polarity, or an extra magnetic observation.
  // Both walk only the established journey; neither searches the map or skips
  // unobserved geographic markers. Ten following poles use the map's existing
  // unique-sequence property. All observations remain in the bounded ledger.
  void resolvePending() {
    const auto& history=evidenceBridge_.history();
    if(pendingCount_>history.size() || pendingCount_>ngr_evidence::DnaHistory::kCapacity ||
       pendingCount_<11)return;
    const size_t first=history.size()-pendingCount_;
    unsigned matches=0; uint8_t resolved=mm_; bool counted=false;
    for(unsigned countLeading=0;countLeading<2;++countLeading) {
      uint8_t mm=static_cast<uint8_t>(pendingTarget_.sequence);
      if(countLeading)mm=navi_one::nextMarker(mm,direction_);
      bool agrees=true;
      for(size_t i=first+1;i<history.size();++i) {
        const auto& e=history.at(i);
        if(e.direction!=direction_ || e.encounter.polarity!=mapPole(mm)){agrees=false;break;}
        resolved=mm; mm=navi_one::nextMarker(mm,direction_);
      }
      if(agrees){++matches;counted=countLeading;}
    }
    if(matches!=1)return;
    uint8_t assign=static_cast<uint8_t>(pendingTarget_.sequence);
    if(counted)assign=navi_one::nextMarker(assign,direction_);
    for(size_t i=first+1;i<history.size();++i) {
      evidenceBridge_.recordConfirmed(i,assign);
      resolved=assign;assign=navi_one::nextMarker(assign,direction_);
    }
    const auto& e=history.at(history.size()-1).encounter;
    target_=mappedTargetAfter(navi_one::nextMarker(resolved,-direction_),direction_);
    pendingCount_=0;
    confirm({e.serial,e.openedUs,lastHall_.raw,e.irUm,e.irValid});
    checkDnaJourney();
    ++geographicRevision_;
    push(EwoEventKind::DnaResolved);
  }

  struct IrBoundary {uint64_t receivedUs=0,um=0,boot=0;bool valid=false;};
  void addIrBoundary(const ir_movement::WireSnapshot& w,uint64_t receivedUs) {
    if(irBoundaryCount_==kBoundaryCapacity) {
      for(size_t i=1;i<irBoundaryCount_;++i)irBoundaries_[i-1]=irBoundaries_[i];
      --irBoundaryCount_;
    }
    irBoundaries_[irBoundaryCount_++]={receivedUs,w.nominalUm,w.bootId,true};
  }
  IrBoundary irBoundaryAt(uint64_t hallUs)const {
    for(size_t i=irBoundaryCount_;i>0;--i) {
      const auto& b=irBoundaries_[i-1];
      if(b.receivedUs<=hallUs && hallUs-b.receivedUs<=kIrFreshUs &&
         b.boot==latestIr_.bootId)return b;
    }
    return {};
  }

  void beginDecision(uint64_t nowUs) {
    decisionUs_ = nowUs; ++consumptionId_;
    // Use the existing observation freshness bound. Continuing Type-5 reports
    // cannot keep an absent Type-6 stream authoritative. After a pause, collect
    // a fresh population rather than inheriting one from before the pause.
    if(lastPulseReceivedUs_ && nowUs>=lastPulseReceivedUs_ &&
       nowUs-lastPulseReceivedUs_>kIrFreshUs) {
      pulseEvidence_.reset(); irPulseObservable_=false;
    }
  }
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
  ngr_evidence::NaviEvidenceBridge evidenceBridge_{};
  bool irWindowQuestionable_=false,reanchorNeeded_=false,irPulseObservable_=false;
  ngr_evidence::IrPulseEvidence pulseEvidence_{};
  ngr_evidence::PulseAssessment lastPulseAssessment_{};
  ngr_evidence::DnaCheck dnaCheck_{};
  ngr_evidence::Encounter lastEncounter_{};
  int16_t diagnosticHall_[5]{}; unsigned diagnosticCount_=0;
  uint32_t dnaChecks_=0,irTimingQuestions_=0,pendingCount_=0;
  uint64_t lastPulseReceivedUs_=0,anchorOpenedUs_=0;
  uint32_t geographicRevision_=0;
  TargetSpec pendingTarget_{};
  static constexpr size_t kBoundaryCapacity=64;
  IrBoundary irBoundaries_[kBoundaryCapacity]{};
  size_t irBoundaryCount_=0;
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
