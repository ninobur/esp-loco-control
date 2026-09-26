#pragma once
// ---------------------------------------------------------------------------
// ExcursionDetectorX22R -- successor of the NAVI_ONE X22 ExcursionDetector for
// the current NAVI_COHERENCE lineage (20Q3, operator ruling 2026-09-26).
//
// THIS IS X22 WITH THE OBSOLETE REFRACTORY REMOVED. IT IS NOT A REDESIGN.
// Source: firmware/programs/NAVI_ONE/variants/NAVI_ONE_X22/ExcursionDetector.h,
// copied verbatim except for exactly these changes:
//   * DetectorConfig::refractoryMs, Detection::guardUntilMs, refractory(),
//     refractoryUntil(), refractoryUntil_ and rearmPending_ are removed, with
//     every branch that set, tested or cleared them;
//   * the class lives in namespace ngr_hall so the historical X22 header can be
//     compiled alongside it (equivalence tests); X22's medianOfThree is reused;
//   * comments that described the guard as live now say it is superseded.
// Unchanged: the per-interval LOCKED baseline cycle, the 2-sample same-sign
// persistence run, detection and polarity at the opening sample, electrical
// rearm (a signal fact, not a timer), the 400 ms acquisition window, dwell and
// old-field handling, lost-lock recovery, shadow median and all telemetry.
//
// The ~645 ms detection refractory is SUPERSEDED by NAVI's 650 ms Hall-only
// fallback (decision 0093), applied by NAVI only when valid MM-referenced IR
// distance is unavailable. Hall detector detects -> NAVI judges. The historical
// X22 header, and every older build that uses it, is untouched.
// tests/test_20q_standards.cpp proves equivalence with X22 at refractoryMs=0.
//
// The X22 notes below are retained as provenance.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// ExcursionDetector -- X22.  THE REFERENCE IS LOCKED, AND IT IS RE-ESTABLISHED
// ONCE PER INTERVAL BETWEEN TWO MAPPED MARKERS.
//
// The class name and the file name are X19's and are kept so the diff against
// X21 stays readable. The mechanism inside is not X19's.
//
// WHAT WAS DELETED, AND WHY
// -------------------------
// X19 measured every departure against L(t): the quietest sample in a trailing
// 300 ms window, anchored on a CONTINUOUSLY ADAPTIVE measured rest (restRef_,
// noteRest(), a 512-bin window histogram, and a quiet-level persistence rule
// of localWindowMs + refractoryMs).
//
// That architecture failed in the field on 2026-09-16, three times in one
// session, all at Grillers. Otto's quiet line was ~1950. He came to rest at
// ~2150 -- standing on a magnet -- and rest had ALREADY migrated onto the
// shelf before PWM reached 0, during the twelve-second zero ramp, because a
// level only has to hold for 945 ms to be believed. X20's freeze-at-dwell then
// froze an already-corrupted value. On departure the signal fell ~190 counts
// back to the true line, which read as a large South excursion, was counted as
// MM60, and the real MM60 arrived to be judged against MM59.
//
// The same failure, in five different reference architectures, is the whole
// history of this program:
//
//   ungated rolling median (0.3-0.6)     captured by the magnet it parks on,
//                                        1853 -> 1899 (finding 10)
//   median gated on PWM 25 (0.7)         worked, and exposed findings 11/13
//   median + openMigrateMs (X16)         a sub-threshold +26 shelf displaced
//                                        the reference; the passage could not
//                                        close; 1569/1200/2446 ms (Northpoint)
//   fixed after prime (X17)              audited as PREDICTED to latch
//   adaptive measured rest (X19-X21)     learns the Grillers shelf in a ramp
//
// The invariant under all five: the detector's decision is a difference
// against a reference estimated from the same signal the magnets are in, and
// each design differs only in which rule decides when the signal is quiet
// enough to become reference. Every such rule is defeated by a field that
// persists longer than the rule's own time constant, and this railway produces
// those routinely -- a twelve-second ramp beats 945 ms; a 36.7 s dwell in a
// fringe field (finding 13) beats a 1025 ms median window.
//
// THERE IS NO TIME CONSTANT THAT SEPARATES "A LONG APPROACH INTO A MAGNET"
// FROM "A NEW RESTING LEVEL". So X22 stops trying to find one.
//
// WHAT REPLACES IT
// ----------------
// One locked value, and an explicit re-establishment cycle tied to the map:
//
//    recognise N  ->  get clear of N  ->  establish a level for the N->N+1
//    interval  ->  LOCK it  ->  detect N+1 against it  ->  repeat
//
// baseline_ has exactly TWO write sites in this class: the prime, and the
// acceptance at the end of a collection. Between them it is a constant. There
// is no rate-limited path, no rolling median with authority, no noteRest, no
// histogram, and no quiet-level persistence state. A locked reference can be
// WRONG; what it cannot do is MIGRATE ONTO A MAGNET DURING A SLOW APPROACH,
// which is the failure mode that has now recurred five times.
//
// THE COLLECTION, AND ITS FIVE GATES
// ----------------------------------
// After a detection, the interval baseline is re-established once, and only
// under all five of:
//
//   CLEAR     clearSamples consecutive samples within departCounts of the
//             current lock, after the acquisition window and the guard have
//             both expired.  This proves ELECTRICAL quiet.  It does not prove
//             spatial clearance and is not claimed to.
//   SETTLE    settleMs after that confirmation before collection starts.
//   MOVING    mayAdapt (PWM above this locomotive's measured tractive floor)
//             for every sample of the collection.  A stationary or ramping
//             locomotive establishes nothing -- which is finding 10, and it is
//             now structural rather than a freeze rule that can be outrun.
//   CADENCE   the interval just completed arrived within cadenceMaxMs of the
//             one before it, i.e. the locomotive is running normally between
//             two mapped markers rather than entering or leaving a station.
//   QUIET     spread over the collection <= spreadMax counts.
//
// and then one more, which is new in X22 and is the important one:
//
//   THE LEASH  |candidate - baseline_| < departCounts.
//
// THE LEASH, AND WHAT IT TURNED OUT TO BE
// ---------------------------------------
// A displaced magnetic shelf is FLAT. Grillers at 2163/2176 against a line of
// 1950 has a spread of a few counts; finding 13's fringe field held 41 counts
// flat, at sd 2-6, for 36.7 seconds. A flatness test cannot tell a quiet track
// from a quiet magnet, so QUIET is necessary and nowhere near sufficient.
//
// The leash was written to close that: reject a candidate departCounts or more
// from the lock, on the argument that a level a magnet's distance away is not a
// resting level, it is a magnet. departCounts is already in use, so no new
// constant enters.
//
// IT IS UNREACHABLE, AND THE GATES PROVED IT. CLEAR requires 30 consecutive
// samples WITHIN departCounts of the lock before a collection may start, and
// Collect abandons on any sample that goes over. Every sample in a collection
// is therefore already inside the leash, and so is their median. The Grillers
// shelf at ~200 counts never reaches a collection at all: CLEAR simply never
// completes while the locomotive sits on it.
//
// So the protection is real but it is not the leash -- it is CLEAR. The leash
// is kept as a guarded invariant, costing one comparison, so that the property
// stays true if CLEAR is ever relaxed. It is expected never to fire, and
// `reject:"LEASH"` appearing in a field record means CLEAR has been changed.
//
// WHAT THIS DOES NOT CLOSE, STATED PLAINLY
// ----------------------------------------
// A SUB-THRESHOLD field still becomes the lock. Measured on this build:
// two magnets at cadence, then a flat +45-count fringe -- finding 13's 41
// counts, near enough -- and the collection accepts it. 45 is inside CLEAR,
// inside the leash, and its spread is nothing.
//
// Nothing in X22 refuses that, and nothing in X22 pretends to. A tighter leash
// would refuse it and would also refuse the largest genuine drift measured on
// this railway (~33 counts across one direction reversal, session C3B93D0B),
// which is why the X18-X21 audit judged a leash weak without measured drift
// evidence and why no tighter number is invented here.
//
// The consequence is bounded and reported rather than hidden: the lock moves by
// at most the fringe's own displacement, the next interval's collection can
// move it back, and every acceptance publishes its candidate, its delta and
// the level it replaced on diag/baseline. The real answer is the question the
// audit named -- WHEN IS THE TRACK DEMONSTRABLY CLEAR -- and it needs the X18
// recorder's data, not another threshold.
//
// THE AGE OF THE LOCK IS REPORTED, AND HAS NO AUTHORITY
// -----------------------------------------------------
// Every rejection retains the previous lock. That is the correct failure
// direction and it stays. But retention in X22's predecessor was unbounded AND
// INVISIBLE: nothing tracked how old the operative reference was, and the
// rejection conditions correlate with exactly the states that persist, so a
// whole station sequence can complete without one successful collection.
//
// X22 tracks baselineSetMs_, publishes the age, the consecutive-rejection
// count and the last rejection reason on every record, and raises a STALE flag
// past staleMs. THE FLAG GATES NOTHING. Whether a stale lock should change
// behaviour is a policy question with no measured answer yet; what the field
// test needs first is the measurement, and the measurement costs one uint32_t.
//
// WHAT IS UNCHANGED FROM X21, DELIBERATELY
// ----------------------------------------
//   * polarity is the sign of the departure at the opening sample, fixed
//     there, and finish() may not revise it (the MM136 mechanism)
//   * navigation is queued at detection, not 400 ms later
//   * the 400 ms window is telemetry; it cannot delay, reverse or reject
//   * [X22R: the 645 ms guard is removed -- superseded by NAVI's 650 ms
//     Hall-only fallback]
//   * stopped (ramped PWM 0) suppresses declaration entirely
//   * the dwell / IN_OLD_FIELD machinery, now anchored on the lock
//   * no morphology, no width floor, no speed-dependent threshold, no IR,
//     no second threshold, no closure test
//
// ONE THING IS NECESSARILY DIFFERENT AND IS DECLARED
// --------------------------------------------------
// The persistence run now requires the two samples to have the SAME SIGN.
// Against X19's trailing local minimum an opposite-signed pair was not
// reachable; against a fixed lock it is, and two samples on opposite sides of
// the reference are not one excursion. This is mechanically required by the
// reference change rather than an independent screen.
//
// WHAT THIS BUILD DOES NOT DO
// ---------------------------
// The opening-vs-window disagreement -- sign(departAtDetect) against
// sign(peakSigned) -- is measured and published on every record, and it is the
// best discriminator in the field archive: it fired once in 97 records on X21
// (MM117) and twice in 266 on X20 (MM136 the first). IT IS STILL TELEMETRY.
// Acting on it needs a navigator that can hold an observation as ambiguous
// instead of advancing or striking, which is a larger change than this one and
// has not been asked for.
//
// NOT FIELD ACCEPTED. The reference architecture is new to the railway and its
// acceptance gates have never run on a locomotive. The predicted failure to
// watch for is the opposite of X21's: not a reference that migrates, but a
// reference that goes STALE because collections keep being refused. The number
// that tells you is base_age_ms on diag/excursion and state/status; the number
// that tells you why is base_rej.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include "../../../NAVI_ONE/variants/NAVI_ONE_X22/MagnetRecognizer.h"

