#pragma once
#include <stdint.h>
#include "RouteMap.h"

namespace navi_one {
struct Passage { uint32_t openedAtMs=0; uint8_t polarity=0; bool directionConflict=false, restartUncertain=false; };
enum class NavState : uint8_t { Unset, Declared, Unresolved, LimitStopped };
inline const char* navStateName(NavState n) {
  switch(n) { case NavState::Declared:return "DECLARED"; case NavState::Unresolved:return "UNRESOLVED";
    case NavState::LimitStopped:return "LIMIT_STOPPED"; default:return "UNSET"; }
}
enum class Ruling : uint8_t { Advanced, NoPosition, Ambiguous, Reestablished, UnresolvedLimitStop };
inline const char* rulingName(Ruling r) {
  switch(r) { case Ruling::Advanced:return "ADVANCED"; case Ruling::Ambiguous:return "AMBIGUOUS";
    case Ruling::Reestablished:return "REESTABLISHED"; case Ruling::UnresolvedLimitStop:return "UNRESOLVED_LIMIT_STOP";
    default:return "NO_POSITION"; }
}
enum class Trust : uint8_t { Declared, Sequence };
inline const char* trustName(Trust t) { return t==Trust::Sequence ? "SEQUENCE" : "DECLARED"; }
struct NavStatus {
  NavState state=NavState::Unset;
  uint8_t navMm=0, target=0;
  int8_t navDir=0;
  Trust trust=Trust::Declared;
  uint32_t advances=0, refusals=0;
  uint8_t unresolvedCount=0, sequenceLength=0;
  uint16_t sequenceMatches=0;
  Ruling lastRuling=Ruling::NoPosition;
};
class Navigator {
 public:
  const NavStatus& status() const { return s_; }
  bool positionKnown() const { return s_.state==NavState::Declared && s_.navDir!=0; }
  bool unresolved() const { return s_.state==NavState::Unresolved; }
  bool takeResetRequest() { bool r=resetPending_; resetPending_=false; return r; }
  void haltForLoss() { s_.state=NavState::LimitStopped; s_.lastRuling=Ruling::NoPosition; }
  void declare(uint8_t mm,int8_t dir) {
    s_=NavStatus{}; s_.navMm=mm%ROUTE_N; s_.navDir=dir; s_.target=nextMarker(s_.navMm,dir);
    s_.state=dir?NavState::Declared:NavState::Unset; sequenceLength_=0; resetPending_=true;
  }
  void setDirection(int8_t dir) {
    if (!dir || dir==s_.navDir) return;
    s_.navDir=dir; s_.target=nextMarker(s_.navMm,dir); beginUnresolved(); resetPending_=true;
  }
  Ruling judge(const Passage& p) {
    if (s_.state==NavState::Unset || s_.state==NavState::LimitStopped || !s_.navDir)
      return s_.lastRuling=Ruling::NoPosition;
    if (positionKnown() && !p.directionConflict && !p.restartUncertain && p.polarity==polarityAt(s_.target)) {
      s_.navMm=s_.target; s_.target=nextMarker(s_.navMm,s_.navDir); ++s_.advances;
      return s_.lastRuling=Ruling::Advanced;
    }
    const bool first=!unresolved();
    if (first) { ++s_.refusals; beginUnresolved(); }
    push(p.polarity);
    // The contradictory opening is #0; only subsequent NAV-eligible openings count.
    if (!first) ++s_.unresolvedCount;
    uint8_t found=0;
    s_.sequenceMatches=sequenceLength_==10 ? matchTen(found) : 0;
    if (s_.sequenceMatches==1 && !p.directionConflict) {
      s_.navMm=found; s_.target=nextMarker(found,s_.navDir); s_.state=NavState::Declared;
      s_.trust=Trust::Sequence; s_.unresolvedCount=0;
      return s_.lastRuling=Ruling::Reestablished;
    }
    if (s_.unresolvedCount>=10) {
      s_.state=NavState::LimitStopped;
      return s_.lastRuling=Ruling::UnresolvedLimitStop;
    }
    return s_.lastRuling=Ruling::Ambiguous;
  }
 private:
  void beginUnresolved() {
    s_.state=NavState::Unresolved; s_.unresolvedCount=0; s_.sequenceLength=0;
    s_.sequenceMatches=0; sequenceLength_=0;
  }
  void push(uint8_t pole) {
    if (sequenceLength_<10) sequence_[sequenceLength_++]=pole;
    else { for (uint8_t i=1;i<10;++i) sequence_[i-1]=sequence_[i]; sequence_[9]=pole; }
    s_.sequenceLength=sequenceLength_;
  }
  uint16_t matchTen(uint8_t& found) const {
    uint16_t hits=0;
    for (uint16_t end=0;end<ROUTE_N;++end) {
      bool fits=true;
      for (uint8_t i=0;i<10;++i) {
        uint8_t mm=routeMod((int32_t)end-(int32_t)s_.navDir*(9-i));
        if (polarityAt(mm)!=sequence_[i]) { fits=false; break; }
      }
      if (fits) { found=(uint8_t)end; ++hits; }
    }
    return hits;
  }
  NavStatus s_{};
  uint8_t sequence_[10]{};
  uint8_t sequenceLength_=0;
  bool resetPending_=false;
};
}
