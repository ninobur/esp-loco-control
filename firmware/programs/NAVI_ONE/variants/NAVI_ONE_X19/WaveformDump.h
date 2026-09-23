#pragma once
// ---------------------------------------------------------------------------
// WaveformDump -- X19 wire format, one chunk of one buffered CANDIDATE RECORD
// from WaveformWindow, sized to fit inside one MQTT message (PubMsg.payload,
// 704 bytes in the sketch).
//
// VERSION 2. The X18 header (version-less, 40 bytes) described a closed
// passage: openedAtMs/closedAtMs, decimation, a Gaussian residual. X19 has no
// close, never decimates, and computes no morphology, so the header carries
// what an excursion actually has: the detection instant, the local reference
// the record is expressed against, the departure that triggered it, where the
// pre-roll ends, and where the polarity-bearing excursion begins and ends.
//
// `version` is the FIRST byte precisely so a host decoder can tell the two
// apart without guessing: 40-byte X18 records begin with slotIndex, which is
// 0..slotTotal-1 and therefore small; X19 records begin with the literal 2.
// Decoders should branch on it rather than on message length.
//
// Samples are int16 counts relative to that record's own localRef, oriented so
// the excursion's own pole is positive -- the same convention X18 used, with
// the excursion's local zero in place of the frozen entry baseline.
//
// Pure encode/decode, no MQTT, no FreeRTOS -- host-testable.
//
// Host decoder (struct module, little-endian, 57 bytes):
//   "<BBBBBBBBB" "HHHHHHHHHH" "hhhhhh" "f" "III"
//   version slotIndex slotTotal chunkIndex chunkTotal polarity outcome
//   isMagnet widthRejected
//   sampleCount chunkSampleCount chunkOffset preSamples peakCounts gain
//   excursionFirst excursionLast widthCaliperMs widthFracMs
//   peakSigned rawAtDetect localRef reference shadowRef departAtDetect
//   amplitudeRatio
//   gapMs detectedAtMs windowEndMs
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <string.h>

namespace navi_one {

static constexpr uint8_t WAV_FORMAT_VERSION = 2;

#pragma pack(push, 1)
struct WavHeader {
  uint8_t  version;           // WAV_FORMAT_VERSION
  uint8_t  slotIndex;         // 0 = most recently pushed ... slotTotal-1 = oldest
  uint8_t  slotTotal;
  uint8_t  chunkIndex;        // 0-based chunk of THIS slot's samples
  uint8_t  chunkTotal;
  uint8_t  polarity;          // 1 = N, 0 = S
  uint8_t  outcome;           // navi_one::Outcome
  uint8_t  isMagnet;
  uint8_t  widthRejected;     // 1 only if widthFloorMs was configured non-zero
  uint16_t sampleCount;       // TOTAL samples in the record (pre-roll + window)
  uint16_t chunkSampleCount;
  uint16_t chunkOffset;
  uint16_t preSamples;        // index of the DETECTION sample within the record
  uint16_t peakCounts;        // |peak| over the window, judged copy
  uint16_t gain;
  uint16_t excursionFirst;    // first/last sample index used for POLARITY
  uint16_t excursionLast;
  uint16_t widthCaliperMs;    // samples at or beyond the 25-count caliper
  uint16_t widthFracMs;       // samples at or beyond excursionFrac * peak
  int16_t  peakSigned;
  int16_t  rawAtDetect;
  int16_t  localRef;          // L, raw counts: the record's zero
  int16_t  reference;         // operative baseline at detection (telemetry)
  int16_t  shadowRef;         // rolling median at detection (telemetry)
  int16_t  departAtDetect;    // signed raw(detect) - L
  float    amplitudeRatio;
  uint32_t gapMs;
  uint32_t detectedAtMs;
  uint32_t windowEndMs;
};
#pragma pack(pop)

// Maximum samples that fit in one message of the given capacity.
constexpr uint16_t wavChunkCapacity(uint16_t payloadCapacity) {
  return (uint16_t)((payloadCapacity - sizeof(WavHeader)) / sizeof(int16_t));
}

// How many chunks a record with `sampleCount` samples needs. At least 1.
inline uint8_t wavChunkCount(uint16_t sampleCount, uint16_t perChunk) {
  if (perChunk == 0) return 1;
  uint16_t n = (uint16_t)((sampleCount + perChunk - 1) / perChunk);
  return n == 0 ? 1 : (uint8_t)n;
}

// Encodes one chunk into `out`. Returns the total bytes written, 0 if it
// would not fit.
inline uint16_t wavEncodeChunk(
    uint8_t* out, uint16_t outCapacity,
    uint8_t slotIndex, uint8_t slotTotal,
    uint8_t chunkIndex, uint8_t chunkTotal,
    uint8_t polarity, uint8_t outcome, uint8_t isMagnet, uint8_t widthRejected,
    uint16_t sampleCount, uint16_t preSamples, uint16_t peakCounts,
    uint16_t gain, uint16_t excursionFirst, uint16_t excursionLast,
    uint16_t widthCaliperMs, uint16_t widthFracMs,
    int16_t peakSigned, int16_t rawAtDetect, int16_t localRef,
    int16_t reference, int16_t shadowRef, int16_t departAtDetect,
    float amplitudeRatio, uint32_t gapMs,
    uint32_t detectedAtMs, uint32_t windowEndMs,
    const int16_t* samples, uint16_t chunkOffset, uint16_t chunkSampleCount) {
  const uint16_t need = (uint16_t)(sizeof(WavHeader) + chunkSampleCount * sizeof(int16_t));
  if (need > outCapacity) return 0;
  WavHeader h{};
  h.version = WAV_FORMAT_VERSION;
  h.slotIndex = slotIndex; h.slotTotal = slotTotal;
  h.chunkIndex = chunkIndex; h.chunkTotal = chunkTotal;
  h.polarity = polarity; h.outcome = outcome;
  h.isMagnet = isMagnet; h.widthRejected = widthRejected;
  h.sampleCount = sampleCount; h.chunkSampleCount = chunkSampleCount;
  h.chunkOffset = chunkOffset; h.preSamples = preSamples;
  h.peakCounts = peakCounts; h.gain = gain;
  h.excursionFirst = excursionFirst; h.excursionLast = excursionLast;
  h.widthCaliperMs = widthCaliperMs; h.widthFracMs = widthFracMs;
  h.peakSigned = peakSigned; h.rawAtDetect = rawAtDetect;
  h.localRef = localRef; h.reference = reference; h.shadowRef = shadowRef;
  h.departAtDetect = departAtDetect;
  h.amplitudeRatio = amplitudeRatio; h.gapMs = gapMs;
  h.detectedAtMs = detectedAtMs; h.windowEndMs = windowEndMs;
  memcpy(out, &h, sizeof(WavHeader));
  memcpy(out + sizeof(WavHeader), samples + chunkOffset,
         chunkSampleCount * sizeof(int16_t));
  return need;
}

// Decodes a header from a received message. Returns false if too short or if
// it is not an X19 record.
inline bool wavDecodeHeader(const uint8_t* in, uint16_t inLen, WavHeader& out) {
  if (inLen < sizeof(WavHeader)) return false;
  if (in[0] != WAV_FORMAT_VERSION) return false;
  memcpy(&out, in, sizeof(WavHeader));
  return true;
}

}  // namespace navi_one