namespace ngr_hall {
using navi_one::medianOfThree;

// Why a collection was refused. Reported, never acted on.
enum class BaselineReject : uint8_t {
  None = 0, Moving, Cadence, Spread, Leash, Interrupted, Stopped
};

struct DetectorConfig {
  // Departure from the LOCKED reference that declares a candidate.
  // Otto: HALL_DEADBAND_COUNTS + HALL_ENTRY_MARGIN_COUNTS = 70.
  int16_t  departCounts   = 70;
  // Consecutive same-signed samples that must satisfy the departure test. The
  // only transient screen. Bench 2026-09-03 measured the single-conversion
  // population at 1.1 in 1000; two in a row is 1.2 in a million.
  uint8_t  persistSamples = 2;
  // Fixed acquisition window from the detection instant. Telemetry only.
  uint16_t windowMs       = 400;
  // Raw samples retained ahead of every detection.
  uint16_t preRollMs      = 512;
  // Fraction of the window peak that defines the EXCURSION, for the published
  // sum. Decides nothing.
  float    excursionFrac  = 0.34f;
  // Caliper for the reported width, in counts from the lock. Reporting only.
  int16_t  widthCaliper   = 25;
  // 0 = disabled. Do not set this from X18's 82.
  uint16_t widthFloorMs   = 0;
  // ---- the interval baseline ----
  // Samples within departCounts of the lock that confirm electrical quiet.
  uint16_t clearSamples   = 30;
  // Settling time after that confirmation, before collection begins.
  uint16_t settleMs       = 80;
  // Samples collected for the candidate level.
  uint16_t collectSamples = 200;
  // max-min ceiling over the collection.
  uint16_t spreadMax      = 32;
  // The interval must have been entered at running cadence.
  uint32_t cadenceMaxMs   = 3000;
  // Age past which the lock is REPORTED stale. Gates nothing.
  uint32_t staleMs        = 120000;
  // THE LOST-LOCK BOUND. Continuous time above departCounts, WHILE MOVING,
  // beyond which the lock is declared implausible rather than the signal. No
  // magnet on this railway can do this: spans are 300 mm and the slowest
  // powered running measured is ~240 mm/s, so 2 s above threshold is at least
  // 480 mm of continuous field. 0 disables the recovery entirely.
  uint32_t lostMs         = 2000;
  // AND IT MUST BE AT CRUISE, NOT MERELY MOVING. The tractive floor is not a
  // safe bound here: a station approach ramp is above the floor for its first
  // seconds AND inside a platform field, which is exactly the 2026-09-16
  // Grillers condition. At cruise the locomotive covers 240+ mm/s and never
  // meets a platform shelf, so 2 s of continuous departure cannot be a magnet.
  // A time bound alone would repeat noteRest's mistake one level up.
  uint8_t  lostMinPwm     = 70;
  // Priming, unchanged from X18.
  uint16_t primeMs        = 2000;
  // Shadow rolling median. Telemetry only in X22; it has no write path to the
  // operative reference at all.
  uint16_t baselineMs     = 25;
};

// One completed candidate's telemetry record.
struct Excursion {
  uint32_t detectedAtMs   = 0;   // the sample that completed the persistence run
  uint32_t windowEndMs    = 0;
  int16_t  rawAtDetect    = 0;
  int32_t  localRef       = 0;   // THE LOCK at detection. The record's zero.
  int32_t  departAtDetect = 0;   // signed, raw(detect) - lock
  int32_t  restRef        = 0;   // retained field: equals localRef in X22
  int32_t  reference      = 0;   // same lock, kept for wire compatibility
  int32_t  shadowRef      = 0;   // rolling median at detection, telemetry
  uint32_t baselineAgeMs  = 0;   // age of the lock AT DETECTION
  bool     baselineStale  = false;  // age > staleMs. Gates nothing.
  uint16_t peakCounts     = 0;   // max |v - lock| over the window, judged copy
  int16_t  peakSigned     = 0;
  uint8_t  polarity       = 1;   // 1 = N, 0 = S. FIXED AT THE OPENING.
  bool     openingDisagree = false; // sign(peakSigned) != sign(departAtDetect)
  int64_t  excursionSum   = 0;   // signed sum over EXCURSION samples only
  uint16_t excursionCount = 0;
  uint16_t excursionFirst = 0;
  uint16_t excursionLast  = 0;
  uint16_t widthCaliperMs = 0;
  uint16_t widthFracMs    = 0;
  uint16_t sampleCount    = 0;
  uint16_t preSamples     = 0;
  bool     clipped        = false;
  bool     widthRejected  = false;
  bool     stoppedShort   = false;
  uint16_t measuredMs     = 0;
  const int16_t* oriented = nullptr;
  const int16_t* judged   = nullptr;
};

// THE NAVIGATION EVENT. Emitted at the opening sample, complete.
struct Detection {
  uint32_t detectedAtMs   = 0;
  int16_t  rawAtDetect    = 0;
  int32_t  localRef       = 0;   // THE LOCK
  int32_t  departAtDetect = 0;   // signed, raw(detect) - lock. >= departCounts.
  int32_t  restRef        = 0;   // retained field: equals localRef in X22
  int32_t  reference      = 0;
  int32_t  shadowRef      = 0;
  uint32_t baselineAgeMs  = 0;
  bool     baselineStale  = false;
  uint8_t  polarity       = 1;   // THE SIGN OF departAtDetect.
};

// One completed or refused interval-baseline collection. Telemetry.
struct BaselineOutcome {
  bool     accepted   = false;
  BaselineReject reject = BaselineReject::None;
  int32_t  candidate  = 0;
  int32_t  previous   = 0;
  int32_t  delta      = 0;
  uint16_t spread     = 0;
  uint32_t atMs       = 0;
  uint32_t priorGapMs = 0;
  uint32_t consecutiveRejects = 0;
  bool     recovery   = false;   // the lock had been declared implausible
};

enum class DwellEvent : uint8_t { None = 0, Begin, Depart, OldFieldClear };

template <uint16_t RAW_RING = 1152, uint16_t REC = 960, uint8_t MED = 41>
class ExcursionDetector {
 public:
  ExcursionDetector() = default;
  explicit ExcursionDetector(const DetectorConfig& c) : cfg_(c) {}
  void begin(const DetectorConfig& c) { cfg_ = c; }

