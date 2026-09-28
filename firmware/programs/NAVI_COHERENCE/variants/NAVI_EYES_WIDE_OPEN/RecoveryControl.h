#pragma once
#include <string.h>
#include "Navigator.h"
#include "Stations.h"
namespace ngr_nav {
struct ConsensusOrder{bool agrees=false;navi_one::StationOrder order{};navi_one::StationMachine next{};};
inline ConsensusOrder stationConsensus(const navi_one::StationMachine& current,const navi_one::PositionView& nav,uint8_t pwm,uint8_t cruise,uint32_t now){
 ConsensusOrder out;if(!nav.count())return out;auto trial=current;const auto& p=nav.hypothesis(0);const auto section=navi_one::cruisePwmAt(p.mm,nav.direction(),cruise);auto order=trial.tick(p.mm,nav.direction(),pwm,section,now);if(order.event&&(!strcmp(order.event,"MISSED")||!strcmp(order.event,"PHASE_TIMEOUT")))return out;out.order=order;out.next=trial;out.agrees=true;return out;
}
}
