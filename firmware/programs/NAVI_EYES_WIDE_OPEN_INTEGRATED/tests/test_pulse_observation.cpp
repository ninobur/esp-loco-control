#include "../NaviPulseTelemetry.h"
#include <cassert>
#include <cmath>
#include <cstring>

using namespace navi_pulse;
static PulseEventPacket event(uint32_t seq,uint64_t count,uint64_t us,uint64_t boot=42) {
  PulseEventPacket e{}; e.magic=0x4952;e.version=1;e.type=6;e.sid=7;e.sequence=seq;
  e.bootId=boot;e.completedPulses=count;e.completedUs=us;e.intervalUs=40000;
  e.pitchUm=9652;e.nominalUm=count*9652;e.opticalReason=ir_movement::TRACKING;return e;
}
static Rx input(PulseEventPacket e,uint8_t source=1) {
  e.crc=crc(reinterpret_cast<const uint8_t*>(&e),offsetof(PulseEventPacket,crc));
  Rx r{};r.mac[0]=source;r.receivedUs=e.completedUs+1000;r.length=sizeof(e);std::memcpy(r.bytes,&e,sizeof(e));return r;
}
int main() {
  Observation o;
  o.receive(input(event(1,1,40000))); assert(o.state().current&&!o.state().speedValid);
  o.receive(input(event(2,2,80000))); assert(o.state().speedValid&&o.state().distanceAdvancedUm==9652);
  // A later cumulative endpoint bridges three absent observations without
  // manufacturing them: four pulses / 160 ms is the measured gap average.
  o.receive(input(event(6,6,240000))); assert(o.state().missing==3);
  assert(o.state().distanceAdvancedUm==4*9652&&o.state().physicalIntervalUs==160000);
  assert(std::abs(o.state().averageMmps-241.3)<1e-9);
  const auto accepted=o.state().accepted;
  o.receive(input(event(6,6,240000))); assert(o.state().invalid==1&&o.state().accepted==accepted);
  o.receive(input(event(7,7,280000),2)); assert(o.state().invalid==2&&o.state().sourceMismatches==1);
  // A contradictory count/timestamp/order never moves the accepted high-water mark.
  auto contradictory=event(7,8,280000); o.receive(input(contradictory));
  assert(o.state().invalid==3&&o.state().accepted==accepted&&(o.state().flags&ORDER_FAULT));
  auto older=event(7,7,200000); o.receive(input(older));
  assert(o.state().invalid==4&&o.state().accepted==accepted&&(o.state().flags&TIME_FAULT));
  o.receive(input(event(1,1,40000,99))); assert(o.state().accepted==accepted+1&&(o.state().flags&BOOT_CHANGE));
  auto bad=input(event(2,2,80000,99));bad.bytes[0]=0;o.receive(bad);assert(o.state().invalid==5);
  char json[640];const int n=format(json,sizeof(json),o.state(),o.state().receivedUs,1000000);
  assert(n>0&&n<int(sizeof(json))&&std::strstr(json,"OBSERVATION_ONLY"));
}
