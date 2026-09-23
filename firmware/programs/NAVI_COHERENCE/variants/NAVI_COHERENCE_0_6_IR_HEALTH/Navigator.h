#pragma once
#include <stdint.h>
#include "RouteMap.h"
#include "MovementEvidence.h"
#include "ProximalRecovery.h"

namespace navi_one {
static constexpr uint32_t MIN_MARKER_MS=500;
static constexpr uint8_t DNA_WORD=10;
static constexpr double IR_WINDOW_FRACTION=0.15; // operator ruling 2026-09-22: widened from 0.10 after 4/78 real magnets were refused
// PROXIMAL_R1 supersedes the 0.5 whole-route 10/10 sequence authority.
// Unknown passages occupy physical history but never count as observed poles.

enum class EvidenceClass:uint8_t{None,Confirmed,NonLandmarkHall,PolarityDiscrepancy,MissedObservation,MissedWithPolarityDiscrepancy,IrUnavailable,LocationUnresolved};
inline const char* evidenceClassName(EvidenceClass e){switch(e){case EvidenceClass::Confirmed:return "CONFIRMED";case EvidenceClass::NonLandmarkHall:return "NON_LANDMARK_HALL";case EvidenceClass::PolarityDiscrepancy:return "POLARITY_DISCREPANCY";case EvidenceClass::MissedObservation:return "MISSED_OBSERVATION";case EvidenceClass::MissedWithPolarityDiscrepancy:return "MISSED_OBSERVATION+POLARITY_DISCREPANCY";case EvidenceClass::IrUnavailable:return "IR_UNAVAILABLE";case EvidenceClass::LocationUnresolved:return "LOCATION_UNRESOLVED";default:return "NONE";}}
struct NavObservation{uint32_t openedAtMs=0;uint8_t polarity=0,windowPolarity=0;bool directionConflict=false,restartUncertain=false,windowValid=false;ngr_nav::MotionPoint movement{};ngr_nav::IrOdometryPoint odometry{};};
enum class NavState:uint8_t{Unset,Tracking,Uncertain,Lost};
inline const char* navStateName(NavState s){switch(s){case NavState::Tracking:return "TRACKING";case NavState::Uncertain:return "LOCATION_UNRESOLVED";case NavState::Lost:return "LOST";default:return "UNSET";}}
inline const char* consoleNavName(NavState s){switch(s){case NavState::Tracking:return "NORMAL";case NavState::Uncertain:return "UNRESOLVED";case NavState::Lost:return "LOST";default:return "UNSET";}}
inline bool consoleHasReference(NavState s){return s==NavState::Tracking;}
enum class Ruling:uint8_t{Advanced,NoPosition,NonLandmark,MissedAndAdvanced,AdvancedWithDiscrepancy,Uncertain,Reestablished,Retained};
inline const char* rulingName(Ruling r){switch(r){case Ruling::Advanced:return "ADVANCED";case Ruling::NonLandmark:return "NON_LANDMARK_HALL";case Ruling::MissedAndAdvanced:return "MISSED_AND_ADVANCED";case Ruling::AdvancedWithDiscrepancy:return "ADVANCED_WITH_DISCREPANCY";case Ruling::Uncertain:return "LOCATION_UNRESOLVED";case Ruling::Retained:return "RETAINED";default:return "NO_POSITION";}}
enum class Trust:uint8_t{OperatorDeclared,Continuity,SequenceRecovered};
inline const char* trustName(Trust t){return t==Trust::Continuity?"CONTINUITY":t==Trust::SequenceRecovered?"SEQUENCE_RECOVERED":"OPERATOR_DECLARED";}
struct SequenceCorrection{uint8_t fromMm=0,toMm=0;int16_t offset=0;uint8_t matchesBefore=0;};
struct NavStatus{NavState state=NavState::Unset;uint8_t navMm=0,target=0;int8_t navDir=0;Trust trust=Trust::OperatorDeclared;uint32_t corrections=0;EvidenceClass evidence=EvidenceClass::None;uint32_t advances=0,refusals=0,irWaits=0;uint8_t unresolvedCount=0,sequenceLength=0;uint16_t sequenceMatches=0;uint8_t missedSinceLast=0;bool distanceConfirmed=false,distanceAssessable=false;ngr_nav::MotionIssue irIssue=ngr_nav::MotionIssue::NoSource;};
struct PositionEvidence{uint8_t mm=0;uint32_t lastRealMs=0;uint8_t faults=0,causes=0,clean=0;bool distanceAgrees=false;ngr_nav::MotionIssue distanceIssue=ngr_nav::MotionIssue::NoSource;ngr_nav::MotionPoint movement{};};
class PositionView{public:uint8_t count()const{return valid_?1:0;}const PositionEvidence& hypothesis(uint8_t)const{return p_;}int8_t direction()const{return dir_;}void set(bool v,uint8_t mm,int8_t d,uint32_t at,const ngr_nav::MotionPoint&m,bool ok,ngr_nav::MotionIssue issue,uint8_t clean){valid_=v;dir_=d;p_.mm=mm;p_.lastRealMs=at;p_.movement=m;p_.distanceAgrees=ok;p_.distanceIssue=issue;p_.clean=clean;}private:bool valid_=false;int8_t dir_=0;PositionEvidence p_{};};

class Navigator{
public:
 const NavStatus& status()const{return s_;} const PositionView& hypotheses()const{return view_;}
 const ProximalRecovery& recovery()const{return recovery_;}
 bool positionKnown()const{return s_.state==NavState::Tracking;} bool unresolved()const{return s_.state==NavState::Uncertain;} bool evaluating()const{return unresolved();}
 bool takeResetRequest(){bool r=reset_;reset_=false;return r;}
 bool takeCorrection(SequenceCorrection& c){if(!corrected_)return false;c=correction_;corrected_=false;return true;}
 void haltForLoss(){s_.state=NavState::Lost;refreshView();}
 void declare(uint8_t mm,int8_t dir,uint32_t nowMs=0){s_=NavStatus{};s_.navMm=mm;s_.navDir=dir;s_.target=dir?nextMarker(mm,dir):mm;s_.state=dir?NavState::Tracking:NavState::Unset;s_.trust=Trust::OperatorDeclared;haveAnchor_=false;lastAcceptedMs_=nowMs;recovery_.reset();corrected_=false;reset_=true;repeatAfterReverse_=false;refreshView();}
 // Reversal keeps positional authority/history. The point last passed in the old
 // direction is the first expected point in the new direction. carryResetRequest()
 // starts a fresh unsigned-IR frame. Recovery keeps the physical observations
 // and waits for ten new-direction positions before considering relocation.
 // 0.5: a direction set before any declaration records the direction only. 0.4
 // set TRACKING from UNSET, reporting a known MM0 that nobody declared -- and
 // with AUTO enabled, admitAuto() reads positionKnown().
 void setDirection(int8_t dir,uint32_t nowMs=0){if(!dir||dir==s_.navDir)return;if(s_.state==NavState::Unset||s_.state==NavState::Lost){s_.navDir=dir;refreshView();return;}s_.navDir=dir;s_.target=s_.navMm;s_.state=NavState::Tracking;recovery_.reverse();repeatAfterReverse_=true;s_.evidence=EvidenceClass::None;s_.unresolvedCount=0;haveAnchor_=false;lastAcceptedMs_=nowMs;reset_=true;refreshView();}
 Ruling judge(const NavObservation&o){
  if(s_.state==NavState::Unset||s_.state==NavState::Lost)return Ruling::NoPosition;
  if(o.directionConflict||!s_.navDir)return Ruling::NoPosition;
  s_.evidence=EvidenceClass::None;s_.missedSinceLast=0;s_.distanceAssessable=false;s_.distanceConfirmed=false;s_.irIssue=o.movement.issue;
  // LOCATION_UNRESOLVED is conspicuous and never supplies a stale MM to AUTO.
  // Manual may continue; this first field build only records incoming evidence.
  if(s_.state==NavState::Uncertain){if(s_.unresolvedCount<255)++s_.unresolvedCount;s_.evidence=EvidenceClass::LocationUnresolved;refreshView();return Ruling::Uncertain;}
  // Before the first accepted Hall point (or after IR frame loss/reversal), timing
  // remains the fallback gate. There is never an ungated Hall advance.
  if(!haveAnchor_){if(lastAcceptedMs_&&uint32_t(o.openedAtMs-lastAcceptedMs_)<MIN_MARKER_MS)return reject();uint8_t mm=s_.target;bool pol=hallSupports(o,polarityAt(mm));acceptThrough(mm,o,1,pol?EvidenceClass::Confirmed:EvidenceClass::PolarityDiscrepancy);return pol?Ruling::Advanced:Ruling::AdvancedWithDiscrepancy;}
  uint32_t elapsed=o.openedAtMs-anchorMs_;auto interval=ngr_nav::between(anchorMovement_,o.movement);s_.distanceAssessable=interval.usable();s_.irIssue=interval.issue;
  if(!interval.usable()){++s_.irWaits;s_.evidence=EvidenceClass::IrUnavailable;if(elapsed<MIN_MARKER_MS)return reject();uint8_t mm=s_.target;bool pol=hallSupports(o,polarityAt(mm));acceptThrough(mm,o,1,pol?EvidenceClass::Confirmed:EvidenceClass::PolarityDiscrepancy);return pol?Ruling::Advanced:Ruling::AdvancedWithDiscrepancy;}
  // Valid IR: a Hall event may declare only a mapped point whose cumulative
  // distance from the last declared point falls inside that point's +/-15% window.
  uint32_t cum=0;uint8_t cur=s_.navMm,steps=0;
  for(uint8_t step=1;step<=DNA_WORD;++step){cum+=spanMm(cur,s_.navDir);cur=nextMarker(cur,s_.navDir);double lo=double(cum)*(1.0-IR_WINDOW_FRACTION),hi=double(cum)*(1.0+IR_WINDOW_FRACTION);if(interval.nominalMm>=lo&&interval.nominalMm<=hi){steps=step;break;}if(interval.nominalMm<lo)break;}
  // An out-of-window observation does not erase the incumbent world view.
  if(!steps)return reject();
  s_.distanceConfirmed=true;uint8_t mm=s_.navMm;for(uint8_t i=0;i<steps;++i)mm=nextMarker(mm,s_.navDir);bool pol=hallSupports(o,polarityAt(mm));
  if(steps>1){s_.missedSinceLast=uint8_t(steps-1);acceptThrough(mm,o,steps,pol?EvidenceClass::MissedObservation:EvidenceClass::MissedWithPolarityDiscrepancy);return Ruling::MissedAndAdvanced;}
  acceptThrough(mm,o,1,pol?EvidenceClass::Confirmed:EvidenceClass::PolarityDiscrepancy);return pol?Ruling::Advanced:Ruling::AdvancedWithDiscrepancy;
 }
private:
 static bool hallSupports(const NavObservation&o,uint8_t expected){return o.polarity==expected||(o.windowValid&&o.windowPolarity==expected);}
 Ruling reject(){++s_.refusals;s_.evidence=EvidenceClass::NonLandmarkHall;refreshView();return Ruling::NonLandmark;}
 void acceptThrough(uint8_t finalMm,const NavObservation&o,uint8_t steps,EvidenceClass why){
  uint8_t walk=s_.navMm;
  if(repeatAfterReverse_) {
    recovery_.repeated(finalMm,s_.navDir,o.polarity,o.odometry);repeatAfterReverse_=false;
  } else for(uint8_t i=1;i<=steps;++i) {
    walk=nextMarker(walk,s_.navDir);
    recovery_.append(walk,s_.navDir,i==steps,o.polarity,i==steps?o.odometry:ngr_nav::IrOdometryPoint{});
  }
  s_.navMm=finalMm;s_.target=nextMarker(finalMm,s_.navDir);s_.state=NavState::Tracking;
  s_.trust=Trust::Continuity;s_.evidence=why;s_.unresolvedCount=0;s_.advances+=steps;
  anchorMs_=o.openedAtMs;lastAcceptedMs_=o.openedAtMs;
  if(o.movement.issue==ngr_nav::MotionIssue::None){anchorMovement_=o.movement;haveAnchor_=true;}else haveAnchor_=false;
  const auto d=recovery_.evaluate(finalMm,s_.navDir);
  s_.sequenceLength=d.observations;s_.sequenceMatches=d.incumbent;
  if(d.offset) {
    correction_={s_.navMm,routeMod(int(s_.navMm)+d.offset),d.offset,d.incumbent};
    s_.navMm=correction_.toMm;s_.target=nextMarker(s_.navMm,s_.navDir);
    corrected_=true;++s_.corrections;s_.trust=Trust::SequenceRecovered;s_.sequenceMatches=d.best;
  }
  refreshView();
 }
 void refreshView(){view_.set(consoleHasReference(s_.state),s_.navMm,s_.navDir,anchorMs_,anchorMovement_,s_.distanceConfirmed,s_.irIssue,recovery_.count());}
 NavStatus s_{};PositionView view_{};ProximalRecovery recovery_{};
 bool reset_=false,haveAnchor_=false,repeatAfterReverse_=false;
 uint32_t anchorMs_=0,lastAcceptedMs_=0;ngr_nav::MotionPoint anchorMovement_{};
 bool corrected_=false;SequenceCorrection correction_{};
};
} // namespace navi_one
