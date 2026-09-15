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
//   FLOOR cfg_.floorMs       completed passages shorter than the configured
//                            field-test boundary; an acquisition transient.
//
// Nothing else screens. In particular there is no amplitude floor above the
// entry margin and no duration ceiling: real magnets on this railway run down
// to 101 counts, and a passage spanning a station dwell legitimately lasted
// 18,707 ms on 2026-08-29 (decision 0057).
//
// EXIT hysteresis at 25 counts, held for 8 ms, so a magnet whose signal
// wobbles near the threshold produces one passage rather than several.
//
// The baseline is established by a median during startup. A field-test policy
// may then hold that baseline fixed for the run while the same rolling median
// continues in shadow, where it can be observed but cannot affect capture.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include "NAVIFieldConfig.h"
#include "MagnetRecognizer.h"

namespace navi_one {

struct CaptureConfig {
  int16_t  entryMargin   = 38;
  int16_t  exitMargin    = 25;
  uint16_t exitHoldMs    = 8;
  uint16_t floorMs       = NAVI_PASSAGE_FLOOR_MS;
  uint16_t baselineMs    = 25;    // one baseline sample every 25 ms
  uint16_t primeMs       = 2000;  // 2 s before the baseline is trusted
  // How long a passage must have been open before the LIVE baseline is allowed
  // to migrate underneath it. Above the tractive floor the reference must be
  // able to walk onto a stale offset and close a false latch, but it must not
  // do that to a real magnet still being crossed.
  //
  // Sized from measurement, not taste. The median needs 21 of 41 samples at
  // 25 ms to move, i.e. ~525 ms of open line. The longest LEGITIMATE passage
  // recorded above the floor is 554 ms (2026-09-01 16:55:02, departing Bamboo
  // through the ramp band); at the ~30 mm effective magnet width these
  // locomotives show, PWM 30 gives about 1.5 s. Every latched passage on record
  // is far longer: 3,628 ms, 12,717 ms, 554,998 ms. 2 s sits above the one and
  // an order of magnitude below the others.
  // The clock runs from whichever is LATER: the passage opening, or the moment
  // motion permission arrived. Both matter. From the opening, so a real magnet
  // being crossed is never migrated out from under. From the permission,
  // because a locomotive standing in a magnet's fringe field holds a passage
  // open for the whole dwell, and the instant the departure throttle crosses
  // the floor that passage is already older than the guard -- migration would
  // walk the reference straight onto the parked field, which is the Bamboo
  // capture again by another road. Waiting the guard out from the permission
  // means the locomotive is actually rolling, and clear, before the reference
  // is allowed to follow it.
  uint16_t openMigrateMs = 2000;
  // Experimental field-test policy. When true, the baseline established by
  // the prime window remains authoritative until reboot. The rolling median
  // still runs as shadow telemetry under the normal motion/open gates.
  bool fixedAfterPrime = false;
};

// One-shot evidence for an acquisition event rejected by the completed-
// passage duration floor. It is deliberately not a Passage: callers may
// report it, but cannot accidentally send it through recognition/navigation.
struct FloorRejection {
  uint32_t openedAtMs = 0, closedAtMs = 0;
  uint16_t durationMs = 0, floorMs = 0;
  uint16_t sampleCount = 0, preSamples = 0, decimation = 1;
  uint16_t rawPeakMagnitude = 0;
  int32_t entryBaseline = 0;
  int64_t signedSum = 0;
  bool truncated = false, clipped = false;
};

template <uint16_t RING = 512, uint8_t PRE = 12, uint8_t MED = 41>
class HallCapture {
 public:
  explicit HallCapture(const CaptureConfig& cfg) : cfg_(cfg) {}

  int32_t baseline() const { return baseline_; }
  int32_t shadowBaseline() const { return shadowBaseline_; }
  bool    open()     const { return open_; }
  bool    ready()    const { return primed_; }
  const Passage& passage() const { return out_; }
  uint32_t floorRejects() const { return floorRejects_; }

  // Apply a completed-lap correction only between passages. The pre-roll is
  // already expressed relative to the old baseline, so move it into the new
  // reference frame as well. The raw rolling window remains shadow telemetry.
  bool adjustBaseline(int8_t delta) {
    if (open_) return false;
    baseline_ += delta;
    entryBaseline_ = baseline_;
    for (uint8_t i = 0; i < preLen_; ++i) pre_[i] -= delta;
    return true;
  }

  // Consume exactly once. A floor rejection remains outside the Passage path
  // and therefore outside MagnetRecognizer and Navigator authority.
  bool takeFloorRejection(FloorRejection& r) {
    if (!floorRejectPending_) return false;
    r = floorReject_;
    floorRejectPending_ = false;
    return true;
  }

