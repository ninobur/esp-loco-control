#pragma once

#include <algorithm>
#include <stdint.h>
#include <vector>

#include "NaviEvidence.h"
#include "NaviMapAdapter.h"
#include "NaviDartEvidence.h"
#include "NaviStartupLap.h"
#include "NaviRunClock.h"
#include "IrInstrument.h"

#include "RvmRecognizer.h"
#include "RvmReference.h"

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

// RVM adapter to the preserved Otto controller/recorder interfaces. Legacy
// DART field names below are telemetry compatibility only; no event detector,
// polarity admission, nearest-opening search, or IR discrepancy veto runs.
class NaviIntegratedCore {
 public:
  static constexpr uint64_t kIrFreshUs=1000000;
  NaviIntegratedCore():recognizer_(rvm_reference::cw) {}
  void setBootId(uint64_t id){recognizer_.setBoot(id);}
  bool loadStartupTimingLap(uint32_t loco){return startup_lap::compatible(loco);}
  void declare(uint8_t mm,int8_t direction,uint64_t us){
    if(mm>=171 || (direction!=1 && direction!=-1))return;
    declared_=true;mm_=mm;direction_=direction;stop_=DartStop::None;stopText_="NONE";
    anchored_=fineAnchor_=candidateAnchorValid_=false;lastSerial_=0;selectedId_=0;selectedUs_=selectedClock_=0;
    contextUs_=us;blockCount_=0;block_={};zeroMovement_=false;
    intervalIntegrity_=true;eventHead_=eventTail_=0;tokenHead_=tokenTail_=0;
    recognizer_.useModel(direction>0?rvm_reference::cw:rvm_reference::ccw);
    recognizer_.declare(mm,direction,us);chooseTarget();push(EwoEventKind::Declared,us);
  }
  void reverse(int8_t,uint64_t us){requestStop(DartStop::TimingContextChanged);stopText_="DIRECTION_CHANGE_REDECLARE";push(EwoEventKind::Reversed,us);}
  void observeIr(const ir_movement::WireSnapshot& w,uint64_t received,uint8_t pwm,
                 const uint8_t* mac=nullptr,uint64_t judgment=0){
    ++irCount_;++consumption_;(void)judgment;
    bool same=haveIr_ && w.bootId==ir_.bootId;
    if(mac){for(int i=0;i<6;++i){same=same&&mac[i]==mac_[i];mac_[i]=mac[i];}}
    const bool ordered=!same || (w.sequence>ir_.sequence && w.capturedUs>ir_.capturedUs && w.completedPulses>=ir_.completedPulses);
    const bool shape=w.bootId && ir_movement::validConfiguredDistance(w);
    health_=ngr_nav::classifyIrInstrument(w);
    const bool increments=!same || (w.inferredAdded==ir_.inferredAdded && w.inferredRemoved==ir_.inferredRemoved &&
       w.unreliableSamples==ir_.unreliableSamples && w.saturatedSamples==ir_.saturatedSamples && w.sampleGaps==ir_.sampleGaps && w.openAborts==ir_.openAborts);
    const bool valid=ordered && shape && health_.measurementReady() && increments;
    if(!valid)intervalIntegrity_=false;
    if(same && ordered && pwm==0 && w.completedPulses>ir_.completedPulses && declared_){
      zeroMovement_=true;++zeroMovements_;push(EwoEventKind::PwmZeroDisplacement,received);
    }
    if(!same || !ordered){++epoch_;speedCount_=0;fineAnchor_=false;}
    if(ordered && shape){
      if(speedCount_==16){for(int i=1;i<16;++i)speedPoints_[i-1]=speedPoints_[i];--speedCount_;}
      speedPoints_[speedCount_++]={w.capturedUs,w.completedPulses};
    }
    speedReady_=false;
    if(valid)for(int i=int(speedCount_)-2;i>=0;--i){
      const uint64_t dt=w.capturedUs-speedPoints_[i].us;
      if(dt>1500000)break;
      if(dt>=900000){speed_=float(w.completedPulses-speedPoints_[i].pulses)*w.pitchUm*1000.f/dt;speedReady_=true;break;}
    }
    ir_=w;haveIr_=true;irValid_=valid;receivedUs_=received;irPwm_=pwm;
    history_[historyHead_]={received,w.nominalUm,epoch_,speed_,valid,speedReady_,pwm!=0};
    historyHead_=(historyHead_+1)%32;if(historyCount_<32)++historyCount_;
  }
  void observeHall(const HallSample& sample){observeHall(sample,sample.timestampUs);}
  void observeHall(const HallSample& sample,uint64_t judgment){
    ++hallCount_;++consumption_;lastHall_=sample;processedUs_=sample.timestampUs;
    hallMedian_=sample.raw;
    if(!declared_ || sample.timestampUs<=contextUs_ || stopRequested())return;
    const bool boundary=blockCount_ && (sample.timestampUs/20000!=block_.firstUs/20000 || sample.pwm!=block_.pwm || sample.direction!=(block_.direction>0?1:2));
    if(boundary)flushBlock(judgment);
    if(!blockCount_){block_={};block_.firstUs=sample.timestampUs;block_.firstSerial=sample.sampleSerial;block_.pwm=sample.pwm;block_.direction=sample.direction==1?1:-1;sum_=0;}
    if(lastSerial_ && sample.sampleSerial!=lastSerial_+1)block_.gap=true;
    lastSerial_=sample.sampleSerial;block_.lastSerial=sample.sampleSerial;block_.lastUs=sample.timestampUs;
    sum_+=sample.raw;++blockCount_;
  }
  void completeComparison(uint64_t,uint64_t){} // No legacy DART decision path.
  bool takeToken(rvm::Token& t){if(tokenTail_==tokenHead_)return false;t=tokens_[tokenTail_];tokenTail_=(tokenTail_+1)%8;return true;}
  const rvm::Recognizer& recognizer()const{return recognizer_;}
  bool progressionStopped()const{return recognizer_.stalled();}
  bool stopRequested()const{return stop_!=DartStop::None;}
  DartStop stopReason()const{return stop_;}
  const char* stopReasonText()const{return stopText_;}
  void requestStop(DartStop why){if(stopRequested())return;stop_=why;stopText_=dartStopName(why);}
  void setManualStopped(bool yes,uint64_t us){runClock_.stopped(yes,us);}
  bool manualTimingPaused()const{return runClock_.paused();}
  uint64_t runningClockUs(uint64_t us)const{return runClock_.now(us);}
  void noteOperatingContext(uint8_t,uint64_t=0){}
  bool takeEvent(EwoEvent& e){if(eventHead_==eventTail_)return false;e=events_[eventTail_];eventTail_=(eventTail_+1)%16;return true;}
  void noteObservationLoss(uint32_t hall,uint32_t ir,uint64_t=0){if(hall!=hallDrops_)block_.gap=true;if(ir!=irDrops_)intervalIntegrity_=false;hallDrops_=hall;irDrops_=ir;}
  bool irApplicable(uint64_t us)const{return haveIr_&&irValid_&&us>=receivedUs_&&us-receivedUs_<=kIrFreshUs;}
  bool irMeasurementEpochActive()const{return haveIr_;} uint64_t irMeasurementEpochId()const{return epoch_;}
  uint8_t irHealthFault()const{return uint8_t(health_.fault);} uint8_t irReadiness()const{return uint8_t(health_.readiness);}
  bool declared()const{return declared_;} bool positionReliable()const{return declared_&&!stopRequested();}
  uint8_t mm()const{return mm_;} int8_t direction()const{return direction_;}
  TargetSpec target()const{return target_;}
  bool physicalPositionAnchored()const{return anchored_;}
  bool physicalOffsetUm(uint64_t us,int64_t& out)const{
    if(!fineAnchor_ || zeroMovement_ || stopRequested() || !irApplicable(us) || !irPwm_ || epoch_!=anchorEpoch_ || ir_.nominalUm<anchorIr_)return false;
    out=int64_t(ir_.nominalUm-anchorIr_);return true;
  }
  bool fineIrAvailable(uint64_t us)const{int64_t x;return physicalOffsetUm(us,x);}
  bool selectedAnchorAvailable()const{return fineAnchor_;}
  bool relationshipReliable()const{return fineAnchor_&&irValid_;}
  bool pwmZeroMovementRequiresDeclaration()const{return zeroMovement_;}
  const char* irDistanceState(uint64_t us)const{return irApplicable(us)?"RVM_IR_SUPPORT":"RVM_IR_UNAVAILABLE";}
  bool irSpeedAvailable(uint64_t us)const{return speedReady_&&irApplicable(us);}
  double irSpeedMmS()const{return speed_;}
  bool initialReferenceReady()const{return declared_;}
  int16_t activeReference()const{return int16_t((direction_>0?rvm_reference::cw:rvm_reference::ccw).baselines4[target_.sequence]*0.25f+recognizer_.offset());}
  int16_t hallMedian()const{return hallMedian_;} bool hallSupport()const{return recognizer_.evidence()>0;}
  HallSample lastHall()const{return lastHall_;} uint32_t lastHallSerial()const{return lastHall_.sampleSerial;}
  ir_movement::WireSnapshot latestIr()const{return ir_;} const uint8_t* latestIrMac()const{return mac_;}
  uint64_t latestIrReceivedUs()const{return receivedUs_;} uint64_t processedHallUs()const{return processedUs_;}
  uint64_t hallObservationCount()const{return hallCount_;} uint64_t irObservationCount()const{return irCount_;}
  uint32_t confirmedCount()const{return selectedId_;} uint32_t missedCount()const{return 0;}
  uint32_t pwmZeroDisplacements()const{return zeroMovements_;}
  uint32_t hallLoss()const{return hallDrops_;} uint32_t irLoss()const{return irDrops_;} uint32_t eventLoss()const{return eventDrops_;}
  uint64_t consumptionId()const{return consumption_;}
  uint32_t selectedOpeningId()const{return selectedId_;} uint64_t selectedDecisionUs()const{return selectedUs_;}
  uint64_t selectedDecisionClockUs()const{return selectedClock_;} uint64_t selectedTimeUs()const{return selectedUs_;}
  uint32_t openingSerial()const{return selectedSerial_;} uint64_t openingIrUm()const{return anchorIr_;}
  // Preserved prescribed throttle timing is a separate immutable recording.
  const DartTravel& travelHistory(uint8_t mm,int8_t dir)const{return dir==1&&mm<171?startup_lap::kIntervals[mm]:unavailableTravel_;}
  bool startupTimingReference()const{return true;} bool timingReferenceReady()const{return true;}
  int8_t timingReferenceDirection()const{return 1;} uint32_t timingReferenceLap()const{return startup_lap::kRecordedLap;}
  uint32_t completedTimingLaps()const{return 0;} uint64_t timingExpectedUs()const{return travelHistory(mm_,direction_).intervalUs;}
  IrIntervalClassification irCredibility()const{return intervalIntegrity_&&irValid_?IrIntervalClassification::Consistent:IrIntervalClassification::Unavailable;}
  IrGeographicAuthority irAuthority()const{return irValid_?IrGeographicAuthority::Retained:IrGeographicAuthority::Withdrawn;}
  bool distanceHolding()const{return !fineAnchor_||!irValid_;}
  // Retired DART diagnostics keep the old dashboard payload schema readable.
  DartMetric comparisonMetric()const{return DartMetric::None;}
  bool reassessmentArmed()const{return false;} bool provisionalIr()const{return false;}
  bool takeDartRecord(DartRecord&){return false;} const DartTiming& timing(size_t)const{return emptyTiming_;}
  size_t timingCount()const{return 0;} uint32_t diagnosticDrops()const{return tokenDrops_;}
  uint32_t openingCount()const{return selectedId_;} uint32_t anchorMismatchCount()const{return recognizer_.mismatches();}
  uint32_t incompleteBatches()const{return hallDrops_;} uint32_t pendingOverflows()const{return 0;}
  bool comparisonIncomplete()const{return false;} bool candidateAvailable()const{return false;}
  uint64_t candidateBoundaryUm()const{return 0;} uint64_t candidateCoordinateUm()const{return 0;}
  bool inferredAnchor()const{return false;} int64_t cumulativeResidualUm()const{return 0;}
  size_t pendingHighWater()const{return 0;} uint64_t spatialOriginUm()const{return 0;} uint8_t spatialPhase()const{return 0;}
 private:
  struct IrPoint{uint64_t us=0,um=0,epoch=0;float speed=0;bool valid=false,speedValid=false,powered=false;};
  IrPoint irAt(uint64_t us)const{for(size_t i=0;i<historyCount_;++i){auto p=history_[(historyHead_+31-i)%32];if(p.us<=us){p.valid=p.valid&&us-p.us<=kIrFreshUs;return p;}}return {};}
  void chooseTarget(){target_=mappedTargetAfter(mm_,direction_);}
  void push(EwoEventKind kind,uint64_t us){
    const auto next=(eventHead_+1)%16;if(next==eventTail_){++eventDrops_;return;}
    EwoEvent e{};e.kind=kind;e.timestampUs=us;e.hallSerial=lastHall_.sampleSerial;e.mm=mm_;e.target=target_.sequence;e.direction=direction_;e.irUm=ir_.nominalUm;e.median=hallMedian_;e.reference=activeReference();e.positionReliable=positionReliable();e.openingSerial=selectedSerial_;e.consumptionId=consumption_;
    events_[eventHead_]=e;eventHead_=next;
  }
  void flushBlock(uint64_t judgment){
    block_.hall=float(sum_)/blockCount_;const auto p=irAt(block_.lastUs);
    block_.irValid=p.valid&&p.speedValid&&p.powered;block_.irSpeed=p.speed;
    rvm::Token t;
    if(!zeroMovement_ && recognizer_.observe(block_,t,judgment)){
      t.irValid=t.irValid&&intervalIntegrity_;
      if(!t.finalQuality && !t.stopDecision)intervalIntegrity_=true; // Only a new tile starts a new IR interval.
      if(!t.finalQuality && !t.stopDecision){
        mm_=t.committed;chooseTarget();++selectedId_;selectedUs_=t.commitUs;selectedClock_=runClock_.now(selectedUs_);selectedSerial_=t.lastSerial;anchored_=true;
        candidateAnchorIr_=p.um;candidateAnchorEpoch_=p.epoch;candidateAnchorValid_=p.valid;
        fineAnchor_=false;
        push(EwoEventKind::TargetConfirmed,judgment);
      }
      // Fine controller offset is available only from coherent multi-piece
      // context. Atypical pieces never install a fresh measurement anchor.
      if(t.finalQuality){
        fineAnchor_=!t.atypical&&t.trusted==t.committed&&candidateAnchorValid_;
        if(fineAnchor_){anchorIr_=candidateAnchorIr_;anchorEpoch_=candidateAnchorEpoch_;}
      }
      const auto next=(tokenHead_+1)%8;if(next==tokenTail_)++tokenDrops_;else{tokens_[tokenHead_]=t;tokenHead_=next;}
    }
    if(recognizer_.lossOfPattern()){requestStop(DartStop::NoApplicableReference);stopText_="LOSS_OF_PATTERN_FIVE_NEW_PIECES";}
    if(recognizer_.stalled()){requestStop(DartStop::NoApplicableReference);stopText_="LOSS_OF_PROGRESSION_ESTOP";}
    blockCount_=0;
  }
  rvm::Recognizer recognizer_;NaviRunClock runClock_{};
  bool declared_=false,anchored_=false,fineAnchor_=false,haveIr_=false,irValid_=false,zeroMovement_=false,intervalIntegrity_=true,speedReady_=false;
  uint8_t mm_=0,irPwm_=0;int8_t direction_=1;TargetSpec target_{};
  DartStop stop_=DartStop::None;const char* stopText_="NONE";
  uint64_t contextUs_=0,processedUs_=0,selectedUs_=0,selectedClock_=0,anchorIr_=0,anchorEpoch_=0,receivedUs_=0,epoch_=1,consumption_=0,hallCount_=0,irCount_=0;
  uint64_t candidateAnchorIr_=0,candidateAnchorEpoch_=0;bool candidateAnchorValid_=false;
  uint32_t selectedId_=0,selectedSerial_=0,lastSerial_=0,hallDrops_=0,irDrops_=0,eventDrops_=0,tokenDrops_=0,zeroMovements_=0;
  int16_t hallMedian_=0;HallSample lastHall_{};rvm::Block block_{};uint32_t blockCount_=0;int64_t sum_=0;
  ir_movement::WireSnapshot ir_{};uint8_t mac_[6]{};ngr_nav::IrInstrumentState health_{};
  struct SpeedPoint{uint64_t us,pulses;};SpeedPoint speedPoints_[16]{};size_t speedCount_=0;float speed_=0;
  IrPoint history_[32]{};size_t historyHead_=0,historyCount_=0;
  EwoEvent events_[16]{};size_t eventHead_=0,eventTail_=0;
  rvm::Token tokens_[8]{};size_t tokenHead_=0,tokenTail_=0;
  DartTravel unavailableTravel_{};DartTiming emptyTiming_{};
};
} // namespace navi_eyes
