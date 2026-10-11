#include "../NaviIntegratedCore.h"
#include <cassert>
#include <cstdio>
#include <type_traits>

using navi_eyes::NaviIntegratedCore;
using navi_eyes::HallSample;

// A read-only caller cannot accidentally evaluate freshness at the last input's
// frozen timestamp. Time must be explicit, including when both streams stop.
static_assert(!std::is_invocable_v<decltype(&NaviIntegratedCore::irMagnetVetoAvailable),
                                   const NaviIntegratedCore&>);
static_assert(ngr_evidence::HallQualifier::kThreshold == 70);
static_assert(ngr_evidence::HallQualifier::kGuardUs == 650000);
static_assert(NaviIntegratedCore::kIrFreshUs == 1000000);
static_assert(ir_movement::kInstalledPitchUm == 9652);

struct Pipeline {
  NaviIntegratedCore n;
  navi_pulse::Observation receiver;
  uint64_t now=100000, count=0, lastCompletedUs=0;
  uint32_t pulseSeq=0, snapshotSeq=0, hallSeq=0;
  uint8_t mac[6]={1,2,3,4,5,6};

  Pipeline() {
    hall(1000);
    snapshot();
    n.declare(27,1,++now);
    neutral();
  }
  void snapshot() {
    ir_movement::WireSnapshot w{};
    w.bootId=42; w.sequence=++snapshotSeq; w.capturedUs=now;
    w.completedPulses=w.observedRises=count;
    w.nominalUm=count*uint64_t(w.pitchUm);
    w.opticalReason=ir_movement::TRACKING;
    n.observeIr(w,now,90,mac);
  }
  void deliver(PulseEventPacket e) {
    e.crc=navi_pulse::crc(reinterpret_cast<const uint8_t*>(&e),offsetof(PulseEventPacket,crc));
    navi_pulse::Rx rx{};
    std::memcpy(rx.mac,mac,6); rx.receivedUs=now; rx.length=sizeof(e);
    std::memcpy(rx.bytes,&e,sizeof(e));
    receiver.receive(rx);
    n.observePulse(receiver.state(),90,1,now);
  }
  void pulse(uint64_t dt=60000,bool delivered=true) {
    now+=dt; ++count;
    PulseEventPacket e{};
    e.magic=0x4952; e.version=1; e.type=6; e.sid=42; e.bootId=42;
    e.sequence=++pulseSeq; e.completedPulses=count; e.completedUs=now;
    e.intervalUs=lastCompletedUs?now-lastCompletedUs:0; lastCompletedUs=now;
    e.pitchUm=ir_movement::kInstalledPitchUm;
    e.nominalUm=count*uint64_t(e.pitchUm); e.opticalReason=ir_movement::TRACKING;
    snapshot();
    if(delivered)deliver(e);
  }
  void hall(int raw) {
    HallSample h{};
    h.sampleSerial=++hallSeq; h.timestampUs=now+=1000;
    h.raw=raw; h.pwm=90; h.direction=1;
    n.observeHall(h,now);
  }
  void neutral(){hall(n.activeReference()); hall(n.activeReference());}
  bool valid()const{return n.irMagnetVetoAvailable(now);}
  void warm(){for(unsigned i=0;i<9;++i)pulse(); assert(valid());}
  void anchor(){warm(); hall(1070); hall(1070); assert(n.mm()==28);}
  void advance(unsigned pulses) {
    // Every event carries the actual elapsed fixture time, including Hall work.
    for(unsigned i=0;i<pulses;++i)pulse(i?10000:13000);
  }
};

static void mappedDistanceAndThresholds() {
  // Real parser -> actual core -> Hall qualifier. Quantized measured distance
  // is tested against the unchanged map, on both sides of both tolerance edges.
  for(int percent:{84,90,100,114,116}) {
    Pipeline p; p.anchor(); p.neutral();
    const uint64_t expected=uint64_t(p.n.target().distanceMm)*1000;
    const unsigned pulses=(expected*percent/100+4826)/9652;
    p.advance(pulses);
    assert(p.valid());
    const uint64_t elapsed=p.now-p.n.dnaHistory().at(0).encounter.detectedUs;
    assert(elapsed<650000); // Admission cannot be attributed to fallback.
    p.hall(1069); p.hall(1069); assert(p.n.confirmedCount()==1);
    p.hall(1070); assert(p.n.confirmedCount()==1);
    p.hall(1070);
    const bool inside=uint64_t(pulses)*9652*100>=expected*85 &&
                      uint64_t(pulses)*9652*100<=expected*115;
    assert(p.n.confirmedCount()==(inside?2:1));
    assert(p.n.mm()==(inside?29:28));
  }
}

