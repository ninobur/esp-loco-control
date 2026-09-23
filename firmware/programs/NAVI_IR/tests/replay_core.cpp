#include <iostream>
#include "Navigator.h"

// Input adapter only. The recorded MM is checked AFTER judgement, not supplied
// as identity evidence. Old close-time Hall observations cannot test acquisition.
int main(){
  navi_one::Navigator nav;nav.declare(40,1);
  uint64_t us,pulses;int pole,quality,recorded;unsigned count=0,known=0,ambiguous=0,lost=0,mismatch=0,recovered=0,reseeded=0,peak=0;
  while(std::cin>>us>>pulses>>pole>>quality>>recorded){
    navi_one::NavObservation e;e.openedAtMs=us/1000;e.polarity=pole;
    e.movement.issue=quality?ngr_nav::MotionIssue::None:ngr_nav::MotionIssue::Unaligned;
    e.movement.frame=1;e.movement.wire.bootId=1;e.movement.wire.capturedUs=us;
    e.movement.wire.observedRises=e.movement.wire.completedPulses=pulses;
    e.movement.wire.opticalReason=ir_movement::TRACKING;
    const auto ruling=nav.judge(e);++count;
    // 0.4 has no WaitingDistance: CANNOT_ASSESS is folded into ordinary
    // tracking, so anything not positionKnown() is either unresolved or lost.
    if(nav.positionKnown()){++known;if(nav.status().navMm!=recorded)++mismatch;}
    else if(nav.unresolved())++ambiguous;else ++lost;
    if(ruling==navi_one::Ruling::Reestablished)++recovered;
    if(ruling==navi_one::Ruling::MovementReseeded){++recovered;++reseeded;}
    if(nav.hypotheses().count()>peak)peak=nav.hypotheses().count();
  }
  std::cout<<"{\"events\":"<<count<<",\"tracking\":"<<known
           <<",\"ambiguous\":"<<ambiguous<<",\"lost\":"<<lost
           <<",\"different_from_recorded_mm\":"<<mismatch<<",\"reestablished\":"<<recovered
           <<",\"movement_reseeded\":"<<reseeded
           <<",\"peak_branches\":"<<peak<<"}\n";
  return mismatch?1:0;
}
