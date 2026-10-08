#include "../NaviPulseTelemetry.h"
#include "../EwoStationStop.h"
#include "../NaviIntegratedCore.h"
#include <cassert>
#include <cmath>
#include <climits>
#include <cstring>
#include <iostream>

using namespace navi_pulse;

static PulseEventPacket event(uint32_t seq,uint64_t count,uint64_t us,uint64_t interval=40000) {
  PulseEventPacket e{}; e.magic=0x4952;e.version=1;e.type=6;e.sid=42;e.bootId=42;
  e.sequence=seq;e.completedPulses=count;e.completedUs=us;e.intervalUs=interval;
  e.pitchUm=9652;e.nominalUm=count*9652;e.opticalReason=ir_movement::TRACKING;e.span=1007;
  return e;
}
static Rx rx(PulseEventPacket e,uint8_t source=1,uint32_t replaced=0,bool corrupt=false) {
  e.crc=crc(reinterpret_cast<uint8_t*>(&e),offsetof(PulseEventPacket,crc));
  if(corrupt)++e.crc;
  Rx r{};r.mac[0]=source;r.receivedUs=e.completedUs+1000000;r.replaced=replaced;
  r.length=sizeof(e);std::memcpy(r.bytes,&e,sizeof(e));return r;
}
int main(int argc,char**){
  Observation o;
  o.receive(rx(event(10,10,400000))); // First endpoint is reference-only.
  assert(o.state().accepted==1&&!o.state().speedValid);
  o.receive(rx(event(15,15,600000)));
  assert(o.state().speedValid&&o.state().distanceAdvancedUm==5*9652);
  assert(o.state().physicalIntervalUs==200000);
  assert(std::abs(o.state().averageMmps-241.3)<1e-9);
  assert(o.state().missing==4&&(o.state().flags&SEQUENCE_GAP));

  // Arrival jitter does not enter the physical average.
  auto jitter=rx(event(20,20,800000));jitter.receivedUs+=500000;o.receive(jitter);
  assert(o.state().speedValid&&std::abs(o.state().averageMmps-241.3)<1e-9);

  // Supersession is visible but the newest cumulative endpoint remains usable.
  o.receive(rx(event(25,25,1000000),1,3));
  assert(o.state().speedValid&&o.state().queueReplacements==3&&(o.state().flags&QUEUE_REPLACED));

  // Duplicate, stale, source and malformed packets cannot move the endpoint.
  const auto accepted=o.state().accepted;const auto endpoint=o.state().event.completedPulses;
  o.receive(rx(event(25,25,1000000)));assert(o.state().invalid==1&&o.state().accepted==accepted);
  o.receive(rx(event(26,26,1040000),2));assert(o.state().sourceMismatches==1&&o.state().accepted==accepted);
  auto bad=event(26,26,1040000);bad.pitchUm=1;o.receive(rx(bad));
  assert(o.state().invalid==3&&o.state().event.completedPulses==endpoint);

  // A boot change creates a new reference, then forward cumulative evidence resumes.
  auto reboot=event(1,1,40000);reboot.bootId=43;reboot.sid=77;o.receive(rx(reboot));
  assert(o.state().bootChanges==1&&o.state().accepted==accepted+1&&!o.state().speedValid);
  auto after=event(4,4,160000);after.bootId=43;after.sid=77;o.receive(rx(after));
  assert(o.state().speedValid&&o.state().distanceAdvancedUm==3*9652&&o.state().physicalIntervalUs==120000);

  // Real transmitter packet construction retains cumulative endpoint semantics.
  ir_movement::Measurement m{99,0,9.652,true};PulseEventEvidence tx;Observation receiver;
  unsigned emitted=0;
  for(uint64_t us=1000;us<800000;us+=1000){m.sample(us,(us/20000)%2?2000:1000);PulseEventPacket wire;
    if(tx.observe(m.snapshot(),m.detector(),42,wire)){++emitted;receiver.receive(rx(wire));}}
  assert(emitted>10&&receiver.state().accepted==emitted&&receiver.state().event.completedPulses==emitted);

  uint8_t header[110]={0x52,0x49,1,6};assert(isPulseFrame(header,110)&&isPulseFrame(header,61));
  header[3]=5;assert(!isPulseFrame(header,110));
  assert(sizeof(PulseEventPacket)==61&&offsetof(PulseEventPacket,crc)==59);
  if(argc>1){Report r{};r.pulse=o.state();char buffer[1200];
    const int n=format(buffer,sizeof(buffer),r,r.pulse.receivedUs,
      navi_eyes::NaviIntegratedCore::kIrFreshUs,navi_eyes::EWO_PKPH_MM_PER_SEC,0);
    assert(n>0&&n<int(sizeof(buffer)));std::cout<<buffer<<'\n';}
  else std::puts("PASS cumulative Type-6 endpoints, gaps, source/boot/order handling and physical-time averages");
}
