// NaviWaveformMatcher.h — the simple next-target DNA matcher (Option B).
//
// Assignment section 3; design (ARCHITECTURE.md) section 3 (event contract)
// and section 4 (one target), WITHOUT section 4.3.2: no bounded sequence
// re-alignment, no rearview mirror, no competing hypotheses, no automatic
// geographic correction. Those remain research; this experiment establishes
// whether they are needed.
//
// State: last confirmed marker, next expected marker and its mapped polarity,
// direction of travel. The chronological record of recognized encounters
// (Observation record) is kept in full — nothing is erased or reinterpreted.
//
// For each recognized event:
//   - polarity agrees with the expected target: confirm, advance to the next
//     mapped magnet.
//   - polarity disagrees: record the observation and the discrepancy.
//     Retain the last confirmed position; leave the expected target unchanged.
//   - duplicate guard (separate from recognition, provisional): an advance
//     is refused when the previous advance's passage time is less than
//     400 ms earlier. The observation is still recorded (outcome
//     DuplicateGuard); only the advance is refused.
//
// ORDERING RULE (controls vs observations). A control (declare/suspend)
// takes effect at the moment it is applied, and it starts a new navigation
// sequence. An event whose passage time predates the most recent control
// belongs to the previous sequence: it is recorded as an observation but
// is never judged against the new target (outcome NoClaim), so a
// re-declaration cannot reinterpret a stale observation as a confirmation
// of the new target. This matters because the recognizer declares an event
// ~359 ms after its passage, so an event can be declared after a control
// was applied even though the magnet passed before it. rvmTask also judges
// events already declared before applying a pending control, so this guard
// only fires for that latency window — but it is the guarantee, not the
// ordering in rvmTask. The control timestamp is the time the control is
// applied (esp_timer_get_time), on the same clock as the event passage
// times.
//
// No Arduino dependencies (only RouteMap.h, which is plain C++), so this
// file is host-testable alongside the recognizer core.

#pragma once

#include <stdint.h>
#include "RouteMap.h"
#include "NaviMatchedFilter.h"

namespace navi_power {

enum class MatcherState : uint8_t { Unset = 0, Tracking };

enum class MatcherOutcome : uint8_t {
  NoClaim = 0,       // no expected target (not declared); observation recorded
  Confirmed = 1,     // polarity agreed; position advanced
  Discrepancy = 2,   // polarity disagreed; position and target unchanged
  DuplicateGuard = 3 // polarity agreed but too soon after the last advance
};

// One entry in the chronological Observation record (design section 3).
struct WaveformObservation {
  uint32_t seq = 0;           // per-declaration sequence number (1-based)
  uint64_t passageUs = 0;     // template-peak time
  uint64_t recognizedUs = 0;  // decision time (passage + 359 ms)
  float r = 0;                // shape score
  float amplitude = 0;        // ADC counts
  int8_t polarity = 0;        // +1 North (ADC rises), -1 South
  uint8_t expectedMm = 0;     // next expected marker at the time (0 = none)
  int8_t expectedPolarity = 0; // mapped polarity of the expected marker
  MatcherOutcome outcome = MatcherOutcome::NoClaim;
  uint8_t lastMmAfter = 0;    // last confirmed marker after processing
};

class WaveformMatcher {
 public:
  // Duplicate-advance guard, in microseconds. PROVISIONAL, not an
  // established hardware-validated constant: derived from the fastest
  // observed traverses in the research event ledger (~650 mm/s over the
  // 280 mm minimum spacing, ~430 ms peak-to-peak), with the earliest
  // observed genuine traverse at 466 ms peak-to-peak. The separation is
  // narrow. The track test must measure the real fast-traverse intervals;
  // if this guard ever fires on a genuine magnet, that is visible in
  // telemetry (outcome DuplicateGuard) and is evidence the constant is
  // wrong. The guard lives here, in the matcher — never in the recognizer
  // — and it never suppresses an observation, only an advance.
  static constexpr uint64_t kMinAdvanceUs = 400000u;

  // nowUs is the time the control takes effect (esp_timer_get_time, the
  // same clock as the event passage times). It starts a new navigation
  // sequence; see the ordering rule in the header comment.
  void declare(uint8_t last, uint8_t next, int8_t dir, uint64_t nowUs) {
    last_ = last;
    next_ = next;
    dir_ = dir;
    state_ = MatcherState::Tracking;
    seq_ = 0;
    lastAdvancePassageUs_ = 0;
    lastControlUs_ = nowUs;
  }

