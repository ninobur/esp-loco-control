#include <cassert>
#include <cmath>
#include <iostream>
#include "IrSpeedTelemetry.h"
using namespace ngr_nav;
static const uint8_t MAC[6]={2,3,4,5,6,7};
static void send(IrHealthMonitor& m,uint32_t seq,uint64_t us,uint64_t pulses,
                 uint8_t reason=ir_movement::TRACKING,uint64_t boot=7,uint32_t pitch=9652) {
  WireSnapshot w;w.sequence=seq;w.capturedUs=us;w.completedPulses=w.observedRises=pulses;
  w.bootId=boot;w.calibrationId=0;w.pitchUm=pitch;w.nominalUm=pulses*pitch;w.opticalReason=reason;
  w.crc=movementCrc(reinterpret_cast<const uint8_t*>(&w),offsetof(WireSnapshot,crc));
  m.receive(MAC,reinterpret_cast<const uint8_t*>(&w),sizeof(w),us+100000);
}
int main() {
  assert(std::abs(130/PKPH_MM_PER_SEC-24.1939)<0.0001);
  assert(std::abs(212/PKPH_MM_PER_SEC-39.4547)<0.0001);
  IrHealthMonitor h;h.pair(MAC,0);IrSpeedTelemetry speed;
  assert(!speed.sample(h,1,false,true).valid);
  send(h,1,1000000,100);
  assert(!speed.sample(h,1100000,true,true).valid);
  send(h,2,2000000,120);
  auto r=speed.sample(h,2100000,true,true);
  assert(r.valid && r.deltaPulses==20 && r.windowUs==1000000);
  assert(std::abs(r.mmps-193.04)<1e-9);
  assert(!speed.sample(h,2100001,true,true).valid); // Duplicate endpoint isn't zero.
  send(h,3,3000000,120,ir_movement::SIGNAL_STALE);
  r=speed.sample(h,3100000,true,true);assert(r.valid && r.mmps==0);
  IrSpeedQualification qualification;
  assert(!qualification.assess(r,3100000,false,false,false).valid);
  assert(qualification.assess(r,3100000,true,false,false).valid);
  assert(!qualification.assess(r,3100000,true,false,true).valid);
  assert(qualification.assess(r,4000000,true,true,false).valid);
  assert(!qualification.assess(r,7000000,true,true,false).valid);
  assert(qualification.assess(r,8000000,true,false,false).valid);
  send(h,4,4000000,130,ir_movement::INADEQUATE_CONTRAST);
  assert(!speed.sample(h,4100000,true,true).valid);
  send(h,5,5000000,140);assert(!speed.sample(h,5100000,true,true).valid);
  send(h,6,6000000,150);assert(speed.sample(h,6100000,true,true).valid);
  // Unavailable/recovered entirely between display ticks still changes epoch.
  send(h,7,6200000,150,ir_movement::REACQUIRING);
  send(h,8,6400000,151);
  assert(!speed.sample(h,6500000,true,true).valid);
  send(h,9,7400000,160);assert(speed.sample(h,7500000,true,true).valid);
  send(h,1,8000000,0,ir_movement::TRACKING,8);
  assert(!speed.sample(h,8100000,true,true).valid); // Reboot.
  send(h,2,9000000,10,ir_movement::TRACKING,8,10000);
  assert(!speed.sample(h,9100000,true,true).valid); // New calibration pitch.
  send(h,3,10000000,20,ir_movement::TRACKING,8,10000);
  r=speed.sample(h,10100000,true,true);assert(r.valid && r.mmps==100);
  send(h,4,11000000,2,ir_movement::TRACKING,8,10000);
  assert(!speed.sample(h,11100000,true,true).valid); // Counter regression.
  assert(!speed.sample(h,12100001,true,true).valid); // Radio stale.
  send(h,5,13000000,10,ir_movement::TRACKING,8,10000);
  assert(!speed.sample(h,13100000,true,true).valid);
  h.queueGap(13200000);assert(!speed.sample(h,13200001,true,true).valid);
  send(h,6,14000000,20,ir_movement::TRACKING,8,10000);
  assert(!speed.sample(h,14100000,true,true).valid);
  assert(!speed.sample(h,14100000,true,false).valid);
  send(h,7,15000000,30,ir_movement::TRACKING,8,10000);
  assert(!speed.sample(h,15100000,true,true).valid);
  send(h,8,16000000,40,ir_movement::TRACKING,8,10000);
  r=speed.sample(h,16100000,true,true);assert(r.valid && r.mmps==100);
  char json[400];int n=formatIrSpeed(json,sizeof(json),r,true);
  assert(n>0 && n<int(sizeof(json)));std::cout<<'{'<<json<<"}\n";
  r.valid=false;r.reason="LINK_STALE";
  n=formatIrSpeed(json,sizeof(json),r,true);assert(n>0 && n<int(sizeof(json)));
  assert(strstr(json,"\"ir_mmps\":null") && strstr(json,"\"ir_pkph\":null"));
  std::cout<<'{'<<json<<"}\n";
}
