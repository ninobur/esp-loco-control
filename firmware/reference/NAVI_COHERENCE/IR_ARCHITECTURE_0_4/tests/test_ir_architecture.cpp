#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include "MmDistanceReference.h"

using namespace ngr_nav;
using ir_movement::WireSnapshot;

static WireSnapshot sample(uint64_t us, uint64_t pulses,
                           uint8_t reason = ir_movement::TRACKING) {
  WireSnapshot w;
  w.bootId = 7;
  w.calibrationId = 1;
  w.capturedUs = us;
  w.completedPulses = w.observedRises = pulses;
  w.nominalUm = pulses * w.pitchUm;
  w.opticalReason = reason;
  return w; // Already-decoded fixtures; transport CRC/source tests are separate.
}
static bool close(double a, double b) { return std::abs(a-b) < 0.000001; }
static void synchronize(MmDistanceReference& r, const IrOdometryEpoch& o) {
  assert(r.synchronize(40,1000,o.point(),o));
}

static void classification() {
  for(uint8_t reason=0;reason<=ir_movement::TRACKING;++reason) {
    auto s=classifyIrInstrument(sample(1000,0,reason));
    const bool ready=reason==ir_movement::TRACKING || reason==ir_movement::SIGNAL_STALE;
    const bool initializing=reason==ir_movement::PRIMING || reason==ir_movement::REACQUIRING;
    assert(s.healthy==(ready || initializing));
    assert(s.measurementReady()==ready);
    assert(s.detectorReason==reason);
    assert(std::strlen(irHealthName(s.fault))>0 && std::strlen(irReadinessName(s.readiness))>0);
  }
  auto w=sample(1000,0,255);
  assert(classifyIrInstrument(w).fault==IrHealthFault::PacketInvalid);
  w=sample(1000,0);w.bootId=0;
  assert(classifyIrInstrument(w).fault==IrHealthFault::CalibrationFault);
  w=sample(1000,0);w.calibrationId=0;
  assert(!classifyIrInstrument(w).healthy);
  w=sample(1000,0);w.pitchUm=0;
  assert(!classifyIrInstrument(w).healthy);
}

static void startup() {
  IrOdometryEpoch o;MmDistanceReference r;
  assert(!o.epochDistance().available && !r.distanceFromMm(o).available);
  assert(!r.synchronize(40,0,o.point(),o));
  auto u=o.ingest(sample(1000,0,ir_movement::PRIMING));
  assert(!u.epochStarted && !u.epochEnded && !o.haveMeasurement());
  u=o.ingest(sample(2000,0,ir_movement::REACQUIRING));
  assert(!u.epochStarted && !u.epochEnded && o.epochId()==0);
  u=o.ingest(sample(3000,10));
  assert(u.epochStarted && u.measurementReady && u.epochId==1);
  assert(u.reason==IrEpochBreak::FirstObservation);
  assert(o.epochDistance().available && o.epochDistance().mm==0);
  assert(!r.validFor(o));
  synchronize(r,o);
  assert(r.mm()==40 && r.hallDetectedAtMs()==1000 && r.capturedUs()==3000);
}

static void stopDwellRestart() {
  IrOdometryEpoch o;MmDistanceReference r;
  o.ingest(sample(1000,10));synchronize(r,o);
  auto u=o.ingest(sample(1000000,20));
  assert(!u.epochStarted && close(r.distanceFromMm(o).mm,96.52));
  for(uint64_t sec=2;sec<=30;++sec) {
    auto w=sample(sec*1000000,20,ir_movement::SIGNAL_STALE);
    w.unreliableSamples=sec*1000;
    u=o.ingest(w);
    assert(u.measurementReady && !u.epochEnded && !u.epochStarted);
    assert(o.epochId()==1 && r.validFor(o));
    assert(close(r.distanceFromMm(o).mm,96.52));
  }
  auto w=sample(31000000,21);w.unreliableSamples=30000;
  o.ingest(w);
  assert(o.epochId()==1 && close(r.distanceFromMm(o).mm,106.172));
}

static void outage(uint8_t reason) {
  IrOdometryEpoch o;MmDistanceReference old;
  o.ingest(sample(1000,100));synchronize(old,o);
  const auto before=o.point();
  auto u=o.ingest(sample(2000,100,reason));
  assert(u.epochEnded && !u.epochStarted && !old.validFor(o));
  const auto cause=u.reason;
  for(unsigned i=3;i<=30;++i) {
    u=o.ingest(sample(i*100000,100,reason));
    assert(!u.epochEnded && !u.epochStarted && !o.epochDistance().available);
  }
  u=o.ingest(sample(3100000,100));
  assert(u.epochStarted && !u.epochEnded && u.epochId==2 && u.reason==cause);
  assert(!old.validFor(o) && !old.distanceFromMm(o).available);
  assert(!o.contains(before));
  assert(o.epochDistance().available && o.epochDistance().mm==0);
  o.ingest(sample(3200000,103));
  assert(close(o.epochDistance().mm,28.956) && !old.validFor(o));
  assert(old.synchronize(41,3200,o.point(),o));
  assert(old.validFor(o) && old.mm()==41 && old.distanceFromMm(o).mm==0);
}

