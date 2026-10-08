#include "../NaviPulseTelemetry.h"
#include "../NaviIntegratedCore.h"
#include <cassert>
#include <cmath>
#include <climits>
#include <iostream>
using namespace navi_pulse;
static constexpr double NAVI_PKPH_MM_PER_SEC = 5.37325;
static PulseEventPacket event(uint32_t seq=1,uint64_t count=1,uint64_t us=40000){
  PulseEventPacket e{};e.magic=0x4952;e.version=1;e.type=6;e.sid=42;e.bootId=42;
  e.sequence=seq;e.completedPulses=count;e.completedUs=us;e.intervalUs=40000;
  e.pitchUm=9652;e.nominalUm=count*9652;e.opticalReason=ir_movement::TRACKING;e.span=1007;
  return e;
}
static Rx rx(PulseEventPacket e,uint32_t loss=0,uint8_t source=1,bool corrupt=false){
  e.crc=crc(reinterpret_cast<uint8_t*>(&e),offsetof(PulseEventPacket,crc));
  if(corrupt)++e.crc;
  Rx r{};r.mac[0]=source;r.receivedUs=e.completedUs+1000000;
  r.length=sizeof(e);r.queueDrops=loss;std::memcpy(r.bytes,&e,sizeof(e));return r;
}
static PulseTransportStatusPacket status(uint64_t boot=42,uint32_t seq=1,
                                         uint32_t generated=0,uint32_t sent=0){
  PulseTransportStatusPacket p{};p.magic=0x4952;p.version=1;p.type=7;
  p.sid=uint32_t(boot);p.sequence=seq;p.bootId=boot;p.generated=generated;p.sent=sent;
  return p;
}
static StatusRx statusRx(PulseTransportStatusPacket p,bool corrupt=false){
  p.crc=crc(reinterpret_cast<uint8_t*>(&p),offsetof(PulseTransportStatusPacket,crc));
  if(corrupt)++p.crc;StatusRx r{};r.length=sizeof(p);std::memcpy(r.bytes,&p,sizeof(p));return r;
}
static void prime(Observation& o){o.receive(rx(event()));assert(!o.state().eventValid);
  o.receive(rx(event(2,2,80000)));assert(o.state().eventValid);}
