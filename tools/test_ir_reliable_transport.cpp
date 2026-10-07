#include "../firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX/ReliablePulseTransport.h"
#include <cassert>
#include <cstdio>
using namespace ir_link;
static PulseEventPacket pulse(uint64_t n,uint64_t boot=42){
  PulseEventPacket e{};e.magic=0x4952;e.version=1;e.type=6;e.sid=uint32_t(boot);e.bootId=boot;
  e.sequence=uint32_t(n);e.completedPulses=n;e.completedUs=n*40000;e.intervalUs=n==1?0:40000;
  e.pitchUm=9652;e.nominalUm=n*9652;e.opticalReason=ir_movement::TRACKING;seal(e);return e;
}
static Ack acknowledgment(const Event& e){Ack a{};a.session=e.session;a.txBoot=e.pulse.bootId;
  a.count=1;a.items[0]={e.generation,e.pulse.sequence,e.slot};seal(a);return a;}
static void deliver(Receiver& r,const Event& e,int result,uint64_t clock=10000000){
  AckItem item{};assert(r.accept(e,clock,clock+100,result?item:item)==result);
}
int main(){
  Transmitter t;Receiver r;t.connect(99);r.start(99);r.bind(42);
  auto original=pulse(1);assert(t.enqueue(original));
  auto lost=t.submit(t.choose(40001),40001); // Drop the first packet.
  assert(t.enqueue(pulse(2)));auto next=t.submit(t.choose(80001),80001);
  assert(next.pulse.sequence==2);deliver(r,next,1);assert(r.missing==1);
  assert(t.acknowledge(acknowledgment(next),80010)==1);
  assert(t.choose(140000)==-1);assert(t.choose(140001)==int(lost.slot));
  auto retry=t.submit(lost.slot,140001);assert(std::memcmp(&retry.pulse,&original,sizeof(original))==0);
  deliver(r,retry,2);assert(r.current.sequence==2 && r.received==2 && r.missing==0);
  // Lost ACK, including intervening newer evidence, must not create another observation.
  deliver(r,retry,3);assert(r.received==2 && r.duplicates==1);
  assert(t.acknowledge(acknowledgment(retry),150000)==1);assert(t.count==0);
  assert(t.acknowledge(acknowledgment(retry),150001)==0);
  // The lowest unresolved pulse survives thousands of later independently ACKed events.
  Transmitter freeSlots;Receiver sparse;freeSlots.connect(99);sparse.start(99);sparse.bind(42);
  assert(freeSlots.enqueue(pulse(1)));auto hole=freeSlots.submit(0,40000);
  for(uint64_t n=2;n<1200;++n){
    assert(freeSlots.enqueue(pulse(n)));auto e=freeSlots.submit(freeSlots.choose(n*40000),n*40000);
    assert(e.pulse.completedPulses==n);deliver(sparse,e,1,n*40000+1);
    assert(freeSlots.acknowledge(acknowledgment(e),n*40000+2)==1);
  }
  assert(freeSlots.count==1);deliver(sparse,hole,2);assert(!sparse.missing && sparse.received==1199);
  // Reused slot cannot be freed by its previous generation's ACK.
  assert(freeSlots.enqueue(pulse(1200)));auto reused=freeSlots.submit(freeSlots.choose(48000000),48000000);
  auto wrong=acknowledgment(reused);--wrong.items[0].generation;seal(wrong);
  assert(freeSlots.acknowledge(wrong,48000001)==0);
  // Full buffer preserves all 256 events and refuses the 257th.
  Transmitter backlog;backlog.connect(99);
  for(unsigned n=1;n<=Capacity;++n)assert(backlog.enqueue(pulse(n)));
  assert(!backlog.enqueue(pulse(257)));assert(backlog.count==256);
  for(unsigned i=0;i<Capacity;++i)assert(backlog.slots[i].pulse.sequence==i+1);
  // Exact per-event 100/200/400/800 ms backoff, bounded aggregate retries.
  Transmitter cadence;cadence.connect(99);cadence.enqueue(pulse(1));cadence.enqueue(pulse(2));
  cadence.submit(0,100000);cadence.submit(1,100001);
  assert(cadence.choose(199999)<0);cadence.submit(cadence.choose(200000),200000);
  assert(cadence.slots[0].nextUs==400000);assert(cadence.choose(200001)<0);
  assert(cadence.choose(220000)==1);cadence.submit(1,220000);
  cadence.submit(0,400000);assert(cadence.slots[0].nextUs==800000);
  cadence.submit(0,800000);assert(cadence.slots[0].nextUs==1600000);
  cadence.submit(0,1600000);assert(cadence.slots[0].nextUs==2400000);
  // Receiver reboot: stale ACKs cannot release unresolved evidence; resend in new session.
  auto preboot=backlog.submit(0,20000000);backlog.connect(100);r.start(100);r.bind(42);
  assert(backlog.acknowledge(acknowledgment(preboot),20000010)==0);
  auto replay=backlog.submit(0,20000020);deliver(r,replay,1);assert(r.received==1);
  assert(backlog.acknowledge(acknowledgment(replay),20000030)==1);
  assert(backlog.priorUnresolved==256 && backlog.sessionChanges==1);
  Transmitter priority;priority.connect(99);priority.enqueue(pulse(1));priority.submit(0,40000);
  priority.connect(100);priority.enqueue(pulse(2));
  assert(priority.choose(80000)==1); // New physical evidence beats reboot replay.
  // Transmitter reboot is explicitly bound by discovery; stale old-boot data is rejected.
  r.bind(43);deliver(r,replay,0);Transmitter reboot;reboot.connect(100);reboot.enqueue(pulse(1,43));
  auto fresh=reboot.submit(0,40001);deliver(r,fresh,1);assert(r.bootChanges==1 && r.current.sequence==1);
  // Native sequence wraps without confusing cumulative ordering.
  Receiver wrap;wrap.start(99);wrap.bind(42);Transmitter wt;wt.connect(99);
  wt.enqueue(pulse(UINT32_MAX));wt.enqueue(pulse(uint64_t(UINT32_MAX)+1));
  auto w1=wt.submit(0,uint64_t(UINT32_MAX)*40000+1),w2=wt.submit(1,(uint64_t(UINT32_MAX)+1)*40000+1);
  deliver(wrap,w2,1);deliver(wrap,w1,2);assert(wrap.received==2 && wrap.current.sequence==0);
  // Every native CRC bit and outer envelope are validated before ACK ownership.
  Receiver corrupt;corrupt.start(100);corrupt.bind(43);
  auto damaged=fresh;++damaged.pulse.nominalUm;seal(damaged);deliver(corrupt,damaged,0);
  damaged=fresh;++damaged.crc;deliver(corrupt,damaged,0);assert(!corrupt.received);
  // Type-7 gaps are status discontinuities, not physical missing events.
  Status st{};st.session=100;st.txBoot=43;st.sequence=2;seal(st);assert(r.observeStatus(st,1));
  st.sequence=5;seal(st);assert(r.observeStatus(st,2));assert(r.statusGaps==3 && r.missing==0);
  assert(!r.observeStatus(st,3) && r.statusOrder==1);
  assert(timing(1000001)==Timing::Late && timing(1,1000000)==Timing::Timely);
  assert(timing(1,1000001)==Timing::Uncertain && timing(1)==Timing::Uncertain);
  Receiver queued;queued.start(100);queued.bind(43);AckItem ignored{};
  assert(queued.accept(fresh,100,1000200,ignored)==1);
  assert(timing(queued.acceptanceLowerUs)==Timing::Late);
  assert(queued.currentLowerUs==fresh.ageUs); // Queue delay belongs to local age, not native timestamp.
  const uint8_t allowed[6]={2,1,2,3,4,5},unknown[6]={4,1,2,3,4,5};
  assert(configured(allowed) && !sameMac(allowed,unknown));
  std::puts("PASS selective ACK loss/recovery, immutable native evidence, nonblocking window, slot reuse, backlog, cadence, reboot, rollover, validation, status gaps and timing");
}