  int32_t  baseline()       const { return baseline_; }
  int32_t  shadowBaseline() const { return shadowBaseline_; }
  bool     ready()          const { return primed_; }
  bool     acquiring()      const { return acquiring_; }
  // Retained accessors. In X22 all three are THE LOCK: there is no second
  // reference concept for them to differ from, which is the point of the build.
  int32_t  localRefNow()    const { return baseline_; }
  int32_t  restRef()        const { return baseline_; }
  bool     restValid()      const { return primed_; }
  const Excursion& excursion() const { return out_; }

  // ---- the lock's provenance. Reported; nothing here gates anything. ----
  uint32_t baselineSetMs()  const { return baselineSetMs_; }
  uint32_t baselineAgeMs(uint32_t nowMs) const {
    return primed_ ? (uint32_t)(nowMs - baselineSetMs_) : 0;
  }
  bool     baselineStale(uint32_t nowMs) const {
    return primed_ && cfg_.staleMs && baselineAgeMs(nowMs) > cfg_.staleMs;
  }
  uint32_t baselineAccepted() const { return baseAccepted_; }
  uint32_t baselineRejected() const { return baseRejected_; }
  uint32_t baselineConsecutiveRejects() const { return baseRunRejects_; }
  uint32_t lostLockTotal()  const { return lostTotal_; }
  bool     lostLock()       const { return lostMode_; }
  uint32_t rearmSuppressed()const { return rearmSuppressed_; }
  bool     armedElectrically() const { return armed_; }
  BaselineReject lastBaselineReject() const { return lastReject_; }
  bool collecting() const { return bstate_ == BState::Collect; }
  bool takeBaselineOutcome(BaselineOutcome& o) {
    if (!pendingBaseline_) return false;
    o = bout_; pendingBaseline_ = false; return true;
  }