int main(int argc,char**){
  Observation o;prime(o);
  assert(std::abs(o.state().mmps-241.3)<1e-9);
  assert(std::abs(o.state().mmps/NAVI_PKPH_MM_PER_SEC-44.9076443493)<1e-6);
  // Arrival jitter has no effect: only physical timestamps determine speed.
  auto jitter=rx(event(3,3,121000));jitter.receivedUs+=30000;
  auto e=event(3,3,121000);e.intervalUs=41000;jitter=rx(e);jitter.receivedUs+=30000;o.receive(jitter);
  assert(o.state().eventValid && std::abs(o.state().mmps-9652000.0/41000)<1e-9);
  // Actual transmitter event construction and CRC contract interoperate.
  ir_movement::Measurement m{42,0,9.652,true};PulseEventEvidence tx;Observation receiver;
  unsigned emitted=0,valid=0;
  for(uint64_t us=1000;us<2000000;us+=1000){
    m.sample(us,(us/20000)%2?2000:1000);PulseEventPacket wire;
    if(tx.observe(m.snapshot(),m.detector(),42,wire)){
      ++emitted;receiver.receive(rx(wire));valid+=receiver.state().eventValid;
      assert(receiver.state().invalid==0);
    }
  }
  assert(emitted>40 && valid==emitted-1);
  // Every malformed field is rejected, clears predecessor, and cannot refresh
  // trusted ordering. Next good event is reference-only; following is valid.
  for(unsigned fault=0;fault<11;++fault){
    Observation r;prime(r);auto bad=event(3,3,120000);
    switch(fault){
      case 0:bad.magic=0;break;case 1:bad.version=2;break;case 2:bad.type=5;break;
      case 3:bad.bootId=0;break;case 4:bad.sid=99;break;
      case 5:bad.pitchUm=9653;bad.nominalUm=bad.completedPulses*bad.pitchUm;break;
      case 6:++bad.nominalUm;break;case 7:bad.opticalReason=7;break;
      case 8:bad.completedPulses=UINT64_MAX/9652+1;bad.nominalUm=bad.completedPulses*9652;break;
      case 9:bad.completedUs=0;break;case 10:break;
    }
    r.receive(rx(bad,0,1,fault==10));assert(r.state().invalid==1 && !r.state().eventValid);
    r.receive(rx(event(4,4,160000)));assert(!r.state().eventValid);
    r.receive(rx(event(5,5,200000)));assert(r.state().eventValid);
  }
  for(unsigned fault=0;fault<8;++fault){
    Observation r;prime(r);auto bad=event(3,3,120000);auto input=rx(bad);
    if(fault==0){bad.sequence=4;input=rx(bad);}
    if(fault==1){bad.completedPulses=4;bad.nominalUm=4*9652;input=rx(bad);}
    if(fault==2){bad.intervalUs=39999;input=rx(bad);}
    if(fault==3){bad.intervalUs=0;input=rx(bad);}
    if(fault==4){bad.opticalReason=ir_movement::INADEQUATE_CONTRAST;input=rx(bad);}
    if(fault==5)input=rx(bad,1);
    if(fault==6)input=rx(bad,0,2);
    if(fault==7){bad.bootId=bad.sid=43;input=rx(bad);}
    r.receive(input);assert(!r.state().eventValid && r.state().discontinuities==1);
    ++bad.sequence;++bad.completedPulses;bad.completedUs+=40000;bad.intervalUs=40000;
    bad.nominalUm=bad.completedPulses*9652;bad.opticalReason=ir_movement::TRACKING;
    r.receive(rx(bad,fault==5?1:0,fault==6?2:1));
    assert(r.state().eventValid==(fault!=4));
  }
  // Duplicate and reversed evidence must not move the trusted predecessor back.
  for(unsigned fault=0;fault<4;++fault){
    Observation r;prime(r);auto bad=event(3,3,120000);
    if(fault==0)bad.sequence=1;
    if(fault==1){bad.completedPulses=1;bad.nominalUm=9652;}
    if(fault==2)bad.completedUs=40000;
    auto input=rx(bad);if(fault==3)input.receivedUs=1;
    r.receive(input);assert(r.state().invalid==1 && !r.state().eventValid);
    r.receive(rx(event(3,3,120000)));assert(!r.state().eventValid);
    r.receive(rx(event(4,4,160000)));assert(r.state().eventValid);
  }
  Observation duplicate;prime(duplicate);duplicate.receive(rx(event(2,2,80000)));
  assert(duplicate.state().invalid==1 && !duplicate.state().eventValid &&
    (duplicate.state().flags&SEQUENCE_ORDER));
  // Exact receipt ledger: count absent values, never manufacture their evidence.
  Observation ledger;ledger.receive(rx(event(100,100,4000000)));
  ledger.receive(rx(event(101,101,4040000)));
  assert(ledger.state().missing==0 && ledger.state().ledgerReceived==2);
  ledger.receive(rx(event(110,110,4400000)));
  assert(ledger.state().missing==8 && ledger.state().lastSequence==110);
  Observation exact;exact.receive(rx(event(100,100,4000000)));
  exact.receive(rx(event(110,110,4400000)));
  assert(exact.state().missing==9 && exact.state().ledgerReceived==2);
  exact.receive(rx(event(110,110,4400000)));assert(exact.state().duplicates==1);
  exact.receive(rx(event(105,105,4200000)));assert(exact.state().outOfOrder==1 &&
    exact.state().lastSequence==110);
  auto newBoot=event(1,1,40000);newBoot.bootId=newBoot.sid=77;exact.receive(rx(newBoot));
  assert(exact.state().ledgerBootId==77 && exact.state().firstSequence==1 &&
    exact.state().lastSequence==1 && exact.state().ledgerReceived==1 && exact.state().missing==0);
  // Type-7 is independently validated and cannot alter Type-6 state.
  TransportObservation transportObservation;auto good=status(42,1,110,109);transportObservation.receive(statusRx(good));
  assert(transportObservation.state().valid && transportObservation.state().status.sent==109 && sizeof(PulseTransportStatusPacket)==70);
  transportObservation.receive(statusRx(good,true));assert(!transportObservation.state().valid && transportObservation.state().invalid==1);
  transportObservation.receive(statusRx(status(42,1)));assert(transportObservation.state().orderFaults==1);
  const bool wrongBoot=transportObservation.state().valid && transportObservation.state().status.bootId==exact.state().ledgerBootId;
  assert(!wrongBoot);transportObservation.receive(statusRx(status(77,2,200,199)));
  assert(transportObservation.state().valid && transportObservation.state().status.bootId==exact.state().ledgerBootId);
  Observation fallback;auto zeroSid=event();zeroSid.sid=0;zeroSid.bootId=1;
  fallback.receive(rx(zeroSid));assert(fallback.state().accepted==1);
  zeroSid.sequence=2;zeroSid.completedPulses=2;zeroSid.nominalUm=19304;zeroSid.completedUs=80000;
  fallback.receive(rx(zeroSid));assert(fallback.state().eventValid);
  Observation wrap;wrap.receive(rx(event(UINT32_MAX,100,40000)));
  wrap.receive(rx(event(0,101,80000)));assert(wrap.state().eventValid);
  auto longStop=event(1,102,UINT64_C(0x100000000));
  longStop.intervalUs=longStop.completedUs-80000;longStop.opticalReason=ir_movement::SIGNAL_STALE;
  wrap.receive(rx(longStop));assert(wrap.state().eventValid); // No arbitrary physical-interval timeout.
  auto malformed=rx(event());malformed.length=60;wrap.receive(malformed);assert(!wrap.state().eventValid);
  uint8_t header[110]={0x52,0x49,1,6};
  assert(isPulseFrame(header,110) && isPulseFrame(header,4) && isPulseFrame(header,61));
  header[3]=5;assert(!isPulseFrame(header,110) && !isPulseFrame(header,3));
  assert(sizeof(PulseEventPacket)==61 && offsetof(PulseEventPacket,crc)==59);
  // Emit real formatter outputs for strict JSON and payload-size verification.
  if(argc>1){
    Report r{};char buffer[1200];
    auto emit=[&](uint64_t now){int n=format(buffer,sizeof(buffer),r,now,
      navi_eyes::NaviIntegratedCore::kIrFreshUs,NAVI_PKPH_MM_PER_SEC,true,0,0,0);
      assert(n>0 && n<int(sizeof(buffer)));std::cout<<buffer<<'\n';};
    emit(0);r.pulse=o.state();r.legacyValid=true;r.legacyPkph=45;emit(r.pulse.receivedUs);
    emit(r.pulse.receivedUs+1000001);
    r.pulse.flags=r.pulse.received=r.pulse.accepted=r.pulse.invalid=r.pulse.discontinuities=UINT32_MAX;
    r.pulse.sequenceBreaks=r.pulse.pulseBreaks=r.pulse.timeBreaks=r.pulse.intervalBreaks=UINT32_MAX;
    r.pulse.sourceChanges=r.pulse.bootChanges=UINT32_MAX;r.pulse.event.sequence=UINT32_MAX;
    r.pulse.event.completedPulses=UINT64_MAX/9652;r.pulse.event.completedUs=UINT64_MAX;
    r.pulse.event.intervalUs=UINT64_MAX;r.pulse.receivedUs=UINT64_MAX-1;
    r.pulse.event.bootId=UINT64_MAX;r.pulse.event.sid=UINT32_MAX;r.pulse.event.span=UINT16_MAX;
    r.pulse.mmps=9652000;r.legacyPkph=2000000;r.comparedUs=r.legacyBoot=UINT64_MAX;
    std::memset(r.pulse.mac,255,6);emit(UINT64_MAX);
  }else std::puts("PASS native pulse measurements, source/boot/order/loss recovery, TX interoperability, wrap and wire validation");
}
