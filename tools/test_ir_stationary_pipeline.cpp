#include "../firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/IrSpeedTelemetry.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace ngr_nav;
static const uint8_t MAC[6]={2,3,4,5,6,7};
struct Rig {
  ir_movement::Measurement tx{123,0,9.652,true};
  IrHealthMonitor rx;
  IrSpeedTelemetry speed;
  IrSpeedQualification qualify;
  IrSpeedReading reading;
  uint64_t us=0;uint32_t seq=0,validZero=0,validMoving=0;
  Rig(){rx.pair(MAC,0);}
  void hold(uint16_t raw,unsigned ms) {
    for(unsigned i=0;i<ms;++i) {
      us+=1000;tx.sample(us,raw);
      if(us%100000==0) {
        const auto& detector=tx.detector();
        auto w=ir_movement::encode(tx.snapshot(),++seq,detector.high-detector.low);
        w.crc=movementCrc(reinterpret_cast<const uint8_t*>(&w),offsetof(WireSnapshot,crc));
        rx.receive(MAC,reinterpret_cast<const uint8_t*>(&w),sizeof(w),us+10000);
      }
      if(us%1000000==0) {
        reading=qualify.assess(speed.sample(rx,us+10000,true,true),us+10000,true,false,false);
        if(reading.valid){if(reading.mmps==0)++validZero;else ++validMoving;}
      }
    }
  }
  void prime(){for(unsigned n=0;n<30;++n){hold(1000,100);hold(2000,100);}hold(1000,100);}
  void dump() {
    char out[400];const int n=formatIrSpeed(out,sizeof(out),reading,true);
    assert(n>0 && n<int(sizeof(out)));std::cout<<'{'<<out<<"}\n";
  }
};
int main() {
  for(unsigned high=0;high<2;++high) {
    Rig r;r.prime();assert(r.validMoving>0);
    if(high)r.hold(2000,100);
    const auto epoch=r.rx.odometry().epochId(),pulses=r.tx.snapshot().completedPulses;
    const auto zeros=r.validZero;
    r.hold(high?2000:1000,120000);
    assert(r.tx.snapshot().completedPulses==pulses);
    assert(r.rx.odometry().epochId()==epoch && r.rx.odometry().epochActive());
    assert(r.validZero-zeros>=118 && r.reading.valid && r.reading.mmps==0);
    r.dump();
    const auto moving=r.validMoving;
    r.prime();assert(r.validMoving>moving && r.rx.odometry().epochId()==epoch);
    r.dump();
    r.hold(4095,1000);assert(!r.reading.valid && !r.rx.odometry().epochActive());r.dump();
    r.prime();assert(r.rx.odometry().epochId()>epoch);
    r.reading=r.speed.sample(r.rx,r.us+1010001,true,true);
    assert(!r.reading.valid && !r.rx.odometry().epochActive());r.dump();
  }
  Rig cold;cold.hold(1000,5000);assert(!cold.reading.valid);cold.dump();
  std::cerr<<"PASS real detector -> type5 CRC packet -> health/epoch -> qualified speed JSON: stop low/high, resume, saturation, stale radio, unproven startup\n";
}
