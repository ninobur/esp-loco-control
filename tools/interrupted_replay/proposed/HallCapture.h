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

  // ======================================================================
  // SETTLE DETECTION (decision 0070). Does the field under the sensor sit
  // still? Nothing else in this file asks that question, and no PWM, station
  // state or map position answers it.
  //
  // THE CONSTANTS ARE MEASURED, AND THE FIRST PROPOSAL FAILED THE MEASUREMENT.
  // A raw +/-8 count band held for 400 ms was proposed on 2026-09-01 and is
  // arithmetically incapable of firing on this sensor. Four stationary
  // plateaus in the field captures -- 261 to 386 real 1 kHz samples each,
  // taken while Toby was demonstrably not moving -- give:
  //
  //   plateau (capture)                sd    16-sample window span
  //                                          raw p95 / max   trimmed p95 / max
  //   finding 11, parked on MM59      4.22      48 / 50            11 / 12
  //   finding 13, parked in a fringe  3.62      46 / 46             8 /  9
  //   finding 09 A, latched offset    3.02      21 / 30            11 / 13
  //   finding 09 B, latched offset    3.81      22 / 24            11 / 13
  //
  // The raw span is dominated by single-sample outliers -- the same population
  // that made decision 0065 necessary. Discarding the one highest and the one
  // lowest sample of the window collapses it to a maximum of 13 counts across
  // every plateau on record. 20 sits above that with margin and an order of
  // magnitude below the 148-count excursion a departing magnet produces.
  //
  // 16 samples at 25 ms is the same cadence the baseline sampler already runs
  // at, so this costs one more ring of 64 bytes and no new timebase.
  uint16_t settleStepMs   = 25;
  uint16_t settleWindowMs = 400;   // = 16 * settleStepMs; see kSettle
  int16_t  settleSpan     = 20;    // trimmed span ceiling, counts

  // A changing stretch shorter than the passage floor is a transient, not a
  // traversal. Restated from floorMs so the interrupted path and the ordinary
  // one cannot drift apart; MagnetRecognizer::checkInterruptedConfig() and
  // gate 12 both assert the equality.
  uint16_t interruptMinMs = 40;

  // HOW LONG THE SENSOR MUST BE CLEAR BEFORE AN EPISODE IS OVER.
  //
  // Not exitHoldMs. Ending a PASSAGE early costs a few samples of tail; ending
  // an EPISODE early truncates the departure evidence, and a departure that
  // crosses the idle level on its way to a magnet passes through the exit band
  // for tens of milliseconds. Gate 12 case F7 -- parked in an opposite-sign
  // fringe, departing across a same-sign magnet -- spends 49 ms inside the
  // band on the way up, and an 8 ms rule cut the segment off there, judged the
  // 13-count stub NO_CURVE, and withdrew AUTO on a departure that was about to
  // be textbook.
  //
  // 400 ms is the settle window, reused. The check is that it still fits
  // between real magnets at the fastest the railway runs: at PWM 110 Toby
  // makes 340 mm/s, markers are 300 mm apart, and a 210-count magnet with a
  // 15 mm sigma is inside the 25-count exit band beyond 31 mm, so the flat
  // between two magnets is 238 mm -- 700 ms. It fits, with 300 ms to spare.
  uint16_t episodeClearMs = 400;
};

// What sample() has just produced. `None` is zero, so the historical
// `if (cap.sample(...))` still reads "something is ready" -- but a caller that
// treats a true return as "a passage closed" is now WRONG unless it also
// passes stopIntent false, which is what every gate written before decision
// 0070 does. New call sites must switch on event().
enum class HallEvent : uint8_t {
  None = 0,
  Passage,      // an ordinary completed traversal -- passage(), full recognizer
  PreStop,      // segment(): the changing evidence before a controlled stop
  Departure,    // segment(): the changing evidence after it
  Abandoned,    // an interruption episode cleared having established nothing
};

inline const char* hallEventName(HallEvent e) {
  switch (e) {
    case HallEvent::Passage:   return "PASSAGE";
    case HallEvent::PreStop:   return "PRE_STOP";
    case HallEvent::Departure: return "DEPARTURE";
    case HallEvent::Abandoned: return "ABANDONED";
    default:                   return "NONE";
  }
}

