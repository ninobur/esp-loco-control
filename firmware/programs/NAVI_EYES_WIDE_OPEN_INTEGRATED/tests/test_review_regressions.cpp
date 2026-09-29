#include "../NaviIntegratedCore.h"
#include "../NaviCompatibility.h"
#include "../NaviEstop.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <deque>
using namespace navi_eyes;

static ir_movement::WireSnapshot wire(uint32_t seq, uint64_t pulses, uint64_t boot=42,
                                      uint32_t pitch=1000) {
  ir_movement::WireSnapshot w;
  w.sequence=seq; w.bootId=boot; w.capturedUs=uint64_t(seq)*10000;
  w.completedPulses=w.observedRises=pulses; w.pitchUm=pitch; w.nominalUm=pulses*pitch;
  w.opticalReason=ir_movement::TRACKING;
  return w;
}
static HallSample h(uint32_t serial, uint64_t us, int raw, uint8_t pwm=40) {
  HallSample s; s.sampleSerial=serial; s.timestampUs=us; s.raw=raw; s.pwm=pwm; s.direction=1;
  return s;
}
static void boot(NaviIntegratedCore& n) {
  n.observeHall(h(1,110000,115));
  n.observeIr(wire(1,0),120000,40);
  n.observeIr(wire(2,10),220000,40);
  assert(n.initialReferenceReady() && n.activeReference()==115);
}
static void first(NaviIntegratedCore& n) {
  boot(n); n.declare(0,1,500000);
  n.observeIr(wire(8,340),600000,40);
  for(unsigned i=0;i<5;++i) n.observeHall(h(100+i,610000+i*1000,190));
  assert(n.confirmedCount()==1 && n.spatialPhase()==1);
}
static void b1() {
  NaviIntegratedCore n; boot(n); n.declare(0,1,500000);
  n.observeIr(wire(8,50),1200000,40);
  for(unsigned i=0;i<5;++i) n.observeHall(h(300+i,1190000+i*1000,190),1201000+i*1000);
  assert(n.irApplicable(1205000) && n.confirmedCount()==0 && !n.degraded());
  n.observeIr(wire(9,340),1250000,40);
  n.observeHall(h(305,1240000,190),1251000);
  assert(n.confirmedCount()==1 && !n.degraded());
  EwoEvent e; bool found=false;
  while(n.takeEvent(e)) if(e.kind==EwoEventKind::TargetConfirmed) {
    found=true; assert(e.timestampUs==1251000 && e.consumptionId==n.consumptionId() && e.irSequence==9);
  }
  assert(found);
  NaviIntegratedCore declared; boot(declared); declared.declare(0,1,1250000);
  declared.observeIr(wire(8,340),1260000,40);
  for(unsigned i=0;i<5;++i)
    declared.observeHall(h(500+i,1240000+i*1000,190),1261000+i*1000);
  assert(declared.confirmedCount()==0); // acquisition predates new context
}
static void bootReferenceIsImmediate() {
  NaviIntegratedCore n;
  assert(!n.initialReferenceReady());
  n.observeHall(h(1,100000,222));
  assert(n.initialReferenceReady() && n.activeReference()==222);
  n.observeIr(wire(1,0),110000,0);
  assert(n.initialReferenceReady() && n.activeReference()==222);
}
static void frames() {
  for (unsigned scenario=0;scenario<4;++scenario) {
    NaviIntegratedCore n; first(n);
    if(scenario==1) {
      n.observeIr(wire(9,450),700000,40);
      n.observeHall(h(200,710000,900)); // must not survive invalidation
      assert(n.spatialPhase()==2);
    }
    if(scenario<2) n.observeIr(wire(10,0,43),800000,40);
    else if(scenario==2) n.observeIr(wire(10,450),800000,0);
    else {
      const uint8_t a[6]={1}, b[6]={2};
      n.observeIr(wire(9,350),700000,40,a);
      n.observeIr(wire(10,350),800000,40,b);
    }
    assert(!n.relationshipReliable() && n.spatialPhase()==0 && n.activeReference()==115);
    const uint8_t b[6]={2};
    n.observeIr(wire(11,scenario<2?1:451,scenario<2?43:42),900000,40,scenario==3?b:nullptr);
    for(unsigned i=0;i<5;++i) n.observeHall(h(400+i,1300000+i*1000,0));
    assert(n.confirmedCount()==2 && n.mm()==2 && n.relationshipReliable() && n.spatialPhase()==1);
    assert(n.activeReference()==115);
    EwoEvent e; bool reanchored=false;
    while(n.takeEvent(e)) if(e.kind==EwoEventKind::Reanchored) {
      reanchored=true; assert(e.timestampUs==1304000);
    }
    assert(reanchored);
  }
}
static void loss() {
  NaviIntegratedCore n; boot(n); n.declare(0,1,500000);
  n.noteObservationLoss(0,1,600000);
  n.observeIr(wire(8,20),700000,40);
  for(unsigned i=0;i<5;++i) n.observeHall(h(500+i,1200000+i*1000,190));
  assert(n.relationshipReliable());
  n.noteObservationLoss(1,1,1210000);
  assert(n.relationshipReliable() && n.irApplicable(1210000));
  n.noteObservationLoss(1,2,1220000);
  assert(!n.relationshipReliable() && n.spatialPhase()==0);
}
static void estop() {
  OrderedEstop latch;
  std::deque<uint64_t> full;
  for(unsigned i=0;i<16;++i) full.push_back(latch.received(false));
  const uint64_t droppedStop=latch.received(true); // no queue room
  assert(latch.asserted()); // immediate withdrawal does not need dequeue
  while(!full.empty()) { assert(latch.apply(full.front(),false)); full.pop_front(); }
  assert(latch.asserted());
  const uint64_t newerRelease=latch.received(false);
  assert(!latch.apply(newerRelease,false));
  assert(!latch.apply(droppedStop,true)); // reverse processing order
  const uint64_t newestStop=latch.received(true);
  assert(latch.apply(newerRelease,false));
  assert(latch.apply(newestStop,true));
  assert(!latch.apply(latch.received(false),false));
}
int main(int argc,char**) {
  if(argc>1) {
    NaviIntegratedCore n; char out[384];
    formatConsoleNav(out,sizeof(out),n,0); puts(out);
    boot(n); n.declare(12,1,500000);
    formatConsoleNav(out,sizeof(out),n,1); puts(out);
    n.observeIr(wire(8,20),600000,40);
    formatConsoleIr(out,sizeof(out),n,600000,true); puts(out);
    auto stopped=wire(9,20); stopped.opticalReason=ir_movement::INADEQUATE_CONTRAST;
    n.observeIr(stopped,700000,0);
    formatConsoleIr(out,sizeof(out),n,700000,true); puts(out);
    formatConsoleIr(out,sizeof(out),n,1700001,true); puts(out);
    return 0;
  }
  b1(); bootReferenceIsImmediate(); frames(); loss(); estop();
  puts("PASS: B1/B2, provisional boot reference, frame/re-anchor, causal time, cumulative loss");
}