static void deliveryLossAndFallback() {
  for(uint64_t elapsed:{uint64_t(649999),uint64_t(650000)}) {
    Pipeline p; p.anchor(); const uint64_t detected=p.now; p.neutral();
    p.pulse(64000,false); // A generated event is actually withheld from RX.
    p.pulse();
    assert(p.receiver.state().flags&navi_pulse::SEQUENCE_GAP);
    assert(p.receiver.state().flags&navi_pulse::PULSE_GAP);
    assert(!(p.receiver.state().flags&navi_pulse::INTERVAL_MISMATCH));
    assert(p.receiver.state().missing==1 && !p.valid());
    assert(p.n.latestIr().completedPulses==p.count); // No invented count.
    p.now=detected+elapsed-2000; p.snapshot();
    p.hall(1070); p.hall(1070);
    assert(p.n.confirmedCount()==(elapsed==650000?2:1));
    // Only two pulse pitches since the last magnet: invalid IR cannot reject
    // the encounter after 650 ms or permit it before 650 ms.
  }
}

static void timingVariationAndExpiry() {
  Pipeline p; p.warm();
  for(uint64_t dt:{60000,100000,60000,180000,60000,90000,60000}) {
    p.pulse(dt); assert(p.receiver.state().eventValid && p.valid());
  }
  assert(p.n.irTimingQuestions()>0); // Diagnostics survive, without a veto.
  const uint64_t last=p.now;
  assert(p.n.irMagnetVetoAvailable(last+1000000));
  assert(!p.n.irMagnetVetoAvailable(last+1000001));
  assert(!p.n.irMagnetVetoAvailable(last+100000000)); // No input needed to expire.
  for(unsigned i=0;i<15;++i){p.now+=100000; p.snapshot();}
  assert(!p.valid()); // Current Type-5 cannot refresh absent Type-6.
  p.pulse(1560000); assert(!p.valid()); // Old timing population is not inherited.
  for(unsigned i=0;i<7;++i)p.pulse();
  assert(p.valid());
}

static void detectedInterruptedCycle() {
  // Use the production detector and transmitter: a supplied low/high/low cycle
  // is interrupted by a saturated sample while open. The completion is lost;
  // the next Type-6 reports unavailable interval evidence, not a timing guess.
  ir_movement::Measurement measurement{42,0,9.652,true};
  PulseEventEvidence tx;
  Pipeline p;
  auto sample=[&](uint16_t raw) {
    p.now+=1000; measurement.sample(p.now,raw);
    const auto s=measurement.snapshot();
    p.count=s.completedPulses; p.snapshot();
    PulseEventPacket wire{};
    if(tx.observe(s,measurement.detector(),42,wire))p.deliver(wire);
  };
  auto plateau=[&](uint16_t raw){for(unsigned i=0;i<20;++i)sample(raw);};
  auto cycle=[&](){plateau(2000); plateau(1000);};
  for(unsigned i=0;i<30;++i)cycle();
  assert(p.valid());
  const auto before=measurement.snapshot();
  plateau(2000); assert(measurement.detector().inPulse());
  sample(4095); plateau(1000);
  const auto interrupted=measurement.snapshot();
  assert(interrupted.observedRises==before.observedRises+1);
  assert(interrupted.completedPulses==before.completedPulses);
  assert(interrupted.openAborts>before.openAborts);
  assert(interrupted.saturatedSamples==before.saturatedSamples+1);
  cycle();
  assert(p.receiver.state().flags&navi_pulse::NO_INTERVAL);
  assert(!p.receiver.state().eventValid && !p.valid());
  assert(p.count==before.completedPulses+1); // The lost cycle was not filled in.
  for(unsigned i=0;i<9;++i)cycle();
  assert(p.valid());
  std::puts("PASS: detected interrupted optical cycle invalidates IR; continuous evidence restores it");
}

int main() {
  mappedDistanceAndThresholds(); deliveryLossAndFallback();
  timingVariationAndExpiry(); detectedInterruptedCycle();
  std::puts("PASS: parser-to-Hall distance/fallback selection, 70 x 2, timing variation, expiry and recovery");
  std::puts("LIMITATION: an entirely undetected optical cycle cannot be distinguished from slower rotation by these packets");
}