  bool takeDetection(Detection& d) {
    if (!pendingDetection_) return false;
    d = det_; pendingDetection_ = false; return true;
  }

  uint32_t suppressed()     const { return suppressed_; }
  bool     dwellActive()    const { return dwell_; }
  bool     dwellDisplaced() const { return dwellDisplaced_; }
  bool     oldFieldHold()   const { return oldFieldHold_; }
  bool     armedForNew()    const { return !dwell_ && !oldFieldHold_; }
  int32_t  dwellRest()      const { return dwellRest_; }
  int32_t  dwellDepartAtEntry() const { return dwellDepartAtEntry_; }
  int16_t  dwellRawAtEntry()const { return dwellRawAtEntry_; }
  uint32_t dwellSuppressed()const { return dwellSuppressed_; }
  uint32_t holdSuppressed() const { return holdSuppressed_; }
  uint32_t dwellBeganMs()   const { return dwellBeganMs_; }
  uint32_t oldFieldSinceMs()const { return oldFieldSinceMs_; }
  DwellEvent takeDwellEvent() {
    const DwellEvent e = pendingDwell_; pendingDwell_ = DwellEvent::None; return e;
  }
  uint32_t widthRejects()   const { return widthRejects_; }
  int32_t  primeSpread()    const { return primeHi_ - primeLo_; }
  int32_t  primeValue()     const { return primeValue_; }

  // The lap-baseline controller's correction, applied to THE LOCK. It is a
  // second write path and it is deliberately narrow: bounded by the caller to
  // +-2 counts per lap, refused mid-window and refused mid-collection so that
  // one record reports one reference. It does not reset the lock's age: a
  // nudged reference is the same reference, and the age is what the field test
  // is measuring.
  bool adjustBaseline(int8_t delta) {
    if (acquiring_ || bstate_ == BState::Collect) return false;
    baseline_ += delta;
    return true;
  }

  // A declaration or a direction change ends the frame.
  void reset() {
    acquiring_ = false;
    persistRun_ = 0;
    persistSign_ = 0;
    suppressedRun_ = 0;
    pendingDetection_ = false;
    // The interval is broken: whatever collection was in flight belonged to a
    // frame that no longer exists, and the cadence that would have qualified
    // the next one is meaningless across a declaration.
    abortCollection(BaselineReject::Interrupted, true);
    overSinceMs_ = 0;
    armed_ = true;
    haveLastDetect_ = false;
    dwell_ = false; oldFieldHold_ = false;
    dwellDisplaced_ = false; truncateNow_ = false; clearRun_ = 0;
    pendingDwell_ = DwellEvent::None;
  }

