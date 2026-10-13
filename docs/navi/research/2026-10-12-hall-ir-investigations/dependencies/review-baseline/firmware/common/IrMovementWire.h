#pragma once
#include <stddef.h>
#include "IrMovementContract.h"

namespace ir_movement {
// ESP32 little-endian transport. Type 5 is separate from legacy raw/fusion.
struct __attribute__((packed)) WireSnapshot {
  uint16_t magic=0x4952;
  uint8_t version=1, type=5;
  uint32_t sequence=0;
  uint64_t bootId=0, capturedUs=0, observedRises=0, completedPulses=0;
  uint64_t inferredAdded=0, inferredRemoved=0, unreliableSamples=0;
  uint64_t saturatedSamples=0, sampleGaps=0, openAborts=0;
  uint32_t calibrationId=0, pitchUm=9652;
  uint64_t nominalUm=0;
  uint8_t opticalReason=PRIMING, distanceValidated=0;
  uint16_t span=0, crc=0;
};
static_assert(sizeof(WireSnapshot)==110,"movement wire size");
static_assert(offsetof(WireSnapshot,crc)==108,"movement crc offset");
inline WireSnapshot encode(const Snapshot& s, uint32_t sequence, uint16_t span) {
  WireSnapshot w;
  w.sequence=sequence; w.bootId=s.bootId; w.capturedUs=s.capturedUs;
  w.observedRises=s.observedRises; w.completedPulses=s.completedPulses;
  w.inferredAdded=s.inferredAdded; w.inferredRemoved=s.inferredRemoved;
  w.unreliableSamples=s.unreliableSamples; w.saturatedSamples=s.saturatedSamples;
  w.sampleGaps=s.sampleGaps; w.openAborts=s.openAborts;
  w.calibrationId=s.calibrationId; w.pitchUm=uint32_t(s.mmPerPulse*1000+0.5);
  w.nominalUm=s.completedPulses*w.pitchUm;
  w.opticalReason=s.reason; w.span=span;
  return w;
}
} // namespace ir_movement