// Where an interruption episode stands. None means the ordinary world.
enum class Occupancy : uint8_t {
  None = 0,
  Pending,      // the pre-stop evidence did not establish a magnet
  Counted,      // it did, exactly once; everything else is suppressed
};

inline const char* occupancyName(Occupancy o) {
  switch (o) {
    case Occupancy::Pending: return "OCCUPIED_PENDING";
    case Occupancy::Counted: return "OCCUPIED_COUNTED";
    default:                 return "TRAVERSING";
  }
}

template <uint16_t RING = 512, uint8_t PRE = 12, uint8_t MED = 41>
class HallCapture {
 public:
  explicit HallCapture(const CaptureConfig& cfg) : cfg_(cfg) {}

  int32_t baseline() const { return baseline_; }
  bool    ready()    const { return primed_; }
  const Passage& passage() const { return out_; }
  uint32_t floorRejects() const { return floorRejects_; }

  int32_t entryBaseline() const { return entryBaseline_; }

  // ---- interruption episode (decision 0070) -------------------------------
  HallEvent event()      const { return ev_; }
  const Segment& segment() const { return seg_; }
  Occupancy occupancy()  const { return occ_; }
  bool     settled()     const { return settled_; }
  int32_t  settledLevel() const { return settledLevel_; }

  // THE LIVENESS RULE. A passage that is open while a controlled stop runs its
  // course, and never settles, is neither an ordinary traversal nor an
  // interruption this layer can reason about -- and it is about to swallow a
  // 30 s dwell into one Gaussian, which is findings 11 and 13 exactly. The
  // caller asks this at the one moment it matters, when it is about to
  // authorise departure, and holds the locomotive if it is true.
  bool openUnsettled() const { return open_ && occ_ == Occupancy::None; }

  // The caller reports back what the recognizer made of a segment. Only an
  // ACCEPTANCE is reported: a rejection leaves the episode Pending, so the
  // departure still has its chance, and an episode that ends Pending is
  // abandoned rather than counted. Exactly zero or one advance, per episode.
  void episodeCounted() { occ_ = Occupancy::Counted; counted_ = true; }

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
  //                sample, and therefore the polarity, the peak, the amplitude
  //                ratio and the Gaussian residual, is measured against it.
  //
  // Without the split, letting the reference migrate to clear a latch would
  // drag the recording with it and deform the very curve decisions 0064 and
  // 0065 exist to protect. With it, the two jobs stop fighting: the recording
  // is a fixed-reference measurement of the field, and the open/close test is
  // free to follow the sensor.
  //
  // stopIntent is the caller's statement that THE FIRMWARE IS EXECUTING AN
  // IDENTIFIED CONTROLLED STOP -- today, a station zero-ramp or dwell. It is
  // the mode selector of decision 0070 and it is NOT evidence: it cannot
  // create a magnet, supply identity or authorise an advance, and every test
  // below this line is a Hall test. Settling alone must never select the
  // alternate rule, so an uninterrupted slow crossing whose apex looks flat
  // for half a second keeps the complete Gaussian recognizer.
  bool sample(uint32_t nowMs, int16_t raw, bool mayAdapt, bool stopIntent = false) {
    ev_ = HallEvent::None;
    updateBaseline(nowMs, raw, mayAdapt);
    updateSettle(nowMs, raw);
    if (!primed_) return false;

    // LIVE reference: threshold arithmetic only.
    const int32_t delta = (int32_t)raw - baseline_;
    const int32_t mag   = delta < 0 ? -delta : delta;

    // An interruption episode owns the sensor until its field clears. Nothing
    // opens, nothing closes and nothing is recorded through the dwell.
    if (occ_ != Occupancy::None) return serviceOccupied(nowMs, raw, mag);

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
      // excused the shape test on the next magnet.
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

    // BOTH conditions, never one. The stop is what selects the mode; the
    // settle is what proves the traversal was cut in half.
    if (stopIntent && settled_) return interrupt(nowMs);

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
  // truncated_, which made the recognizer ABSTAIN from the shape test. So at
  // crawl speed -- a station approach, or the 18.7 s throttle-off dwell of
  // decision 0057 -- the second-strongest test was systematically absent, and
  // the passage was accepted on amplitude and the guard alone.
  //
  // Nothing is dropped now. When the buffer fills, the whole arc is halved in
  // place and the sample rate halves with it. The Gaussian fit derives its own
  // centre and sigma from the data in sample-index units, so scaling x by a
  // constant leaves the normalised residual unchanged. A 512-sample buffer at
  // dec_ = 32768 still covers four and a half hours of passage.
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
    // An episode belongs to the frame that has ended, like everything else
    // here. It may not carry a suppression, or a pending judgement, across a
    // declaration into a frame that never saw the stop.
    occ_ = Occupancy::None; counted_ = false; depOpen_ = false; depJudged_ = false;
    depSince_ = 0; ev_ = HallEvent::None; seg_ = Segment{};
    stlLen_ = 0; stlHead_ = 0; settled_ = false;
  }

