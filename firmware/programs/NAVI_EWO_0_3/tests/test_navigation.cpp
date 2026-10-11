#include "../NaviIntegratedCore.h"
#include "../NaviStopOverlay.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace navi_eyes;
using ngr_evidence::Polarity;
struct Run {
  NaviIntegratedCore n;
  uint64_t now=100000,um=100*9652ULL;
  uint32_t serial=0,seq=0;
  int8_t dir=1;
  Run(uint8_t mm=27,int8_t direction=1,bool withIr=true):dir(direction) {
    sample(1000);
    if(withIr)ir(um);
    n.declare(mm,dir,now+=1000); neutral();
  }
  void sample(int raw,uint8_t pwm=90,uint64_t acquired=0) {
    HallSample h{};h.sampleSerial=++serial;h.timestampUs=acquired?acquired:now+1000;
    h.raw=raw;h.pwm=pwm;h.direction=dir>0?1:2;
    n.observeHall(h,now+=1000);
  }
  void neutral(){sample(n.activeReference());sample(n.activeReference());}
  void ir(uint64_t distance,uint8_t pwm=90) {
    ir_movement::WireSnapshot w{};w.bootId=42;w.sequence=++seq;w.capturedUs=now;
    w.completedPulses=w.observedRises=distance/w.pitchUm;
    w.nominalUm=w.completedPulses*w.pitchUm;w.opticalReason=ir_movement::TRACKING;
    um=w.nominalUm;n.observeIr(w,now,pwm);
  }
  void wait(uint64_t us=700000){now+=us;}
  void field(uint8_t physicalMm,uint64_t distance=300000,bool withIr=true) {
    neutral();wait();if(withIr)ir(um+distance);
    const int raw=n.activeReference()+(navi_one::polarityAt(physicalMm)?70:-70);
    sample(raw);sample(raw);
  }
  void pulse(uint64_t count,uint64_t time,uint8_t pwm=90,bool valid=true) {
    navi_pulse::State s{};s.have=true;s.eventValid=valid;s.receivedUs=now;
    s.event.bootId=42;s.event.completedPulses=count;s.event.completedUs=time;
    n.observePulse(s,pwm,dir);
  }
};
static void thresholdAndBoundary() {
  Run r;r.wait();r.ir(r.um+106000);
  r.sample(1069);r.sample(1000);assert(r.n.confirmedCount()==0);
  const uint32_t first=r.serial+1;
  const uint64_t opening=r.um;
  r.sample(1070);assert(r.n.confirmedCount()==0);
  r.now+=500;r.ir(r.um+9652); // packet between opening and detection
  r.sample(1100);
  assert(r.n.confirmedCount()==1&&r.n.mm()==28);
  assert(r.n.openingSerial()==first&&r.n.openingIrUm()==opening);
  assert(r.n.dnaHistory().at(0).encounter.openedUs<r.n.dnaHistory().at(0).encounter.detectedUs);
  // Queue consumption after a later packet cannot substitute its coordinate.
  Run q;q.wait();q.ir(q.um+300000);
  const uint64_t boundary=q.um,acquired=q.now+1000;
  q.now+=10000;q.ir(q.um+100000);
  q.sample(1100,90,acquired);q.sample(1100,90,acquired+1000);
  assert(q.n.confirmedCount()==1&&q.n.openingIrUm()==boundary);
  assert(q.n.latestIr().nominalUm>q.n.openingIrUm());
  Run discontinuous;discontinuous.wait();discontinuous.ir(discontinuous.um+300000);
  discontinuous.sample(1100);discontinuous.serial+=1;discontinuous.sample(1100);
  assert(discontinuous.n.confirmedCount()==0);
  Run gap;gap.wait();gap.ir(gap.um+300000);gap.sample(1100);
  gap.now+=2000;gap.sample(1100);assert(gap.n.confirmedCount()==0);
}
static void duplicatesAndSpatial() {
  Run r(28);r.field(29);assert(r.n.mm()==29);
  for(unsigned i=1;i<=7;++i)r.pulse(i,i*60000ULL);
  const unsigned history=r.n.dnaHistory().size();
  r.neutral();r.wait(1000000);r.ir(r.um+10000);
  r.sample(1100);r.sample(1100);
  assert(r.n.confirmedCount()==1&&r.n.dnaHistory().size()==history);
  r.wait();r.sample(1100);r.sample(1100);assert(r.n.confirmedCount()==1);
  // spatial clearance and collection cannot hide a genuine next magnet.
  r.neutral();r.wait();r.ir(r.um+290000);r.sample(1100);r.sample(1100);
  assert(r.n.mm()==30&&r.n.confirmedCount()==2);
  Run collecting;collecting.field(28);collecting.neutral();collecting.wait();
  collecting.ir(collecting.um+130000);collecting.neutral();
  assert(collecting.n.spatialPhase()==2);
  collecting.sample(1100);collecting.sample(1100);
  assert(collecting.n.mm()==29&&collecting.n.confirmedCount()==2);
  Run noIr(27,1,false);noIr.field(28,0,false);assert(noIr.n.mm()==28);
  noIr.neutral();noIr.wait(100000);noIr.sample(1100);noIr.sample(1100);
  assert(noIr.n.confirmedCount()==1);
  noIr.neutral();noIr.wait();noIr.sample(1100);noIr.sample(1100);
  assert(noIr.n.mm()==29&&noIr.n.confirmedCount()==2);
}
static void sequentialAndUndercount() {
  Run r;
  for(uint8_t mm=28;mm<=48;++mm) {
    r.field(mm,200000);
    assert(r.n.mm()==mm&&r.n.target().sequence==uint8_t(mm+1));
    assert(r.n.positionReliable());
    // Drain bounded diagnostic output as the operational shell does.
    EwoEvent event;while(r.n.takeEvent(event)){}
  }
  assert(r.n.confirmedCount()==21&&r.n.dnaChecks()==21);
  assert(r.n.dnaHistory().size()==12&&r.n.dnaCheck().compared==12);
  assert(r.n.dnaCheck().contradictions==0);
  assert(r.n.missedCount()==0); // cumulative IR never assigns MM alone
}
static void irCredibility() {
  Run r;r.field(28);
  for(unsigned i=1;i<=7;++i)r.pulse(i,i*60000ULL);
  assert(r.n.irMagnetVetoAvailable());
  r.pulse(8,600000);assert(!r.n.irMagnetVetoAvailable());
  r.pulse(9,660000);
  assert(r.n.pulseAssessment().questionable);
  const uint64_t count=r.n.latestIr().completedPulses;
  r.field(29,200000); // lost veto cannot block the true expected marker
  assert(r.n.mm()==29&&r.n.openingIrUm()==r.um);
  assert(r.n.latestIr().completedPulses>=count); // counter was never rewritten
  r.pulse(10,720000);assert(r.n.irMagnetVetoAvailable());
  // A telemetry record gap is missing observability, not a missing physical pulse.
  r.pulse(12,840000,90,false);assert(!r.n.irMagnetVetoAvailable());
  for(unsigned i=13;i<=20;++i)r.pulse(i,840000+(i-12)*60000ULL);
  assert(r.n.irMagnetVetoAvailable());
  // Acceleration changes measured PWM: do not label its interval skipped.
  r.pulse(21,1500000,35);assert(!r.n.pulseAssessment().questionable);
  assert(!r.n.irMagnetVetoAvailable());
  for(unsigned i=22;i<=27;++i)r.pulse(i,1500000+(i-21)*60000ULL,35);
  assert(r.n.irMagnetVetoAvailable());
  r.n.reverse(-1,r.now+=1000);
  assert(!r.n.irMagnetVetoAvailable());
  // Ordinary credible distance agreement still exercises DNA at every marker.
  Run credible;credible.field(28);
  for(unsigned i=1;i<=7;++i)credible.pulse(i,i*60000ULL);
  assert(credible.n.irMagnetVetoAvailable());
  credible.field(29,uint64_t(navi_one::spanMm(28,1))*1000);
  assert(credible.n.mm()==29&&credible.n.positionReliable());
  assert(credible.n.dnaChecks()==2&&credible.n.dnaCheck().matched==2);
}
static void contradictionAndRetrospection() {
  // Real MM028 wrong polarity: retain the encounter, then ten subsequent
  // map-specific poles justify identifying the current physical marker.
  Run r;
  r.field(27); // S instead of expected N at MM028
  assert(!r.n.positionReliable()&&r.n.mm()==27&&r.n.dnaHistory().size()==1);
  assert(r.n.dnaHistory().at(0).encounter.polarity==Polarity::South);
  for(uint8_t mm=29;mm<=38;++mm)r.field(mm);
  assert(r.n.positionReliable()&&r.n.mm()==38&&r.n.geographicRevision()==1);
  assert(r.n.dnaHistory().size()==11&&!r.n.dnaHistory().at(0).identified);
  assert(r.n.dnaCheck().matched==10&&r.n.openingIrUm()==r.um);
  r.field(39);assert(r.n.mm()==39&&r.n.dnaHistory().size()==12);
  // Leading magnetic artifact: next ten actual sequential magnets resolve
  // from exactly the same incumbent journey, not from a route-wide lookup.
  Run extra;extra.field(27);
  for(uint8_t mm=28;mm<=37;++mm)extra.field(mm);
  assert(extra.n.positionReliable()&&extra.n.mm()==37);
  // An unrelated valid DNA sequence cannot teleport this journey.
  Run remote;remote.field(27);
  for(uint8_t mm=100;mm<=111;++mm)remote.field(mm);
  assert(!remote.n.positionReliable()&&remote.n.mm()==27&&remote.n.geographicRevision()==0);
}
static void declarationsAndReversal() {
  Run unanchored(45,1);
  unanchored.n.reverse(-1,unanchored.now+=1000);unanchored.dir=-1;
  assert(unanchored.n.target().sequence==45);
  unanchored.n.reverse(1,unanchored.now+=1000);unanchored.dir=1;
  assert(unanchored.n.target().sequence==46);
  unanchored.neutral();unanchored.field(46,106000);
  assert(unanchored.n.mm()==46&&unanchored.n.confirmedCount()==1);
  for(int8_t direction:{int8_t(1),int8_t(-1)}) {
    Run r(45,direction);
    r.field(direction>0?46:44,106000);
    assert(r.n.confirmedCount()==1&&r.n.mm()==(direction>0?46:44));
    const size_t oldHistory=r.n.dnaHistory().size();
    r.neutral();r.wait();r.ir(r.um+150000);
    r.dir=-direction;r.n.reverse(r.dir,r.now);
    assert(r.n.dnaHistory().size()==oldHistory);
    assert(r.n.target().sequence==r.n.mm());
    r.field(r.n.mm(),150000);
    assert(r.n.confirmedCount()==2&&r.n.mm()==(direction>0?46:44));
    assert(r.n.target().sequence==45&&r.n.dnaHistory().size()==2);
  }
  Run noIr(27,1,false);noIr.field(28,0,false);noIr.neutral();noIr.wait();
  noIr.dir=-1;noIr.n.reverse(-1,noIr.now);
  assert(noIr.n.target().sequence==28&&noIr.n.dnaHistory().size()==1);
  noIr.field(28,0,false);
  assert(noIr.n.mm()==28&&noIr.n.confirmedCount()==2&&noIr.n.target().sequence==27);
  Run inside;inside.field(28);inside.dir=-1;inside.n.reverse(-1,inside.now+=1000);
  assert(inside.n.target().sequence==27); // ongoing field cannot be a new return
  inside.field(27);assert(inside.n.mm()==27&&inside.n.confirmedCount()==2);
  Run r;r.field(28);r.n.declare(40,1,r.now+=1000);r.neutral();r.field(41,100000);
  assert(r.n.mm()==41&&r.n.dnaHistory().size()==1);
  // Sustained-field/no-motion failure: time alone never retriggers a field.
  Run stationary(13,-1);stationary.sample(1100);stationary.sample(1100);
  for(unsigned i=0;i<10;++i){stationary.wait();stationary.ir(stationary.um);stationary.sample(1100);stationary.sample(1100);}
  assert(stationary.n.mm()==13&&stationary.n.confirmedCount()==0);
}
static void mapDnaAndRearmedDeclaration() {
  // The retained ten-marker rule is grounded in the actual active map.
  for(int8_t direction:{int8_t(-1),int8_t(1)}) {
    for(unsigned a=0;a<navi_one::ROUTE_N;++a)for(unsigned b=a+1;b<navi_one::ROUTE_N;++b) {
      uint8_t ma=a,mb=b;bool differs=false;
      for(unsigned i=0;i<10;++i) {
        differs|=navi_one::polarityAt(ma)!=navi_one::polarityAt(mb);
        ma=navi_one::nextMarker(ma,direction);mb=navi_one::nextMarker(mb,direction);
      }
      assert(differs);
    }
  }
  Run r;
  for(unsigned episode=0;episode<2;++episode) {
    if(episode){r.n.declare(27,1,r.now+=1000);r.neutral();}
    r.field(27);
    assert(!r.n.positionReliable()&&r.n.geographicJudgmentPending());
    assert(r.n.dnaHistory().size()==1);
    for(uint8_t mm=29;mm<=38;++mm)r.field(mm);
    assert(r.n.positionReliable()&&r.n.mm()==38&&!r.n.geographicJudgmentPending());
  }
}
int main(){mapDnaAndRearmedDeclaration();thresholdAndBoundary();duplicatesAndSpatial();sequentialAndUndercount();irCredibility();contradictionAndRetrospection();declarationsAndReversal();std::puts("PASS: 0.3 integrated navigation acceptance");}
