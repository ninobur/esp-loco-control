#pragma once
#include "HypothesisNavigator.h"

namespace navi_one {
struct NavObservation {
  uint32_t openedAtMs=0;
  uint8_t polarity=0,windowPolarity=0;
  bool directionConflict=false,restartUncertain=false,windowValid=false;
  ngr_nav::MotionPoint movement{};
};

enum class NavState : uint8_t { Unset, Declared, Unresolved, LimitStopped };
inline const char* navStateName(NavState s) {
  switch(s) {
    case NavState::Declared:return "TRACKING";
    case NavState::Unresolved:return "AMBIGUOUS";
    case NavState::LimitStopped:return "LOST";
    default:return "UNSET";
  }
}
inline const char* consoleNavName(NavState s) {
  switch(s) {
    case NavState::Declared:return "NORMAL";
    case NavState::Unresolved:return "EVALUATING";
    case NavState::LimitStopped:return "LOST";
    default:return "UNSET";
  }
}
inline bool consoleHasReference(NavState s) {
  return s==NavState::Declared || s==NavState::Unresolved;
}

enum class Ruling : uint8_t {
  Advanced, NoPosition, Ambiguous, Reestablished, UnresolvedLimitStop, Retained, MovementReseeded
};
inline const char* rulingName(Ruling s) {
  switch(s) {
    case Ruling::Advanced:return "ADVANCED";
    case Ruling::Ambiguous:return "AMBIGUOUS";
    case Ruling::Reestablished:return "REESTABLISHED";
    case Ruling::UnresolvedLimitStop:return "RECOVERY_EXHAUSTED";
    case Ruling::Retained:return "FALSE_OBSERVATION";
    case Ruling::MovementReseeded:return "MOVEMENT_RESEEDED";
    default:return "NO_POSITION";
  }
}

enum class Trust : uint8_t { Declared, Sequence };
inline const char* trustName(Trust t){return t==Trust::Sequence?"CONDITIONAL_SEQUENCE":"DECLARED";}

struct NavStatus {
  NavState state=NavState::Unset;
  uint8_t navMm=0,target=0;
  int8_t navDir=0;
  Trust trust=Trust::Declared;
  uint32_t advances=0,refusals=0,irWaits=0;
  uint8_t unresolvedCount=0;
  uint16_t sequenceMatches=0;
  bool distanceConfirmed=false,distanceAssessable=false;
  uint8_t movementPredictedMm=0;
  uint32_t movementPredictedAtMs=0;
  bool movementPredictionValid=false;
  ngr_nav::MotionIssue irIssue=ngr_nav::MotionIssue::NoSource;
};

class Navigator {
 public:
  const NavStatus& status() const{return s_;}
  const navi_hypothesis::HypothesisNavigator& hypotheses() const{return core_;}
  bool positionKnown() const{return s_.state==NavState::Declared;}
  bool unresolved() const{return s_.state==NavState::Unresolved;}
  bool evaluating() const{return unresolved();}
  bool takeResetRequest(){bool r=reset_;reset_=false;return r;}
  void haltForLoss(){s_.state=NavState::LimitStopped;s_.distanceConfirmed=false;}

  void declare(uint8_t mm,int8_t dir) {
    s_=NavStatus{};s_.navMm=mm;s_.navDir=dir;s_.target=nextMarker(mm,dir);
    core_.declare(mm,dir,0);s_.state=dir?NavState::Declared:NavState::Unset;
    haveConfirmedAnchor_=false; lastMovementSpeedMmS_=0;
    reset_=true;recoveryActive_=false;
  }

  // Unsigned wheel movement cannot establish a reversed route frame.
  void setDirection(int8_t dir) {
    if(dir!=s_.navDir) {
      s_.navDir=dir;s_.target=nextMarker(s_.navMm,dir);s_.state=NavState::Unset;
      s_.distanceConfirmed=false;s_.distanceAssessable=false;
      haveConfirmedAnchor_=false;lastMovementSpeedMmS_=0;reset_=true;
    }
  }

