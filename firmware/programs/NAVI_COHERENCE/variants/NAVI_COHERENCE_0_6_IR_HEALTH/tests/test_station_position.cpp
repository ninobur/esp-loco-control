// Position, visit history and pause/resume drive station targets, not an entry edge.
#include <cassert>
#include <cstring>
#include <iostream>
#define NAVI_APPROACH_MARKER_MS {1390,1539,1725,1960,2271}
#include "RecoveryControl.h"
using namespace navi_one;

static bool event(const StationOrder& o, const char* s) {
  return o.event && !std::strcmp(o.event,s);
}
int main() {
  unsigned positions=0, resumes=0;
  for (int dir : {-1,1}) for (uint8_t i=0;i<STATION_COUNT;++i) {
    const auto& st=STATIONS[i];
    auto at=[&](int off){return uint8_t(routeMod(int(st.centre)+dir*off));};
    const auto sp=stationPwm(st,dir);
    const auto stop=stopOffsetFor(st,dir);
    // Enter at ANY position in the existing station region, including a late
    // declaration. Repeated ticks keep the target without repeating events.
    for (int off=APPROACH_START;off<=OVERSHOOT_ABANDON;++off) {
      StationMachine m;
      auto o=m.tick(at(off),dir,0,90,100);
      const int expected=off>=stop?0:off>=ZONE_START?sp:90-(90-sp)*(off+11)/5;
      assert(o.setThrottle && o.pwm==expected);
      assert(m.stationIdx()==i);
      o=m.tick(at(off),dir,expected?expected:20,90,101);
      assert(o.setThrottle && o.pwm==expected && !o.event);
      // Resume at the same position after five minutes. No false timeout,
      // cruise-speed launch or forgotten stop obligation.
      m.setRunning(false,110);
      m.setRunning(false,300000);
      m.setRunning(true,300110);
      o=m.tick(at(off),dir,0,90,300111);
      assert(o.setThrottle && o.pwm==expected);
      assert(!event(o,"PHASE_TIMEOUT"));
      ++positions; ++resumes;
    }
    // A skipped approach/zone boundary still obeys the new position.
    StationMachine m;
    m.tick(at(-10),dir,90,90,0);
    auto o=m.tick(at(-4),dir,80,90,1000);
    assert(o.pwm==sp && m.phase()==StPhase::Zone);
    o=m.tick(at(-9),dir,sp,90,1100);
    assert(o.pwm==90-(90-sp)*2/5 && m.phase()==StPhase::Approach);
    o=m.tick(at(-4),dir,sp,90,1200);
    assert(o.pwm==sp && m.phase()==StPhase::Zone);
    o=m.tick(at(stop+1),dir,sp,90,2000);
    assert(o.pwm==0 && event(o,"ZERO_RAMP"));
    o=m.tick(at(stop+1),dir,0,90,3000);
    assert(o.pwm==0 && event(o,"DWELL_BEGIN"));
    m.setRunning(false,3100);
    m.setRunning(true,4000);
    o=m.tick(at(stop+1),dir,0,90,4000);
    assert(o.setThrottle && o.pwm==0 && m.phase()==StPhase::Dwell);
    // Wall-clock dwell remains five seconds even with a deliberate pause.
    o=m.tick(at(stop+1),dir,0,90,7999);
    assert(o.pwm==0 && m.phase()==StPhase::Dwell);
    o=m.tick(at(stop+1),dir,0,90,8000);
    const auto departure=departPwmFor(st,dir,90);
    assert(event(o,"DEPART") && o.pwm==departure);
    m.setRunning(false,8100);m.setRunning(true,308100);
    o=m.tick(at(stop+1),dir,0,90,308101);
    assert(o.setThrottle && o.pwm==departure && !o.event);
    ++resumes;
    o=m.tick(at(stop+3),dir,90,90,309000);
    assert(event(o,"DEPARTED") && m.phase()==StPhase::Idle);
    // Completed visit does not re-arm while still within the station region,
    // including another pause or a small backwards position correction.
    m.setRunning(false,309001);m.setRunning(true,610000);
    for (int off : {stop+3,stop+2,5}) {
      o=m.tick(at(off),dir,0,90,610001);
      assert(!o.setThrottle && !o.event && m.phase()==StPhase::Idle);
    }
    m.tick(at(6),dir,90,90,611000);
    o=m.tick(at(-10),dir,90,90,620000); // next visit after leaving the region
    assert(o.setThrottle && event(o,"ARMED"));
    // Active-time watchdog still works; pausing does not renew its budget.
    StationMachine timed;
    timed.tick(at(-9),dir,90,90,0);
    timed.setRunning(false,60000);timed.setRunning(true,360000);
    o=timed.tick(at(-9),dir,0,90,420001);
    assert(event(o,"PHASE_TIMEOUT"));
    StationMachine missed;
    missed.tick(at(-5),dir,sp,90,0);
    o=missed.tick(at(6),dir,sp,90,1);
    assert(event(o,"MISSED"));
    StationMachine corrected;
    corrected.tick(at(-9),dir,90,90,0);
    o=corrected.tick(at(-12),dir,78,90,1);
    assert(!o.setThrottle && event(o,"POSITION_REEVALUATED") && corrected.phase()==StPhase::Idle);
    // Existing consensus layer rejects failures rather than obeying fallback cruise.
    Navigator nav;nav.declare(at(-5),dir,0);
    StationMachine timeout;timeout.tick(at(-5),dir,sp,90,0);
    assert(!ngr_nav::stationConsensus(timeout,nav.hypotheses(),sp,90,120001).agrees);
    nav.declare(at(6),dir,0);
    assert(!ngr_nav::stationConsensus(timeout,nav.hypotheses(),sp,90,1).agrees);
  }
  // Exhaustive cold-start positions, both directions: only station regions act.
  for (int dir : {-1,1}) for (int mm=0;mm<ROUTE_N;++mm) {
    bool inside=false;
    for (const auto& st:STATIONS) {
      int off=offsetToCentre(mm,dir,st.centre);
      inside|=off>=APPROACH_START && off<=OVERSHOOT_ABANDON;
    }
    StationMachine m;
    assert(m.tick(mm,dir,0,90,0).setThrottle==inside);
  }
  // Actual field regression: pause outside Patio, pass MM25 while paused,
  // then resume at MM24 CCW. The approach is acquired here, not lost forever.
  StationMachine patio;
  patio.setRunning(false,0);patio.setRunning(true,35000);
  auto o=patio.tick(24,-1,0,90,35000);
  assert(o.pwm==78 && patio.phase()==StPhase::Approach);
  o=patio.tick(20,-1,60,90,40000);assert(o.pwm==60);
  o=patio.tick(15,-1,60,90,45000);assert(o.pwm==0);
  // Normal arrival from the CCW 105-PWM grade keeps the existing approach curve.
  StationMachine grade;
  o=grade.tick(25,-1,105,90,0);assert(o.pwm==96);
  o=grade.tick(24,-1,96,90,1000);assert(o.pwm==87);
  // Unsigned clock rollover during a pause.
  StationMachine wrap;
  wrap.tick(20,-1,60,90,UINT32_MAX-200);
  wrap.setRunning(false,UINT32_MAX-100);wrap.setRunning(true,200000);
  o=wrap.tick(20,-1,0,90,200001);assert(o.pwm==60 && !o.event);
  Navigator unset;StationMachine idle;
  assert(!ngr_nav::stationConsensus(idle,unset.hypotheses(),0,90,0).agrees);
  assert(!idle.tick(20,0,0,90,0).setThrottle);
  // Three complete policy traversals each way: one dwell per station per lap.
  // PWM settles instantly in this policy test, not a physical train simulation.
  for (int dir : {-1,1}) {
    StationMachine lap;unsigned stops[STATION_COUNT]={};uint32_t now=0;uint8_t pwm=90;
    for(int step=0;step<3*ROUTE_N;++step) {
      const auto mm=uint8_t(routeMod(dir*step));
      const auto cruise=cruisePwmAt(mm,dir,90);
      auto order=lap.tick(mm,dir,pwm,cruise,now+=1000);
      if(order.setThrottle)pwm=order.pwm;
      if(lap.phase()==StPhase::Idle)pwm=cruise;
      if(lap.phase()==StPhase::Ramp) {
        const auto idx=lap.stationIdx();
        assert(order.offset==stopOffsetFor(STATIONS[idx],dir));
        order=lap.tick(mm,dir,0,cruise,++now);
        assert(event(order,"DWELL_BEGIN"));++stops[idx];
        order=lap.tick(mm,dir,0,cruise,now+=STATION_DWELL_MS);
        assert(event(order,"DEPART"));pwm=order.pwm;
      }
    }
    for(auto count:stops)assert(count==3);
  }
  std::cout<<"position station checks passed ("<<positions<<" entry positions, "
           <<resumes<<" resumes, 342 route positions, 6 policy laps / 24 stops, watchdog/visit/consensus checks)\n";
}
