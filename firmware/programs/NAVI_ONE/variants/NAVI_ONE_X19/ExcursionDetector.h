#pragma once
// ---------------------------------------------------------------------------
// ExcursionDetector -- X19. 1 kHz acquisition on GPIO 33, and EVENT FRAMING
// WITHOUT CLOSURE.
//
// X18 asked: "has the Hall signal returned to within 25 counts of the global
// reference?" and used the answer to end an event. X19 never asks it. It asks
// only the forward question:
//
//        "has the signal begun doing something that identifies a magnet?"
//
// THE MECHANISM
// -------------
//   L(t)        the RAW sample, within the trailing WINDOW_MS, whose distance
//               from the operative reference is smallest -- the quietest thing
//               the sensor has seen recently.
//   departure   raw(t) - L(t).  SIGNED.  A candidate is declared when
//               |departure| >= departCounts for persistSamples consecutive
//               samples.
//
// Three properties, all arithmetic rather than tuning:
//
//   1. THE REFERENCE CANCELS.  departure = raw(t) - raw(L). Whatever constant
//      offset the reference carries appears in both terms and subtracts out. A
//      reference 112 counts wrong -- as Otto's prime was at 11:50 on
//      2026-09-15 -- cannot blind the detector. The reference survives in one
//      place only: choosing WHICH trailing sample is quietest. On a displaced
//      but flat line every candidate for L is equally displaced, so a wrong
//      reference mis-selects by nothing.
//
//   2. A PERSISTENT LEVEL CANNOT REPEAT.  L is a MINIMUM-MAGNITUDE, so on a
//      shelf it equals the shelf and the departure is zero, for ever. On a
//      decaying tail the most recent samples ARE the smallest, so L follows
//      the signal down and the departure stays near zero. Decay cannot
//      masquerade as arrival. Verified on the 2026-09-15 corpus over fields
//      held for 1.9 s, 2.8 s, 22 s, 25 s and 111 s: zero manufactured events.
//
//   3. BOTH POLES DETECT.  departure is signed and the test is on its
//      magnitude, so a South magnet arriving on a North shelf drives L down
//      through zero and then produces a large NEGATIVE departure. The minimum
//      is a minimum of DISTANCE FROM THE REFERENCE, never of signed value;
//      writing it as a signed minimum would make the detector blind to one
//      pole and is the single easiest way to get this wrong.
//
// PARAMETERS ARE NOT OPTIMISED.  The 2026-09-15 replay
// (docs/NAVI_NEXT_EVENT_WITHOUT_CLOSURE_20260915.md) found plateaus: departure
// threshold 50..80, trailing window 150..600 ms, refractory 300..1000 ms. X19
// takes 70 / 300 / 500 deliberately. 70 is not a new constant -- it is the
// existing entryMargin, re-anchored from a global reference to a local one.
//
// WHAT IS NOT HERE, AND MUST NOT BE ADDED WITHOUT FIELD EVIDENCE
// --------------------------------------------------------------
//   * no exit margin, no exitHold, no ±25 closure;
//   * no requirement to fall below the departure threshold before re-arming;
//   * no local closure, waveform-return closure or peak-relative closure;
//   * no PWM and no motion state in any Hall decision;
//   * no duration floor -- see THE 82 ms FLOOR below;
//   * no decimation. A candidate record is preRoll + window samples at 1 kHz,
//     always, so every field record is full-rate.
//
// THE 82 ms FLOOR (decision 0085) IS NOT IMPLEMENTED HERE, DELIBERATELY
// ---------------------------------------------------------------------
// 0085's 82 ms is a COMPLETED-PASSAGE duration: entry crossing to exit
// crossing against a global reference. X19 has neither crossing. The replay
// measured the conversion -- an excursion measured at a 25-count caliper from
// L runs about 0.93 of the production duration, and at 34% of peak about 0.83
// -- so transplanting the number literally rejects genuine magnets, including
// Otto's known ~83 ms borderline passage. A replacement is NOT invented here.
// Instead every candidate reports both widths, so 0085 can be re-derived from
// X19's own evidence. `widthFloorMs` exists, defaults to 0 (disabled) and is
// the one place to put a number once that derivation is done.
//
// KNOWN CONSEQUENCE, STATED PLAINLY: with the floor disabled, short
// acquisition transients that X18 refused will reach the recognizer. On
// 2026-09-15 there were 24 such events in ~2.5 hours; six of them were shorter
// than 50 ms while the locomotive was moving. `persistSamples` refuses only
// the single-conversion population (decision 0073). This is the largest known
// risk in the build and it is accepted on purpose: the field test exists to
// measure it.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include "MagnetRecognizer.h"

