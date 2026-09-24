#include "../firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/IrSpeedTelemetry.h"
#include <cassert>
#include <cstdio>
using namespace ir_movement;
static const uint8_t MAC[6]={2,3,4,5,6,7};
struct Rig {
  Measurement m{123,0,9.652,true};
  ngr_nav::IrHealthMonitor rx;
  uint64_t us=0;uint32_t seq=0;
  Rig(){rx.pair(MAC,0);}
  void hold(unsigned raw,unsigned ms) {
    for(unsigned i=0;i<ms;++i){us+=1000;m.sample(us,raw);}
  }
  void wave(unsigned lo=1000,unsigned hi=2000,unsigned cycles=20) {
    for(unsigned i=0;i<cycles;++i){hold(lo,100);hold(hi,100);}
    hold(lo,100);
  }
  void send() {
    const auto& d=m.detector();auto w=encode(m.snapshot(),++seq,d.high-d.low);
    w.crc=ngr_nav::movementCrc(reinterpret_cast<const uint8_t*>(&w),offsetof(WireSnapshot,crc));
    rx.receive(MAC,reinterpret_cast<const uint8_t*>(&w),sizeof(w),us+10000);
  }
};
int main() {
  // Acquisition may need extra transitions, but ONE counted cycle cannot
  // establish a retained stationary reference; two agreeing cycles can.
  for(unsigned target=1;target<=2;++target) {
    Rig r;
    while(r.m.detector().completed<target) {
      r.hold(((r.us/100000)%2)?2000:1000,1);
      assert(r.us<3000000);
    }
    assert(r.m.detector().completed==target);
    r.hold(1000,3000);
    assert(r.m.detector().reason==(target==1?INADEQUATE_CONTRAST:SIGNAL_STALE));
  }
  // Do not deliver any outage packets: cumulative evidence alone must end
  // the epoch and invalidate the old landmark reference after recovery.
  for(unsigned fault=0;fault<4;++fault) {
    Rig r;r.wave();r.hold(1000,1000);r.send();
    assert(r.rx.odometry().epochActive());
    const auto epoch=r.rx.odometry().epochId();
    ngr_nav::MmDistanceReference ref;
    assert(ref.synchronize(41,0,r.rx.odometry().point(),r.rx.odometry()));
    const auto aborts=r.m.detector().aborts;
    if(fault==0)r.hold(2600,1); // Quiet plateau amplitude fault.
    if(fault==1)r.hold(1500,700); // Mid-edge stop.
    if(fault==2)r.wave(1400,1600,5); // Moving but inadequate live contrast.
    if(fault==3){r.hold(4095,1);} // Saturation, including existing counter.
    assert(r.m.detector().aborts>aborts);
    r.wave();assert(r.m.detector().reason==TRACKING);r.send();
    assert(r.rx.odometry().epochActive());
    assert(r.rx.odometry().epochId()>epoch);
    assert(!ref.validFor(r.rx.odometry()));
  }
  // Sustained invalid plateau must not manufacture a growing fault count
  // after the retained/armed state has already been discarded.
  Rig r;r.wave();r.hold(1000,2000);r.hold(2600,1);
  const auto once=r.m.detector().aborts;r.hold(1000,2000);
  assert(r.m.detector().aborts==once);
  // Fresh transport cannot turn an old optical capture into a current speed.
  Rig fresh;ngr_nav::IrSpeedTelemetry speed;
  fresh.wave();fresh.send();
  assert(!speed.sample(fresh.rx,fresh.us+10000,true,true).valid);
  fresh.wave(1000,2000,4);fresh.send();
  auto reading=speed.sample(fresh.rx,fresh.us+10000,true,true);
  assert(reading.valid && reading.mmps>0);
  fresh.hold(1000,1000);fresh.send();
  reading=speed.sample(fresh.rx,fresh.us+10000,true,true);
  assert(reading.valid && reading.mmps==0);
  auto frozen=encode(fresh.m.snapshot(),++fresh.seq,1000);
  frozen.crc=ngr_nav::movementCrc(reinterpret_cast<const uint8_t*>(&frozen),offsetof(WireSnapshot,crc));
  fresh.rx.receive(MAC,reinterpret_cast<const uint8_t*>(&frozen),sizeof(frozen),fresh.us+500000);
  reading=speed.sample(fresh.rx,fresh.us+500000,true,true);
  assert(!reading.valid && !strcmp(reading.reason,"NO_NEW_SAMPLE"));
  reading=speed.sample(fresh.rx,fresh.us+1010001,true,true);
  assert(!reading.valid && !strcmp(reading.reason,"LINK_STALE"));
  std::puts("PASS two-cycle acquisition; hidden faults break epoch and MM reference; no repeated quiet fault count; stopped speed is zero; frozen capture is unavailable despite new radio sequence");
}
