#include <algorithm>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
// Synthetic coordinate fixtures use the real implementation's internal state;
// no second comparator is implemented and no production test hook is added.
#define private public
#include "../NaviIntegratedCore.h"
#undef private
#include "../NaviStopOverlay.h"
#include "../NaviDartTelemetry.h"
using namespace navi_eyes;
static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); std::exit(1); } }while(0)
struct Fixture {
  NaviIntegratedCore n; uint64_t t=10000; uint32_t serial=0,seq=0; uint8_t pwm=40,dir=1;
  void ir(uint64_t pulses) { ir_movement::WireSnapshot w{}; w.bootId=1;w.sequence=++seq;
    w.capturedUs=t+=1000;w.completedPulses=w.observedRises=pulses;w.nominalUm=pulses*w.pitchUm;
    w.opticalReason=ir_movement::TRACKING;n.observeIr(w,t,pwm); }
  void tick(int raw) {t+=1000;for(int i=0;i<5;++i){HallSample s{};s.sampleSerial=++serial;
    s.timestampUs=t+i;s.raw=raw;s.pwm=pwm;s.direction=dir;n.observeHall(s,t+i);}t+=5;}
  void field(int pole){tick(2000);tick(2000+pole*100);tick(2000+pole*100);}
  void start(uint8_t mm=16,int direction=1){dir=direction>0?1:2;tick(2000);ir(0);n.declare(mm,direction,t);field(n.target().polarity==HallOpeningPolarity::AboveReference?1:-1);CHECK(n.confirmedCount()==1);}
  void drain(){DartRecord r;while(n.takeDartRecord(r)){};EwoEvent e;while(n.takeEvent(e)){};}
  void complete(){n.completeComparison(n.processedHallUs(),t);}
};
static void syntheticSeed(Fixture& f,uint64_t d=300000) {
  f.start(); f.n.expectedCumulativeUm_=d;f.n.spatialPhase_=NaviIntegratedCore::SpatialPhase::None;
}
static void syntheticOpening(Fixture& f,uint64_t x,int pole=-1) {
  DartOpening o{};o.id=++f.n.nextOpeningId_;o.serial=++f.serial;o.timeUs=f.t+=1000;
  o.pole=pole;o.pwm=40;o.direction=1;o.ir=f.n.irAt(f.t);o.ir.um=x;
  o.ir.applicable=true;f.n.beginDecision(f.t);f.n.pending_[f.n.pendingCount_++]=o;f.n.evaluateOpening(o);
}
int main(){
 {Fixture f;syntheticSeed(f);syntheticOpening(f,250000);CHECK(f.n.candidateBoundaryUm()==350000);
  syntheticOpening(f,280000);CHECK(f.n.candidateBoundaryUm()==320000);
  syntheticOpening(f,295000);CHECK(f.n.candidateBoundaryUm()==305000);
  syntheticOpening(f,303000);CHECK(f.n.confirmedCount()==2);CHECK(f.n.openingIrUm()==303000);CHECK(f.n.anchorMismatchCount()==0);}
 {Fixture f;syntheticSeed(f);syntheticOpening(f,290000);syntheticOpening(f,310000);
  CHECK(f.n.confirmedCount()==2);CHECK(f.n.openingIrUm()==300000);CHECK(f.n.inferredAnchor());}
 {Fixture f;syntheticSeed(f);syntheticOpening(f,299000);CHECK(f.n.candidateBoundaryUm()==301000);CHECK(f.n.confirmedCount()==1);}
 {Fixture f;f.start();f.ir(26);f.field(-1);CHECK(f.n.candidateAvailable());CHECK(f.n.confirmedCount()==1);
  const uint64_t selected=26*ir_movement::kInstalledPitchUm;CHECK(f.n.candidateCoordinateUm()==selected);
  f.ir(37);f.tick(2000);f.complete();CHECK(f.n.confirmedCount()==2);CHECK(f.n.openingIrUm()==selected);
  CHECK(f.n.physicalOriginUm_==selected);CHECK(f.n.latestIr().nominalUm!=selected);}
 // MM017: native synthetic ADC preserving measured event polarity/count facts.
 for(uint64_t delay:{1028000ULL,1179000ULL}){Fixture f;f.start();f.t+=402000;f.ir(1);f.field(1);
  f.t+=delay-402000;f.ir(3);f.field(1);f.complete();CHECK(f.n.mm()==17);CHECK(f.n.confirmedCount()==1);
  CHECK(f.n.openingCount()==3);f.ir(100);f.tick(2000);f.complete();CHECK(f.n.mm()==17);CHECK(f.n.missedCount()==0);}
 {Fixture f;f.start();f.ir(31);f.field(-1);CHECK(f.n.confirmedCount()==1); // early, boundary not yet reached
  f.ir(32);f.field(-1);CHECK(f.n.confirmedCount()==2);auto count=f.n.confirmedCount();
  f.tick(1900);f.tick(1900);CHECK(f.n.confirmedCount()==count); // sustained support, no repeated event
  f.field(-1);f.complete();CHECK(f.n.confirmedCount()==count);}
 // Freshest receiver-available snapshot, not newest at processing time.
 {Fixture f;f.start();f.ir(26);uint64_t openingTime=f.t+3000;f.t=openingTime+10000;f.ir(35);
  uint64_t judgment=f.t; f.t=openingTime-4000;
  for(int raw:{2000,1900,1900}) {f.t+=1000;for(int i=0;i<5;++i){HallSample s{};s.sampleSerial=++f.serial;s.timestampUs=f.t+i;s.raw=raw;s.pwm=40;s.direction=1;f.n.observeHall(s,judgment);}}
  CHECK(f.n.candidateCoordinateUm()==26*ir_movement::kInstalledPitchUm);}
 {Fixture f;f.start();f.t+=2000000;f.field(-1);f.complete();CHECK(f.n.mm()==17);CHECK(!f.n.candidateAvailable());}
 {Fixture f;f.tick(2000);f.n.declare(16,1,f.t);f.field(1);CHECK(f.n.mm()==17);CHECK(!f.n.targetOriginValid_);f.field(-1);CHECK(f.n.mm()==17);}
 {Fixture f;f.start();auto w=f.n.latestIr();w.sequence=1;f.n.observeIr(w,++f.t,40);f.field(-1);f.complete();CHECK(f.n.mm()==17);CHECK(!f.n.relationshipReliable());}
 {Fixture f;f.start();f.pwm=0;f.field(-1);CHECK(f.n.mm()==17);f.pwm=40;f.ir(32);f.field(-1);CHECK(f.n.mm()==18);}
 {Fixture f;f.start();f.pwm=0;f.ir(1);CHECK(f.n.pwmZeroMovementRequiresDeclaration());f.pwm=40;f.field(-1);CHECK(f.n.mm()==17);}
 {Fixture f;f.start(170);CHECK(f.n.mm()==0);CHECK(f.n.target().sequence==1);}
 {Fixture f;f.start(1,-1);CHECK(f.n.mm()==0);CHECK(f.n.target().sequence==170);}
 {Fixture f;f.start();f.ir(10);f.n.reverse(-1,f.t);CHECK(f.n.direction()==-1);CHECK(!f.n.candidateAvailable());CHECK(f.n.target().sequence==17);}
 {Fixture f;f.start();f.t+=2000000;const auto v=stopView(f.n,stopMarkerUm(45)+150000,f.t);
  CHECK(v.identityAvailable);CHECK(v.identifiedMm==17);CHECK(!v.available);CHECK(!v.fine);
  NaviStopOverlay overlay;StopGeography empty;auto cmd=overlay.command(empty,false,1,90,45,45,0,true);CHECK(cmd.pwm==45);}
 // Diagnostic backpressure never changes geographic results.
 {Fixture a,b;a.start();b.start();for(int i=0;i<16;++i){a.ir(25+i*2);b.ir(25+i*2);a.field(-1);b.field(-1);a.drain();}
  a.complete();b.complete();CHECK(a.n.mm()==b.n.mm());CHECK(a.n.openingIrUm()==b.n.openingIrUm());CHECK(b.n.diagnosticDrops()>0);}
 {Fixture f;f.start();f.n.noteObservationLoss(1,0,++f.t);f.ir(32);f.field(-1);f.complete();CHECK(f.n.mm()==17);CHECK(f.n.comparisonIncomplete());}
 {Fixture f;f.start();f.ir(26);f.field(-1);f.n.noteObservationLoss(0,1,++f.t);f.ir(37);f.tick(2000);f.complete();CHECK(f.n.mm()==18);}
 {Fixture f;f.start();auto start=std::chrono::steady_clock::now();for(int i=0;i<10000;++i)f.tick(2000);
  auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
  std::printf("host_native_samples=50000 elapsed_ns=%lld core_bytes=%zu opening_bytes=%zu diagnostic_bytes=%zu\n",(long long)ns,sizeof(f.n),sizeof(DartOpening),sizeof(DartRecord));}
 DartRecord r{};char json[1200];CHECK(formatDart(json,sizeof(json),r,1)>0);CHECK(std::strlen(json)<sizeof(json));
 std::printf("PASS %u assertions\n",checks); return 0;
}