  // One ADC sample. Returns true when a candidate's window has just expired
  // and excursion() holds it.
  //
  // mayAdapt (PWM above this locomotive's measured tractive floor) no longer
  // gates a continuous adaptation, because there is not one. It gates the
  // COLLECTION, which is stronger: a ramping or stationary locomotive cannot
  // establish a reference at all, so there is no freeze rule to outrun and the
  // twelve-second Grillers ramp has nothing to corrupt.
  // pwm DEFAULTS TO 0, meaning "propulsion unknown". A caller that does not
  // say what the throttle is doing must not be able to trigger the lost-lock
  // recovery by omission -- re-priming the reference is the most consequential
  // thing this class can do, and it happens only when a caller has explicitly
  // stated the locomotive is at cruise.
  bool sample(uint32_t nowMs, int16_t raw, bool mayAdapt, bool stopped = false,
              uint8_t pwm = 0) {
    updateShadow(nowMs, raw);
    if (raw <= 8 || raw >= 4087) clipRing_ = true;
    ring_[head_] = raw;
    head_ = (uint16_t)((head_ + 1) % RAW_RING);
    if (filled_ < RAW_RING) ++filled_;
    if (stopped != dwell_) { if (stopped) beginDwell(nowMs, raw); else endDwell(nowMs); }
    if (!primed_) return false;

    const int32_t L = baseline_;
    const int32_t departure = (int32_t)raw - L;
    const int32_t mag = departure < 0 ? -departure : departure;
    const bool over = mag >= cfg_.departCounts;
    // ELECTRICAL REARM, maintained in EVERY state -- during a window, a guard,
    // a dwell and an old-field hold alike. It is a fact about the signal, not
    // about what the detector happens to be doing, and a dwell that returns to
    // the lock must leave the detector armed or the next magnet is invisible.
    if (!over) armed_ = true;

    // The interval-baseline cycle runs on every sample, before and
    // independently of the declaration path. It never ends a candidate, never
    // re-arms anything, and no signal has to return anywhere for a candidate
    // to be admitted. It decides one thing: what the NEXT departure is
    // measured from.
    serviceBaseline(nowMs, raw, mayAdapt, stopped, over, pwm);

    if (acquiring_) {
      const int32_t dw = (int32_t)raw - localRefAt_;
      const int32_t mw = dw < 0 ? -dw : dw;
      if (mw < cfg_.departCounts) sawQuietInWindow_ = true;
      if (mw >= cfg_.departCounts && sawQuietInWindow_) {
        ++suppressedRun_;
        if (suppressedRun_ == cfg_.persistSamples) {
          ++suppressed_;
          lastSuppressedMs_ = nowMs;
          lastSuppressedDepart_ = dw;
        }
      } else if (mw < cfg_.departCounts) suppressedRun_ = 0;
      if (truncateNow_) { truncateNow_ = false; return finish(nowMs, true); }
      if ((int32_t)(nowMs - windowEndMs_) >= 0) return finish(nowMs, false);
      return false;
    }

    // A STATION DWELL, AND THE OLD FIELD IT MAY HAVE STOPPED IN, DECLARE
    // NOTHING. Everything that would have been a candidate is counted.
    if (dwell_ || oldFieldHold_) {
      if (over) {
        ++suppressedRun_;
        if (suppressedRun_ == cfg_.persistSamples) {
          ++suppressed_;
          if (dwell_) ++dwellSuppressed_; else ++holdSuppressed_;
          lastSuppressedMs_ = nowMs;
          lastSuppressedDepart_ = departure;
        }
      } else suppressedRun_ = 0;
      if (oldFieldHold_) checkOldFieldClear(nowMs, raw);
      return false;
    }

    // THE PERSISTENCE RUN. X22 requires the samples to agree in SIGN.
    // Against X19's trailing local minimum an opposite-signed pair was not
    // reachable; against a fixed lock it is, and two samples on opposite sides
    // of the reference are not one excursion.
    // ELECTRICAL REARM. X19's reference was a trailing minimum, so it walked
    // onto any level that persisted and a sustained departure could not repeat
    // -- the detector got its rearm for free from the reference moving. A
    // LOCKED reference does not move, so without this a 110-count shelf
    // re-declares every time X22's (now removed) 645 ms guard expired, for as long as it
    // lasts. The condition is the project's accepted one: electrical quiet MAY
    // rearm the detector, and does NOT claim spatial clearance.
    if (!over) { persistRun_ = 0; persistSign_ = 0; return false; }
    if (!armed_) {
      // Counted, never silent: this is the same total the guard and the dwell
      // report into, so a field record can show whether the rearm ever hid a
      // real marker.
      ++suppressedRun_;
      if (suppressedRun_ == cfg_.persistSamples) {
        ++suppressed_; ++rearmSuppressed_;
        lastSuppressedMs_ = nowMs;
        lastSuppressedDepart_ = departure;
      }
      persistRun_ = 0; persistSign_ = 0;
      return false;
    }
    const int8_t sign = departure > 0 ? 1 : -1;
    if (persistRun_ && sign != persistSign_) persistRun_ = 0;
    persistSign_ = sign;
    if (++persistRun_ < cfg_.persistSamples) return false;

    // ---------------------------------------------------------------------
    // THE DETECTION INSTANT. Polarity, the guard and the navigation event all
    // happen here, exactly as in X21. The only difference is what the
    // departure was measured against.
    // ---------------------------------------------------------------------
    persistRun_ = 0;
    persistSign_ = 0;
    suppressedRun_ = 0;
    sawQuietInWindow_ = false;
    armed_        = false;      // until the line comes back to the lock
    acquiring_    = true;
    detectedAtMs_ = nowMs;
    windowEndMs_  = nowMs + cfg_.windowMs;
    detectHead_   = head_;                 // one past the detection sample
    localRefAt_   = L;
    rawAtDetect_  = raw;
    departAt_     = departure;
    refAt_        = baseline_;
    shadowAt_     = shadowBaseline_;
    ageAt_        = baselineAgeMs(nowMs);
    staleAt_      = baselineStale(nowMs);
    clipAt_       = clipRing_;
    clipRing_     = false;
    detPolarity_  = departure >= 0 ? 1 : 0;

    // The interval that just ended, and the cadence that qualifies the next
    // collection. A collection in flight belongs to the interval this
    // detection has just closed and is abandoned.
    priorGapMs_ = haveLastDetect_ ? (uint32_t)(nowMs - lastDetectMs_) : UINT32_MAX;
    lastDetectMs_ = nowMs;
    haveLastDetect_ = true;
    abortCollection(BaselineReject::Interrupted, false);
    bstate_ = BState::WaitGuard;

    det_ = Detection{};
    det_.detectedAtMs   = nowMs;
    det_.rawAtDetect    = raw;
    det_.localRef       = L;
    det_.departAtDetect = departure;
    det_.restRef        = L;
    det_.reference      = baseline_;
    det_.shadowRef      = shadowBaseline_;
    det_.baselineAgeMs  = ageAt_;
    det_.baselineStale  = staleAt_;
    det_.polarity       = detPolarity_;
    pendingDetection_   = true;
    return false;
  }

 private:
  enum class BState : uint8_t { Idle, WaitGuard, Clear, Settle, Collect };