 private:
  // =========================================================================
  // INTERRUPTION HANDLING (decision 0070). Everything below is reachable only
  // when the caller passes stopIntent true AND the field has settled. With
  // stopIntent false -- every gate written before this decision, and every
  // moment of ordinary running -- none of it executes and the class behaves
  // exactly as it did.
  // =========================================================================

  // Is the field sitting still? A 400 ms window of samples at the baseline
  // sampler's own cadence, with the single highest and single lowest
  // discarded, must span no more than settleSpan counts. The trim is not
  // fussiness: on four real stationary plateaus the untrimmed span reached 50
  // counts while the trimmed span never passed 13. One outlier sample is the
  // population decision 0065 exists for, and it is present at rest too.
  void updateSettle(uint32_t nowMs, int16_t raw) {
    if (!lastStlMs_) { lastStlMs_ = nowMs; return; }
    if (nowMs - lastStlMs_ < cfg_.settleStepMs) return;
    lastStlMs_ = nowMs;
    stl_[stlHead_] = raw; stlN_[stlHead_] = n_; stlAt_[stlHead_] = nowMs;
    stlHead_ = (uint8_t)((stlHead_ + 1) % kSettle);
    if (stlLen_ < kSettle) ++stlLen_;
    settled_ = false;
    if (stlLen_ < kSettle) return;
    int16_t c[kSettle];
    for (uint8_t i = 0; i < kSettle; ++i) c[i] = stl_[i];
    for (uint8_t i = 1; i < kSettle; ++i) {
      int16_t v = c[i]; int j = (int)i - 1;
      while (j >= 0 && c[j] > v) { c[j + 1] = c[j]; --j; }
      c[j + 1] = v;
    }
    if ((int32_t)c[kSettle - 2] - (int32_t)c[1] > (int32_t)cfg_.settleSpan) return;
    settled_ = true;
    settledLevel_ = c[kSettle / 2];
    // stlHead_ now indexes the OLDEST entry: the start of this window, and
    // therefore the last moment the field was demonstrably still changing.
    settleFromMs_ = stlAt_[stlHead_];
    settleFromN_  = stlN_[stlHead_];
  }

  // The traversal was cut in half. Hand out everything recorded BEFORE the
  // settle window opened; the window itself, and every stationary sample after
  // it, is excluded by construction.
  bool interrupt(uint32_t nowMs) {
    (void)nowMs;
    const uint16_t segN = settleFromN_ > n_ ? n_ : settleFromN_;
    seg_ = Segment{};
    seg_.v = buf_;
    seg_.n = segN;
    seg_.preSamples = preAt_ > segN ? segN : preAt_;
    seg_.fromMs = openedAtMs_;
    // A passage that opened onto an ALREADY settled field -- a latch -- has a
    // window that predates it and therefore no changing evidence at all. Zero
    // duration, refused by the recognizer's floor.
    seg_.toMs = settleFromMs_ > openedAtMs_ ? settleFromMs_ : openedAtMs_;
    int64_t sum = 0;
    for (uint16_t i = 0; i < segN; ++i) sum += buf_[i];
    seg_.signedSum = sum;
    seg_.polarity  = sum >= 0 ? 1 : 0;            // decision 0064, on the segment
    const int32_t sign = seg_.polarity ? 1 : -1;
    // The level the field sat at before it arrived: the median of the first
    // three samples of the pre-roll, never one sample. With no pre-roll -- the
    // first passage after a declaration -- this reads inside the arc itself,
    // the growth comes out small, and the segment is refused. That is the
    // conservative direction: entry cannot be proved without a before.
    seg_.growthFrom = (int16_t)(sign * (int32_t)med3At(buf_, segN, 1));
    seg_.clipped    = clipped_;
    seg_.decimation = dec_;
    seg_.departure  = false;
    // FROZEN HERE. settledLevel_ keeps tracking -- it has to, so that a later
    // stop is measured from where the locomotive then is -- but the level this
    // EPISODE is referenced to is the one it parked at, and it may not drift
    // under the episode any more than entryBaseline_ may drift under a
    // passage. Same argument, same shape, one layer up.
    parkedLevel_ = settledLevel_;
    occ_ = Occupancy::Pending;      // the caller promotes it if the Hall says so
    counted_ = false; depOpen_ = false; depJudged_ = false; pendingEnd_ = false;
    quietSince_ = 0; depSince_ = 0;
    ev_ = HallEvent::PreStop;
    return true;
  }