  int32_t entryBaseline() const { return entryBaseline_; }

  // One ADC sample. Returns true when a passage has just CLOSED and passage()
  // holds it. Runs on the Hall task; touches nothing else.
  //
  // mayAdapt is positive evidence of tractive motion -- actualPwm above THIS
  // locomotive's measured floor. It governs the baseline only. See
  // updateBaseline() for why the reference may not be maintained at rest.
  //
  // TWO REFERENCES, ONE SENSOR (decision pending; findings 09 and 10)
  // -----------------------------------------------------------------
  // baseline_      LIVE. Decides only whether a passage is OPEN or CLOSED.
  //                It is allowed to move under an open passage so that a
  //                stale offset cannot hold one open for ever.
  // entryBaseline_ FROZEN at the instant the passage opened. Every stored
  //                sample, and therefore polarity, peak, and amplitude ratio,
  //                is measured against it.
  //
  // Without the split, letting the reference migrate to clear a latch would
  // drag the recording with it and deform the very curve decisions 0064 and
  // 0065 exist to protect. With it, the two jobs stop fighting: the recording
  // is a fixed-reference measurement of the field, and the open/close test is
  // free to follow the sensor.
  bool sample(uint32_t nowMs, int16_t raw, bool mayAdapt) {
    updateBaseline(nowMs, raw, mayAdapt);
    if (!primed_) return false;

    // LIVE reference: threshold arithmetic only.
    const int32_t delta = (int32_t)raw - baseline_;
    const int32_t mag   = delta < 0 ? -delta : delta;

    if (!open_) {
      // Keep a short pre-roll so the recognizer sees the foot of the arc. The
      // entry-crossing sample itself is NOT put here: 0.1 stored it, replayed
      // it, and then pushed it again through the main path, so it appeared
      // twice at the pre/passage boundary of every waveform.
      if (mag < cfg_.entryMargin) {
        // RAW, not delta. The pre-roll is replayed at open against
        // entryBaseline_, so it must not carry a reference of its own.
        pre_[preHead_] = raw;
        preHead_ = (uint8_t)((preHead_ + 1) % PRE);
        if (preLen_ < PRE) ++preLen_;
        return false;
      }
      open_ = true;
      openedAtMs_ = nowMs;
      // The reference this passage will be MEASURED against, fixed here and not
      // touched again until it closes.
      entryBaseline_ = baseline_;
      peak_ = 0; n_ = 0; quietSince_ = 0; truncated_ = false;
      // The pole is NOT decided here. 0.3 latched it from this one sample --
      // the entry crossing -- and on 2026-08-31 a single-sample artifact of
      // +41 (MM70) and -43 (MM119), each a few counts over entryMargin and
      // opposite to the field that was arriving, latched the wrong pole twice
      // and stopped the locomotive twice. Findings 05 and 06, decision 0064.
      sum_ = 0;
      // A rail that happened between passages says nothing about THIS one.
      // 0.1 set clipped_ from updateBaseline() at any moment and cleared it
      // only when a passage closed, so a supply transient minutes earlier
      // contaminated the next passage's instrument record.
      clipped_ = false;
      dec_ = 1; decPhase_ = 0;
      // replay the pre-roll, oriented
      uint8_t start = (uint8_t)((preHead_ + PRE - preLen_) % PRE);
      for (uint8_t i = 0; i < preLen_; ++i) {
        const int16_t v = (int16_t)((int32_t)pre_[(start + i) % PRE] - entryBaseline_);
        push(v); tally(v);
      }
      preAt_ = n_;
    }

    // Signed, ENTRY-baseline-relative, unoriented. The buffer is oriented once
    // at close(), when the pole is known from the whole passage. Note this is
    // deliberately NOT `delta`: the recording may not move when the live
    // reference does.
    const int32_t rec = (int32_t)raw - entryBaseline_;
    push((int16_t)rec);
    tally((int16_t)rec);

    if (mag < cfg_.exitMargin) {
      if (!quietSince_) quietSince_ = nowMs;
      if (nowMs - quietSince_ >= cfg_.exitHoldMs) return close(nowMs);
    } else {
      quietSince_ = 0;
    }
    return false;
  }

