#pragma once

#include <cstring>
#include "../IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX/PulseEventEvidence.h"

// Type-6 acquisition only. It has no NAVI, route, PWM, MQTT, task, or actuator
// dependency; the integrated sketch reports this state as OBSERVATION_ONLY.
namespace navi_pulse {
enum Flag : uint32_t {
  BAD_WIRE=1, SOURCE_MISMATCH=2, BOOT_CHANGE=4, SEQUENCE_GAP=8,
  COUNT_GAP=16, ORDER_FAULT=32, TIME_FAULT=64, FIRST=128,
  SPEED_UNAVAILABLE=256, OPTICAL_DEGRADED=512, QUEUE_REPLACED=1024
};
inline uint16_t crc(const uint8_t* p,size_t n) {
  uint16_t c=0xffff; while(n--){c^=uint16_t(*p++)<<8;
    for(unsigned i=0;i<8;++i)c=c&0x8000?uint16_t((c<<1)^0x1021):uint16_t(c<<1);}
  return c;
}
inline bool isPulseFrame(const uint8_t* p,int n) {
  return n==int(sizeof(PulseEventPacket)) ||
    (n>=4 && p[0]==0x52 && p[1]==0x49 && p[3]==6);
}
struct Rx { uint8_t mac[6]{}; uint64_t receivedUs=0; uint32_t replaced=0;
  uint16_t length=0; uint8_t bytes[sizeof(PulseEventPacket)]{}; };
struct State {
  PulseEventPacket event{}; uint8_t mac[6]{};
  bool have=false,current=false,speedValid=false;
  uint64_t receivedUs=0,distanceAdvancedUm=0,physicalIntervalUs=0;
  double averageMmps=0;
  uint32_t flags=FIRST,received=0,accepted=0,invalid=0,discontinuities=0;
  uint32_t missing=0,sourceMismatches=0,bootChanges=0,orderFaults=0;
  uint32_t queueReplacements=0;
};
class Observation {
 public:
  const State& state() const { return state_; }
  void receive(const Rx& rx) {
    // Rejections are diagnostic events.  They must never erase the last
    // accepted physical endpoint or its derived gap-average measurement.
    auto& s=state_; ++s.received; s.flags=0;
    if(rx.replaced!=replacements_){s.flags|=QUEUE_REPLACED;
      s.queueReplacements+=rx.replaced-replacements_;replacements_=rx.replaced;}
    PulseEventPacket e{};if(rx.length==sizeof(e))std::memcpy(&e,rx.bytes,sizeof(e));
    if(rx.length!=sizeof(e)||e.magic!=0x4952||e.version!=1||e.type!=6||!e.bootId||
       !e.completedPulses||!e.completedUs||e.pitchUm!=ir_movement::kInstalledPitchUm||
       e.completedPulses>UINT64_MAX/e.pitchUm||e.nominalUm!=e.completedPulses*uint64_t(e.pitchUm)||
       e.opticalReason>ir_movement::TRACKING||crc(rx.bytes,offsetof(PulseEventPacket,crc))!=e.crc){
      s.flags|=BAD_WIRE;++s.invalid;return;
    }
    if(!have_){accept(e,rx);s.flags|=FIRST;return;}
    // A different source is not a continuation of the established instrument.
    if(std::memcmp(rx.mac,priorMac_,6)!=0){s.flags|=SOURCE_MISMATCH;++s.sourceMismatches;++s.invalid;return;}
    if(e.bootId!=prior_.bootId){++s.bootChanges;++s.discontinuities;accept(e,rx);s.flags|=BOOT_CHANGE|FIRST;return;}
    if(e.sid!=prior_.sid){s.flags|=SOURCE_MISMATCH;++s.sourceMismatches;++s.invalid;return;}
    const uint32_t sequenceStep=e.sequence-prior_.sequence;
    if(sequenceStep==0||sequenceStep>=0x80000000u||e.completedPulses<=prior_.completedPulses||e.completedUs<=prior_.completedUs){
      s.flags|=(sequenceStep==0||sequenceStep>=0x80000000u)?ORDER_FAULT:TIME_FAULT;
      ++s.orderFaults;++s.invalid;return; // Never move the trusted endpoint backward.
    }
    const uint64_t countStep=e.completedPulses-prior_.completedPulses;
    if(countStep!=sequenceStep){s.flags|=ORDER_FAULT;++s.orderFaults;++s.invalid;return;}
    const uint64_t elapsed=e.completedUs-prior_.completedUs;
    if(sequenceStep>1){s.flags|=SEQUENCE_GAP|COUNT_GAP;s.missing+=sequenceStep-1;++s.discontinuities;}
    const bool usable=e.opticalReason==ir_movement::TRACKING||e.opticalReason==ir_movement::SIGNAL_STALE;
    if(!usable)s.flags|=OPTICAL_DEGRADED;
    if(!elapsed||!usable)s.flags|=SPEED_UNAVAILABLE;
    accept(e,rx);s.distanceAdvancedUm=countStep*uint64_t(e.pitchUm);s.physicalIntervalUs=elapsed;
    // Endpoint average over cumulative physical distance/time; no synthetic pulses.
    if(usable&&elapsed){s.speedValid=true;s.averageMmps=double(s.distanceAdvancedUm)*1000.0/double(elapsed);}
  }
 private:
  void accept(const PulseEventPacket& e,const Rx& rx) {
    state_.event=e;std::memcpy(state_.mac,rx.mac,6);state_.receivedUs=rx.receivedUs;
    state_.have=true;state_.current=true;state_.speedValid=false;
    state_.distanceAdvancedUm=0;state_.physicalIntervalUs=0;state_.averageMmps=0;
    ++state_.accepted;prior_=e;
    std::memcpy(priorMac_,rx.mac,6);have_=true;
  }
  State state_{};PulseEventPacket prior_{};uint8_t priorMac_[6]{};
  uint32_t replacements_=0;bool have_=false;
};
struct Report { State pulse;uint64_t comparedUs=0,legacyBoot=0;double legacyPkph=0;
  bool legacyValid=false,legacySameSource=false,coupled=false; };
} // namespace navi_pulse