namespace navi_one {

struct DetectorConfig {
  // Departure from the trailing local minimum that declares a candidate.
  // Otto: HALL_DEADBAND_COUNTS + HALL_ENTRY_MARGIN_COUNTS = 70.
  int16_t  departCounts   = 70;
  // Trailing window over which the local minimum is taken.
  uint16_t localWindowMs  = 300;
  // Consecutive samples that must satisfy the departure test. The ONLY
  // transient screen in X19. Bench 2026-09-03 measured the single-conversion
  // population at 1.1 in 1000 held still; two in a row is 1.2 in a million.
  uint8_t  persistSamples = 2;
  // Fixed acquisition window from the detection instant. Measurement ends
  // when it expires; nothing about the signal ends it.
  uint16_t windowMs       = 400;
  // No further candidate may be declared until this has elapsed FROM THE
  // DETECTION INSTANT (not from the end of the window).
  uint16_t refractoryMs   = 500;
  // Raw samples retained ahead of every detection.
  uint16_t preRollMs      = 512;
  // Fraction of the window peak that defines the EXCURSION. Polarity is summed
  // over these samples only; see excursionSum below.
  float    excursionFrac  = 0.34f;
  // Caliper for the reported width, in counts from L. Reporting only: it gates
  // nothing unless widthFloorMs is set non-zero.
  int16_t  widthCaliper   = 25;
  // 0 = disabled. See THE 82 ms FLOOR above. Do not set this from X18's 82.
  uint16_t widthFloorMs   = 0;
  // Reference maintenance, carried over from X18 unchanged.
  uint16_t baselineMs     = 25;
  uint16_t primeMs        = 2000;
  bool     fixedAfterPrime = true;
};

// One completed candidate. It is the X19 analogue of X18's Passage and is
// converted to one for MagnetRecognizer, which is unchanged.
struct Excursion {
  uint32_t detectedAtMs   = 0;   // the sample that completed the persistence run
  uint32_t windowEndMs    = 0;
  int16_t  rawAtDetect    = 0;
  int32_t  localRef       = 0;   // L, in RAW counts. The excursion's zero.
  int32_t  departAtDetect = 0;   // signed, raw(detect) - L
  int32_t  restRef        = 0;   // the MEASURED local resting level at detection
  int32_t  reference      = 0;   // operative baseline at detection, telemetry
  int32_t  shadowRef      = 0;   // rolling median at detection, telemetry
  uint16_t peakCounts     = 0;   // max |v - L| over the window, judged copy
  int16_t  peakSigned     = 0;
  uint8_t  polarity       = 1;   // 1 = N, 0 = S
  int64_t  excursionSum   = 0;   // signed sum over EXCURSION samples only
  uint16_t excursionCount = 0;   // how many samples that sum used
  uint16_t excursionFirst = 0;   // index into samples[] of the first
  uint16_t excursionLast  = 0;   // ... and the last
  uint16_t widthCaliperMs = 0;   // samples with |v-L| >= widthCaliper
  uint16_t widthFracMs    = 0;   // samples with |v-L| >= excursionFrac * peak
  uint16_t sampleCount    = 0;   // preSamples + window samples
  uint16_t preSamples     = 0;   // index of the detection sample in samples[]
  bool     clipped        = false;
  bool     widthRejected  = false;   // only ever true if widthFloorMs != 0
  const int16_t* oriented = nullptr; // L-relative, oriented, full record
  const int16_t* judged   = nullptr; // median-of-three copy of the above
};

// RAW ring must hold preRoll + window with slack, since the record is only
// read out once the window has expired.
template <uint16_t RAW_RING = 1152, uint16_t REC = 960, uint8_t MED = 41>
class ExcursionDetector {
 public:
  explicit ExcursionDetector(const DetectorConfig& cfg) : cfg_(cfg) {}

