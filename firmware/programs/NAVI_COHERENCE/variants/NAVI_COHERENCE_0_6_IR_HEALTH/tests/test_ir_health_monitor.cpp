#include <cassert>
#include <cmath>
#include <iostream>
#include "IrHealthMonitor.h"
using namespace ngr_nav;
static const uint8_t MAC[6]={2,3,4,5,6,7};
static WireSnapshot packet(uint32_t seq,uint64_t us,uint64_t count,uint8_t reason=ir_movement::TRACKING) {
  WireSnapshot w;w.sequence=seq;w.capturedUs=us;w.completedPulses=w.observedRises=count;
  w.nominalUm=count*w.pitchUm;w.bootId=7;w.calibrationId=1;w.opticalReason=reason;return w;
}
static WireSnapshot seal(WireSnapshot w) {
  w.nominalUm=w.completedPulses*w.pitchUm;
  w.crc=movementCrc(reinterpret_cast<const uint8_t*>(&w),offsetof(WireSnapshot,crc));return w;
}
static void send(IrHealthMonitor& m,WireSnapshot w,uint64_t at,const uint8_t* mac=MAC) {
  w=seal(w);m.receive(mac,reinterpret_cast<const uint8_t*>(&w),sizeof(w),at);
}
static void dump(IrHealthMonitor& m,uint64_t now) {
  char json[960];const int n=m.format(json,sizeof(json),now,0x1234);
  assert(n>0 && n<int(sizeof(json)));std::cout<<json<<'\n';
}
int main() {
  // Exact Pi serial capture: do not re-encode or reseal this wire fixture.
  const char* hex="5249010509310000b894e7bf324232888759e64a000000007d00000000000000770000000000000000000000000000000000000000000000700613000000000000000000000000000100000000000000000000000000000000000000b4250000ac8611000000000001000f001e96";
  uint8_t raw[110];assert(strlen(hex)==sizeof(raw)*2);
  for(size_t i=0;i<sizeof(raw);++i) {
    unsigned byte=0;assert(sscanf(hex+2*i,"%2x",&byte)==1);raw[i]=uint8_t(byte);
  }
  WireSnapshot captured;memcpy(&captured,raw,sizeof(captured));
  assert(captured.calibrationId==0 && captured.completedPulses==119);
  IrHealthMonitor field;field.pair(MAC,0);
  uint64_t at=captured.capturedUs+1000000;
  field.receive(MAC,raw,sizeof(raw),at);
  assert(field.accepted()==1 && field.rejected()==0);
  assert(field.health().fault==IrHealthFault::InadequateContrast);
  assert(!field.odometry().epochActive());dump(field,at);
  auto next=captured;next.sequence++;next.capturedUs+=100000;
  next.opticalReason=ir_movement::TRACKING;at+=100000;send(field,next,at);
  assert(field.health().measurementReady() && field.odometry().epochId()==1);
  assert(field.acceptedMm(41,0,at,1,at));dump(field,at);
  next.sequence++;next.capturedUs+=100000;next.opticalReason=ir_movement::SIGNAL_STALE;
  at+=100000;send(field,next,at);
  assert(field.reference().validFor(field.odometry()));
  assert(field.reference().distanceFromMm(field.odometry()).mm==0);
  next.sequence++;next.capturedUs+=100000;next.opticalReason=ir_movement::INADEQUATE_CONTRAST;
  at+=100000;send(field,next,at);assert(!field.reference().validFor(field.odometry()));
  next.sequence++;next.capturedUs+=100000;next.opticalReason=ir_movement::TRACKING;
  at+=100000;send(field,next,at);assert(field.odometry().epochId()==2);
  assert(!field.reference().validFor(field.odometry()));
  next.sequence++;next.capturedUs+=100000;next.calibrationId=1;
  at+=100000;send(field,next,at);assert(field.odometry().epochId()==3);
  next.sequence++;next.capturedUs+=100000;next.calibrationId=0;
  at+=100000;send(field,next,at);assert(field.odometry().epochId()==4);
  IrHealthMonitor m;m.pair(MAC,0);dump(m,1);
  send(m,packet(1,100000,10,ir_movement::PRIMING),1100000);
  assert(m.health().healthy && !m.odometry().haveMeasurement());dump(m,1100000);
  send(m,packet(2,200000,10),1200000);
  assert(m.odometry().epochId()==1);
  assert(m.acceptedMm(40,1200,1200000,1,1200000));
  for(uint32_t sec=1;sec<=30;++sec) {
    auto w=packet(2+sec,200000+sec*1000000ULL,10,ir_movement::SIGNAL_STALE);
    w.unreliableSamples=sec*1000;send(m,w,1200000+sec*1000000ULL);
    assert(m.odometry().epochId()==1 && m.reference().validFor(m.odometry()));
  }
  dump(m,31200000);
  send(m,packet(33,30300000,12),31300000);
  assert(std::abs(m.reference().distanceFromMm(m.odometry()).mm-19.304)<0.000001);
  m.invalidateReference();assert(m.odometry().epochId()==1 && !m.reference().validFor(m.odometry()));
  assert(m.acceptedMm(41,31300,31300000,2,31300000));
  m.tick(32300001);assert(m.health().fault==IrHealthFault::LinkStale && !m.odometry().haveMeasurement());
  const auto ends=m.ends();m.tick(33300000);assert(m.ends()==ends);dump(m,33300000);
  // Duplicates cannot restore freshness or measurement readiness.
  send(m,packet(33,30300000,12),33400000);
  assert(m.duplicates()==1 && !m.odometry().haveMeasurement());
  send(m,packet(34,32500000,15),33500000);
  assert(m.odometry().epochId()==2 && !m.reference().validFor(m.odometry()));dump(m,33500000);

  IrHealthMonitor aligned;aligned.pair(MAC,0);
  send(aligned,packet(1,100000,10),1100000);
  send(aligned,packet(2,200000,15),1200000);
  send(aligned,packet(3,300000,20),1300000);
  send(aligned,packet(4,400000,30),1400000);
  assert(aligned.acceptedMm(42,1100,1100000,3,1400000));
  assert(std::abs(aligned.reference().distanceFromMm(aligned.odometry()).mm-193.04)<0.000001);
  dump(aligned,1400000);
  assert(!aligned.acceptedMm(42,800,800000,4,1400000)); // >150 ms alignment error.
  send(aligned,packet(5,500000,30,ir_movement::INADEQUATE_CONTRAST),1500000);
  send(aligned,packet(6,600000,31),1600000);
  assert(aligned.odometry().epochId()==2);
  assert(!aligned.acceptedMm(43,1500,1500000,5,1600000)); // nearest snapshot was invalid.
  assert(!aligned.acceptedMm(42,1400,1400000,6,1600000)); // old epoch.
  assert(aligned.acceptedMm(43,1600,1600000,7,1600000));
  dump(aligned,1600000);
  // Gap must be noticed in receive even if tick was delayed by a backlog.
  send(aligned,packet(7,1700001,32),2700001);
  assert(aligned.odometry().epochId()==3 && !aligned.reference().validFor(aligned.odometry()));
  aligned.queueGap(2800000);
  send(aligned,packet(8,1750000,33),2750000); // queued before the loss barrier.
  assert(!aligned.odometry().haveMeasurement());
  send(aligned,packet(9,1900000,34),2900000);
  assert(aligned.odometry().epochId()==4);dump(aligned,2900000);

  IrHealthMonitor validation;validation.pair(MAC,0);
  auto a=packet(1,100000,10);send(validation,a,1100000);
  uint8_t foreign[6]={2,9,9,9,9,9};send(validation,a,1200000,foreign);
  assert(validation.accepted()==1 && validation.odometry().epochId()==1);
  auto corrupt=seal(packet(2,200000,11));corrupt.crc^=1;
  validation.receive(MAC,reinterpret_cast<const uint8_t*>(&corrupt),sizeof(corrupt),1200000);
  assert(validation.rejected()==1 && !validation.odometry().haveMeasurement());dump(validation,1200000);
  send(validation,packet(3,300000,12),1300000);
  assert(validation.odometry().epochId()==2);
  auto w=packet(4,400000,13);w.sampleGaps=1;send(validation,w,1400000);
  assert(validation.odometry().epochId()==3);
  // Calibration transitions are observed without changing the legacy adapter.
  MovementSource legacy;legacy.pair(MAC);auto first=seal(w);
  assert(legacy.receive(MAC,reinterpret_cast<const uint8_t*>(&first),sizeof(first),1400000)==RxResult::Accepted);
  w.sequence=5;w.capturedUs=500000;w.calibrationId=2;send(validation,w,1500000);
  auto changed=seal(w);
  assert(legacy.receive(MAC,reinterpret_cast<const uint8_t*>(&changed),sizeof(changed),1500000)==RxResult::Old);
  assert(validation.odometry().epochId()==4 && validation.health().healthy);
  w.sequence=1;w.capturedUs=1000;w.bootId=8;send(validation,w,1600000);
  assert(validation.odometry().epochId()==5);
  send(validation,changed,1700000); // retired boot must not return.
  assert(validation.odometry().epochId()==5 && validation.accepted()==5);
  validation.pair(foreign,1800000);
  send(validation,w,1800001);assert(!validation.odometry().haveMeasurement());
  send(validation,w,1800001,foreign);assert(validation.odometry().epochId()==6);
  dump(validation,1800001);

  IrHealthMonitor big;big.pair(MAC,0);
  auto huge=packet(UINT32_MAX,uint64_t(INT64_MAX/4)-1000000,0);
  huge.bootId=UINT64_MAX;huge.calibrationId=UINT32_MAX;huge.pitchUm=1;
  huge.completedPulses=huge.observedRises=UINT64_MAX;
  send(big,huge,huge.capturedUs+1000000);dump(big,huge.capturedUs+1000000);
  std::cerr<<"PASS shadow monitor: health/zero, watchdog, alignment, epoch boundaries, "
             "queued-loss fence, CRC/source/order, calibration/reboot and JSON sizing\n";
}
