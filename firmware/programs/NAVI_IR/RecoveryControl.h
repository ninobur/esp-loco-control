#pragma once
#include <string.h>
#include "Navigator.h"
#include "Stations.h"
namespace ngr_nav {
struct ConsensusOrder { bool agrees=false; navi_one::StationOrder order{}; navi_one::StationMachine next{}; };
// Evaluate every possible position against the SAME actual controller state.
// The common state advances only if every branch requests the same action.
inline ConsensusOrder stationConsensus(const navi_one::StationMachine& current,
    const navi_hypothesis::HypothesisNavigator& nav,uint8_t pwm,uint8_t cruise,uint32_t now) {
  ConsensusOrder out;
  if(!nav.count())return out;
  for(uint8_t i=0;i<nav.count();++i){
    auto trial=current;
    const auto section=navi_one::cruisePwmAt(nav.hypothesis(i).mm,nav.direction(),cruise);
    auto order=trial.tick(nav.hypothesis(i).mm,nav.direction(),pwm,section,now);
    // A failed station approach withdraws authority, never restores cruise.
    if(order.event && (!strcmp(order.event,"MISSED") || !strcmp(order.event,"PHASE_TIMEOUT"))) {
      out.agrees=false;return out;
    }
    if(!i){out.order=order;out.next=trial;out.agrees=true;}
    else if(order.setThrottle!=out.order.setThrottle || order.pwm!=out.order.pwm ||
        order.stepMs!=out.order.stepMs || !trial.sameState(out.next) ||
        strcmp(order.station,out.order.station) ||
        bool(order.event)!=bool(out.order.event) ||
        (order.event && strcmp(order.event,out.order.event))){out.agrees=false;return out;}
  }
  return out;
}
}