  int32_t  baseline()       const { return baseline_; }
  int32_t  shadowBaseline() const { return shadowBaseline_; }
  bool     ready()          const { return primed_; }
  bool     acquiring()      const { return acquiring_; }
  bool     refractory()     const { return refractoryUntil_ != 0; }
  uint32_t refractoryUntil()const { return refractoryUntil_; }
  int32_t  localRefNow()    const { return localRef(); }
  int32_t  restRef()        const { return restValid_ ? restRef_ : baseline_; }
  bool     restValid()      const { return restValid_; }
  const Excursion& excursion() const { return out_; }
  uint32_t suppressed()     const { return suppressed_; }
  uint32_t widthRejects()   const { return widthRejects_; }
  int32_t  primeSpread()    const { return primeHi_ - primeLo_; }
  int32_t  primeValue()     const { return primeValue_; }

  // X18's adjustBaseline() moved the reference AND the pre-roll, because the
  // pre-roll was stored relative to it. Here the ring is RAW, so a reference
  // change touches nothing but the reference itself -- which is the whole
  // point of the differential detector. Refused mid-window only so that one
  // candidate's telemetry reports one reference.
  bool adjustBaseline(int8_t delta) {
    if (acquiring_) return false;
    baseline_ += delta;
    return true;
  }

  // A declaration or a direction change ends the frame. The in-flight window
  // is abandoned: it would otherwise be judged against the new target. The
  // raw ring, the reference and the refractory are properties of the SENSOR,
  // not of the frame, and survive.
  //
  // X18 also had to drop an OPEN passage here, because a locomotive declared
  // while parked in a field held one open across the declaration and emitted
  // it on drive-off. X19 has no open passage: driving out of a fringe field is
  // a decay, L follows it down, and no candidate is produced.
  void reset() {
    // Abandoning the in-flight window is not enough on its own. If the sensor
    // is halfway across a magnet when the frame ends, the very next sample
    // still sees a 70-count departure and would emit that same magnet again,
    // now judged against the new target -- which is the 2026-08-31 failure
    // X18 fixed by dropping its open passage here. So a full refractory is
    // armed from the next sample: whatever is under the sensor at the moment
    // of a declaration belongs to the frame that just ended.
    acquiring_ = false;
    persistRun_ = 0;
    suppressedRun_ = 0;
    rearmPending_ = true;
  }

