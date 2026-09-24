// Behavioral baseline for the operator's decisional-inertia clarification.
// These tests protect existing continuity behavior; they do not claim that
// PROXIMAL_R1 implements the full new evidentiary policy.
#include "Navigator.h"
#include <cassert>
#include <cstdio>
using namespace navi_one;

struct Rig {
  Navigator nav;
  ngr_nav::IrOdometryEpoch odo;
  uint32_t ms=1000;
  uint64_t distance=0;
  uint8_t actual;
  int8_t dir;
  Rig(uint8_t start,int8_t direction):actual(start),dir(direction) {
    nav.declare(start,dir,ms);assert(nav.takeResetRequest());
  }
  NavObservation next(unsigned steps=1,bool wrongPole=false,bool available=true) {
    for(unsigned i=0;i<steps;++i){distance+=spanMm(actual,dir);actual=nextMarker(actual,dir);}
    ms+=1000;
    NavObservation o;o.openedAtMs=ms;o.polarity=polarityAt(actual)^wrongPole;
    if(available) {
      o.movement.issue=ngr_nav::MotionIssue::None;o.movement.frame=1;
      auto& w=o.movement.wire;w.bootId=7;w.pitchUm=1000;w.capturedUs=uint64_t(ms)*1000;
      w.completedPulses=w.observedRises=distance;w.nominalUm=distance*1000;
      w.opticalReason=ir_movement::TRACKING;
      odo.ingest(w);o.odometry=odo.point();
    }
    return o;
  }
  void confirm(unsigned n=12) {
    for(unsigned i=0;i<n;++i) {nav.judge(next());assert(nav.positionKnown() && nav.status().navMm==actual);}
  }
};

int main() {
  unsigned scenarios=0;
  for(int dir=-1;dir<=1;dir+=2)for(unsigned start=0;start<ROUTE_N;++start) {
    Rig r{uint8_t(start),int8_t(dir)};r.confirm();
    const auto history=r.nav.recovery().count();
    r.odo.endEpoch(ngr_nav::IrEpochBreak::InstrumentUnavailable);
    // Instrument continuity ends, not NAVI's accumulated trajectory.
    assert(r.nav.positionKnown() && r.nav.status().navMm==r.actual);
    assert(r.nav.recovery().count()==history && !r.nav.takeResetRequest());
    for(unsigned i=0;i<3;++i) {
      const auto ruling=r.nav.judge(r.next(1,i==0,false));
      assert(ruling==(i==0?Ruling::AdvancedWithDiscrepancy:Ruling::Advanced));
      assert(r.nav.positionKnown() && r.nav.status().navMm==r.actual);
      assert(!r.nav.status().corrections && !r.nav.takeResetRequest());
    }
    NavObservation extra;extra.openedAtMs=r.ms+100;extra.polarity=polarityAt(r.actual);
    assert(r.nav.judge(extra)==Ruling::NonLandmark);
    assert(r.nav.status().navMm==r.actual && r.nav.positionKnown());
    assert(r.nav.recovery().count()==history);
    r.confirm(2); // Fresh IR begins a new epoch without relocating NAVI.
    auto missed=r.next(2);
    assert(r.nav.judge(missed)==Ruling::MissedAndAdvanced);
    assert(r.nav.status().navMm==r.actual && r.nav.status().missedSinceLast==1);
    assert(!r.nav.status().corrections && r.nav.positionKnown());
    const auto n=r.nav.recovery().count();
    assert(!r.nav.recovery().entry(n-2).observed);
    ++scenarios;
  }
  std::printf("PASS %u route/direction scenarios: history survives IR loss, isolated wrong polarity, early Hall noise, reacquisition and missed landmark; UNKNOWN retained\n",scenarios);
}
