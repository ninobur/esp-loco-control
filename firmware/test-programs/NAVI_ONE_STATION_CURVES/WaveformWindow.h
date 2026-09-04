#pragma once
// ---------------------------------------------------------------------------
// WaveformWindow -- a short trailing memory of raw Hall passages, kept only so
// that when AUTO is withdrawn for a NAVIGATION reason (a strike or a
// contradiction), the samples the recognizer actually judged are still
// available to publish.
//
// Built on the operator's ruling, 2026-08-31, after two field events (MM110
// and MM147/145) in which a WRONG_SHAPE rejection produced a silent one-marker
// lag that only surfaced markers later as a WRONG_MAGNET strike -- with no way
// to see what had actually distorted the rejected waveform, because
// HallCapture's own buffer is a single passage, overwritten by the next one.
//
//   Window depth: 6.    Covers the measured worst-case lag (decision 0059).
//   Trigger:      whatever calls withdraw() in NAVI_ONE.ino -- i.e. any event
//                 that shuts AUTO down for a navigation reason (WrongMagnet,
//                 Contradicted). NOT manual auto-off, dispatcher_release,
//                 e-stop, or low voltage -- none of those go through
//                 withdraw() for a shape/polarity reason, and none of them
//                 benefit from a waveform dump.
//   Fidelity:     exactly what HallCapture handed to the recognizer -- the
//                 decimated oriented samples, verbatim. No further resampling.
//                 "Do not discard information the firmware processes to
//                 compute the value" (operator's ruling).
//
// Position-free and ruling-free, like MagnetRecognizer: it does not know what
// a WrongMagnet is, does not decide when to dump, and does not touch the
// network. It only remembers what it is pushed, and hands entries back on
// request. Owned and pushed to exclusively by the Hall task, same as
// HallCapture and MagnetRecognizer (review closeout finding 3) -- pushing
// happens at the one point (NAVI_ONE.ino hallTask()) where a just-closed
// Passage's samples are still valid, before the next capture.sample() begins
// overwriting HallCapture's own buffer.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <string.h>
#include "MagnetRecognizer.h"

namespace navi_one {

template <uint8_t DEPTH = 6, uint16_t RING = 512>
class WaveformWindow {
 public:
  struct Entry {
    bool     valid          = false;
    uint32_t openedAtMs     = 0;
    uint32_t closedAtMs     = 0;
    uint16_t peakCounts     = 0;
    uint8_t  polarity       = 0;
    uint16_t decimation     = 1;
    uint8_t  outcome        = 0;
    uint8_t  isMagnet       = 0;
    uint8_t  shapeTested    = 0;
    float    amplitudeRatio = 0.0f;
    float    residual       = 0.0f;
    uint32_t gapMs          = 0;
    uint16_t gain           = 0;
    uint16_t sampleCount    = 0;    // may exceed what fits in one MQTT message
    int16_t  samples[RING]  = {};
  };

  // Copies now. Must be called before the Hall task's next capture.sample()
  // call overwrites the Passage's backing buffer -- i.e. right where the
  // passage and its Verdict are both still fresh in hallTask().
  void push(const Passage& p, const Verdict& v) {
    Entry& e = ring_[head_];
    e.valid          = true;
    e.openedAtMs     = p.openedAtMs;
    e.closedAtMs     = p.closedAtMs;
    e.peakCounts     = p.peakCounts;
    e.polarity       = p.polarity;
    e.decimation     = p.decimation;
    e.outcome        = (uint8_t)v.outcome;
    e.isMagnet       = v.isMagnet ? 1 : 0;
    e.shapeTested    = v.shapeTested ? 1 : 0;
    e.amplitudeRatio = v.amplitudeRatio;
    e.residual       = v.residual;
    e.gapMs          = v.gapMs;
    e.gain           = v.gain;
    uint16_t n = p.sampleCount > RING ? RING : p.sampleCount;
    e.sampleCount = n;
    if (p.oriented) memcpy(e.samples, p.oriented, n * sizeof(int16_t));
    head_ = (uint8_t)((head_ + 1) % DEPTH);
    if (filled_ < DEPTH) ++filled_;
  }

  uint8_t count() const { return filled_; }

  // 0 = most recently pushed (closest to whatever triggered the dump),
  // count()-1 = oldest still held.
  const Entry& at(uint8_t indexFromNewest) const {
    uint8_t i = (uint8_t)((head_ + DEPTH - 1 - indexFromNewest) % DEPTH);
    return ring_[i];
  }

 private:
  Entry   ring_[DEPTH];
  uint8_t head_ = 0, filled_ = 0;
};

}  // namespace navi_one