  // One ADC sample. Returns true when a candidate's window has just expired
  // and excursion() holds it.
  //
  // PWM APPEARS NOWHERE IN THE EVENT PATH. mayAdapt gates the rolling median
  // only -- a median taken while parked on a magnet becomes the magnet
  // (finding 10, 2026-09-01) -- and under fixedAfterPrime that median is
  // telemetry with no authority over anything at all.
  bool sample(uint32_t nowMs, int16_t raw, bool mayAdapt) {
    updateBaseline(nowMs, raw, mayAdapt);
    if (raw <= 8 || raw >= 4087) clipRing_ = true;
    // The window histogram tracks exactly the trailing localWindowMs samples:
    // the arriving sample enters, and the one falling out of the window leaves.
    const uint16_t want = cfg_.localWindowMs ? cfg_.localWindowMs : 1;
    if (filled_ >= want) {
      const uint16_t out = (uint16_t)((head_ + RAW_RING - want) % RAW_RING);
      histAdd(ring_[out], false);
    }
    ring_[head_] = raw;
    histAdd(raw, true);
    head_ = (uint16_t)((head_ + 1) % RAW_RING);
    if (filled_ < RAW_RING) ++filled_;
    noteRest(nowMs);
    if (!primed_) return false;

    if (rearmPending_) {
      rearmPending_ = false;
      refractoryUntil_ = nowMs + cfg_.refractoryMs;
      if (refractoryUntil_ == 0) refractoryUntil_ = 1;
    }

    if (acquiring_) {
      // The window is a suppression period too. A second magnet arriving
      // inside it does not just go uncounted -- it CORRUPTS the measurement of
      // the first, because peak and polarity are taken over the whole window.
      // So it is counted here exactly as one inside the refractory is, and
      // suppressed_total on diag/excursion is the field-test's evidence about
      // whether either period ever hid a real marker.
      const int32_t dw = (int32_t)raw - localRefAt_;
      const int32_t mw = dw < 0 ? -dw : dw;
      // The candidate's OWN arc is not a suppressed second candidate. Only an
      // excursion that reappears after the signal has returned toward the
      // local zero is counted. This is bookkeeping for telemetry alone: it
      // gates nothing, ends nothing, and is not a closure test.
      if (mw < cfg_.departCounts) sawQuietInWindow_ = true;
      if (mw >= cfg_.departCounts && sawQuietInWindow_) {
        ++suppressedRun_;
        if (suppressedRun_ == cfg_.persistSamples) {
          ++suppressed_;
          lastSuppressedMs_ = nowMs;
          lastSuppressedDepart_ = dw;
        }
      } else if (mw < cfg_.departCounts) suppressedRun_ = 0;
      if ((int32_t)(nowMs - windowEndMs_) >= 0) return finish(nowMs);
      return false;
    }

    // Refractory: the signal is still watched and the local minimum still
    // tracks, but no candidate may be declared. Anything that WOULD have
    // triggered is counted and reported, so a field record can show whether
    // the refractory ever hid a real magnet.
    const int32_t L = localRef();
    const int32_t departure = (int32_t)raw - L;
    const int32_t mag = departure < 0 ? -departure : departure;
    const bool over = mag >= cfg_.departCounts;

    if (refractoryUntil_) {
      if ((int32_t)(nowMs - refractoryUntil_) >= 0) refractoryUntil_ = 0;
      else {
        if (over) {
          ++suppressedRun_;
          if (suppressedRun_ == cfg_.persistSamples) {
            ++suppressed_;
            lastSuppressedMs_ = nowMs;
            lastSuppressedDepart_ = departure;
          }
        } else suppressedRun_ = 0;
        return false;
      }
    }

    if (!over) { persistRun_ = 0; return false; }
    if (++persistRun_ < cfg_.persistSamples) return false;

    // The detection instant is the sample that COMPLETED the run, matching the
    // replay. The persistSamples-1 samples before it are inside the record.
    persistRun_ = 0;
    suppressedRun_ = 0;
    sawQuietInWindow_ = false;
    acquiring_    = true;
    detectedAtMs_ = nowMs;
    windowEndMs_  = nowMs + cfg_.windowMs;
    detectHead_   = head_;                 // one past the detection sample
    localRefAt_   = L;
    restAt_       = restValid_ ? restRef_ : baseline_;
    rawAtDetect_  = raw;
    departAt_     = departure;
    refAt_        = baseline_;
    shadowAt_     = shadowBaseline_;
    clipAt_       = clipRing_;
    clipRing_     = false;
    return false;
  }

