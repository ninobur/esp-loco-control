// 0.5: station logic relies on the working (corrected) model.
// Build: g++ -std=c++17 -I. tests/test_station_correction.cpp
#include <cassert>
#include <iostream>
#define NAVI_APPROACH_MARKER_MS { 1390, 1539, 1725, 1960, 2271 }  // Toby, LL_LocoConfig_9950012.h
#include "Stations.h"
using namespace navi_one;
int main(){
 int checked=0;
 for(uint8_t i=0;i<STATION_COUNT;++i)for(int dir=-1;dir<=1;dir+=2){
  const uint8_t c=STATIONS[i].centre;
  auto at=[&](int off){return routeMod(dir>0?int32_t(c)+off:int32_t(c)-off);};
  // Ordinary running now obeys the current approach position without a trigger.
  {StationMachine m;assert(m.tick(at(-9),int8_t(dir),90,90,0).setThrottle);assert(m.phase()==StPhase::Approach);}
  // A correction inside an approach uses the same current-position rule as GO.
  for(int off=APPROACH_START+1;off<ZONE_START;++off){
   StationMachine m;auto o=m.tick(at(off),int8_t(dir),90,90,10);
   assert(m.phase()==StPhase::Approach);assert(o.setThrottle);assert(o.offset==off);assert(o.pwm<=90);++checked;}
  // Outside a station region there is no station order.
  {StationMachine m;assert(!m.tick(at(-20),int8_t(dir),90,90,0).setThrottle);assert(m.phase()==StPhase::Idle);}
  {StationMachine m;m.tick(at(APPROACH_START),int8_t(dir),90,90,0);assert(m.phase()==StPhase::Approach);
   // Mid-approach correction: the machine simply recomputes from the corrected MM.
   auto o=m.tick(at(-7),int8_t(dir),90,90,10);assert(o.offset==-7);assert(m.phase()==StPhase::Approach);}
 }
 std::cout<<"station correction checks passed ("<<checked<<" position-derived approaches)\n";
}
