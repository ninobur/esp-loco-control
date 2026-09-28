#include <cassert>
#include <cstring>
#include <iostream>
#include "IrSpeedTelemetry.h"
using namespace ngr_nav;
int main() {
  NaviSpeedInterpretation n;
  WireSnapshot w;w.bootId=7;w.pitchUm=9652;w.completedPulses=42;
  IrSpeedReading raw;raw.reason="INADEQUATE_CONTRAST";
  uint64_t now=1000000;
  auto step=[&](bool commandZero=true,bool powered=false,bool hall=false,
                bool fresh=true,bool coupled=true) {
    ++w.sequence;w.capturedUs=now;
    auto r=n.assess(raw,now,coupled,commandZero,powered,hall,fresh,w);
    now+=1000000;return r;
  };
  assert(!step().valid);assert(!step().valid);assert(!step().valid);
  auto r=step();assert(r.valid && r.mmps==0 && !strcmp(r.reason,"STOPPED"));
  assert(!raw.valid && !strcmp(raw.reason,"INADEQUATE_CONTRAST"));
  char json[560];int count=formatIrSpeed(json,sizeof(json),raw,true,&r);
  assert(count>0 && count<int(sizeof(json)));
  assert(strstr(json,"\"ir_mmps\":null"));
  assert(strstr(json,"\"navi_speed_mmps\":0.000"));
  assert(strstr(json,"\"navi_speed_reason\":\"STOPPED\""));
  std::cout<<'{'<<json<<"}\n";
  ++w.completedPulses;assert(!step().valid); // Movement even during optical outage.
  assert(!step().valid);assert(!step().valid);assert(step().valid);
  assert(!step(true,false,true).valid); // Hall advance cancels stop.
  assert(!step(false).valid); // New motion command cancels before PWM ramps.
  for(int i=0;i<5;++i)assert(!step(true,true).valid); // Ramp not finished.
  for(int i=0;i<5;++i)assert(!step(true,false,false,false).valid); // Link lost.
  for(int i=0;i<5;++i)assert(!step(true,false,false,true,false).valid);
  assert(!step().valid);assert(!step().valid);assert(step().valid);
  ++w.bootId;assert(!step().valid); // New source baseline.
  assert(!step().valid);assert(!step().valid);assert(step().valid);
  assert(!n.assess(raw,now,true,true,false,false,true,w).valid); // Repeated sample.
  for(const char* fault:{"SATURATED","PACKET_INVALID","LINK_STALE","WARMUP"}) {
    raw.reason=fault;
    for(int i=0;i<5;++i)assert(!step().valid);
  }
  raw.valid=true;raw.mmps=100;raw.deltaPulses=10;raw.reason="MEASURED";
  for(int i=0;i<5;++i){w.completedPulses+=10;r=step();assert(r.valid && r.mmps==100);}
  // Stop display does not alter odometry, reference or navigation objects.
  std::cout<<"NAVI stop interpretation tests passed\n";
}