 private:
  // THE LOCAL RESTING LEVEL: the level the sensor has spent the most time at
  // during the trailing window, measured from the Hall stream and NOTHING ELSE.
  //
  // THIS USED TO CONSULT baseline_ AND THAT WAS A DEFECT.
  // ------------------------------------------------------
  // The first version picked the trailing sample of smallest |raw - baseline_|.
  // The departure arithmetic still cancelled the reference -- raw(t) - raw(L)
  // -- but the SELECTION did not, so the reference kept detection authority
  // through the back door, and a wrong one did not merely weaken the detector,
  // it inverted it. Measured on the shipped code with the reference primed 112
  // counts off the resting level and a magnet pulling the signal back TOWARD
  // that erroneous reference:
  //
  //     amplitude   82   100   120   137   173      (the genuine range; the
  //     detected   +134  +127  +125  +127  +130      whole arc is over by +140)
  //     peak        82   100   112   112   113      (clamped at |E|, not A)
  //     polarity     N     N     N     N     N      (every one of them is S)
  //
  // The mechanism: as the arc moves toward the reference every new sample is
  // closer to it, so the selected sample walks down with the signal and the
  // departure stays at zero -- the same behaviour that correctly absorbs a
  // decaying tail, applied to an arriving magnet because the reference was
  // wrong about which way "rest" lay. Detection then happens on the arc's
  // TRAILING edge, against a zero taken at the apex, which reports the
  // recovery as if it were the magnet and therefore reports the opposite pole.
  // A wrong count is recoverable; an inverted pole strikes.
  //
  // THE REPLACEMENT USES NO REFERENCE AT ALL.
  // -----------------------------------------
  // Rest is where the signal has been, and a magnet is where it briefly goes.
  // So take the most-occupied level of the window, at a resolution of one
  // detection threshold -- two levels less than a threshold apart are not
  // distinguishable events, and the band width is therefore departCounts and
  // not a new constant. Within an arc that band still lands on the shoulders
  // the signal dwells on rather than the apex it passes through, and on a
  // decaying tail it follows the bulk down, which is what stops a recovery
  // from being read as an arrival.
  //
  // THE SHAPE OF THE ANSWER
  // -----------------------
  // The original selection was right in FORM and wrong in its anchor. Picking
  // the trailing sample nearest a known resting level is what makes a decaying
  // tail invisible: as the signal falls back toward rest, each new sample is
  // nearer to it, the selection walks down with the signal, and the departure
  // stays at zero. That property is worth keeping -- taking it out costs four
  // of the slow magnets of 2026-09-15 a spurious second candidate of the
  // OPPOSITE pole, emitted the moment the refractory expires on their tails.
  //
  // So the anchor changes and nothing else does. `baseline_` is replaced by
  // restRef_, a resting level the detector MEASURES for itself from the Hall
  // stream (see noteRest), and the global reference leaves the event path
  // entirely. Before the first rest has been established -- only ever true in
  // the first few hundred ms after priming -- the window's own most-occupied
  // level stands in, which is also reference-free.
  int32_t localRef() const {
    // Unreachable once running: sample() pushes to the ring before it asks.
    // Kept so the function is total, and it is the only line in the detection
    // path where baseline_ still appears at all.
    if (!filled_) return restValid_ ? restRef_ : baseline_;
    const int32_t anchor = restValid_ ? restRef_ : windowLevel(nullptr);
    const uint16_t want = cfg_.localWindowMs ? cfg_.localWindowMs : 1;
    uint16_t n = want < filled_ ? want : filled_;
    int32_t best = anchor; int32_t bestMag = INT32_MAX;
    uint16_t i = head_;
    for (uint16_t k = 0; k < n; ++k) {
      i = (uint16_t)((i + RAW_RING - 1) % RAW_RING);
      const int32_t d = (int32_t)ring_[i] - anchor;
      const int32_t m = d < 0 ? -d : d;
      if (m < bestMag) { bestMag = m; best = ring_[i]; }
    }
    return best;
  }

  // The most-occupied level of the trailing window, and how much of the window
  // it holds. Levels are resolved at one detection threshold: two levels less
  // than departCounts apart are not distinguishable events, so the band width
  // is departCounts and is not a new constant. Scans the bin table, not the
  // samples -- ~512 iterations, a few microseconds of a 1000 us tick.
  int32_t windowLevel(uint32_t* holdOut) const {
    const uint16_t span = (uint16_t)(cfg_.departCounts > 0
                                       ? (cfg_.departCounts + kBinSize - 1) / kBinSize
                                       : 1);
    uint32_t bestCount = 0; int64_t bestSum = 0;
    uint32_t count = 0; int64_t sum = 0;
    for (uint16_t b = 0; b < kBins + span; ++b) {
      if (b < kBins) { count += histN_[b]; sum += histS_[b]; }
      if (b >= span) { count -= histN_[b - span]; sum -= histS_[b - span]; }
      if (count > bestCount) { bestCount = count; bestSum = sum; }
    }
    if (holdOut) *holdOut = bestCount;
    if (!bestCount) return baseline_;      // unreachable once filled_ > 0
    return (int32_t)(bestSum / (int64_t)bestCount);
  }

