#pragma once
// ---------------------------------------------------------------------------
// WaveformDump -- wire format for one chunk of one buffered passage from
// WaveformWindow, sized to fit inside one MQTT message (PubMsg.payload, 704
// bytes in NAVI_ONE.ino).
//
// A slot's samples are chunked, never truncated: "do not discard information
// the firmware processes to compute the value" (operator's ruling,
// 2026-08-31) rules out capping sampleCount to whatever fits in one message.
// Most real passages fit in a single chunk; HallCapture's own decimation only
// engages on passages long enough that this would matter, and even then the
// chunker just emits more messages.
//
// Pure encode/decode, no MQTT, no FreeRTOS -- host-testable like everything
// else this recognizer depends on.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <string.h>

namespace navi_one {

#pragma pack(push, 1)
struct WavHeader {
  uint8_t  slotIndex;        // 0 = most recently pushed slot ... slotTotal-1 = oldest
  uint8_t  slotTotal;        // number of valid slots in this dump
  uint8_t  chunkIndex;       // 0-based chunk of THIS slot's samples
  uint8_t  chunkTotal;       // total chunks for this slot
  uint8_t  polarity;
  uint8_t  outcome;
  uint8_t  isMagnet;
  uint8_t  shapeTested;
  uint16_t sampleCount;      // TOTAL samples for the whole slot
  uint16_t chunkSampleCount; // samples carried in THIS chunk
  uint16_t chunkOffset;      // starting sample index of this chunk within the slot
  uint16_t decimation;
  uint16_t peakCounts;
  uint16_t gain;
  float    amplitudeRatio;
  float    residual;
  uint32_t gapMs;
  uint32_t openedAtMs;
  uint32_t closedAtMs;
};
#pragma pack(pop)

// Maximum samples that fit in one message of the given capacity, alongside
// the header. Capacity is the full payload buffer size (e.g. PubMsg.payload,
// 704 on this build).
constexpr uint16_t wavChunkCapacity(uint16_t payloadCapacity) {
  return (uint16_t)((payloadCapacity - sizeof(WavHeader)) / sizeof(int16_t));
}

// How many chunks a slot with `sampleCount` samples needs, given `perChunk`
// samples per chunk. At least 1, even for an empty passage.
inline uint8_t wavChunkCount(uint16_t sampleCount, uint16_t perChunk) {
  if (perChunk == 0) return 1;
  uint16_t n = (uint16_t)((sampleCount + perChunk - 1) / perChunk);
  return n == 0 ? 1 : (uint8_t)n;
}

// Encodes one chunk into `out` (must hold at least sizeof(WavHeader) +
// chunkSampleCount*2 bytes). Returns the total bytes written.
inline uint16_t wavEncodeChunk(
    uint8_t* out, uint16_t outCapacity,
    uint8_t slotIndex, uint8_t slotTotal,
    uint8_t chunkIndex, uint8_t chunkTotal,
    uint8_t polarity, uint8_t outcome, uint8_t isMagnet, uint8_t shapeTested,
    uint16_t sampleCount, uint16_t decimation, uint16_t peakCounts,
    uint16_t gain, float amplitudeRatio, float residual, uint32_t gapMs,
    uint32_t openedAtMs, uint32_t closedAtMs,
    const int16_t* samples, uint16_t chunkOffset, uint16_t chunkSampleCount) {
  const uint16_t need = (uint16_t)(sizeof(WavHeader) + chunkSampleCount * sizeof(int16_t));
  if (need > outCapacity) return 0;
  WavHeader h{};
  h.slotIndex = slotIndex; h.slotTotal = slotTotal;
  h.chunkIndex = chunkIndex; h.chunkTotal = chunkTotal;
  h.polarity = polarity; h.outcome = outcome;
  h.isMagnet = isMagnet; h.shapeTested = shapeTested;
  h.sampleCount = sampleCount; h.chunkSampleCount = chunkSampleCount;
  h.chunkOffset = chunkOffset; h.decimation = decimation;
  h.peakCounts = peakCounts; h.gain = gain;
  h.amplitudeRatio = amplitudeRatio; h.residual = residual;
  h.gapMs = gapMs; h.openedAtMs = openedAtMs; h.closedAtMs = closedAtMs;
  memcpy(out, &h, sizeof(WavHeader));
  memcpy(out + sizeof(WavHeader), samples + chunkOffset,
         chunkSampleCount * sizeof(int16_t));
  return need;
}

// Decodes a header from a received message. Returns false if too short.
inline bool wavDecodeHeader(const uint8_t* in, uint16_t inLen, WavHeader& out) {
  if (inLen < sizeof(WavHeader)) return false;
  memcpy(&out, in, sizeof(WavHeader));
  return true;
}

}  // namespace navi_one
