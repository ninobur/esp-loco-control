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

  // =========================================================================
  // A PASSAGE MAY NOT SPAN A STOP (decision 0070). A controlled stop pauses
  // the measurement; it does not end it and it does not judge it. There is no
  // second, weaker way to establish a magnet: the stitched waveform goes to
  // the unchanged recognizer, shape test and all.
  //
  // LOSS OF PROGRESSION. A moving magnet moves the field. When the field stops
  // moving, the measurement stops. 16 samples at 25 ms -- the cadence the
  // baseline sampler already runs at -- and the window's span with the single
  // highest and single lowest discarded must sit at or below settleSpan.
  //
  // THE CONSTANT IS MEASURED, AND THE FIRST PROPOSAL FAILED THE MEASUREMENT.
  // A raw +/-8 count band was proposed on 2026-09-01 and is arithmetically
  // incapable of firing on this sensor. Four stationary plateaus in the field
  // captures -- 261 to 386 real 1 kHz readings each, taken while Toby was
  // demonstrably parked -- give:
  //
  //   plateau (capture)                sd    16-sample window span
  //                                          raw p95 / max   trimmed p95 / max
  //   finding 11, parked on MM59      4.22      48 / 50            11 / 12
  //   finding 13, parked in a fringe  3.62      46 / 46             8 /  9
  //   finding 09 A, latched offset    3.02      21 / 30            11 / 13
  //   finding 09 B, latched offset    3.81      22 / 24            11 / 13
  //
  // The raw span is dominated by single-sample outliers -- the population
  // decision 0065 exists for, present at rest as well as in motion. Discarding
  // one high and one low collapses it to 13 counts at worst. 20 sits above
  // that with margin and far below the excursion a departing magnet makes.
  uint16_t settleStepMs   = 25;
  uint16_t settleWindowMs = 400;   // = kProgress * settleStepMs
  int16_t  settleSpan     = 20;    // trimmed span ceiling, counts

  // RESUMPTION. Nothing resumes because the throttle came up. The field must
  // show sustained, organised continuation of the arc: a monotone run over
  // resumeWindowMs that covers at least resumeMove counts without reversing
  // through the baseline. resumeMove is exitMargin -- the same number that
  // decides a passage has left a magnet decides that it has started moving
  // through one again.
  uint16_t resumeWindowMs = 200;   // 8 samples at settleStepMs
  int16_t  resumeMove     = 25;
  uint8_t  resumeAgree    = 6;     // of the 7 steps, how many must run the same way

  // A CEILING on how much is stitched back in once resumption is confirmed --
  // not the amount. The amount is decided by the same plateau test that cut
  // the pre-stop side, so the excised interval is the stationary one at BOTH
  // ends and the arc does not come out lopsided.
  //
  // Why it has to be the same test: the pre-stop side is cut at the start of
  // the plateau window, keeping everything down to a field rate of about
  // settleSpan per settleWindowMs -- 50 counts a second. Resumption is only
  // CONFIRMED at 125 counts a second, deliberately harder because it must
  // reject spikes and steps. Restarting the recording there would drop
  // everything between 50 and 125 counts a second that the approach side
  // kept. Measured in gate 12 D: restarting at confirmation put a stop on the
  // falling flank at 0.134 against a 0.13 ceiling, and a fixed 400 ms stitch
  // made it worse still (0.160) by dragging stationary samples back in.
  uint16_t stitchBackMaxMs = 512;

  // PROVISIONAL DEPARTURE RETENTION (Bamboo CCW, 2026-09-02 12:44:09).
  //
  // How much of the resumed arc is stitched back was decided by lastFlatMs_ --
  // the far end of the last window whose trimmed span was still inside
  // settleSpan. That window is 400 ms wide and TRAILING, and near a magnet's
  // apex the field is flat by nature, so a locomotive departing from the top of
  // an arc keeps the window flat well into its own descent. The reach-back then
  // starts far down the flank and the whole falling side is thrown away.
  //
  // What that cost: Toby rested at 199 counts against a gain of 213 -- 93% of
  // peak, all but on top of MM156 -- dwelled, and departed. The record joins a
  // complete rising side straight onto the far tail:
  //
  //     arrival    35 -> ... -> 192 -> 199
  //     stitch     199 -> 38                  <- 160 counts in one 2 ms sample
  //     departure  38 -> ... -> 27 -> 18
  //
  // Residual 0.1832, refused, session stopped. 820 ms of the passage moved and
  // 532 ms of it was recorded: 288 MILLISECONDS OF ARC WAS DISCARDED, and it
  // sat inside the 512-sample ring the whole time. The evidence was retained
  // and then thrown away, which is worse than never having had it.
  //
  // So retention no longer waits for the strong resumption test. From the
  // FIRST loss of the plateau band the samples are counted as provisionally
  // belonging to the passage; the sustained-movement test that already exists
  // then commits them or throws them away. Nothing weaker decides a magnet --
  // resumeReady() is unchanged and is still the only thing that resumes one.
  //
  // The band is settleSpan/2: half the span that decided the field was flat,
  // and about 2.5 sigma of the measured stationary noise. A provisional run
  // that never departs by resumeMove counts is cancelled by onsetHoldMs of
  // quiet -- that is a spike, not a departure. Once it HAS departed that far it
  // is latched, so a locomotive leaving across its own resting level on the way
  // down the far side does not cancel its own evidence halfway through.
  uint16_t onsetHoldMs = 100;

  // WALL-CLOCK WATCHDOGS. They keep running while the measurement clock is
  // paused, and they are the only thing that ends a pause that never resumes.
  // A paused passage that outlives either one is ABANDONED -- zero advances,
  // and a controlled stop with a diagnostic. Neither is a magnet test.
  //
  //   pauseMaxMs   the whole pause: the rest of a zero ramp, a 30 s dwell, and
  //                whatever it takes to get moving. 90 s is twice that.
  //   resumeMaxMs  after departure has been commanded and nothing coherent has
  //                appeared. Deliberately long: Toby spins, stalls and takes
  //                his time on the Grillers grade, and none of that is a
  //                navigation fault.
  uint32_t pauseMaxMs  = 90000;
  uint32_t resumeMaxMs = 30000;
};