  // ESTABLISHING REST, WITHOUT A REFERENCE AND WITHOUT A CLOSURE TEST.
  //
  // A level counts as rest when the trailing window has spent nearly all of
  // itself there -- three quarters, so noise and a couple of stray conversions
  // cannot deny it. Nothing about an EVENT depends on this. It does not end a
  // candidate, does not re-arm anything, is not consulted by the refractory,
  // and no signal has to return anywhere for a candidate to be admitted: the
  // boundary is still detect -> 400 ms -> 500 ms, unconditional. This decides
  // one thing only -- what the next departure will be measured from.
  //
  // A LEVEL MUST OUTLAST A MAGNET BEFORE IT IS BELIEVED TO BE REST.
  //
  // Three quarters of one window is not enough on its own, and the slow
  // magnets of 2026-09-15 are why. At PWM 42 an arc runs 343 to 672 ms and its
  // shoulders sit inside a 70-count band for about 400 ms, so the magnet fills
  // the window, passes the three-quarters test, and installs ITSELF as rest --
  // after which its own tail departs from it and is counted as a second
  // magnet of the opposite pole. Four records did exactly that.
  //
  // So the level must also hold CONTINUOUSLY for longer than the detector's
  // own blind period: localWindowMs + refractoryMs. Nothing shorter can be
  // trusted, because anything shorter might be one magnet the detector is
  // still in the middle of. The number is derived from the parameters already
  // in use and is not a new one. At cruise the line between two magnets is
  // quiet for about 950 ms, so rest is still re-measured on most intervals;
  // and it need not be re-measured often, since the level it tracks moves at
  // 3.6 to 7.2 counts per MINUTE.
  //
  // This is not a closure test. It ends nothing, re-arms nothing, gates no
  // candidate, and no signal has to return anywhere for one to be admitted.
  // It decides one thing: what the next departure is measured from.
  void noteRest(uint32_t nowMs) {
    const uint16_t want = cfg_.localWindowMs ? cfg_.localWindowMs : 1;
    const uint32_t n = want < filled_ ? want : filled_;
    if (acquiring_ || n < want) { quietRun_ = 0; return; }
    uint32_t hold = 0;
    const int32_t level = windowLevel(&hold);
    if (hold * 4 < n * 3) { quietRun_ = 0; return; }
    const int32_t moved = level - quietLevel_;
    if (!quietRun_ || (moved < 0 ? -moved : moved) > kBinSize) {
      quietRun_ = 1; quietLevel_ = level; quietSinceMs_ = nowMs; return;
    }
    quietLevel_ = level;
    ++quietRun_;
    if (nowMs - quietSinceMs_ >= (uint32_t)cfg_.localWindowMs + cfg_.refractoryMs) {
      restRef_ = level; restValid_ = true;
    }
  }

  // Add or remove one raw sample from the window histogram. Bins are 8 counts
  // wide, which is finer than any decision this feeds and keeps the table to
  // 512 entries; each bin also carries the sum of its raw samples so the
  // returned level is the mean of the band rather than its quantised centre.
  void histAdd(int16_t raw, bool add) {
    int32_t v = raw; if (v < 0) v = 0; if (v > 4095) v = 4095;
    const uint16_t b = (uint16_t)(v / kBinSize);
    if (add) { ++histN_[b]; histS_[b] += v; }
    else if (histN_[b]) { --histN_[b]; histS_[b] -= v; }
  }