  void suspend(uint64_t nowUs) {
    state_ = MatcherState::Unset;
    seq_ = 0;
    lastAdvancePassageUs_ = 0;
    lastControlUs_ = nowUs;
  }

  // Judge one recognized event against the current target. Always records
  // the observation; returns the outcome. Position advances only on
  // Confirmed.
  MatcherOutcome onEvent(const NaviRecognizedEvent& e) {
    WaveformObservation o;
    o.seq = ++seq_;
    o.passageUs = e.tPassageUs;
    o.recognizedUs = e.tRecognizedUs;
    o.r = e.r;
    o.amplitude = e.amplitude;
    o.polarity = e.polarity;
    o.lastMmAfter = last_;

    MatcherOutcome out = MatcherOutcome::NoClaim;
    // Ordering rule: an event whose passage predates the most recent
    // control belongs to the previous navigation sequence. Record it
    // (expectedMm 0: no target was applied to it) but do not judge it
    // against the new target — a re-declaration must not reinterpret a
    // stale observation as a confirmation.
    if (e.tPassageUs < lastControlUs_) {
      o.expectedMm = 0;
      o.expectedPolarity = 0;
    } else if (state_ == MatcherState::Tracking) {
      o.expectedMm = next_;
      o.expectedPolarity = expectedHallSign(next_);
      if (e.polarity != expectedHallSign(next_)) {
        out = MatcherOutcome::Discrepancy;
        ++discrepancies_;
      } else if (lastAdvancePassageUs_ != 0 &&
                 e.tPassageUs - lastAdvancePassageUs_ < kMinAdvanceUs) {
        out = MatcherOutcome::DuplicateGuard;
        ++duplicateGuards_;
      } else {
        out = MatcherOutcome::Confirmed;
        last_ = next_;
        next_ = navi_one::nextMarker(next_, dir_);
        lastAdvancePassageUs_ = e.tPassageUs;
        ++confirmed_;
        o.lastMmAfter = last_;
      }
    } else {
      o.expectedMm = 0;
      o.expectedPolarity = 0;
    }
    o.outcome = out;
    record_(o);
    latest_ = o;
    return out;
  }

  MatcherState state() const { return state_; }
  uint8_t lastMm() const { return last_; }
  uint8_t nextMm() const { return next_; }
  int8_t dir() const { return dir_; }
  uint32_t confirmed() const { return confirmed_; }
  uint32_t discrepancies() const { return discrepancies_; }
  uint32_t duplicateGuards() const { return duplicateGuards_; }

  // The most recently processed observation (for telemetry).
  const WaveformObservation& latest() const { return latest_; }
  // Number of observations in the ring.
  uint32_t observationCount() const { return obsCount_; }
  // i-th oldest observation in the ring (i = 0 is oldest).
  const WaveformObservation& observation(uint32_t i) const {
    return obs_[(obsHead_ - obsCount_ + (int)i + 2 * kObsRing) % kObsRing];
  }

  static int8_t expectedHallSign(uint8_t mm) {
    // Same convention as the retired NaviTarget: ROUTE 1 = North = ADC above
    // the level.
    return navi_one::polarityAt(mm) ? 1 : -1;
  }

 private:
  static constexpr int kObsRing = 32;  // chronological record, last 32 events

  void record_(const WaveformObservation& o) {
    obs_[obsHead_] = o;
    obsHead_ = (obsHead_ + 1) % kObsRing;
    if (obsCount_ < (uint32_t)kObsRing) ++obsCount_;
  }

  MatcherState state_ = MatcherState::Unset;
  uint8_t last_ = 0, next_ = 0;
  int8_t dir_ = 0;
  uint32_t seq_ = 0;
  uint64_t lastAdvancePassageUs_ = 0;  // 0 = no advance yet this declaration
  uint64_t lastControlUs_ = 0;  // time the most recent control took effect;
                                // 0 = no control yet (no event can predate it)
  uint32_t confirmed_ = 0, discrepancies_ = 0, duplicateGuards_ = 0;
  WaveformObservation obs_[kObsRing];
  int obsHead_ = 0;
  uint32_t obsCount_ = 0;
  WaveformObservation latest_;
};

}  // namespace navi_power
