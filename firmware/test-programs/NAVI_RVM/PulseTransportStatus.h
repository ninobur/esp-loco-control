#pragma once
#include <stddef.h>
#include <stdint.h>

// IR-family diagnostic truth only. ESP32 little-endian; no movement authority.
struct __attribute__((packed)) PulseTransportStatusPacket {
  uint16_t magic; uint8_t version,type;
  uint32_t sid,sequence; uint64_t bootId;
  uint32_t generated,sent,failed,dropped,logDropped;
  uint32_t queueDepth,queueHigh,logQueueDepth;
  uint32_t lagMaxUs,timeouts,busyDrops,sendErrors;
  uint16_t crc;
};
static_assert(sizeof(PulseTransportStatusPacket)==70,"pulse transport status wire size");
static_assert(offsetof(PulseTransportStatusPacket,bootId)==12,"status boot offset");
static_assert(offsetof(PulseTransportStatusPacket,generated)==20,"status counters offset");
static_assert(offsetof(PulseTransportStatusPacket,queueDepth)==40,"status queue offset");
static_assert(offsetof(PulseTransportStatusPacket,crc)==68,"status CRC offset");
static_assert(sizeof(PulseTransportStatusPacket)<=250,"status exceeds ESP-NOW limit");