  // ------------------------------------------------------------------------
  // THE INTERVAL BASELINE. One cycle per detection, five gates plus the leash.
  // ------------------------------------------------------------------------
  void serviceBaseline(uint32_t nowMs, int16_t raw, bool mayAdapt,
                       bool stopped, bool over, uint8_t pwm) {
    // ------------------------------------------------------------------
    // THE LOST LOCK.
    //
    // Every other rule in this class assumes the lock is approximately right.
    // If it is not -- Otto's prime was 112 counts wrong on 2026-09-15, which
    // is the measurement X19 was built in response to -- then `over` is true
    // for ever, the CLEAR stage never completes, no collection ever runs, and
    // the detector can never recover. A locked reference must therefore carry
    // its own falsification test, or a bad prime is a brick.
    //
    // The test is physical, not statistical. Marker spans are 300 mm and the
    // slowest powered running measured on this railway is ~240 mm/s, so two
    // continuous seconds above threshold WHILE MOVING is at least 480 mm of
    // unbroken field. No magnet does that. The signal is therefore right and
    // the lock is wrong.
    //
    // Recovery re-primes from the line as it actually is: the same collection
    // machinery, with the CADENCE gate and THE LEASH lifted, because both of
    // those are statements about a lock that has just been declared unfit to
    // make them. MOVING and QUIET still apply. Every recovery publishes.
    // NOT during a dwell or an old-field hold. The lost test's whole argument
    // is "no magnet can hold the line this long" -- and an old-field hold is
    // the one state where the firmware KNOWS it is sitting in a magnet it has
    // already identified. Re-priming there would install that magnet as the
    // lock, which is the 2026-09-16 Grillers mechanism arriving by the back
    // door. The hold has its own release and does not need this one.
    if (cfg_.lostMs && !stopped && !dwell_ && !oldFieldHold_
        && mayAdapt && pwm >= cfg_.lostMinPwm && over && !acquiring_) {
      if (!overSinceMs_) overSinceMs_ = nowMs;
      else if (!lostMode_ && (uint32_t)(nowMs - overSinceMs_) >= cfg_.lostMs) {
        lostMode_ = true;
        ++lostTotal_;
        bstate_ = BState::Collect;      // straight to a level measurement
        cn_ = 0; clo_ = raw; chi_ = raw;
      }
    } else if (!over || pwm < cfg_.lostMinPwm) {
      overSinceMs_ = 0;
    }
    if (bstate_ == BState::Idle) return;

    // A stopped locomotive establishes nothing, at any stage. This is not a
    // freeze that can be outrun by a slow approach: the gate is on the
    // collection itself, and a ramp below the tractive floor never reaches it.
    if (stopped) { abortCollection(BaselineReject::Stopped, true); return; }

    switch (bstate_) {
      case BState::WaitGuard:
        // Nothing may be collected until the acquisition window and the guard
        // have both finished with this interval's magnet.
        if (acquiring_) return;
        bstate_ = BState::Clear;
        clearRun2_ = 0;
        return;

      case BState::Clear:
        // ELECTRICAL quiet, and it is not claimed to be anything else.
        if (over) { clearRun2_ = 0; return; }
        if (++clearRun2_ < cfg_.clearSamples) return;
        bstate_ = BState::Settle;
        settleFromMs_ = nowMs;
        return;

      case BState::Settle:
        if (over) { bstate_ = BState::Clear; clearRun2_ = 0; return; }
        if ((uint32_t)(nowMs - settleFromMs_) < cfg_.settleMs) return;
        // CADENCE. A collection only qualifies if this interval was entered at
        // running cadence -- that is, between two mapped markers rather than
        // into or out of a station.
        if (priorGapMs_ > cfg_.cadenceMaxMs) {
          abortCollection(BaselineReject::Cadence, true);
          return;
        }
        bstate_ = BState::Collect;
        cn_ = 0; clo_ = raw; chi_ = raw;
        return;

      case BState::Collect: {
        // MOVING, for every sample of the collection.
        if (!mayAdapt) { abortCollection(BaselineReject::Moving, true); return; }
        // A magnet arriving mid-collection ends it. The declaration path will
        // have abandoned it already; this covers a sub-threshold approach that
        // crosses over. In recovery the signal is BY DEFINITION over the
        // threshold -- that is what declared the lock unfit -- so the test is
        // suspended and QUIET alone has to carry the collection.
        if (over && !lostMode_) { abortCollection(BaselineReject::Interrupted, true); return; }
        if (raw < clo_) clo_ = raw;
        if (raw > chi_) chi_ = raw;
        cbuf_[cn_++] = raw;
        if (cn_ < cfg_.collectSamples) return;

        const uint16_t spread = (uint16_t)((int32_t)chi_ - clo_);
        const int32_t cand = collectionMedian();
        const int32_t delta = cand - baseline_;
        const int32_t mdelta = delta < 0 ? -delta : delta;

        bout_ = BaselineOutcome{};
        bout_.candidate  = cand;
        bout_.previous   = baseline_;
        bout_.delta      = delta;
        bout_.spread     = spread;
        bout_.atMs       = nowMs;
        bout_.priorGapMs = priorGapMs_;

        bout_.recovery = lostMode_;
        if (spread > cfg_.spreadMax) {
          finishCollection(BaselineReject::Spread, nowMs);
        } else if (!lostMode_ && mdelta >= cfg_.departCounts) {
          // THE LEASH -- a guarded invariant, expected never to fire. CLEAR
          // already bounds every collected sample to within departCounts of
          // the lock. If this ever appears in a field record, CLEAR has been
          // relaxed and the header's argument needs rereading.
          finishCollection(BaselineReject::Leash, nowMs);
        } else {
          baseline_     = cand;
          baselineSetMs_= nowMs;
          if (lostMode_) { lostMode_ = false; overSinceMs_ = 0; armed_ = true; }
          ++baseAccepted_;
          baseRunRejects_ = 0;
          lastReject_   = BaselineReject::None;
          bout_.accepted = true;
          bout_.consecutiveRejects = 0;
          pendingBaseline_ = true;
          bstate_ = BState::Idle;
          cn_ = 0;
        }
        return;
      }
      default: return;
    }
  }

  // A refusal that has a measured candidate behind it: publish the numbers.
  void finishCollection(BaselineReject why, uint32_t nowMs) {
    if (lostMode_) { lostMode_ = false; overSinceMs_ = nowMs; }
    ++baseRejected_;
    ++baseRunRejects_;
    lastReject_ = why;
    bout_.accepted = false;
    bout_.reject   = why;
    bout_.consecutiveRejects = baseRunRejects_;
    pendingBaseline_ = true;
    bstate_ = BState::Idle;
    cn_ = 0;
    (void)nowMs;
  }

