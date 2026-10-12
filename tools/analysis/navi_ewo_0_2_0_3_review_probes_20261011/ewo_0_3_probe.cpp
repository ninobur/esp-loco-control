// Read-only review probe against NAVI_EWO_0_3 (16c65f6). Build: c++ -std=c++17 -I<checkout of 16c65f6>/firmware/programs/NAVI_EWO_0_3 ewo_0_3_probe.cpp
#include "NaviIntegratedCore.h"
#include <cstdio>
using namespace navi_eyes;
struct Sim {
  NaviIntegratedCore n; uint64_t t=1000000; uint32_t hs=0, seq=0; uint64_t pulses=0;
  ir_movement::WireSnapshot ir(){ ir_movement::WireSnapshot w{}; w.magic=0x4952; w.version=1; w.type=5; w.bootId=7;
    w.sequence=++seq; w.capturedUs=t; w.completedPulses=pulses; w.observedRises=pulses;
    w.pitchUm=ir_movement::kInstalledPitchUm; w.nominalUm=pulses*uint64_t(w.pitchUm); return w; }
  void sample(int16_t raw){ HallSample h{}; h.sampleSerial=++hs; h.timestampUs=t; h.raw=raw; h.pwm=90; h.direction=1; n.observeHall(h,t); }
  // move `mm` millimetres at 250 mm/s: 1 kHz Hall at reference, IR snapshot every 100 ms
  void move(uint64_t mm){ uint64_t ms=mm*4; for(uint64_t i=0;i<ms;++i){ t+=1000; if(i%100==0){ pulses=(pulses*9652+ 0)/9652; } sample(2000);
      if(i%100==99){ pulses += 3; n.observeIr(ir(),t,90);} } }
  void magnet(bool north){ for(int i=0;i<40;++i){ t+=1000; sample(north?2200:1800);} }   // ~10 mm field
  void show(const char* tag){ printf("  %-34s mm=%3u target=%3u reliable=%d pending=%d confirmed=%u\n",tag,n.mm(),n.target().sequence,n.positionReliable(),n.geographicJudgmentPending(),n.confirmedCount()); }
};
static bool P(int mm){ return navi_one::polarityAt(uint8_t(mm))!=0; }
static void start(Sim& s,int decl){ s.sample(2000); s.n.observeIr(s.ir(),s.t,90); s.t+=1000; s.n.declare(uint8_t(decl),1,s.t); s.move(100); }
int main(){
  puts("A. Single magnet missed by Hall (MM027 absent), next polarity DIFFERS from target");
  { Sim s; start(s,25); s.magnet(P(26)); s.show("after 026");
    // find case: 26 confirmed; 27 missed; 28 polarity vs target 27
    s.move(290); /* 27 missing */ s.move(300); s.magnet(P(28)); s.show("phys 028 (P27!=P28?)");
    for(int k=29;k<=45;++k){ s.move(290); s.magnet(P(k)); char b[40]; snprintf(b,40,"phys %03d",k); s.show(b);} }
  puts("B. Spurious extra encounter between 030 and 031, opposite polarity to 031");
  { Sim s; start(s,25); for(int k=26;k<=30;++k){ s.move(290); s.magnet(P(k)); } s.show("after 030");
    s.move(150); s.magnet(!P(31)); s.show("spurious");
    for(int k=31;k<=44;++k){ s.move(140); s.magnet(P(k)); char b[40]; snprintf(b,40,"phys %03d",k); s.show(b); s.move(150);} }
  puts("C. MM031 missed by Hall, MM032 has the SAME polarity as target 031 (silent slip?)");
  { Sim s; start(s,29); s.move(290); s.magnet(P(30)); s.show("after 030");
    s.move(290); s.move(320); s.magnet(P(32)); s.show("phys 032 (031 missed)");
    for(int k=33;k<=46;++k){ s.move(290); s.magnet(P(k)); char b[40]; snprintf(b,40,"phys %03d",k); s.show(b);} }
  puts("D. Spurious extra encounter 800 ms after 030, opposite polarity to 031");
  { Sim s; start(s,25); for(int k=26;k<=30;++k){ s.move(290); s.magnet(P(k)); } s.show("after 030");
    s.move(190); s.magnet(!P(31)); s.show("spurious @+800ms");
    for(int k=31;k<=44;++k){ if(k==31) s.move(100); else s.move(290); s.magnet(P(k)); char b[40]; snprintf(b,40,"phys %03d",k); s.show(b);} }
  puts("E. Stalled at PWM>0 on a field hovering at threshold (no IR progress, no Type-6)");
  { Sim s; start(s,25); for(int k=26;k<=30;++k){ s.move(290); s.magnet(P(k)); } s.show("after 030 (stalled on its field)");
    for(int ms=0; ms<4000; ++ms){ s.t+=1000; s.sample(ms%3==2? 2068 : 2072); if(ms%100==99) s.n.observeIr(s.ir(),s.t,90); }
    s.show("after 4 s stalled on MM030 field"); }
  return 0; }
