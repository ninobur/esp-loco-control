#include "../firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX/PulseEventEvidence.h"
#include <cassert>
#include <cstdio>
#include <vector>

struct Rig {
  ir_movement::Measurement m{123,0,9.652,true};
  PulseEventEvidence evidence;
  uint64_t us=0;
  std::vector<PulseEventPacket> events;
  void sample(unsigned raw, unsigned dt=1000) {
    us+=dt;
    const auto previous=m.snapshot().completedPulses;
    m.sample(us,raw);
    const auto s=m.snapshot(); const auto& d=m.detector();
    PulseEventPacket e;
    const bool emitted=evidence.observe(s,d,42,e);
    assert(emitted==(s.completedPulses==previous+1));
    assert(emitted==d.fall);
    if(emitted){
      assert(e.completedUs==us && e.completedUs==s.capturedUs);
      assert(e.sequence==events.size()+1 && e.completedPulses==s.completedPulses);
      assert(e.magic==0x4952 && e.type==6 && e.version==1 && e.sid==42 && e.bootId==123);
      assert(e.pitchUm==9652 && e.nominalUm==e.completedPulses*9652);
      assert(e.span==d.high-d.low && e.opticalReason==s.reason);
      events.push_back(e);
    }
  }
  void hold(unsigned raw,unsigned ms){for(unsigned i=0;i<ms;++i)sample(raw);}
  void wave(unsigned halfMs,unsigned cycles){
    for(unsigned i=0;i<cycles;++i){hold(1000,halfMs);hold(2000,halfMs);}
    hold(1000,halfMs);
  }
};

int main(){
  Rig steady; steady.wave(20,100);
  assert(steady.events.size()>90 && steady.events.front().intervalUs==0);
  for(size_t i=2;i<steady.events.size();++i)assert(steady.events[i].intervalUs==40000);
  // Preserve deliberately irregular native intervals, including sub-100ms events.
  for(unsigned half:{15u,27u,19u,36u,21u}){
    steady.hold(2000,half); steady.hold(1000,half);
    const auto n=steady.events.size();
    assert(steady.events[n-1].intervalUs==steady.events[n-1].completedUs-steady.events[n-2].completedUs);
  }
  // Detector faults between completions must revoke predecessor timing, even
  // though the next completion's optical reason has recovered to TRACKING.
  for(unsigned fault=0;fault<4;++fault){
    Rig r; r.wave(20,30); const auto before=r.events.size();
    if(fault==0)r.sample(1000,3000); // Missing sample.
    if(fault==1)r.sample(4095);      // Saturation.
    if(fault==2)r.hold(1500,800);   // Inadequate contrast / lost phase.
    if(fault==3)r.sample(1000,0);   // Non-advancing timestamp.
    r.wave(20,30);
    assert(r.events.size()>before);
    assert(r.events[before].intervalUs==0);
    assert(r.events.back().intervalUs==40000);
  }
  // Quiet retention does not add an arbitrary timeout. Preserve elapsed time
  // across a retained stop; that interval includes the stop, not instant speed.
  Rig stop;stop.wave(20,30);const auto count=stop.events.size();
  stop.hold(1000,3500);assert(stop.events.size()==count);
  stop.hold(2000,20);stop.hold(1000,20);
  assert(stop.events.size()==count+1 && stop.events.back().intervalUs>3500000);
  // Counter/time values remain 64-bit beyond micros() rollover.
  Rig late;late.us=UINT64_C(0x100000000);late.wave(45,20);
  assert(late.events.back().completedUs>UINT32_MAX);
  assert(late.events.back().intervalUs==90000);
  std::puts("PASS authoritative completions, event timing, native intervals, fault recovery, retained stop, wire layout, distance and 64-bit time");
}