  // A refusal with no candidate to report. `count` distinguishes a genuine
  // refusal from simply abandoning an interval that has been superseded.
  void abortCollection(BaselineReject why, bool count) {
    const bool wasRecovery = lostMode_;
    lostMode_ = false;
    const bool had = bstate_ != BState::Idle;
    bstate_ = BState::Idle;
    cn_ = 0;
    clearRun2_ = 0;
    if (!had || !count) return;
    ++baseRejected_;
    ++baseRunRejects_;
    lastReject_ = why;
    // AND IT PUBLISHES. NOT_MOVING, OFF_CADENCE and STOPPED are the commonest
    // refusals on a railway with four stations, and counting them without
    // recording them is how a lock goes stale invisibly -- which is the
    // failure this build predicts of itself. There is no candidate to report,
    // so candidate/spread stay zero and `previous` carries the lock in force.
    bout_ = BaselineOutcome{};
    bout_.accepted   = false;
    bout_.reject     = why;
    bout_.previous   = baseline_;
    bout_.priorGapMs = priorGapMs_;
    bout_.consecutiveRejects = baseRunRejects_;
    bout_.recovery   = wasRecovery;
    pendingBaseline_ = true;
  }

  // Sorts the collection in place; its order is not needed again.
  int32_t collectionMedian() {
    for (uint16_t i = 1; i < cn_; ++i) {
      int16_t v = cbuf_[i]; int j = (int)i - 1;
      while (j >= 0 && cbuf_[j] > v) { cbuf_[j + 1] = cbuf_[j]; --j; }
      cbuf_[j + 1] = v;
    }
    return cbuf_[cn_ / 2];
  }
  bool finish(uint32_t nowMs, bool stoppedShort) {
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
    // X21. THE SUM NO LONGER DECIDES THE POLE. It is measured, published, and
    // ignored by navigation. The pole was fixed at the detection sample from
    // the sign of the departure that declared the candidate, and this function
    // has no authority to revise it -- which is precisely the authority that
    // turned MM136's South opening into a North identification.
    //
    // sign(exc_sum) against sign(depart) on diag/excursion is now the direct
    // measurement of how often the window disagrees with the opening. On
    // 2026-09-15 that was 2 records in 266; both were the window being wrong.
    const uint8_t pol = detPolarity_;

    // Widths, reported for the re-derivation of decision 0085. Neither gates
    // anything unless widthFloorMs has been set.
    uint16_t wCal = 0, wFrac = 0;
    for (uint16_t i = preAt; i < n_; ++i) {
      const int32_t m = judge_[i] < 0 ? -(int32_t)judge_[i] : judge_[i];
      if (m >= cfg_.widthCaliper) ++wCal;
      if (m >= thr) ++wFrac;
    }

    // Orient. buf_ becomes THE RECORDING and is not touched again.
    // peakSigned carries the sign the window ACTUALLY measured at its argmax,
    // not the declared pole. When the two disagree the record says so, which
    // is the whole point of keeping the measurement.
    const int16_t peakSigned = (int16_t)(judge_[peakAt] < 0 ? -peak : peak);
    if (!pol) {
      for (uint16_t i = 0; i < n_; ++i) buf_[i] = (int16_t)(-buf_[i]);
      medianOfThree(buf_, n_, judge_);
    }

    out_ = Excursion{};
    out_.detectedAtMs   = detectedAtMs_;
    out_.windowEndMs    = nowMs;
    out_.rawAtDetect    = rawAtDetect_;
    out_.localRef       = localRefAt_;
    out_.restRef        = localRefAt_;  // X22: one reference only
    out_.departAtDetect = departAt_;
    out_.reference      = refAt_;
    out_.shadowRef      = shadowAt_;
    out_.peakCounts     = (uint16_t)(peak > 65535 ? 65535 : peak);
    out_.peakSigned     = peakSigned;
    out_.polarity       = pol;
    out_.baselineAgeMs  = ageAt_;
    out_.baselineStale  = staleAt_;
    // THE DISAGREEMENT. sign(peakSigned) against sign(departAtDetect): the
    // window's later opinion against the opening's. Measured and published on
    // every record; it has no authority over anything. On X21 it fired once in
    // 97 records and that record was MM117; on X20 twice in 266, the first
    // being MM136. Both times it selected the failure.
    out_.openingDisagree = (peakSigned < 0) != (departAt_ < 0);
    out_.excursionSum   = sum;
    out_.excursionCount = (uint16_t)(last - first + 1);
    out_.excursionFirst = first;
    out_.excursionLast  = last;
    out_.widthCaliperMs = wCal;
    out_.widthFracMs    = wFrac;
    out_.sampleCount    = n_;
    out_.preSamples     = preAt;
    out_.stoppedShort   = stoppedShort;
    out_.measuredMs     = post;
    out_.clipped        = clipAt_;
    out_.oriented       = buf_;
    out_.judged         = judge_;

    // X22R: there is no detector guard. (X21/X22 started one at the detection
    // sample; it is superseded by NAVI's 650 ms Hall-only fallback.)
    suppressedRun_ = 0;

    if (cfg_.widthFloorMs && wCal < cfg_.widthFloorMs) {
      out_.widthRejected = true;
      ++widthRejects_;
      return false;                 // reported by the caller, never a candidate
    }
    return true;
  }

  // ------------------------------------------------------------------------
  // THE STATION DWELL. Unchanged from X20/X21 in behaviour. The only
  // difference is its anchor: it freezes nothing, because there is nothing
  // continuously adapting to freeze. The lock IS the anchor, and the lock was
  // established while the locomotive was demonstrably moving between two
  // markers -- which is exactly the property X20's freeze-at-dwell was trying
  // and failing to obtain.
  // ------------------------------------------------------------------------

  // Arm normal detection RIGHT NOW. The dwell broke the guard's timing
  // continuity -- a detect-to-detect guard measured across a 30 s stop
  // describes nothing.
  void arm() {
    persistRun_      = 0;
    persistSign_     = 0;
    suppressedRun_   = 0;
  }