static void discontinuities() {
  for(unsigned change=0;change<11;++change) {
    IrOdometryEpoch o;MmDistanceReference old;
    auto a=sample(1000,100);
    a.sampleGaps=a.saturatedSamples=a.openAborts=a.inferredAdded=a.inferredRemoved=2;
    o.ingest(a);synchronize(old,o);
    auto b=a;b.capturedUs=2000;
    IrEpochBreak expected=IrEpochBreak::None;
    switch(change) {
      case 0: ++b.sampleGaps;expected=IrEpochBreak::SampleGapOccurred;break;
      case 1: ++b.saturatedSamples;expected=IrEpochBreak::SaturationOccurred;break;
      case 2: ++b.openAborts;expected=IrEpochBreak::PulseAbortOccurred;break;
      case 3: ++b.inferredAdded;expected=IrEpochBreak::InferenceOccurred;break;
      case 4: ++b.inferredRemoved;expected=IrEpochBreak::InferenceOccurred;break;
      case 5: ++b.bootId;b.capturedUs=1;b.completedPulses=b.observedRises=0;expected=IrEpochBreak::BootChanged;break;
      case 6: ++b.calibrationId;expected=IrEpochBreak::CalibrationChanged;break;
      case 7: ++b.pitchUm;expected=IrEpochBreak::PitchChanged;break;
      case 8: --b.completedPulses;expected=IrEpochBreak::CounterOrder;break;
      case 9: --b.sampleGaps;expected=IrEpochBreak::CounterOrder;break;
      default: a.observedRises=102;o.reset();o.ingest(a);synchronize(old,o);
        b=a;b.capturedUs=2000;--b.observedRises;expected=IrEpochBreak::CounterOrder;break;
    }
    b.nominalUm=b.completedPulses*b.pitchUm;
    const auto previous=o.epochId();
    const auto u=o.ingest(b);
    assert(u.epochEnded && u.epochStarted && u.reason==expected && u.epochId==previous+1);
    assert(!old.validFor(o) && o.epochDistance().available && o.epochDistance().mm==0);
    b.capturedUs+=1000;
    o.ingest(b);
    assert(o.epochId()==previous+1 && !old.validFor(o));
  }
}

static void linkAndReset() {
  for(auto reason:{IrEpochBreak::LinkStale,IrEpochBreak::TransportGap,
                   IrEpochBreak::InstrumentUnavailable,IrEpochBreak::Reset,IrEpochBreak::SourceChanged}) {
    IrOdometryEpoch o;MmDistanceReference old;
    o.ingest(sample(1000,100));synchronize(old,o);
    auto u=reason==IrEpochBreak::Reset?o.reset():
           reason==IrEpochBreak::SourceChanged?o.sourceChanged():o.endEpoch(reason);
    assert(u.epochEnded && !o.haveMeasurement() && !old.validFor(o));
    assert(!o.endEpoch(reason).epochEnded);
    u=o.ingest(sample(2000,105));
    assert(u.epochStarted && u.epochId==2 && u.reason==reason);
    assert(!old.validFor(o));
    assert(o.epochDistance().available && o.epochDistance().mm==0);
    for(unsigned i=0;i<100;++i) {
      o.reset();o.ingest(sample(3000+i*1000,106+i));
      assert(!old.validFor(o));
    }
  }
}

static void orderAndOwnership() {
  IrOdometryEpoch o, other;MmDistanceReference r;
  o.ingest(sample(2000,100));synchronize(r,o);
  other.ingest(sample(2000,100));
  assert(!r.validFor(other));
  auto u=o.ingest(sample(1000,99));
  assert(u.epochEnded && !u.snapshotAccepted && u.reason==IrEpochBreak::TimeOrder);
  assert(!o.haveMeasurement() && !r.validFor(o));
  u=o.ingest(sample(2000,100));
  assert(!u.epochStarted && !o.haveMeasurement());
  u=o.ingest(sample(3000,101));
  assert(u.epochStarted && u.reason==IrEpochBreak::TimeOrder && !r.validFor(o));
}

static void alignedReference() {
  IrOdometryEpoch o;MmDistanceReference r;
  o.ingest(sample(1000000,100));
  const auto atHall=o.point();
  o.ingest(sample(1400000,110)); // Hall judgment is delayed; keep the opening point.
  assert(r.synchronize(40,1000,atHall,o));
  assert(close(r.distanceFromMm(o).mm,96.52));
  assert(r.capturedUs()==1000000 && r.hallDetectedAtMs()==1000);
  o.endEpoch(IrEpochBreak::LinkStale);o.ingest(sample(1500000,112));
  assert(!r.validFor(o) && !r.synchronize(40,1000,atHall,o));
  assert(!r.distanceFromMm(o).available);
}

// This deliberately documents a TX limitation, not a claimed stationary fix:
// a finite changing-signal envelope can collapse at rest with this detector.
static void actualDetectorStop() {
  ir_movement::Measurement detector(7,1,9.652);
  uint64_t us=1000;
  for(unsigned i=0;i<2000;++i,us+=1000)detector.sample(us,(i/20)%2?2500:1000);
  assert(detector.snapshot().reason==ir_movement::TRACKING);
  for(unsigned i=0;i<4000;++i,us+=1000)detector.sample(us,1000);
  assert(detector.snapshot().reason==ir_movement::INADEQUATE_CONTRAST);
}

int main() {
  classification();startup();stopDwellRestart();
  for(auto reason:{ir_movement::INADEQUATE_CONTRAST,ir_movement::REACQUIRING,
                   ir_movement::PRIMING,ir_movement::SATURATION,ir_movement::SAMPLE_GAP})outage(reason);
  discontinuities();linkAndReset();orderAndOwnership();alignedReference();actualDetectorStop();
  std::cout<<"PASS IR architecture: health/readiness, zero/dwell, 5 outage types, "
              "11 hidden changes, link/reset/source, order/ownership, aligned MM reference\n"
              "CONFIRMED existing detector stationary contrast limitation (not changed)\n";
}
