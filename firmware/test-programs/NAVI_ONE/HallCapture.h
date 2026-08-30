#pragma once
// ---------------------------------------------------------------------------
// HallCapture — 1 kHz acquisition on GPIO 33, and passage assembly.
//
// It produces Passages. It makes NO judgement about whether one is a magnet:
// that is the recognizer's job, and this layer must not pre-empt it. The only
// things refused here are the two that are properties of the INSTRUMENT rather
// than of magnets:
//
//   ENTRY MARGIN 38 counts   below this is indistinguishable from noise on
//                            this sensor. Verified in the field 2026-08-29:
//                            baseline 1834, thresholds 1872 / 1796.
//   FLOOR 40 ms              shorter than any magnet passage at any speed the
//                            railway reaches; an electrical transient.
//
// Nothing else screens. In particular there is no amplitude floor above the
// entry margin and no duration ceiling: real magnets on this railway run down
// to 101 counts, and a passage spanning a station dwell legitimately lasted
// 18,707 ms on 2026-08-29 (decision 0057).
//
// EXIT hysteresis at 25 counts, held for 8 ms, so a magnet whose signal
// wobbles near the threshold produces one passage rather than several.
//
// The baseline is a rolling median, not a mean: a median is unmoved by the
// passage itself, so a magnet cannot drag its own reference.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include "MagnetRecognizer.h"

namespace navi_one {

struct CaptureConfig {
  int16_t  entryMargin   = 38;
  int16_t  exitMargin    = 25;
  uint16_t exitHoldMs    = 8;
  uint16_t floorMs       = 40;
  uint16_t baselineMs    = 25;    // one baseline sample every 25 ms
  uint16_t primeMs       = 2000;  // 2 s before the baseline is trusted
};

template <uint16_t RING = 512, uint8_t PRE = 12, uint8_t MED = 41>
class HallCapture {
 public:
  explicit HallCapture(const CaptureConfig& cfg) : cfg_(cfg) {}

  int32_t baseline() const { return baseline_; }
  bool    ready()    const { return primed_; }
  const Passage& passage() const { return out_; }
  uint32_t floorRejects() const { return floorRejects_; }

  // One ADC sample. Returns true when a passage has just CLOSED and passage()
  // holds it. Runs on the Hall task; touches nothing else.
  bool sample(uint32_t nowMs, int16_t raw) {
    updateBaseline(nowMs, raw);
    if (!primed_) return false;

    const int32_t delta = (int32_t)raw - baseline_;
    const int32_t mag   = delta < 0 ? -delta : delta;

    if (!open_) {
      // Keep a short pre-roll so the recognizer sees the foot of the arc.
      pre_[preHead_] = (int16_t)delta;
      preHead_ = (uint8_t)((preHead_ + 1) % PRE);
      if (preLen_ < PRE) ++preLen_;
      if (mag < cfg_.entryMargin) return false;
      open_ = true;
      pol_ = delta >= 0 ? 1 : 0;
      openedAtMs_ = nowMs;
      peak_ = 0; n_ = 0; quietSince_ = 0; truncated_ = false;
      // replay the pre-roll, oriented
      uint8_t start = (uint8_t)((preHead_ + PRE - preLen_) % PRE);
      for (uint8_t i = 0; i < preLen_; ++i)
        push(orient(pre_[(start + i) % PRE]));
      preAt_ = n_;
    }

    push(orient((int16_t)delta));
    const int32_t oriented = pol_ ? delta : -delta;
    if (oriented > peak_) peak_ = oriented;

    if (mag < cfg_.exitMargin) {
      if (!quietSince_) quietSince_ = nowMs;
      if (nowMs - quietSince_ >= cfg_.exitHoldMs) return close(nowMs);
    } else {
      quietSince_ = 0;
    }
    return false;
  }

 private:
  int16_t orient(int16_t d) const { return pol_ ? d : (int16_t)(-d); }
  void push(int16_t v) { if (n_ < RING) buf_[n_++] = v; else truncated_ = true; }

  bool close(uint32_t nowMs) {
    open_ = false;
    const uint32_t dur = nowMs - openedAtMs_;
    if (dur < cfg_.floorMs) { ++floorRejects_; return false; }
    out_ = Passage{};
    out_.openedAtMs = openedAtMs_;
    out_.closedAtMs = nowMs;
    out_.peakCounts = (uint16_t)(peak_ > 65535 ? 65535 : peak_);
    out_.polarity   = pol_;
    out_.oriented   = buf_;
    out_.sampleCount= n_;
    out_.preSamples = preAt_;
    out_.truncated  = truncated_;
    out_.clipped    = clipped_;
    clipped_ = false;
    return true;
  }

  void updateBaseline(uint32_t nowMs, int16_t raw) {
    if (raw <= 8 || raw >= 4087) clipped_ = true;
    if (!startMs_) startMs_ = nowMs;
    if (nowMs - lastBaseMs_ < cfg_.baselineMs) return;
    lastBaseMs_ = nowMs;
    if (open_) return;                       // never sample the baseline inside a passage
    med_[medHead_] = raw;
    medHead_ = (uint8_t)((medHead_ + 1) % MED);
    if (medLen_ < MED) ++medLen_;
    int16_t c[MED];
    for (uint8_t i = 0; i < medLen_; ++i) c[i] = med_[i];
    for (uint8_t i = 1; i < medLen_; ++i) {
      int16_t v = c[i]; int j = (int)i - 1;
      while (j >= 0 && c[j] > v) { c[j + 1] = c[j]; --j; }
      c[j + 1] = v;
    }
    baseline_ = c[medLen_ / 2];
    if (!primed_ && (nowMs - startMs_) >= cfg_.primeMs && medLen_ >= MED / 2) primed_ = true;
  }

  CaptureConfig cfg_;
  int16_t  buf_[RING] = {};
  uint16_t n_ = 0, preAt_ = 0;
  int16_t  pre_[PRE] = {}; uint8_t preHead_ = 0, preLen_ = 0;
  int16_t  med_[MED] = {}; uint8_t medHead_ = 0, medLen_ = 0;
  int32_t  baseline_ = 0, peak_ = 0;
  uint32_t startMs_ = 0, lastBaseMs_ = 0, openedAtMs_ = 0, quietSince_ = 0;
  uint32_t floorRejects_ = 0;
  bool     primed_ = false, open_ = false, truncated_ = false, clipped_ = false;
  uint8_t  pol_ = 1;
  Passage  out_;
};

}  // namespace navi_one