// What the caller is doing with the throttle. It ARMS OBSERVATION AND NOTHING
// ELSE: it cannot pause a passage, resume one, decide a polarity, contribute a
// sample or advance anything. Every one of those is decided by the Hall signal
// below. PWM does not prove movement -- 14 counts moves Toby downhill at
// Westpoint and 35 may not move him uphill (decision 0057) -- which is exactly
// why it is a sentinel and not evidence.
enum class StopArming : uint8_t {
  None = 0,
  Decelerating,   // an identified controlled stop: watch for loss of progression
  Departing,      // an identified controlled departure: watch for its return
};

inline const char* stopArmingName(StopArming a) {
  switch (a) {
    case StopArming::Decelerating: return "DECELERATING";
    case StopArming::Departing:    return "DEPARTING";
    default:                       return "NONE";
  }
}

// What sample() has just produced.
enum class HallEvent : uint8_t {
  None = 0,
  Passage,     // a complete traversal -- passage(), and the FULL recognizer
  Abandoned,   // a paused passage outlived a wall-clock watchdog
};

inline const char* hallEventName(HallEvent e) {
  switch (e) {
    case HallEvent::Passage:   return "PASSAGE";
    case HallEvent::Abandoned: return "ABANDONED";
    default:                   return "NONE";
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

  // ---- the paused measurement (decision 0070) -----------------------------
  HallEvent event()  const { return ev_; }
  bool      paused() const { return paused_; }
  bool      open()   const { return open_; }
  // Wall clock spent paused. The measurement clock excludes it; the watchdogs
  // do not.
  uint32_t  pausedMs() const { return pausedTotalMs_; }
  // Openings thrown away because the locomotive was standing still when they
  // happened. Not a fault by itself; a rising count says the sensor is coming
  // to rest in a field somewhere it should not be.
  uint32_t  discards() const { return discards_; }
  // How much of the departure was committed when the measurement resumed, in
  // samples. Carried so a refusal can be read in the field without guessing
  // which end of the passage lost its arc.
  uint16_t  stitchBackMs() const { return stitchBackMs_; }

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
  // `arm` says what the caller is doing with the throttle. It selects WHAT TO
  // WATCH FOR and nothing else -- see StopArming. Every decision below is the
  // Hall signal's.
  bool sample(uint32_t nowMs, int16_t raw, bool mayAdapt,
              StopArming arm = StopArming::None) {
    ev_ = HallEvent::None;
    updateBaseline(nowMs, raw, mayAdapt);
    updateProgress(nowMs, raw);
    if (!primed_) return false;

    // LIVE reference: threshold arithmetic only.
    const int32_t delta = (int32_t)raw - baseline_;
    const int32_t mag   = delta < 0 ? -delta : delta;

    // A PAUSED MEASUREMENT. Nothing is recorded, nothing is judged, and
    // nothing here can close the passage. The only questions are whether the
    // arc has started moving again and whether a watchdog has run out.
    if (paused_) return servicePaused(nowMs, raw, arm);

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
      pausedTotalMs_ = 0; resumedAtMs_ = 0; sawProgress_ = false; stitchAt_ = 0;
      stopEpisode_ = false;
    stitchBackMs_ = 0; provLen_ = 0; provQuiet_ = 0; provPeak_ = 0;
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

    // LOSS OF PROGRESSION. Armed by the throttle coming down; decided, here,
    // by the field having stopped moving. Falling PWM alone pauses nothing --
    // while Toby coasts, the field keeps moving and so does the recording,
    // even at PWM 0.
    // Departing arms it too, for the locomotive that stalls halfway out of a
    // magnet: the field stops moving, so the measurement stops, and the same
    // resumption test applies when it starts again.
    if (arm != StopArming::None) stopEpisode_ = true;
    if (arm != StopArming::None && !plateau_) sawProgress_ = true;
    if (arm != StopArming::None && plateau_ && sawProgress_) {
      pauseMeasurement(nowMs);
      return false;
    }
    // NO OPEN PASSAGE MAY CARRY STATIONARY SAMPLES ACROSS A CONTROLLED STOP.
    //
    // The line above pauses a passage that was MOVING and stopped. A passage
    // that was already stationary when it opened never sets sawProgress_, so
    // before this it could do neither: it could not pause, and it could not
    // close either, because closing needs the field to fall below exitMargin
    // and a resting offset ABOVE that margin never does.
    //
    // Bamboo CCW, 2026-09-02 12:29:07, is what that costs. Toby came to rest
    // with about 30 counts of a neighbouring field on the sensor -- five
    // counts above the 25 that would have closed it -- and a single-sample
    // artifact carried the reading over entryMargin and opened a passage. It
    // stayed open for 35.8 SECONDS. The buffer decimated to 128 ms a sample
    // holding a flat line, and when the real departure magnet finally arrived
    // it was recorded in EIGHT samples. Residual 0.5586, refused, and the
    // marker lost; the next magnet was the opposite pole against a stale
    // expectation, which struck and shut the session down three markers from
    // where Toby actually was.
    //
    // A stationary opening is not partial evidence of a magnet. There is
    // nothing to preserve, so it is discarded rather than paused: the reading
    // is still under the sensor, so nothing re-opens until the field genuinely
    // moves again, and the departure arc gets a clean passage of its own.
    if (arm != StopArming::None && plateau_ && !sawProgress_) {
      // ONLY IF THERE IS NO FIELD THERE. The discard exists for a passage that
      // opened on an ARTIFACT while the locomotive stood in a fringe -- Bamboo,
      // 30 counts on the sensor and one sample over the threshold. It is wrong
      // for a locomotive resting INSIDE a magnet, and 1.0X6 shipped it without
      // that guard.
      //
      // Arches CCW, 2026-09-02 14:14:40, is what that cost. Toby came to rest
      // at 88 counts -- 44% of MM106's gain, a real field, not a fringe. The
      // passage was open through the dwell, and the discard fired on it every
      // sample: opened at mag >= entryMargin, discarded as flat-with-no-
      // progress, reopened on the very next reading because the field was
      // still there. Round and round for thirty seconds, with the pre-roll
      // frozen the whole time because a pre-roll does not fill while a passage
      // is open. It only stopped when the trailing plateau window finally
      // cleared, some 400 ms into the departure -- by which time the top of the
      // arc had gone. What reached the recognizer was twelve stale pre-roll
      // samples at 28 and then a bare falling side, 88 down to 17. Residual
      // 0.2706.
      //
      // THE TEST, STATED NARROWLY, because the broad version is false. A
      // sustained field below entryMargin is NOT evidence that there is no
      // magnet there: finding 13 is a locomotive parked in a real fringe, and
      // Bamboo's 30-count plateau was a real neighbouring field. Below 38 does
      // not mean not magnetic and this code must never say that it does.
      //
      // What is true is narrower and is enough: A SUSTAINED FIELD BELOW
      // entryMargin COULD NOT HAVE OPENED THIS PASSAGE BY ITSELF. If a passage
      // is open and the level the locomotive has settled at is below the
      // opening threshold, then something else opened it -- an excursion that
      // has since gone. That opening is the suspect thing, and it is what is
      // discarded; the fringe it was sitting in is not being judged at all.
      //
      // At or above the threshold the sustained field alone accounts for the
      // opening, so the passage belongs to a magnet the locomotive is in, and
      // it is left to the pause path.
      //
      // AND NOTHING ELSE. Resting in a real field is left exactly as it
      // behaved before the discard existed: the passage stays open and the
      // dwell goes into it. That is a known fault -- it is the Bamboo 12:29
      // fault at a higher field strength -- and it is left alone deliberately.
      // Pausing such a passage instead was tried and is worse: with the
      // locomotive stationary and the field still on the sensor, the passage
      // abandons on its watchdog, reopens on the very next reading because the
      // field has not gone anywhere, pauses, abandons again. Gate 12 F caught
      // it looping, and one run showed PAUSE and RESUME alternating every
      // millisecond. A fault that is understood and bounded beats a fix that
      // thrashes.
      const int32_t rest = plateauLevel_ - entryBaseline_;
      if ((rest < 0 ? -rest : rest) < (int32_t)cfg_.entryMargin) {
        discard();
        return false;
      }
    }

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
      stitchAt_ = (uint16_t)(stitchAt_ / 2);
      // The progression ring remembers WHERE IN THE BUFFER each of its samples
      // was taken, so that a pause can rewind to the last moment the field was
      // moving. Halving the buffer renumbers every one of those, exactly as it
      // renumbers preAt_. Without this line the rewind lands in the wrong
      // place on any passage long enough to decimate -- which is every passage
      // that spans a stop -- and the stitch shows a step where the arc should
      // be continuous. Finding: gate 12, finding 11's own record, a 42-count
      // discontinuity at the junction.
      for (uint8_t i = 0; i < kProgress; ++i) prN_[i] = (uint16_t)(prN_[i] / 2);
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
    // A paused measurement belongs to the frame that has ended, like
    // everything else here. It may not be resumed into a declaration that
    // never saw the stop.
    paused_ = false; sawProgress_ = false;
    pausedAtMs_ = 0; pausedTotalMs_ = 0; departArmedMs_ = 0; resumedAtMs_ = 0;
    stitchAt_ = 0; stopEpisode_ = false;
    ev_ = HallEvent::None;
    stitchBackMs_ = 0; provLen_ = 0; provQuiet_ = 0; provPeak_ = 0;
    prLen_ = 0; prHead_ = 0; plateau_ = false;
    rsLen_ = 0; rsHead_ = 0;
  }

 private:
  // =========================================================================
  // A PASSAGE MAY NOT SPAN A STOP (decision 0070).
  //
  // Everything below pauses and resumes ONE measurement. None of it judges,
  // none of it accepts, and none of it can advance anything: the stitched
  // waveform leaves close() as an ordinary Passage and meets the unchanged
  // recognizer, shape test included. With arm == None -- every gate written
  // before this decision, and every moment of ordinary running -- the pause
  // path cannot be entered and the class behaves exactly as it did.
  // =========================================================================

  // Is the field still moving? A 400 ms window at the baseline sampler's
  // cadence, with the single highest and single lowest sample discarded, must
  // span more than settleSpan counts for the answer to be yes. The trim is not
  // fussiness: on four real stationary plateaus the untrimmed span reached 50
  // counts while the trimmed span never passed 13.
  void updateProgress(uint32_t nowMs, int16_t raw) {
    if (!lastPrMs_) { lastPrMs_ = nowMs; return; }
    if (nowMs - lastPrMs_ < cfg_.settleStepMs) return;
    lastPrMs_ = nowMs;
    pr_[prHead_] = raw; prN_[prHead_] = n_;
    prHead_ = (uint8_t)((prHead_ + 1) % kProgress);
    if (prLen_ < kProgress) ++prLen_;
    plateau_ = false;
    if (prLen_ < kProgress) return;
    int16_t c[kProgress];
    for (uint8_t i = 0; i < kProgress; ++i) c[i] = pr_[i];
    for (uint8_t i = 1; i < kProgress; ++i) {
      int16_t v = c[i]; int j = (int)i - 1;
      while (j >= 0 && c[j] > v) { c[j + 1] = c[j]; --j; }
      c[j + 1] = v;
    }
    if ((int32_t)c[kProgress - 2] - (int32_t)c[1] > (int32_t)cfg_.settleSpan) return;
    plateau_ = true;
    plateauLevel_ = c[kProgress / 2];
    lastFlatMs_ = nowMs;
    // prHead_ now indexes the OLDEST entry: the start of this window, and so
    // the last moment the field was demonstrably still moving.
    progressFromN_ = prN_[prHead_];
  }

  // Stop the measurement. Nothing is judged and nothing is thrown away.
  void pauseMeasurement(uint32_t nowMs) {
    // Drop the samples taken while the field was already flat. The stationary
    // interval is excluded from the waveform, which is the whole point.
    //
    // WHY THE CUT IS THE PLATEAU WINDOW AND NOT A MOVEMENT TEST. An earlier
    // draft rewound to the last window in which the field had travelled
    // resumeMove counts, to make the two ends of the excision symmetric. It
    // is wrong, and gate 12 D showed it: AT A MAGNET'S APEX THE FIELD IS FLAT
    // BY NATURE, whatever the speed, so a movement test excises the top of
    // every arc it is applied to and leaves a cusp for the Gaussian to
    // object to. The window-SPAN test does not have that failure: at speed
    // the 400 ms window covers 55 mm of track and spans the whole apex.
    uint16_t keep = progressFromN_;
    if (keep > n_) keep = n_;
    if (keep < (uint16_t)(preAt_ + 1)) keep = (uint16_t)(preAt_ + 1);
    n_ = keep;
    // sum_ and the running peak have to describe what is actually retained.
    sum_ = 0;
    for (uint16_t i = 0; i < n_; ++i) sum_ += buf_[i];
    sign_ = sum_ >= 0 ? 1 : -1;
    peakSoFar_ = 0;
    for (uint16_t i = 0; i < n_; ++i) {
      const int32_t o = sign_ * (int32_t)buf_[i];
      if (o > peakSoFar_) peakSoFar_ = o;
    }
    pauseAbs_ = sign_ * (plateauLevel_ - entryBaseline_);
    paused_ = true;
    pausedAtMs_ = nowMs;
    provLen_ = 0; provQuiet_ = 0; provPeak_ = 0;
    lastFlatMs_ = nowMs;
    departArmedMs_ = 0;
    rsLen_ = 0; rsHead_ = 0;
    quietSince_ = 0;
  }

  // While paused: watch, and nothing else. A plateau reading, an isolated
  // spike, a reversal, a step or any disorganised variation neither confirms
  // nor vetoes the pending passage -- it simply is not the arc continuing.
  bool servicePaused(uint32_t nowMs, int16_t raw, StopArming arm) {
    // The full-rate ring. When the arc does continue, the two hundred
    // milliseconds that proved it are part of it and get stitched back in.
    rs_[rsHead_] = raw;
    rsHead_ = (uint16_t)((rsHead_ + 1) % kResume);
    if (rsLen_ < kResume) ++rsLen_;

    // PROVISIONALLY THE PASSAGE'S, from the moment the field stopped being
    // flat. This decides nothing: resumeReady() below is still the only thing
    // that resumes a measurement. All it decides is how much of what was
    // already recorded gets committed when it does.
    //
    // THE TEST IS THE PLATEAU TEST -- the same one that cut the pre-stop side,
    // so the two ends of one arc are cut by one rule. It keeps everything down
    // to a field rate of about settleSpan per settleWindowMs and no slower.
    //
    // ANYTHING MORE SENSITIVE RETAINS CREEP, and that was measured twice on
    // 2026-09-02. A 10-count band round the resting level -- half the span that
    // decided the field was flat -- retains a locomotive easing out of a fringe
    // long before it is moving in any sense the pre-stop side would have
    // recorded. It cost finding 13 its acceptance (0.1271 -> 0.1507) and let
    // section E's electrical step advance a marker at 0.1264. Both times.
    if (!plateau_) {
      if (provLen_ < kResume) ++provLen_;
      provQuiet_ = 0;
    } else if (provLen_) {
      if (provLen_ < kResume) ++provLen_;
      if (++provQuiet_ >= cfg_.onsetHoldMs) { provLen_ = 0; provQuiet_ = 0; }
    }

    // THE ARC MAY SIMPLY HAVE FINISHED. Requirement 8 is that a passage closes
    // when the complete waveform has returned to baseline, and that does not
    // stop being true because the measurement is paused. Toby coming to rest
    // on a magnet's TAIL -- inside the entry margin but with only a few counts
    // of field left -- can never satisfy the resumption test, because there is
    // no longer 25 counts of arc to travel. Without this the measurement would
    // hang until a watchdog and withdraw AUTO on a magnet that was crossed
    // perfectly. Gate 12 D, "falling edge, 15% of peak".
    //
    // It needs a settle window of quiet, not the 8 ms an ordinary close takes:
    // 8 ms of noise near the threshold is common and this decision cannot be
    // taken back.
    {
      const int32_t d = (int32_t)raw - baseline_;
      const int32_t m = d < 0 ? -d : d;
      if (m < cfg_.exitMargin) {
        if (!quietSince_) quietSince_ = nowMs;
        if (nowMs - quietSince_ >= cfg_.settleWindowMs) {
          paused_ = false;
          pausedTotalMs_ += nowMs - pausedAtMs_;
          return close(nowMs);
        }
      } else quietSince_ = 0;
    }

    // WALL CLOCK. The measurement clock is paused; these are not.
    if (nowMs - pausedAtMs_ > cfg_.pauseMaxMs) return abandon(nowMs);
    // THE RESUMPTION WATCHDOG belongs to DEPARTING and only to it: it asks
    // "departure was commanded and nothing coherent has appeared", and there is
    // no such question while the throttle is still coming down.
    if (arm == StopArming::Departing) {
      if (!departArmedMs_) departArmedMs_ = nowMs;
      else if (nowMs - departArmedMs_ > cfg_.resumeMaxMs) return abandon(nowMs);
    } else {
      departArmedMs_ = 0;
    }

    // BUT THE ARC CONTINUING IS THE ARC CONTINUING, whichever sentinel is up.
    // Until 1.0X7 this test was reached only when arm was Departing, so a
    // passage paused on an APPROACH RAMP could never resume at all. It could
    // only close on baseline proximity -- throwing its falling side away -- or
    // time out on a watchdog.
    //
    // Arches CCW, 2026-09-02 15:03, is what that costs. The zero ramp is
    // running, the throttle is coming down through 44, 39, 34, 29, 24, 19, and
    // Toby is still coasting into MM106. The field flattens as he slows, the
    // measurement pauses -- correctly -- and then he keeps rolling. Resumption
    // is never even tested. He coasts out the far side, the field returns to
    // baseline, and the passage closes with nothing but its rising flank in it:
    // 33 up to 117 and then nothing, 364 ms of moving arc discarded, residual
    // 0.2816, refused, session stopped.
    //
    // Nothing is loosened by testing it here. resumeReady() is unchanged and
    // still demands resumeMove counts of organised, monotone travel inside
    // resumeWindowMs -- 125 counts a second, which a drifting reference cannot
    // fake and a stationary locomotive cannot produce. The throttle still
    // decides nothing: it only says which question is being asked.
    if (arm != StopArming::None && resumeReady()) resumeMeasurement(nowMs);
    return false;
  }

  // Has the arc started moving again? Not "has the throttle come up".
  //
  //   * a monotone run across the whole resume window, allowing one step to
  //     disagree, so a single sample can neither cause nor block it;
  //   * at least resumeMove counts of travel, which a drift cannot make in
  //     200 ms and a step makes in one sample and therefore fails the run;
  //   * no reversal through the baseline -- the arc has one sign;
  //   * and a direction the arc could actually take from where it stopped.
  //     Below its own peak, Toby is past the top, and only a continued fall is
  //     plausible. At its own peak he may be on the way up or at the apex, and
  //     either a further rise or the fall is plausible.
  bool resumeReady() const {
    if (prLen_ < kProgress) return false;
    const uint8_t N = (uint8_t)(cfg_.resumeWindowMs / cfg_.settleStepMs);
    if (N < 3 || N > kProgress) return false;
    int32_t o[kProgress];
    for (uint8_t i = 0; i < N; ++i) {
      const uint8_t idx = (uint8_t)((prHead_ + kProgress - N + i) % kProgress);
      o[i] = sign_ * ((int32_t)pr_[idx] - entryBaseline_);
      if (o[i] < -(int32_t)cfg_.exitMargin) return false;      // reversed
    }
    const int32_t total = o[N - 1] - o[0];
    const int32_t mv = total < 0 ? -total : total;
    if (mv < cfg_.resumeMove) return false;
    uint8_t agree = 0;
    for (uint8_t i = 1; i < N; ++i) {
      const int32_t d = o[i] - o[i - 1];
      if ((total > 0 && d > 0) || (total < 0 && d < 0)) ++agree;
    }
    if (agree < cfg_.resumeAgree) return false;
    if (total > 0 && pauseAbs_ < peakSoFar_ - (int32_t)cfg_.resumeMove) return false;
    return true;
  }

  // Stitch. The stationary interval is excluded and the measurement clock skips
  // it; the two hundred milliseconds of proof are real moving samples and are
  // put back at full rate, so the arc has no gap in it and no change of
  // sampling density where it was joined.
  void resumeMeasurement(uint32_t nowMs) {
    paused_ = false;
    pausedTotalMs_ += nowMs - pausedAtMs_;
    resumedAtMs_ = nowMs;
    sawProgress_ = false;
    // Back to the first loss of the plateau band: everything provisionally
    // retained since the field started moving again, now committed. The old
    // reach-back stands only if nothing was retained at all, which resumeReady()
    // makes very nearly impossible -- it takes resumeMove counts of travel to
    // get here, and that is well outside the band this counts from.
    //
    // THE PRE-STOP SIDE IS NOT TOUCHED. pauseMeasurement's rewind is a separate
    // excision with a separate hazard: at an apex the field is flat by nature,
    // and a movement test there eats the top of every arc it is applied to.
    // Changing both ends at once on 2026-09-02 cost finding 13 its acceptance
    // and let an electrical step advance a marker. One end at a time.
    uint32_t back = provLen_ ? provLen_
                             : (lastFlatMs_ ? (nowMs - lastFlatMs_) : cfg_.resumeWindowMs);
    if (back > cfg_.stitchBackMaxMs) back = cfg_.stitchBackMaxMs;
    if (back > kResume) back = kResume;
    if (back > rsLen_) back = rsLen_;
    // The boundary between the two movement intervals, in the record's own
    // numbering. Everything before it was observed on the way in, everything
    // from it on the way out. Renumbered with the buffer if it decimates.
    stitchAt_ = n_;
    for (uint16_t i = 0; i < (uint16_t)back; ++i) {
      const uint16_t idx = (uint16_t)((rsHead_ + kResume - (uint16_t)back + i) % kResume);
      const int32_t rec = (int32_t)rs_[idx] - entryBaseline_;
      push((int16_t)rec); tally((int16_t)rec);
    }
    stitchBackMs_ = (uint16_t)back;
    rsLen_ = 0; rsHead_ = 0;
    provLen_ = 0; provQuiet_ = 0; provPeak_ = 0;
    quietSince_ = 0;
  }

  // AN OPENING WITH NO MOVEMENT IN IT. Distinct from abandon(): abandon says a
  // marker may have gone uncounted, because a moving passage was under way and
  // was lost. Nothing was under way here -- the locomotive was standing still
  // when the passage opened -- so there is nothing to report and nothing to
  // withdraw. The passage is simply un-opened, and counted so the field can
  // see how often it happens.
  void discard() {
    open_ = false; paused_ = false;
    n_ = 0; preAt_ = 0; dec_ = 1; decPhase_ = 0; sum_ = 0;
    quietSince_ = 0; truncated_ = false; clipped_ = false;
    pausedTotalMs_ = 0; departArmedMs_ = 0; resumedAtMs_ = 0; sawProgress_ = false;
    stitchAt_ = 0; stopEpisode_ = false;
    rsLen_ = 0; rsHead_ = 0;
    entryBaseline_ = baseline_;
    ++discards_;
    stitchBackMs_ = 0; provLen_ = 0; provQuiet_ = 0; provPeak_ = 0;
  }

  // A pause that never resumed. There is no half a magnet to report and
  // nothing to judge: the passage is discarded and the caller is told, at
  // once, that a marker may have gone uncounted.
  bool abandon(uint32_t nowMs) {
    open_ = false; paused_ = false;
    n_ = 0; preAt_ = 0; dec_ = 1; decPhase_ = 0; sum_ = 0;
    quietSince_ = 0; truncated_ = false; clipped_ = false;
    pausedTotalMs_ = 0; departArmedMs_ = 0; resumedAtMs_ = 0; sawProgress_ = false;
    stitchAt_ = 0;
    rsLen_ = 0; rsHead_ = 0;
    entryBaseline_ = baseline_;
    (void)nowMs;
    stitchBackMs_ = 0; provLen_ = 0; provQuiet_ = 0; provPeak_ = 0;
    ev_ = HallEvent::Abandoned;
    return true;
  }

  bool close(uint32_t nowMs) {
    open_ = false;
    // THE MEASUREMENT CLOCK, not the wall clock: a stop does not make a
    // passage long. The wall clock is what the watchdogs run on, and it is
    // still there in openedAtMs / closedAtMs for the rebound guard.
    const uint32_t dur = (nowMs - openedAtMs_) - pausedTotalMs_;
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
    out_.stitchAt   = stitchAt_;
    out_.stopEpisode = stopEpisode_;
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
      // A PAUSED passage is one the locomotive is sitting inside. Migration
      // is exactly the wrong thing there -- it is finding 10 by another road --
      // and the motion gate above already refuses at PWM 0, but a departure
      // that has crossed the tractive floor while the measurement is still
      // paused would otherwise re-open the door.
      if (paused_) return;
      if (open_) {
        uint32_t from = (openedAtMs_ > adaptSinceMs_) ? openedAtMs_ : adaptSinceMs_;
        // ...and not for openMigrateMs after it resumes, so the reference
        // cannot walk under the second half of a stitched arc.
        if (resumedAtMs_ > from) from = resumedAtMs_;
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

  // ---- the paused measurement (decision 0070). 640 bytes. -----------------
  static constexpr uint8_t  kProgress = 16;    // * settleStepMs = settleWindowMs
  static constexpr uint16_t kResume   = 512;   // full-rate ring for the stitch
  int16_t   pr_[kProgress] = {};
  uint16_t  prN_[kProgress] = {};
  uint8_t   prHead_ = 0, prLen_ = 0;
  uint32_t  lastPrMs_ = 0;
  int32_t   plateauLevel_ = 0;
  uint16_t  progressFromN_ = 0;
  int16_t   rs_[kResume] = {};
  uint16_t  rsHead_ = 0, rsLen_ = 0;
  int32_t   sign_ = 1, peakSoFar_ = 0, pauseAbs_ = 0;
  uint32_t  lastFlatMs_ = 0;
  uint16_t  stitchAt_ = 0;
  bool      stopEpisode_ = false;
  uint16_t  stitchBackMs_ = 0;
  uint16_t  provLen_ = 0, provQuiet_ = 0;
  int32_t   provPeak_ = 0;
  uint32_t  discards_ = 0;
  uint32_t  pausedAtMs_ = 0, pausedTotalMs_ = 0;
  uint32_t  departArmedMs_ = 0, resumedAtMs_ = 0;
  bool      plateau_ = false, sawProgress_ = false, paused_ = false;
  HallEvent ev_ = HallEvent::None;
};

}  // namespace navi_one