  // The window has expired. Build the record: pre-roll + window, expressed
  // against L rather than against any global reference, oriented once the pole
  // is known -- exactly as X18 oriented at close, and for the same reason
  // (MagnetRecognizer must never see N or S).
  bool finish(uint32_t nowMs) {
    acquiring_ = false;
    const uint16_t pre = (uint16_t)(cfg_.preRollMs < filled_ ? cfg_.preRollMs : filled_);
    // Samples from detection to now, inclusive of the detection sample.
    uint16_t post = (uint16_t)((head_ + RAW_RING - detectHead_) % RAW_RING);
    uint16_t total = (uint16_t)(pre + 1 + post);
    if (total > REC) {                       // cannot happen with the shipped
      total = REC;                           // sizes; clamped rather than lost
      post  = (uint16_t)(REC - pre - 1);
    }
    // Walk back `pre` samples before the detection sample.
    uint16_t start = (uint16_t)((detectHead_ + RAW_RING - 1 - pre) % RAW_RING);
    n_ = 0;
    for (uint16_t k = 0; k < total; ++k) {
      const uint16_t i = (uint16_t)((start + k) % RAW_RING);
      int32_t v = (int32_t)ring_[i] - localRefAt_;
      if (v >  32767) v =  32767;
      if (v < -32768) v = -32768;
      buf_[n_++] = (int16_t)v;
    }
    const uint16_t preAt = pre;              // index of the detection sample

    // ------------------------------------------------------------------
    // THE POLE IS DECIDED FROM THE EXCURSION, NOT FROM THE WHOLE WINDOW.
    //
    // Decision 0064 forbids taking it from one sample and takes it from the
    // signed sum instead. X18 summed the whole PASSAGE, which ended when the
    // signal returned. A fixed window does not end, so it accumulates a long
    // quiet tail, and every count of error in the excursion's zero is
    // multiplied by the window length. Measured on the 2026-09-15 dec=1
    // records: summing the full 400 ms window flips the sign with a reference
    // error of 6 counts at worst and 30 at the median; summing only the
    // excursion needs 70 at worst and 104 at the median.
    //
    // So: find the peak over the window (from the median-of-three judgement
    // copy, decision 0065, so no lone sample can set it), take the contiguous
    // run around it that stays above excursionFrac of that peak, and sum only
    // that. The PRE-ROLL IS EXCLUDED from the peak search -- it is evidence
    // about the line before the magnet, not part of the magnet.
    // ------------------------------------------------------------------
    medianOfThree(buf_, n_, judge_);
    int32_t peak = 0; uint16_t peakAt = preAt;
    for (uint16_t i = preAt; i < n_; ++i) {
      const int32_t m = judge_[i] < 0 ? -(int32_t)judge_[i] : (int32_t)judge_[i];
      if (m > peak) { peak = m; peakAt = i; }
    }
    int32_t thr = (int32_t)(peak * cfg_.excursionFrac);
    if (thr < 1) thr = 1;
    uint16_t first = peakAt, last = peakAt;
    while (first > preAt) {
      const int32_t m = judge_[first - 1] < 0 ? -(int32_t)judge_[first - 1] : judge_[first - 1];
      if (m < thr) break;
      --first;
    }
    while (last + 1 < n_) {
      const int32_t m = judge_[last + 1] < 0 ? -(int32_t)judge_[last + 1] : judge_[last + 1];
      if (m < thr) break;
      ++last;
    }
    int64_t sum = 0;
    for (uint16_t i = first; i <= last; ++i) sum += judge_[i];
    const uint8_t pol = sum >= 0 ? 1 : 0;

    // Widths, reported for the re-derivation of decision 0085. Neither gates
    // anything unless widthFloorMs has been set.
    uint16_t wCal = 0, wFrac = 0;
    for (uint16_t i = preAt; i < n_; ++i) {
      const int32_t m = judge_[i] < 0 ? -(int32_t)judge_[i] : judge_[i];
      if (m >= cfg_.widthCaliper) ++wCal;
      if (m >= thr) ++wFrac;
    }

    // Orient. buf_ becomes THE RECORDING and is not touched again.
    const int16_t peakSigned = (int16_t)(pol ? peak : -peak);
    if (!pol) {
      for (uint16_t i = 0; i < n_; ++i) buf_[i] = (int16_t)(-buf_[i]);
      medianOfThree(buf_, n_, judge_);
    }

    out_ = Excursion{};
    out_.detectedAtMs   = detectedAtMs_;
    out_.windowEndMs    = nowMs;
    out_.rawAtDetect    = rawAtDetect_;
    out_.localRef       = localRefAt_;
    out_.restRef        = restAt_;
    out_.departAtDetect = departAt_;
    out_.reference      = refAt_;
    out_.shadowRef      = shadowAt_;
    out_.peakCounts     = (uint16_t)(peak > 65535 ? 65535 : peak);
    out_.peakSigned     = peakSigned;
    out_.polarity       = pol;
    out_.excursionSum   = sum;
    out_.excursionCount = (uint16_t)(last - first + 1);
    out_.excursionFirst = first;
    out_.excursionLast  = last;
    out_.widthCaliperMs = wCal;
    out_.widthFracMs    = wFrac;
    out_.sampleCount    = n_;
    out_.preSamples     = preAt;
    out_.clipped        = clipAt_;
    out_.oriented       = buf_;
    out_.judged         = judge_;

    // The refractory runs from the DETECTION instant, not from here, so a
    // 400 ms window inside a 500 ms refractory leaves 100 ms of live search.
    refractoryUntil_ = detectedAtMs_ + cfg_.refractoryMs;
    if (refractoryUntil_ == 0) refractoryUntil_ = 1;      // 0 means "not set"
    suppressedRun_ = 0;

    if (cfg_.widthFloorMs && wCal < cfg_.widthFloorMs) {
      out_.widthRejected = true;
      ++widthRejects_;
      return false;                 // reported by the caller, never a candidate
    }
    return true;
  }

