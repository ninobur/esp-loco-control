#pragma once
#include <cstring>
// Reuse the exact committed transmitter contract; do not fork its wire layout.
#include "../IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX/PulseEventEvidence.h"
#include "../IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX/PulseTransportStatus.h"

namespace navi_pulse {
enum Flag : uint32_t {
  BAD_WIRE=1, SOURCE_CHANGE=2, BOOT_CHANGE=4, SEQUENCE_GAP=8,
  PULSE_GAP=16, TIME_ORDER=32, INTERVAL_MISMATCH=64, NO_INTERVAL=128,
  OPTICAL_UNUSABLE=256, QUEUE_LOSS=512, FIRST=1024,
  SEQUENCE_ORDER=2048, PULSE_ORDER=4096, RECEIVE_TIME_ORDER=8192
};
inline uint16_t crc(const uint8_t* bytes, size_t length) {
  uint16_t c=0xffff;
  while(length--){c^=uint16_t(*bytes++)<<8;
    for(unsigned i=0;i<8;++i)c=c&0x8000?uint16_t((c<<1)^0x1021):uint16_t(c<<1);}
  return c;
}
// Route recognizable Type-6 frames before the operational queue, even with a
// malformed length. All remaining traffic keeps its existing Type-5 parser.
inline bool isPulseFrame(const uint8_t* bytes, int length) {
  return length==int(sizeof(PulseEventPacket)) ||
    (length>=4 &&
     bytes[0]==0x52 && bytes[1]==0x49 && bytes[3]==6);
}
inline bool isPulseStatusFrame(const uint8_t* bytes,int length) {
  return length==int(sizeof(PulseTransportStatusPacket)) ||
    (length>=4 && bytes[0]==0x52 && bytes[1]==0x49 && bytes[2]==1 && bytes[3]==7);
}
struct Rx {
  uint8_t mac[6]{};
  uint64_t receivedUs=0;
  uint32_t queueDrops=0;
  uint16_t length=0;
  uint8_t bytes[sizeof(PulseEventPacket)]{};
};
struct State {
  PulseEventPacket event{};
  uint8_t mac[6]{};
  bool have=false,eventValid=false;
  uint64_t receivedUs=0;
  double mmps=0;
  uint32_t flags=FIRST,received=0,accepted=0,invalid=0,discontinuities=0;
  uint32_t sequenceBreaks=0,pulseBreaks=0,timeBreaks=0,intervalBreaks=0;
  uint32_t sourceChanges=0,bootChanges=0,queueDrops=0;
  // Exact per-boot delivery ledger. Forward gaps count absent sequence values.
  bool ledgerActive=false;
  uint64_t ledgerBootId=0;
  uint32_t firstSequence=0,lastSequence=0,ledgerReceived=0;
  uint64_t missing=0;
  uint32_t duplicates=0,outOfOrder=0;
};
struct StatusRx {
  uint8_t mac[6]{};
  uint64_t receivedUs=0;
  uint16_t length=0;
  uint8_t bytes[sizeof(PulseTransportStatusPacket)]{};
};
struct TransportState {
  bool have=false,valid=false;
  PulseTransportStatusPacket status{};
  uint8_t mac[6]{};
  uint64_t receivedUs=0;
  uint32_t accepted=0,invalid=0,orderFaults=0,bootChanges=0;
};
// Type-7 diagnostic truth only. It has no reference to NAVI or control state.
class TransportObservation {
 public:
  const TransportState& state() const { return state_; }
  void receive(const StatusRx& rx) {
    auto& s=state_;s.have=true;s.valid=false;s.receivedUs=rx.receivedUs;
    std::memcpy(s.mac,rx.mac,6);s.status={};
    if(rx.length!=sizeof(PulseTransportStatusPacket)){++s.invalid;return;}
    std::memcpy(&s.status,rx.bytes,sizeof(s.status));
    const auto& p=s.status;
    if(p.magic!=0x4952 || p.version!=1 || p.type!=7 || !p.bootId ||
       crc(rx.bytes,offsetof(PulseTransportStatusPacket,crc))!=p.crc){++s.invalid;return;}
    if(priorValid_ && p.bootId==priorBoot_){
      const uint32_t step=p.sequence-priorSequence_;
      if(step==0 || step>=0x80000000u){++s.orderFaults;return;}
    } else if(priorValid_ && p.bootId!=priorBoot_) ++s.bootChanges;
    s.valid=true;++s.accepted;priorValid_=true;priorBoot_=p.bootId;priorSequence_=p.sequence;
  }
 private:
  TransportState state_{};
  bool priorValid_=false;
  uint64_t priorBoot_=0;
  uint32_t priorSequence_=0;
};
// Observation state only: no NAVI, route, PWM, station or actuator references.
class Observation {
 public:
  const State& state() const { return state_; }
  void receive(const Rx& rx) {
    auto& s=state_;
    ++s.received; s.have=true; s.eventValid=false; s.mmps=0; s.flags=0;
    s.receivedUs=rx.receivedUs; std::memcpy(s.mac,rx.mac,6); s.event={};
    if(rx.queueDrops!=s.queueDrops){s.flags|=QUEUE_LOSS;predecessor_=false;}
    s.queueDrops=rx.queueDrops;
    if(rx.length==sizeof(PulseEventPacket))std::memcpy(&s.event,rx.bytes,sizeof(s.event));
    const auto& e=s.event;
    if(rx.length!=sizeof(e) || e.magic!=0x4952 || e.version!=1 || e.type!=6 ||
       !e.bootId || !e.completedPulses || !e.completedUs ||
       e.pitchUm!=ir_movement::kInstalledPitchUm ||
       e.completedPulses>UINT64_MAX/ir_movement::kInstalledPitchUm ||
       e.nominalUm!=e.completedPulses*uint64_t(e.pitchUm) ||
       e.opticalReason>ir_movement::TRACKING ||
       crc(rx.bytes,offsetof(PulseEventPacket,crc))!=e.crc){
      s.flags|=BAD_WIRE; ++s.invalid; predecessor_=false;
      if(s.flags&QUEUE_LOSS)++s.discontinuities;
      return;
    }
    const bool sourceChange=anchor_ && std::memcmp(rx.mac,priorMac_,6)!=0;
    const bool bootChange=anchor_ && e.bootId!=prior_.bootId;
    if(sourceChange){s.flags|=SOURCE_CHANGE;++s.sourceChanges;}
    if(bootChange){s.flags|=BOOT_CHANGE;++s.bootChanges;}
    const bool same=anchor_ && !sourceChange && !bootChange;
    // This accounting is structurally-valid Type-6 receipt, independent of
    // native-speed validity. It neither fills gaps nor changes any authority.
    if(!s.ledgerActive || e.bootId!=s.ledgerBootId){
      s.ledgerActive=true;s.ledgerBootId=e.bootId;
      s.firstSequence=e.sequence;s.lastSequence=e.sequence;s.ledgerReceived=1;
      s.missing=0;s.duplicates=0;s.outOfOrder=0;
    } else {
      const uint32_t ledgerStep=e.sequence-s.lastSequence;
      if(ledgerStep==0)++s.duplicates;
      else if(ledgerStep<0x80000000u){
        if(ledgerStep>1)s.missing+=uint64_t(ledgerStep-1);
        s.lastSequence=e.sequence;++s.ledgerReceived;
      } else ++s.outOfOrder;
    }
    // SID is stable within a MAC/boot stream, but is not universally the low
    // boot word: the transmitter has a nonzero-boot fallback for random zero.
    if(same && e.sid!=prior_.sid){
      s.flags|=BAD_WIRE;++s.invalid;predecessor_=false;
      if(s.flags&QUEUE_LOSS)++s.discontinuities;
      return;
    }
    const uint32_t step=e.sequence-prior_.sequence; // Defined modulo 2^32.
    if(same){
      if(step!=1){s.flags|=(step==0 || step>=0x80000000u)?SEQUENCE_ORDER:SEQUENCE_GAP;++s.sequenceBreaks;}
      if(e.completedPulses<=prior_.completedPulses){s.flags|=PULSE_ORDER;++s.pulseBreaks;}
      else if(e.completedPulses-prior_.completedPulses!=1){s.flags|=PULSE_GAP;++s.pulseBreaks;}
      if(e.completedUs<=prior_.completedUs){s.flags|=TIME_ORDER;++s.timeBreaks;}
      if(rx.receivedUs<priorReceivedUs_){s.flags|=RECEIVE_TIME_ORDER;++s.timeBreaks;}
      if(s.flags&(SEQUENCE_ORDER|PULSE_ORDER|TIME_ORDER|RECEIVE_TIME_ORDER)){
        ++s.invalid;++s.discontinuities;predecessor_=false;
        return; // Never move the accepted high-water reference backwards.
      }
    }
    const bool usable=e.opticalReason==ir_movement::TRACKING || e.opticalReason==ir_movement::SIGNAL_STALE;
    if(!usable)s.flags|=OPTICAL_UNUSABLE;
    if(!e.intervalUs)s.flags|=NO_INTERVAL;
    if(!same || !predecessor_)s.flags|=FIRST;
    if(same && step==1 && e.completedPulses-prior_.completedPulses==1 &&
       e.intervalUs && e.intervalUs!=e.completedUs-prior_.completedUs){
      s.flags|=INTERVAL_MISMATCH;++s.intervalBreaks;
    }
    s.eventValid=same && predecessor_ && usable && e.intervalUs &&
      !(s.flags&(SEQUENCE_GAP|PULSE_GAP|INTERVAL_MISMATCH|QUEUE_LOSS));
    if(s.eventValid)s.mmps=double(e.pitchUm)*1000.0/double(e.intervalUs);
    if(s.flags&(SOURCE_CHANGE|BOOT_CHANGE|SEQUENCE_GAP|PULSE_GAP|INTERVAL_MISMATCH|QUEUE_LOSS) ||
       (same && (s.flags&(NO_INTERVAL|OPTICAL_UNUSABLE))))++s.discontinuities;
    ++s.accepted;
    prior_=e;std::memcpy(priorMac_,rx.mac,6);priorReceivedUs_=rx.receivedUs;
    anchor_=true;predecessor_=usable;
  }
 private:
  State state_;
  PulseEventPacket prior_{};
  uint8_t priorMac_[6]{};
  uint64_t priorReceivedUs_=0;
  bool anchor_=false,predecessor_=false;
};
struct Report {
  State pulse;
  TransportState transport;
  uint64_t comparedUs=0,legacyBoot=0;
  double legacyPkph=0;
  bool legacyValid=false,legacySameSource=false,coupled=false;
};
} // namespace navi_pulse