  // The sensor is in an occupied field. Nothing here can close a passage or
  // record a dwell; the only thing being watched for is the field CHANGING
  // again, which is the departure.
  bool serviceOccupied(uint32_t nowMs, int16_t raw, int32_t mag) {
    if (pendingEnd_) { pendingEnd_ = false; return endEpisode(nowMs); }

    const int32_t d  = (int32_t)raw - parkedLevel_;    // boundaries: where it parked
    const int32_t dm = d < 0 ? -d : d;

    if (!depOpen_) {
      pre_[preHead_] = raw;                      // a pre-roll for the departure
      preHead_ = (uint8_t)((preHead_ + 1) % PRE);
      if (preLen_ < PRE) ++preLen_;

      if (dm < cfg_.entryMargin) {
        depSince_ = 0;
        // Still parked. The episode ends when the ORIGINAL field clears -- a
        // field the locomotive never left cannot be "returned to".
        if (mag < cfg_.exitMargin) {
          if (!quietSince_) quietSince_ = nowMs;
          if (nowMs - quietSince_ >= cfg_.episodeClearMs) return endEpisode(nowMs);
        } else quietSince_ = 0;
        return false;
      }
      // SUSTAINED, for the same hold the exit rule uses. One sample may not
      // open a departure: at rest the raw stream throws single-sample outliers
      // of 25 counts and more -- the F11 plateau reached 47 -- and an outlier
      // that opened a departure segment would spend the episode's evidence on
      // 8 ms of noise. Measured, like everything else here: see gate 12 C.
      if (!depSince_) depSince_ = nowMs;
      if (nowMs - depSince_ < cfg_.exitHoldMs) return false;
      // Moving again. A FRESH recording, referenced to the idle level, with
      // where the locomotive was parked carried as the growth reference.
      depOpen_ = true;
      depFromMs_ = nowMs;
      depRef_ = baseline_;
      n_ = 0; preAt_ = 0; dec_ = 1; decPhase_ = 0; sum_ = 0; clipped_ = false;
      const uint8_t start = (uint8_t)((preHead_ + PRE - preLen_) % PRE);
      for (uint8_t i = 0; i < preLen_; ++i) {
        const int16_t v = (int16_t)((int32_t)pre_[(start + i) % PRE] - depRef_);
        push(v); tally(v);
      }
      preAt_ = n_;
      quietSince_ = 0;
    }

    const int32_t rec = (int32_t)raw - depRef_;
    push((int16_t)rec); tally((int16_t)rec);

    // Two ways for a departure excursion to end: back to where the locomotive
    // was parked, or clear of everything. The second needs the longer hold --
    // see episodeClearMs. The first does not: a field the sensor has returned
    // to is a field it is sitting in.
    const bool clearOfAll = mag < cfg_.exitMargin;
    const bool backToPark = dm < cfg_.exitMargin;
    if (backToPark || clearOfAll) {
      if (!quietSince_) quietSince_ = nowMs;
      const uint32_t need = backToPark ? cfg_.exitHoldMs : cfg_.episodeClearMs;
      if (nowMs - quietSince_ >= need) {
        depOpen_ = false; quietSince_ = 0; depSince_ = 0;
        // THE CAP IS ON ACCEPTANCE, NOT ON EVALUATION. Once a magnet has been
        // established the episode is Counted and nothing more is judged, so
        // there can never be two advances. But a departure excursion that is
        // REFUSED must not consume the episode's only chance -- a rejected
        // rebound followed by the real crossing is an ordinary sequence, and
        // the ordinary path would have judged both.
        const bool judge = (occ_ == Occupancy::Pending);
        depJudged_ = true;
        if (judge) {
          buildDeparture(nowMs);
          pendingEnd_ = clearOfAll;              // ended next tick, after the copy
          return true;
        }
        if (clearOfAll) return endEpisode(nowMs);
      }
    } else quietSince_ = 0;
    return false;
  }