 private:
  // Every sample of the passage is summed, signed. The sign of that sum is the
  // pole. One sample cannot carry it: the +41 and -43 artifacts of findings 05
  // and 06 sit against sums of -12,691 and +19,742. int64 because a passage may
  // legitimately last hours (decision 0057) and a 32-bit sum could wrap.
  //
  // The peak is NOT tracked here. A running maximum is decided by one sample,
  // and on 2026-08-31 one sample of +313 in MM169's tail became its peak. The
  // peak is read from the judgement copy at close() instead. Decision 0065.
  void tally(int16_t d) { sum_ += d; }
  // 0.1 stopped storing at RING samples (512 at 1 kHz = 512 ms) and set
  // truncated_, losing the end of slow passages and station dwells.
  //
  // Nothing is dropped now. When the buffer fills, the whole arc is halved in
  // place and the sample rate halves with it. A 512-sample buffer at dec_ =
  // 32768 still covers four and a half hours of passage.
  void push(int16_t v) {
    if (decPhase_++ % dec_) return;
    if (n_ >= RING) {
      if (dec_ >= 32768) { truncated_ = true; return; }   // unreachable in practice
      for (uint16_t i = 0; i < RING / 2; ++i) buf_[i] = buf_[i * 2];
      n_ = RING / 2;
      preAt_ = (uint16_t)(preAt_ / 2);
      dec_ = (uint16_t)(dec_ * 2);
    }
    buf_[n_++] = v;
  }

 public:
  // A declaration, or a direction change, ends the frame. Any passage still
  // open under the sensor belongs to the old one: 0.1 left it open, so
  // declaring while the sensor sat in a magnet's field produced a passage on
  // drive-off that was judged against the NEXT target -- an immediate strike,
  // half the time, on an otherwise correct declaration. The baseline survives;
  // it is a property of the sensor, not of the frame.
  void reset() {
    open_ = false; n_ = 0; preAt_ = 0; preLen_ = 0; preHead_ = 0;
    peak_ = 0; quietSince_ = 0; truncated_ = false; clipped_ = false;
    dec_ = 1; decPhase_ = 0;
    sum_ = 0;
    entryBaseline_ = baseline_;
    floorRejectPending_ = false;
  }

 private:

  bool close(uint32_t nowMs) {
    open_ = false;
    const uint32_t dur = nowMs - openedAtMs_;
    if (dur < cfg_.floorMs) {
      uint32_t rawPeak = 0;
      for (uint16_t i = 0; i < n_; ++i) {
        const int32_t v = buf_[i];
        const uint32_t mag = (uint32_t)(v < 0 ? -v : v);
        if (mag > rawPeak) rawPeak = mag;
      }
      floorReject_ = FloorRejection{};
      floorReject_.openedAtMs = openedAtMs_;
      floorReject_.closedAtMs = nowMs;
      floorReject_.durationMs = (uint16_t)(dur > 65535 ? 65535 : dur);
      floorReject_.floorMs = cfg_.floorMs;
      floorReject_.sampleCount = n_;
      floorReject_.preSamples = preAt_;
      floorReject_.decimation = dec_;
      floorReject_.rawPeakMagnitude = (uint16_t)(rawPeak > 65535 ? 65535 : rawPeak);
      floorReject_.entryBaseline = entryBaseline_;
      floorReject_.signedSum = sum_;
      floorReject_.truncated = truncated_;
      floorReject_.clipped = clipped_;
      floorRejectPending_ = true;
      ++floorRejects_;
      return false;
    }
    // ------------------------------------------------------------------
    // THE POLE IS DECIDED HERE, and only here, from the completed passage.
    // Never from the entry sample. Never from RouteMap: the map may not be
    // consulted, or a mis-latched passage would simply be told what it ought
    // to have been and the instrument would stop being an instrument.
    // ------------------------------------------------------------------
    pol_ = sum_ >= 0 ? 1 : 0;
    if (!pol_)
      for (uint16_t i = 0; i < n_; ++i) buf_[i] = (int16_t)(-buf_[i]);

    // buf_ is now THE RECORDING and is never touched again: it is what the
    // waveform dump publishes, artifacts included. Everything judged -- the
    // peak here reads judge_ instead, so no lone sample can set the amplitude.
    // Decision 0065.
    medianOfThree(buf_, n_, judge_);
    peak_ = 0;
    for (uint16_t i = 0; i < n_; ++i)
      if (judge_[i] > peak_) peak_ = judge_[i];

    out_ = Passage{};
    out_.openedAtMs = openedAtMs_;
    out_.closedAtMs = nowMs;
    out_.peakCounts = (uint16_t)(peak_ > 65535 ? 65535 : peak_);
    out_.polarity   = pol_;
    out_.signedSum  = sum_;
    out_.oriented   = buf_;          // the recording, unfiltered
    out_.judged     = judge_;        // the median-of-three judgement copy
    out_.sampleCount= n_;
    out_.preSamples = preAt_;
    out_.truncated  = truncated_;
    out_.clipped    = clipped_;
    out_.decimation = dec_;
    out_.entryBaseline = entryBaseline_;
    return true;
  }

