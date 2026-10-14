#define main dart_scenario_main
#include "test_dart.cpp"
#undef main
#include "../NaviPulseObservation.h"
int main(){
 // Preserve 0120's intervention latch in both directions; no sensor recovery.
 for(int direction:{-1,1}){Fixture f;f.start(45,direction);const auto mm=f.n.mm();
  f.pwm=0;for(unsigned i=1;i<=28;++i){f.ir(i);f.field(1);CHECK(f.n.mm()==mm);CHECK(f.n.pwmZeroMovementRequiresDeclaration());}
  f.pwm=40;f.ir(100);f.field(-1);f.complete();CHECK(f.n.mm()==mm);
  f.n.declare(45,direction,++f.t);f.field(f.n.target().polarity==HallOpeningPolarity::AboveReference?1:-1);
  CHECK(!f.n.pwmZeroMovementRequiresDeclaration());CHECK(f.n.confirmedCount()==2);
 }
 // Motionless dwell preserves reference; no implicit departure reset.
 {Fixture f;f.start();const auto origin=f.n.physicalOriginUm_;f.pwm=0;f.ir(0);f.tick(2000);
  CHECK(!f.n.pwmZeroMovementRequiresDeclaration());CHECK(f.n.physicalOriginUm_==origin);
  f.pwm=40;f.ir(32);f.field(-1);CHECK(f.n.confirmedCount()==2);}
 // A partial native batch cannot create a qualifying median or splice ticks.
 {Fixture f;f.start();auto before=f.n.openingCount();f.serial+=2;f.tick(1900);CHECK(f.n.incompleteBatches()>0);CHECK(f.n.openingCount()==before);}
 // Metadata changes while sampling do not discard ADC values (PWM ramp).
 {Fixture f;f.start();f.ir(32);f.tick(2000);for(int tick=0;tick<2;++tick){f.t+=1000;for(int i=0;i<5;++i){HallSample h{};h.sampleSerial=++f.serial;h.timestampUs=f.t+i;h.raw=1900;h.pwm=40+i;h.direction=1;f.n.observeHall(h,h.timestampUs);}}CHECK(f.n.mm()==18);}
 // Preserved later observations must be available to the next target after a
 // delayed boundary decision. MM018 south then MM019 south (same polarity).
 {Fixture f;syntheticSeed(f);syntheticOpening(f,250000);syntheticOpening(f,610000,-1);
  CHECK(f.n.confirmedCount()==3);CHECK(f.n.mm()==19);CHECK(f.n.openingIrUm()==610000);}
 // Exact/late selections and no implicit credibility control on discrepancy.
 {Fixture f;syntheticSeed(f);syntheticOpening(f,300000);CHECK(f.n.mm()==18);CHECK(!f.n.inferredAnchor());}
 {Fixture f;syntheticSeed(f);syntheticOpening(f,270000);f.n.selectBest(DartReason::Boundary);
  CHECK(f.n.cumulativeResidualUm()==-30000);f.n.expectedCumulativeUm_=300000;
  syntheticOpening(f,600000,-1);CHECK(f.n.cumulativeResidualUm()==0);CHECK(f.n.relationshipReliable());}
 {Fixture f;syntheticSeed(f);syntheticOpening(f,270000);f.n.selectBest(DartReason::Boundary);
  f.n.expectedCumulativeUm_=300000;syntheticOpening(f,540000,-1);f.n.selectBest(DartReason::Boundary);
  CHECK(f.n.cumulativeResidualUm()==-60000);CHECK(f.n.relationshipReliable());}
 // Timing records do not become a selection route when IR is absent/stale.
 {Fixture f;f.start();f.n.noteOperatingContext(3,f.t);f.ir(32);f.field(-1);CHECK(f.n.timingCount()==1);
  CHECK(f.n.timing(0).departureUs>0);f.t+=2000000;f.field(-1);f.complete();CHECK(f.n.mm()==18);}
 // Actual Type-6 parser failure storms and absent diagnostic consumers have
 // no calls into NAVI; compare full navigation outputs under identical inputs.
 for(unsigned fault=0;fault<6;++fault){Fixture a,b;a.start();b.start();navi_pulse::Observation pulse;
  for(unsigned j=0;j<20;++j){navi_pulse::Rx rx{};rx.length=sizeof(PulseEventPacket);rx.receivedUs=j*1000;
   PulseEventPacket e{};e.magic=0x4952;e.version=1;e.type=6;e.bootId=1;e.sequence=fault==1?1:j+1;
   e.completedUs=j*1000;e.completedPulses=j;e.pitchUm=9652;e.nominalUm=j*9652;
   e.crc=navi_pulse::crc(reinterpret_cast<const uint8_t*>(&e),offsetof(PulseEventPacket,crc));
   if(fault==2)++e.crc;if(fault==3)rx.queueDrops=j;if(fault==4)rx.length=0;
   std::memcpy(rx.bytes,&e,sizeof(e));if(fault!=5)pulse.receive(rx);
   a.ir(25+j);b.ir(25+j);a.field(-1);b.field(-1);a.complete();b.complete();a.drain();
   CHECK(a.n.mm()==b.n.mm());CHECK(a.n.openingIrUm()==b.n.openingIrUm());
   CHECK(a.n.latestIr().sequence==b.n.latestIr().sequence);CHECK(a.n.confirmedCount()==b.n.confirmedCount());
  }
 }
 // Diagnostic overflow is distinct from bounded navigation history overflow.
 {Fixture f;f.start();for(int i=0;i<33;++i){f.field(1);f.drain();}CHECK(f.n.pendingOverflows()>0);CHECK(f.n.comparisonIncomplete());CHECK(f.n.mm()==17);}
 std::printf("PASS %u invariant assertions\n",checks);return 0;
}
