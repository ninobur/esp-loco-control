#pragma once
#include <stdint.h>
#include "RouteMap.h"
#include "ProximalRecovery.h"

namespace navi_one {
// 20Q3 (decision 0093): Hall-only fallback, open-to-open from the previous
// accepted detection. Consulted ONLY when no valid MM-referenced IR distance
// exists; valid distance is never vetoed by time. Supersedes both the inherited
// 500 ms guard and X22's ~645 ms detector refractory (removed in X22R), so this
// is the one timing rule.
static constexpr uint32_t HALL_ONLY_GUARD_MS=650;
// +/-15% of cumulative mapped distance (operator ruling 2026-09-22: widened from
// 0.10 after 4/78 real magnets were refused). Compared in integer micrometres so
// the inclusive 0.85D and 1.15D boundaries are exact.
static constexpr uint32_t IR_WINDOW_LOW_PERMILLE=850,IR_WINDOW_HIGH_PERMILLE=1150;
// Provisional AUTO landmark-authority limit (operator ruling 2026-09-25):
// this many consecutive expected MMs traversed without an accepted Hall
// landmark ends AUTO. It is not LOST and erases no position or history.
static constexpr uint8_t SANS_MM_AUTO_LIMIT=10;
// PROXIMAL_R1 supersedes the 0.5 whole-route 10/10 sequence authority.
// Unknown passages occupy physical history but never count as observed poles.

enum class EvidenceClass:uint8_t{None,Confirmed,NonLandmarkHall,PolarityDiscrepancy,MissedObservation,MissedWithPolarityDiscrepancy,IrUnavailable,LocationUnresolved};
inline const char* evidenceClassName(EvidenceClass e){switch(e){case EvidenceClass::Confirmed:return "CONFIRMED";case EvidenceClass::NonLandmarkHall:return "NON_LANDMARK_HALL";case EvidenceClass::PolarityDiscrepancy:return "POLARITY_DISCREPANCY";case EvidenceClass::MissedObservation:return "MISSED_OBSERVATION";case EvidenceClass::MissedWithPolarityDiscrepancy:return "MISSED_OBSERVATION+POLARITY_DISCREPANCY";case EvidenceClass::IrUnavailable:return "IR_UNAVAILABLE";case EvidenceClass::LocationUnresolved:return "LOCATION_UNRESOLVED";default:return "NONE";}}
// Where NAVI's distance judgment came from. MM_REFERENCED_EPOCH is the only
// basis that permits the +/-15% window; every other basis is Hall-only.
enum class DistanceBasis:uint8_t{NoReference,Epoch,EpochBreak,NoIrPoint};
inline const char* distanceBasisName(DistanceBasis b){switch(b){case DistanceBasis::Epoch:return "MM_REFERENCED_EPOCH";case DistanceBasis::EpochBreak:return "EPOCH_BREAK";case DistanceBasis::NoIrPoint:return "NO_IR_POINT";default:return "NO_MM_REFERENCE";}}
// 20Q3: the opening polarity at detection is the only Hall polarity with NAV
// authority. The later 400 ms window is diagnostic telemetry and is not here.
struct NavObservation{uint32_t openedAtMs=0;uint8_t polarity=0;bool directionConflict=false,restartUncertain=false;ngr_nav::IrOdometryPoint odometry{};};
enum class NavState:uint8_t{Unset,Tracking,Uncertain,Lost};
inline const char* navStateName(NavState s){switch(s){case NavState::Tracking:return "TRACKING";case NavState::Uncertain:return "LOCATION_UNRESOLVED";case NavState::Lost:return "LOST";default:return "UNSET";}}
inline const char* consoleNavName(NavState s){switch(s){case NavState::Tracking:return "NORMAL";case NavState::Uncertain:return "UNRESOLVED";case NavState::Lost:return "LOST";default:return "UNSET";}}
inline bool consoleHasReference(NavState s){return s==NavState::Tracking;}
enum class Ruling:uint8_t{Advanced,NoPosition,NonLandmark,PositionAdvancedSansMm,AdvancedWithDiscrepancy,Uncertain,Reestablished,Retained};
inline const char* rulingName(Ruling r){switch(r){case Ruling::Advanced:return "ADVANCED";case Ruling::NonLandmark:return "NON_LANDMARK_HALL";case Ruling::PositionAdvancedSansMm:return "POSITION_ADVANCED_SANS_MM";case Ruling::AdvancedWithDiscrepancy:return "ADVANCED_WITH_DISCREPANCY";case Ruling::Uncertain:return "LOCATION_UNRESOLVED";case Ruling::Retained:return "RETAINED";default:return "NO_POSITION";}}
enum class Trust:uint8_t{OperatorDeclared,Continuity,SequenceRecovered};
inline const char* trustName(Trust t){return t==Trust::Continuity?"CONTINUITY":t==Trust::SequenceRecovered?"SEQUENCE_RECOVERED":"OPERATOR_DECLARED";}
struct SequenceCorrection{uint8_t fromMm=0,toMm=0;int16_t offset=0;uint8_t matchesBefore=0;};
struct NavStatus{NavState state=NavState::Unset;uint8_t navMm=0,target=0;int8_t navDir=0;Trust trust=Trust::OperatorDeclared;uint32_t corrections=0;EvidenceClass evidence=EvidenceClass::None;uint32_t advances=0,refusals=0,irWaits=0;uint8_t unresolvedCount=0,sequenceLength=0;uint16_t sequenceMatches=0;uint8_t missedSinceLast=0;bool distanceConfirmed=false,distanceAssessable=false;DistanceBasis distance=DistanceBasis::NoReference;uint8_t sansMmConsecutive=0;uint32_t sansMmTotal=0;};
struct PositionEvidence{uint8_t mm=0;uint32_t lastRealMs=0;uint8_t faults=0,causes=0,clean=0;bool distanceAgrees=false;DistanceBasis distance=DistanceBasis::NoReference;};
class PositionView{public:uint8_t count()const{return valid_?1:0;}const PositionEvidence& hypothesis(uint8_t)const{return p_;}int8_t direction()const{return dir_;}void set(bool v,uint8_t mm,int8_t d,uint32_t at,bool ok,DistanceBasis basis,uint8_t clean){valid_=v;dir_=d;p_.mm=mm;p_.lastRealMs=at;p_.distanceAgrees=ok;p_.distance=basis;p_.clean=clean;}private:bool valid_=false;int8_t dir_=0;PositionEvidence p_{};};
// One position advance made because the expected MM's complete +/-15% window
// was physically traversed without an accepted Hall landmark. No Hall or
// polarity evidence exists for it and none is recorded.
struct SansMmAdvance{uint8_t mm=0,referenceMm=0,consecutive=0;int8_t dir=0;double travelMm=0,windowHighMm=0;};

class Navigator{
public:
 const NavStatus& status()const{return s_;} const PositionView& hypotheses()const{return view_;}
 const ProximalRecovery& recovery()const{return recovery_;}
 bool positionKnown()const{return s_.state==NavState::Tracking;} bool unresolved()const{return s_.state==NavState::Uncertain;} bool evaluating()const{return unresolved();}
 bool autoLandmarkLimitReached()const{return s_.sansMmConsecutive>=SANS_MM_AUTO_LIMIT;}
 bool haveReference()const{return haveRef_;} uint8_t referenceMm()const{return refMm_;}
 // Same-epoch measured travel from the last MM/IR synchronization to p.
 // Never bridges an Epoch break: a point from any other epoch is unavailable.
 bool travelUmAt(const ngr_nav::IrOdometryPoint&p,uint64_t&um)const{
  if(!haveRef_||!sameEpoch(refPoint_,p)||p.pulses<refPoint_.pulses||p.capturedUs<refPoint_.capturedUs)return false;
  um=(p.pulses-refPoint_.pulses)*uint64_t(refPoint_.pitchUm);return true;}
 bool distanceAt(const ngr_nav::IrOdometryPoint&p,double&mm)const{uint64_t um=0;if(!travelUmAt(p,um))return false;mm=double(um)/1000.0;return true;}
 bool takeSansAdvance(SansMmAdvance&a){if(sansTail_==sansHead_)return false;a=sansQ_[sansTail_];sansTail_=uint8_t((sansTail_+1)%SANS_Q);return true;}
 uint32_t sansAdvanceDrops()const{return sansDrops_;}
 bool takeResetRequest(){bool r=reset_;reset_=false;return r;}
 bool takeCorrection(SequenceCorrection& c){if(!corrected_)return false;c=correction_;corrected_=false;return true;}
 void haltForLoss(){s_.state=NavState::Lost;refreshView();}
 void declare(uint8_t mm,int8_t dir,uint32_t nowMs=0){s_=NavStatus{};s_.navMm=mm;s_.navDir=dir;s_.target=dir?nextMarker(mm,dir):mm;s_.state=dir?NavState::Tracking:NavState::Unset;s_.trust=Trust::OperatorDeclared;haveRef_=false;lastAcceptedMs_=nowMs;recovery_.reset();corrected_=false;reset_=true;repeatAfterReverse_=false;sansHead_=sansTail_=0;refreshView();}
 // Reversal keeps positional authority/history. The point last passed in the old
 // direction is the first expected point in the new direction. carryResetRequest()
 // starts a fresh unsigned-IR frame. Recovery keeps the physical observations
 // and waits for ten new-direction positions before considering relocation.
 // 0.5: a direction set before any declaration records the direction only. 0.4
 // set TRACKING from UNSET, reporting a known MM0 that nobody declared -- and
 // with AUTO enabled, admitAuto() reads positionKnown().
 // 20Q3: the MM/IR synchronization is unsigned travel and cannot cross a
 // reversal; the consecutive sans-MM count is reset only by an accepted landmark.
 void setDirection(int8_t dir,uint32_t nowMs=0){if(!dir||dir==s_.navDir)return;if(s_.state==NavState::Unset||s_.state==NavState::Lost){s_.navDir=dir;refreshView();return;}s_.navDir=dir;s_.target=s_.navMm;s_.state=NavState::Tracking;recovery_.reverse();repeatAfterReverse_=true;s_.evidence=EvidenceClass::None;s_.unresolvedCount=0;haveRef_=false;lastAcceptedMs_=nowMs;reset_=true;refreshView();}
 // 20Q3 exact window semantics, evaluated continuously against the current
 // same-epoch IR point. The expected MM is missed when its complete window has
 // been traversed; a later Hall event does not create the miss. Returns the
 // number of position advances made sans MM.
 uint8_t traverse(const ngr_nav::IrOdometryPoint& now){
  if(s_.state!=NavState::Tracking||!s_.navDir||!haveRef_)return 0;
  uint64_t um=0;
  if(!travelUmAt(now,um)){if(now.owner&&!sameEpoch(refPoint_,now))haveRef_=false;return 0;} // ended epoch never reopens
  return advanceTraversed(um);
 }
 Ruling judge(const NavObservation&o){
  if(s_.state==NavState::Unset||s_.state==NavState::Lost)return Ruling::NoPosition;
  if(o.directionConflict||!s_.navDir)return Ruling::NoPosition;
  s_.evidence=EvidenceClass::None;s_.missedSinceLast=0;s_.distanceAssessable=false;s_.distanceConfirmed=false;
  // LOCATION_UNRESOLVED is conspicuous and never supplies a stale MM to AUTO.
  // Manual may continue; this first field build only records incoming evidence.
  if(s_.state==NavState::Uncertain){if(s_.unresolvedCount<255)++s_.unresolvedCount;s_.evidence=EvidenceClass::LocationUnresolved;refreshView();return Ruling::Uncertain;}
  uint64_t um=0;
  if(!travelUmAt(o.odometry,um)){
   // No valid MM-referenced IR distance: the 650 ms Hall-only fallback.
   s_.distance=!haveRef_?DistanceBasis::NoReference:!o.odometry.owner?DistanceBasis::NoIrPoint:DistanceBasis::EpochBreak;
   if(haveRef_){++s_.irWaits;s_.evidence=EvidenceClass::IrUnavailable;}
   if(s_.distance==DistanceBasis::EpochBreak)haveRef_=false;
   if(lastAcceptedMs_&&uint32_t(o.openedAtMs-lastAcceptedMs_)<HALL_ONLY_GUARD_MS)return reject();
   return acceptExpected(o);
  }
  s_.distance=DistanceBasis::Epoch;s_.distanceAssessable=true;
  // Expected MMs whose complete window lies behind this event were already
  // traversed; account for them first, at the event's own measured distance.
  advanceTraversed(um);
  // Too early: this Hall event cannot be the expected MM, which is unchanged.
  if(um*1000<uint64_t(expectedMm())*1000*IR_WINDOW_LOW_PERMILLE)return reject();
  // 0.85D..1.15D inclusive: physically eligible for the expected MM.
  s_.distanceConfirmed=true;return acceptExpected(o);
 }
private:
 static constexpr uint8_t SANS_Q=32;
 static bool sameEpoch(const ngr_nav::IrOdometryPoint&a,const ngr_nav::IrOdometryPoint&b){return a.owner&&a.owner==b.owner&&a.epoch&&a.epoch==b.epoch&&a.boot==b.boot&&a.calibration==b.calibration&&a.pitchUm&&a.pitchUm==b.pitchUm;}
 // Cumulative mapped distance from the last MM/IR synchronization to the expected MM.
 uint32_t expectedMm()const{return refCumMm_+spanMm(s_.navMm,s_.navDir);}
 // Beyond 1.15D: the expected MM's complete window has been traversed.
 bool traversed(uint64_t um)const{return um*1000>uint64_t(expectedMm())*1000*IR_WINDOW_HIGH_PERMILLE;}
 uint8_t advanceTraversed(uint64_t um){
  uint8_t n=0;
  while(n<ROUTE_N && traversed(um)){advanceSansMm(um);++n;}
  return n;
 }
 void advanceSansMm(uint64_t um){
  SansMmAdvance a;a.windowHighMm=double(expectedMm())*IR_WINDOW_HIGH_PERMILLE/1000.0;a.travelMm=double(um)/1000.0;
  refCumMm_+=spanMm(s_.navMm,s_.navDir);
  const uint8_t mm=s_.target;
  recovery_.append(mm,s_.navDir,false,0); // MISSED/UNKNOWN: no Hall or polarity evidence manufactured
  s_.navMm=mm;s_.target=nextMarker(mm,s_.navDir);s_.state=NavState::Tracking;
  s_.evidence=EvidenceClass::MissedObservation;++s_.advances;
  if(s_.missedSinceLast<255)++s_.missedSinceLast;
  if(s_.sansMmConsecutive<255)++s_.sansMmConsecutive;
  ++s_.sansMmTotal;
  a.mm=mm;a.referenceMm=refMm_;a.consecutive=s_.sansMmConsecutive;a.dir=s_.navDir;
  const uint8_t next=uint8_t((sansHead_+1)%SANS_Q);
  if(next==sansTail_){sansTail_=uint8_t((sansTail_+1)%SANS_Q);++sansDrops_;}
  sansQ_[sansHead_]=a;sansHead_=next;
  refreshView();
 }
 Ruling acceptExpected(const NavObservation&o){
  const uint8_t mm=s_.target;const bool pol=o.polarity==polarityAt(mm);
  acceptThrough(mm,o,pol?EvidenceClass::Confirmed:EvidenceClass::PolarityDiscrepancy);
  return pol?Ruling::Advanced:Ruling::AdvancedWithDiscrepancy;
 }
 Ruling reject(){++s_.refusals;s_.evidence=EvidenceClass::NonLandmarkHall;refreshView();return Ruling::NonLandmark;}
 void acceptThrough(uint8_t finalMm,const NavObservation&o,EvidenceClass why){
  if(repeatAfterReverse_) {
    recovery_.repeated(finalMm,s_.navDir,o.polarity,o.odometry);repeatAfterReverse_=false;
  } else recovery_.append(finalMm,s_.navDir,true,o.polarity,o.odometry);
  s_.navMm=finalMm;s_.target=nextMarker(finalMm,s_.navDir);s_.state=NavState::Tracking;
  s_.trust=Trust::Continuity;s_.evidence=why;s_.unresolvedCount=0;s_.advances+=1;
  s_.sansMmConsecutive=0;lastAcceptedMs_=o.openedAtMs;
  // New MM/IR synchronization immediately whenever the accepted strike has a
  // current-epoch IR point. No probation after IR recovery.
  if(o.odometry.owner&&o.odometry.epoch&&o.odometry.pitchUm){refPoint_=o.odometry;haveRef_=true;refCumMm_=0;}else haveRef_=false;
  const auto d=recovery_.evaluate(finalMm,s_.navDir);
  s_.sequenceLength=d.observations;s_.sequenceMatches=d.incumbent;
  if(d.offset) {
    correction_={s_.navMm,routeMod(int(s_.navMm)+d.offset),d.offset,d.incumbent};
    s_.navMm=correction_.toMm;s_.target=nextMarker(s_.navMm,s_.navDir);
    corrected_=true;++s_.corrections;s_.trust=Trust::SequenceRecovered;s_.sequenceMatches=d.best;
  }
  refMm_=s_.navMm; // the synchronized strike's MM identity follows any correction
  refreshView();
 }
 void refreshView(){view_.set(consoleHasReference(s_.state),s_.navMm,s_.navDir,lastAcceptedMs_,s_.distanceConfirmed,s_.distance,recovery_.count());}
 NavStatus s_{};PositionView view_{};ProximalRecovery recovery_{};
 bool reset_=false,haveRef_=false,repeatAfterReverse_=false;
 uint32_t lastAcceptedMs_=0,refCumMm_=0;uint8_t refMm_=0;ngr_nav::IrOdometryPoint refPoint_{};
 SansMmAdvance sansQ_[SANS_Q]{};uint8_t sansHead_=0,sansTail_=0;uint32_t sansDrops_=0;
 bool corrected_=false;SequenceCorrection correction_{};
};
} // namespace navi_one