  Ruling judge(const NavObservation& p) {
    if(s_.state==NavState::Unset || s_.state==NavState::LimitStopped)return Ruling::NoPosition;
    if(p.directionConflict) {
      s_.state=NavState::Unset;s_.distanceConfirmed=false;s_.distanceAssessable=false;
      haveConfirmedAnchor_=false;return Ruling::NoPosition;
    }

    const bool wasEvaluating=evaluating();
    navi_hypothesis::Observation o;
    o.atMs=p.openedAtMs;o.openingPolarity=p.polarity;o.windowPolarity=p.windowPolarity;
    o.windowValid=p.windowValid;o.movement=p.movement;

    auto result=core_.observe(o);
    refreshEvidenceStatus();

    // 0.4: uncertainty itself permits NAVI to use all evidence it has. If the
    // ordinary local hypotheses collapse, or remain ambiguous, try a wider
    // map search from the last confirmed Hall/movement anchor. IR is not an
    // agent and never assigns position: movement narrows the map candidates;
    // this Hall observation must corroborate them.
    if((result==navi_hypothesis::Result::Lost || result==navi_hypothesis::Result::Ambiguous) &&
       haveConfirmedAnchor_) {
      if(core_.reseedFromMovement(confirmedMm_,confirmedMovement_,o)) {
        result=core_.result();
        refreshEvidenceStatus();
        if(core_.positionCertain()) {
          const uint8_t mm=core_.position();
          const bool advanced=mm!=s_.navMm;
          s_.navMm=mm;s_.target=nextMarker(mm,s_.navDir);s_.state=NavState::Declared;
          s_.unresolvedCount=0;recoveryActive_=false;
          if(advanced)++s_.advances;
          acceptConfirmedAnchor(mm,p);
          s_.trust=Trust::Sequence;
          return Ruling::MovementReseeded;
        }
        s_.state=NavState::Unresolved;
        return Ruling::Ambiguous;
      }
    }

    if(result==navi_hypothesis::Result::Lost) {
      haltForLoss();return Ruling::UnresolvedLimitStop;
    }

    if(!s_.distanceAssessable)++s_.irWaits;

    if(result==navi_hypothesis::Result::Ambiguous) {
      s_.state=NavState::Unresolved;++s_.refusals;
      // Ten clean map-pattern observations remain a backstop for restoring
      // certainty; they are not a startup requirement and are not an IR test.
      if(recoveryActive_)++s_.unresolvedCount;
      else {recoveryActive_=true;s_.unresolvedCount=1;}
      if(s_.unresolvedCount>=10){
        haltForLoss();return Ruling::UnresolvedLimitStop;
      }
      return Ruling::Ambiguous;
    }

    const uint8_t mm=core_.position();
    const bool advanced=mm!=s_.navMm;
    s_.navMm=mm;s_.target=nextMarker(mm,s_.navDir);s_.state=NavState::Declared;
    s_.unresolvedCount=0;recoveryActive_=false;
    if(advanced){++s_.advances;acceptConfirmedAnchor(mm,p);}
    if(wasEvaluating)s_.trust=Trust::Sequence;
    return wasEvaluating?Ruling::Reestablished:advanced?Ruling::Advanced:Ruling::Retained;
  }

 private:
  void refreshEvidenceStatus(){
    s_.sequenceMatches=core_.count();
    s_.distanceConfirmed=s_.distanceAssessable=core_.count()>0;
    s_.irIssue=ngr_nav::MotionIssue::None;
    for(uint8_t i=0;i<core_.count();++i) {
      const auto& h=core_.hypothesis(i);
      if(!h.distanceAgrees)s_.distanceConfirmed=false;
      if(h.distanceIssue!=ngr_nav::MotionIssue::None) {
        s_.distanceAssessable=false;
        if(s_.irIssue==ngr_nav::MotionIssue::None)s_.irIssue=h.distanceIssue;
      }
    }
  }

  void acceptConfirmedAnchor(uint8_t mm,const NavObservation& p){
    if(haveConfirmedAnchor_) {
      const auto interval=ngr_nav::between(confirmedMovement_,p.movement);
      const uint32_t dt=p.openedAtMs-confirmedAtMs_;
      if(interval.usable() && dt)
        lastMovementSpeedMmS_=uint32_t(interval.nominalMm*1000.0/dt);
    }
    confirmedMm_=mm;confirmedMovement_=p.movement;confirmedAtMs_=p.openedAtMs;
    haveConfirmedAnchor_=true;updateMovementPrediction();
  }

  void updateMovementPrediction(){
    s_.movementPredictionValid=false;
    if(!haveConfirmedAnchor_ || !lastMovementSpeedMmS_ || !s_.navDir)return;
    s_.movementPredictedMm=nextMarker(confirmedMm_,s_.navDir);
    s_.movementPredictedAtMs=confirmedAtMs_+
      uint32_t((uint64_t(navi_one::spanMm(confirmedMm_,s_.navDir))*1000ULL)/lastMovementSpeedMmS_);
    s_.movementPredictionValid=true;
  }

  navi_hypothesis::HypothesisNavigator core_;
  NavStatus s_{};
  bool reset_=false,recoveryActive_=false;
  bool haveConfirmedAnchor_=false;
  uint8_t confirmedMm_=0;
  uint32_t confirmedAtMs_=0,lastMovementSpeedMmS_=0;
  ngr_nav::MotionPoint confirmedMovement_{};
};
}
