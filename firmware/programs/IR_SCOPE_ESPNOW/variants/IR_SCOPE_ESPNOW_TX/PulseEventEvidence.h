#pragma once
#include "../../../../common/IrMovementWire.h"

// Experimental IR-family type 6 v1, ESP32 little-endian. No control authority.
struct __attribute__((packed)) PulseEventPacket {
  uint16_t magic; uint8_t version,type;
  uint32_t sid,sequence;
  uint64_t bootId,completedPulses,completedUs,intervalUs,nominalUm;
  uint32_t pitchUm; uint8_t opticalReason; uint16_t span; uint16_t crc;
};
static_assert(sizeof(PulseEventPacket)==61,"pulse event wire size changed");
static_assert(offsetof(PulseEventPacket,completedUs)==28,"completion offset changed");
static_assert(offsetof(PulseEventPacket,crc)==59,"pulse event CRC offset changed");
static_assert(sizeof(PulseEventPacket)<=250,"ESP-NOW pulse event too large");

class PulseEventEvidence {
 public:
  // Called immediately after EVERY Measurement::sample, never on a report timer.
  bool observe(const ir_movement::Snapshot& s, const ir_movement::Detector& d,
               uint32_t sid, PulseEventPacket& e) {
    const bool usable = s.reason==ir_movement::TRACKING ||
                        s.reason==ir_movement::SIGNAL_STALE;
    if (!usable || s.sampleGaps!=gaps_ || s.saturatedSamples!=saturated_ ||
        s.openAborts!=aborts_) havePredecessor_=false;
    gaps_=s.sampleGaps; saturated_=s.saturatedSamples; aborts_=s.openAborts;
    // fall is exactly the detector branch that increments completed and sets
    // lastCompleted_ to this sample's timestamp. Do not detect edges again here.
    if (!d.fall) return false;
    e={}; e.magic=0x4952; e.version=1; e.type=6; e.sid=sid;
    e.sequence=++sequence_; e.bootId=s.bootId;
    e.completedPulses=s.completedPulses; e.completedUs=s.capturedUs;
    e.intervalUs=havePredecessor_ && s.capturedUs>priorUs_ ? s.capturedUs-priorUs_ : 0;
    e.pitchUm=ir_movement::kInstalledPitchUm;
    e.nominalUm=e.completedPulses*uint64_t(e.pitchUm);
    e.opticalReason=s.reason; e.span=uint16_t(d.high-d.low);
    priorUs_=s.capturedUs; havePredecessor_=usable;
    // CRC is filled by the transmitter after all wire fields are final.
    return true;
  }
 private:
  uint32_t sequence_=0;
  uint64_t priorUs_=0,gaps_=0,saturated_=0,aborts_=0;
  bool havePredecessor_=false;
};