  void buildDeparture(uint32_t nowMs) {
    seg_ = Segment{};
    seg_.v = buf_; seg_.n = n_; seg_.preSamples = preAt_;
    seg_.fromMs = depFromMs_; seg_.toMs = nowMs;
    seg_.signedSum = sum_;
    seg_.polarity  = sum_ >= 0 ? 1 : 0;
    const int32_t sign = seg_.polarity ? 1 : -1;
    seg_.growthFrom = (int16_t)(sign * (parkedLevel_ - depRef_));
    seg_.clipped    = clipped_;
    seg_.decimation = dec_;
    seg_.departure  = true;
    ev_ = HallEvent::Departure;
  }

  // The field has cleared. If nothing was ever established the caller is told
  // so at once -- decision 0059 measured what waiting costs.
  bool endEpisode(uint32_t nowMs) {
    const bool nothing = !counted_;
    const uint32_t from = openedAtMs_;
    open_ = false; occ_ = Occupancy::None;
    depOpen_ = false; depJudged_ = false; pendingEnd_ = false; depSince_ = 0;
    n_ = 0; preAt_ = 0; dec_ = 1; decPhase_ = 0; sum_ = 0;
    quietSince_ = 0; clipped_ = false; truncated_ = false;
    entryBaseline_ = baseline_;
    counted_ = false;
    if (!nothing) return false;
    seg_ = Segment{};
    seg_.fromMs = from; seg_.toMs = nowMs;
    ev_ = HallEvent::Abandoned;
    return true;
  }

  bool close(uint32_t nowMs) {
    open_ = false;
    const uint32_t dur = nowMs - openedAtMs_;
    if (dur < cfg_.floorMs) { ++floorRejects_; return false; }
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
    // peak here, the Gaussian fit in MagnetRecognizer -- reads judge_ instead,
    // so no lone sample can set an amplitude or a shape. Decision 0065.
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
    ev_ = HallEvent::Passage;
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
    baseline_ = c[medLen_ / 2];
    if (!primed_ && (nowMs - startMs_) >= cfg_.primeMs && medLen_ >= MED / 2) primed_ = true;
  }

  CaptureConfig cfg_;
  int16_t  buf_[RING] = {};        // the recording: signed, oriented at close
  int16_t  judge_[RING] = {};      // median-of-three copy, built once at close
  uint16_t n_ = 0, preAt_ = 0;
  int16_t  pre_[PRE] = {}; uint8_t preHead_ = 0, preLen_ = 0;
  int16_t  med_[MED] = {}; uint8_t medHead_ = 0, medLen_ = 0;
  uint16_t dec_ = 1, decPhase_ = 0;
  int32_t  baseline_ = 0, entryBaseline_ = 0, peak_ = 0;
  uint32_t adaptSinceMs_ = 0;   // when motion permission last arrived; 0 = not permitted
  int64_t  sum_ = 0;
  uint32_t startMs_ = 0, lastBaseMs_ = 0, openedAtMs_ = 0, quietSince_ = 0;
  uint32_t floorRejects_ = 0;
  bool     primed_ = false, open_ = false, truncated_ = false, clipped_ = false;
  uint8_t  pol_ = 1;
  Passage  out_;

  // ---- interruption state (decision 0070). 96 bytes. ----------------------
  static constexpr uint8_t kSettle = 16;     // * settleStepMs = settleWindowMs
  int16_t   stl_[kSettle] = {};
  uint16_t  stlN_[kSettle] = {};
  uint32_t  stlAt_[kSettle] = {};
  uint8_t   stlHead_ = 0, stlLen_ = 0;
  uint32_t  lastStlMs_ = 0, settleFromMs_ = 0, depFromMs_ = 0;
  uint16_t  settleFromN_ = 0;
  int32_t   settledLevel_ = 0, parkedLevel_ = 0, depRef_ = 0;
  bool      settled_ = false, counted_ = false;
  uint32_t  depSince_ = 0;
  bool      depOpen_ = false, depJudged_ = false, pendingEnd_ = false;
  Occupancy occ_ = Occupancy::None;
  HallEvent ev_ = HallEvent::None;
  Segment   seg_;
};

}  // namespace navi_one