  void beginDwell(uint32_t nowMs, int16_t raw) {
    dwell_           = true;
    dwellBeganMs_    = nowMs;
    dwellSuppressed_ = 0;
    truncateNow_     = acquiring_;
    // The anchor is the interval lock, established while moving. X20 had to
    // capture a "rest measured while moving" here because its rest could move
    // afterwards; X22's cannot, so this is a read rather than a freeze.
    dwellRest_       = baseline_;
    const int32_t d  = (int32_t)raw - dwellRest_;
    const int32_t m  = d < 0 ? -d : d;
    dwellDisplaced_     = m >= cfg_.departCounts;
    dwellDepartAtEntry_ = d;
    dwellRawAtEntry_    = raw;
    persistRun_      = 0;
    persistSign_     = 0;
    suppressedRun_   = 0;
    pendingDwell_    = DwellEvent::Begin;
  }

  void endDwell(uint32_t nowMs) {
    dwell_ = false;
    if (dwellDisplaced_) {
      // Still inside the field already encountered while moving. Nothing is
      // declared until the signal is back at the lock.
      oldFieldHold_    = true;
      oldFieldSinceMs_ = nowMs;
      holdSuppressed_  = 0;
      clearRun_        = 0;
    } else {
      arm();
    }
    pendingDwell_ = DwellEvent::Depart;
  }

  void checkOldFieldClear(uint32_t nowMs, int16_t raw) {
    const int32_t d = (int32_t)raw - dwellRest_;
    const int32_t m = d < 0 ? -d : d;
    if (m >= cfg_.departCounts) { clearRun_ = 0; return; }
    if (++clearRun_ < cfg_.persistSamples) return;
    oldFieldHold_ = false;
    clearRun_     = 0;
    arm();
    pendingDwell_ = DwellEvent::OldFieldClear;
    (void)nowMs;
  }

  // PRIMING, AND THE SHADOW.
  //
  // The prime is unchanged: a rolling median over primeMs while the
  // locomotive stands clear of magnets, and it is the lock's first value.
  //
  // After the prime the rolling median keeps running and is published as
  // shadow_baseline, but it has NO WRITE PATH TO baseline_ AT ALL -- not
  // gated, not conditional, not under a config flag. X17 demoted it with
  // `fixedAfterPrime`; X22 removes the branch, so the demotion cannot be
  // undone by a configuration mistake. The shadow's only job is to let a field
  // record show what a continuously adapting reference WOULD have done.
  void updateShadow(uint32_t nowMs, int16_t raw) {
    if (!startMs_) startMs_ = nowMs;
    if (!primed_) {
      if (!primeHave_ || raw < primeLo_) primeLo_ = raw;
      if (!primeHave_ || raw > primeHi_) primeHi_ = raw;
      primeHave_ = true;
    }
    if (nowMs - lastBaseMs_ < cfg_.baselineMs) return;
    lastBaseMs_ = nowMs;
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
    if (!primed_) {
      baseline_ = shadowBaseline_;           // write site 1 of 2
      if ((nowMs - startMs_) >= cfg_.primeMs && medLen_ >= MED / 2) {
        primed_ = true;
        primeValue_ = baseline_;
        baselineSetMs_ = nowMs;
      }
    }
  }

 public:
  uint32_t lastSuppressedMs() const { return lastSuppressedMs_; }
  int32_t  lastSuppressedDepart() const { return lastSuppressedDepart_; }

 private:
  DetectorConfig cfg_;
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
  // the lock's provenance
  uint32_t baselineSetMs_ = 0;
  uint32_t baseAccepted_ = 0, baseRejected_ = 0, baseRunRejects_ = 0;
  BaselineReject lastReject_ = BaselineReject::None;
  bool     pendingBaseline_ = false;
  BaselineOutcome bout_;
  // the lost-lock test
  uint32_t overSinceMs_ = 0, lostTotal_ = 0;
  bool     lostMode_ = false;
  bool     armed_ = true;
  uint32_t rearmSuppressed_ = 0;
  // the collection
  BState   bstate_ = BState::Idle;
  int16_t  cbuf_[512] = {};
  uint16_t cn_ = 0, clearRun2_ = 0;
  int16_t  clo_ = 0, chi_ = 0;
  uint32_t settleFromMs_ = 0;
  uint32_t priorGapMs_ = UINT32_MAX, lastDetectMs_ = 0;
  bool     haveLastDetect_ = false;
  // candidate state
  bool     acquiring_ = false, sawQuietInWindow_ = false;
  uint8_t  persistRun_ = 0, suppressedRun_ = 0;
  int8_t   persistSign_ = 0;
  uint32_t detectedAtMs_ = 0, windowEndMs_ = 0;
  uint16_t detectHead_ = 0;
  int32_t  localRefAt_ = 0, departAt_ = 0, refAt_ = 0, shadowAt_ = 0;
  uint32_t ageAt_ = 0; bool staleAt_ = false;
  uint8_t  detPolarity_ = 1;            // fixed at detection, never revised
  bool     pendingDetection_ = false;
  Detection det_;
  int16_t  rawAtDetect_ = 0;
  bool     clipRing_ = false, clipAt_ = false;
  uint32_t suppressed_ = 0, widthRejects_ = 0, lastSuppressedMs_ = 0;
  int32_t  lastSuppressedDepart_ = 0;
  // dwell state
  bool     dwell_ = false, dwellDisplaced_ = false;
  bool     oldFieldHold_ = false, truncateNow_ = false;
  int32_t  dwellRest_ = 0, dwellDepartAtEntry_ = 0;
  int16_t  dwellRawAtEntry_ = 0;
  uint32_t dwellBeganMs_ = 0, oldFieldSinceMs_ = 0;
  uint32_t dwellSuppressed_ = 0, holdSuppressed_ = 0;
  uint8_t  clearRun_ = 0;
  DwellEvent pendingDwell_ = DwellEvent::None;
  Excursion out_;
};

}  // namespace ngr_hall
