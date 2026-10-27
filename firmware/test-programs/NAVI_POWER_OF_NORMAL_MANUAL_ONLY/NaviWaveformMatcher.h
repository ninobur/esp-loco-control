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

  void declare(uint8_t last, uint8_t next, int8_t dir) {
    last_ = last;
    next_ = next;
    dir_ = dir;
    state_ = MatcherState::Tracking;
    seq_ = 0;
    lastAdvancePassageUs_ = 0;
  }

  void suspend() {
    state_ = MatcherState::Unset;
    seq_ = 0;
    lastAdvancePassageUs_ = 0;
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
    o.expectedMm = (state_ == MatcherState::Tracking) ? next_ : 0;
    o.expectedPolarity =
        (state_ == MatcherState::Tracking) ? expectedHallSign(next_) : 0;
    o.lastMmAfter = last_;

    MatcherOutcome out = MatcherOutcome::NoClaim;
    if (state_ == MatcherState::Tracking) {
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
  uint32_t confirmed_ = 0, discrepancies_ = 0, duplicateGuards_ = 0;
  WaveformObservation obs_[kObsRing];
  int obsHead_ = 0;
  uint32_t obsCount_ = 0;
  WaveformObservation latest_;
};

}  // namespace navi_power