  // WHY THE REFERENCE IS GATED ON MOTION
  // ------------------------------------
  // A rolling median is robust to magnets only while the locomotive is MOVING:
  // a magnet that is traversed contributes at most a sample or two out of 41,
  // and the median ignores it. A magnet the locomotive is PARKED on contributes
  // every sample, and the reference becomes the magnet.
  //
  // Observed, 2026-09-01, coming to rest at Bamboo: the baseline walked from
  // 1853 to 1899 in about one second at throttle 22 -> 17, while Toby was still
  // rolling below the tractive floor. It then held that value through the whole
  // 30 s dwell. On departure the true idle level read 46 counts low, a 3,628 ms
  // passage opened, swallowed the real MM161 magnet whole and was rejected
  // TOO_SOON, and the next magnet disagreed. Finding 10.
  //
  // The capture happened during the DECELERATION, not during the dwell, so a
  // shorter dwell is no defence and the gate has to be closed through the
  // approach ramp. The floor is this locomotive's measured tractive floor,
  // supplied by the caller -- not a borrowed constant.
  //
  // WHY IT MAY NEVERTHELESS MOVE UNDER AN OPEN PASSAGE
  // --------------------------------------------------
  // 0.1 through 0.6 refused outright: `if (open_) return;`. That is what makes
  // a wrong reference permanent. A baseline primed ~70 counts low on
  // 2026-09-01 opened a passage on its first sample and could never close it;
  // the reference was frozen because a passage was open and the passage stayed
  // open because the reference was frozen. It held for 72 minutes across two
  // declarations, because reset() preserves the baseline and the median needs
  // ~525 ms of CLOSED line to move while the offset re-opens a passage after
  // one 25 ms interval. Finding 09.
  //
  // So above the floor the reference is allowed to walk onto a stale offset and
  // close the passage. openMigrateMs keeps it off a real magnet that is still
  // being crossed. The RECORDING is unaffected either way: it is measured
  // against entryBaseline_, which is frozen.
  void updateBaseline(uint32_t nowMs, int16_t raw, bool mayAdapt) {
    if (raw <= 8 || raw >= 4087) clipped_ = true;
    if (!startMs_) startMs_ = nowMs;
    if (nowMs - lastBaseMs_ < cfg_.baselineMs) return;
    lastBaseMs_ = nowMs;
    // Priming is exempt: the locomotive is stationary at boot, so a motion gate
    // above this line would mean there is never a first reference at all.
    if (primed_) {
      if (!mayAdapt) { adaptSinceMs_ = 0; return; }   // at or below the floor: frozen
      if (!adaptSinceMs_) adaptSinceMs_ = nowMs;
      if (open_) {
        const uint32_t from = (openedAtMs_ > adaptSinceMs_) ? openedAtMs_ : adaptSinceMs_;
        if (nowMs - from < cfg_.openMigrateMs) return;
      }
    }
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
    shadowBaseline_ = c[medLen_ / 2];
    if (!primed_ || !cfg_.fixedAfterPrime) baseline_ = shadowBaseline_;
    if (!primed_ && (nowMs - startMs_) >= cfg_.primeMs && medLen_ >= MED / 2) primed_ = true;
  }

  CaptureConfig cfg_;
  int16_t  buf_[RING] = {};        // the recording: signed, oriented at close
  int16_t  judge_[RING] = {};      // median-of-three copy, built once at close
  uint16_t n_ = 0, preAt_ = 0;
  int16_t  pre_[PRE] = {}; uint8_t preHead_ = 0, preLen_ = 0;
  int16_t  med_[MED] = {}; uint8_t medHead_ = 0, medLen_ = 0;
  uint16_t dec_ = 1, decPhase_ = 0;
  int32_t  baseline_ = 0, shadowBaseline_ = 0, entryBaseline_ = 0, peak_ = 0;
  uint32_t adaptSinceMs_ = 0;   // when motion permission last arrived; 0 = not permitted
  int64_t  sum_ = 0;
  uint32_t startMs_ = 0, lastBaseMs_ = 0, openedAtMs_ = 0, quietSince_ = 0;
  uint32_t floorRejects_ = 0;
  FloorRejection floorReject_;
  bool floorRejectPending_ = false;
  bool     primed_ = false, open_ = false, truncated_ = false, clipped_ = false;
  uint8_t  pol_ = 1;
  Passage  out_;
};

}  // namespace navi_one