  // Carried over from X18 verbatim except that there is no open passage to
  // guard against, so openMigrateMs is gone with it. Under fixedAfterPrime the
  // prime value is authoritative until reboot and the rolling median runs as
  // shadow telemetry only.
  void updateBaseline(uint32_t nowMs, int16_t raw, bool mayAdapt) {
    if (!startMs_) startMs_ = nowMs;
    if (!primed_) {
      if (!primeHave_ || raw < primeLo_) primeLo_ = raw;
      if (!primeHave_ || raw > primeHi_) primeHi_ = raw;
      primeHave_ = true;
    }
    if (nowMs - lastBaseMs_ < cfg_.baselineMs) return;
    lastBaseMs_ = nowMs;
    if (primed_ && !mayAdapt) return;        // parked: the median would be the magnet
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
    if (!primed_ && (nowMs - startMs_) >= cfg_.primeMs && medLen_ >= MED / 2) {
      primed_ = true;
      primeValue_ = baseline_;
    }
  }

 public:
  uint32_t lastSuppressedMs() const { return lastSuppressedMs_; }
  int32_t  lastSuppressedDepart() const { return lastSuppressedDepart_; }

 private:
  DetectorConfig cfg_;
  static constexpr uint16_t kBinSize = 8;
  static constexpr uint16_t kBins     = 4096 / kBinSize;   // 512
  uint16_t histN_[kBins] = {};        // samples per bin, trailing window only
  int32_t  histS_[kBins] = {};        // sum of the raw samples in each bin
  int32_t  restRef_ = 0;              // the MEASURED local resting level
  bool     restValid_ = false;
  uint32_t quietRun_ = 0, quietSinceMs_ = 0;
  int32_t  quietLevel_ = 0;
  int16_t  ring_[RAW_RING] = {};
  uint16_t head_ = 0, filled_ = 0;
  int16_t  buf_[REC] = {};
  int16_t  judge_[REC] = {};
  uint16_t n_ = 0;
  int16_t  med_[MED] = {}; uint8_t medHead_ = 0, medLen_ = 0;
  int32_t  baseline_ = 0, shadowBaseline_ = 0;
  int32_t  primeValue_ = 0, primeLo_ = 0, primeHi_ = 0; bool primeHave_ = false;
  uint32_t startMs_ = 0, lastBaseMs_ = 0;
  bool     primed_ = false;
  // candidate state
  bool     acquiring_ = false, sawQuietInWindow_ = false, rearmPending_ = false;
  uint8_t  persistRun_ = 0, suppressedRun_ = 0;
  uint32_t detectedAtMs_ = 0, windowEndMs_ = 0, refractoryUntil_ = 0;
  uint16_t detectHead_ = 0;
  int32_t  localRefAt_ = 0, departAt_ = 0, refAt_ = 0, shadowAt_ = 0, restAt_ = 0;
  int16_t  rawAtDetect_ = 0;
  bool     clipRing_ = false, clipAt_ = false;
  uint32_t suppressed_ = 0, widthRejects_ = 0, lastSuppressedMs_ = 0;
  int32_t  lastSuppressedDepart_ = 0;
  Excursion out_;
};

}  // namespace navi_one
