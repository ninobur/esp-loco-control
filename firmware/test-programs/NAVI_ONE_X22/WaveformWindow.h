#pragma once
// ---------------------------------------------------------------------------
// WaveformWindow -- X19. A short trailing memory of raw Hall candidate
// records, kept so that when AUTO is withdrawn for a NAVIGATION reason, and
// whenever a candidate is refused, the samples the recognizer actually judged
// are still available to publish.
//
// X18 stored one closed Passage per slot: pre-roll of 12 samples plus the
// passage, decimated once it outgrew 512 points. X19 stores a CANDIDATE
// RECORD: 512 ms of raw pre-roll plus the 400 ms acquisition window, at 1 kHz,
// never decimated. That is the evidence 2026-09-15 did not have -- every dump
// that day began at the entry crossing with 12 ms of history, which is why the
// FIRST detection of an isolated magnet could not be replayed at all.
//
// Fidelity is unchanged in principle: exactly what the detector handed to the
// recognizer, verbatim, expressed against the excursion's own local reference
// L rather than against a global baseline. "Do not discard information the
// firmware processes to compute the value" (operator's ruling, 2026-08-31).
//
// Owned and pushed to exclusively by the Hall task, as in X18.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <string.h>
#include "MagnetRecognizer.h"
#include "ExcursionDetector.h"

namespace navi_one {

template <uint8_t DEPTH = 5, uint16_t RING = 960>
class WaveformWindow {
 public:
  struct Entry {
    bool     valid          = false;
    uint32_t detectedAtMs   = 0;
    uint32_t windowEndMs    = 0;
    uint16_t peakCounts     = 0;
    int16_t  peakSigned     = 0;
    uint8_t  polarity       = 0;
    uint8_t  outcome        = 0;
    uint8_t  isMagnet       = 0;
    uint8_t  widthRejected  = 0;
    float    amplitudeRatio = 0.0f;
    uint32_t gapMs          = 0;
    uint16_t gain           = 0;
    uint16_t sampleCount    = 0;
    uint16_t preSamples     = 0;
    int16_t  rawAtDetect    = 0;
    int16_t  localRef       = 0;
    int16_t  reference      = 0;
    int16_t  shadowRef      = 0;
    int16_t  departAtDetect = 0;
    uint16_t excursionFirst = 0;
    uint16_t excursionLast  = 0;
    uint16_t widthCaliperMs = 0;
    uint16_t widthFracMs    = 0;
    int16_t  samples[RING]  = {};
  };

  // Copies now. Must be called while the detector's backing buffer is still
  // the one just judged -- i.e. right where the Excursion and its Verdict are
  // both fresh in hallTask().
  void push(const Excursion& e, const Verdict& v) {
    Entry& s = ring_[head_];
    s.valid          = true;
    s.detectedAtMs   = e.detectedAtMs;
    s.windowEndMs    = e.windowEndMs;
    s.peakCounts     = e.peakCounts;
    s.peakSigned     = e.peakSigned;
    s.polarity       = e.polarity;
    s.outcome        = (uint8_t)v.outcome;
    s.isMagnet       = v.isMagnet ? 1 : 0;
    s.widthRejected  = e.widthRejected ? 1 : 0;
    s.amplitudeRatio = v.amplitudeRatio;
    s.gapMs          = v.gapMs;
    s.gain           = v.gain;
    s.preSamples     = e.preSamples;
    s.rawAtDetect    = e.rawAtDetect;
    s.localRef       = (int16_t)e.localRef;
    s.reference      = (int16_t)e.reference;
    s.shadowRef      = (int16_t)e.shadowRef;
    s.departAtDetect = (int16_t)e.departAtDetect;
    s.excursionFirst = e.excursionFirst;
    s.excursionLast  = e.excursionLast;
    s.widthCaliperMs = e.widthCaliperMs;
    s.widthFracMs    = e.widthFracMs;
    uint16_t n = e.sampleCount > RING ? RING : e.sampleCount;
    s.sampleCount = n;
    if (e.oriented) memcpy(s.samples, e.oriented, n * sizeof(int16_t));
    head_ = (uint8_t)((head_ + 1) % DEPTH);
    if (filled_ < DEPTH) ++filled_;
  }

  uint8_t count() const { return filled_; }

  // 0 = most recently pushed, count()-1 = oldest still held.
  const Entry& at(uint8_t indexFromNewest) const {
    uint8_t i = (uint8_t)((head_ + DEPTH - 1 - indexFromNewest) % DEPTH);
    return ring_[i];
  }

 private:
  Entry   ring_[DEPTH];
  uint8_t head_ = 0, filled_ = 0;
};

}  // namespace navi_one
