// Read-only review probe against NAVI_EWO_0_2 (cbdfe94). Build: c++ -std=c++17 -I<checkout of cbdfe94>/firmware/programs/NAVI_EWO_0_2 ewo_0_2_probe.cpp
#include "NaviIntegratedCore.h"
#include <cstdio>
using namespace navi_eyes;
static uint32_t seq=0, hs=0; static uint64_t pc=0;
static const uint8_t mac[6]={1,2,3,4,5,6};
static ir_movement::WireSnapshot ir(uint64_t pulses, uint64_t us){
  ir_movement::WireSnapshot w{}; w.magic=0x4952; w.version=1; w.type=5; w.bootId=7;
  w.sequence=++seq; w.capturedUs=us; w.completedPulses=pulses; w.observedRises=pulses;
  w.pitchUm=ir_movement::kInstalledPitchUm; w.nominalUm=pulses*uint64_t(w.pitchUm); return w;}
static HallSample hall(uint64_t us,int16_t raw){HallSample h{};h.sampleSerial=++hs;h.timestampUs=us;h.raw=raw;h.pwm=90;h.direction=1;return h;}
struct Sim { NaviIntegratedCore n; uint64_t t=1000; uint64_t irmm=0;
  void irTo(uint64_t mm,uint64_t dtPer10=40000){ // advance IR in 10 mm (1 pulse) steps
    while(irmm<mm){ irmm+=10; t+=dtPer10; n.observeIr(ir(irmm/10,t),t+500,90); n.observeHall(hall(t+600,2000),t+600);} }
  void questionable(){ uint64_t b=t; const uint64_t iv[]={20000,20000,40000,20000,40000,20000};
    for(auto d:iv){ b+=d; n.observePulse(b,++pc,7,mac,90,b);} }
  void field(int16_t raw,int samples=4){ t+=700; for(int i=0;i<samples;i++){ t+=200; n.observeHall(hall(t,raw),t);} for(int i=0;i<3;i++){t+=200;n.observeHall(hall(t,2000),t);} }
  void drain(const char* tag){ EwoEvent e; while(n.takeEvent(e)) if(e.kind==EwoEventKind::HallEncounter||e.kind==EwoEventKind::TargetConfirmed||e.kind==EwoEventKind::MissedMagnet||e.kind==EwoEventKind::DnaPending||e.kind==EwoEventKind::DnaResolved||e.kind==EwoEventKind::DnaUnresolvedStop) printf("  [%s] ev=%d mm=%u target=%u\n",tag,int(e.kind),e.mm,e.target);
    printf("  [%s] => mm=%u target=%u confirmed=%u missed=%u reliable=%d withdraw=%d irQ=%d\n",tag,n.mm(),n.target().sequence,n.confirmedCount(),n.missedCount(),n.positionReliable(),n.autoWithdrawalRequired(),n.irQuestionable()); }
};
int main(){
  const int16_t N=2200, S=1800;   // Above / Below reference 2000
  puts("P1: declaration mid-interval (25->26 CW), first magnet MM026 after 106 mm");
  { Sim s; s.n.observeHall(hall(s.t,2000),s.t); s.n.observeIr(ir(0,s.t),s.t+500,90); s.t+=1000; s.n.declare(25,1,s.t);
    s.irTo(106); s.field(S); s.drain("026@106");
    s.irTo(406); s.field(S); s.drain("027@406"); s.irTo(706); s.field(N); s.drain("028@706");
    s.irTo(1006); s.field(N); s.drain("029@1006"); s.irTo(1321); s.field(N); s.drain("030@1321"); }
  puts("P2: anchored at MM026; IR questionable and undercounting; physical 027(S) then 028(N)");
  { Sim s; s.n.observeHall(hall(s.t,2000),s.t); s.n.observeIr(ir(0,s.t),s.t+500,90); s.t+=1000; s.n.declare(25,1,s.t);
    s.irTo(300); s.field(S); s.drain("026 confirm");
    s.irTo(500,48000); s.questionable(); s.field(S); s.drain("phys027@IR+200");
    s.irTo(740,48000); s.questionable(); s.field(N); s.drain("phys028@IR+440");
    s.irTo(1040); s.field(N); s.drain("phys029 credible +300");
    s.irTo(1355); s.field(N); s.drain("phys030 credible +315");
    s.irTo(1355+320); s.field(N); s.drain("phys031");
    s.irTo(1675+315); s.field(N); s.drain("phys032");
    s.irTo(1990+310); s.field(S); s.drain("phys033 S");
    s.irTo(2300+300); s.field(N); s.drain("phys034 N");
    s.irTo(2600+300); s.field(N); s.drain("phys035 N");
    s.irTo(2900+300); s.field(S); s.drain("phys036 S");
    s.irTo(3200+300); s.field(S); s.drain("phys037 S");
    s.irTo(3500+300); s.field(S); s.drain("phys038 S");
    s.irTo(3800+300); s.field(S); s.drain("phys039 S");
    s.irTo(4100+300); s.field(S); s.drain("phys040 S");
    s.irTo(4400+300); s.field(S); s.drain("phys041 S");
    s.irTo(4700+300); s.field(N); s.drain("phys042 N");
  }
  puts("P3: one physical field with threshold chatter (raw alternating 2071/2069 x6)");
  { Sim s; s.n.observeHall(hall(s.t,2000),s.t); s.n.observeIr(ir(0,s.t),s.t+500,90); s.t+=1000; s.n.declare(25,1,s.t);
    s.irTo(300); s.field(S); s.drain("026");
    s.irTo(500); s.t+=700; for(int k=0;k<6;k++){ s.t+=200; s.n.observeHall(hall(s.t,2071),s.t); s.t+=200; s.n.observeHall(hall(s.t,2072),s.t); s.t+=200; s.n.observeHall(hall(s.t,2069),s.t);} 
    s.drain("chatter"); }
  return 0; }
