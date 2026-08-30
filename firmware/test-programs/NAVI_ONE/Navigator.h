#pragma once
// ---------------------------------------------------------------------------
// Navigator — identity and position. Answers the second question:
//
//                  "Is this the magnet the map says comes next?"
//
// The recognizer has already decided it IS a magnet, without knowing where the
// locomotive is. This layer knows the map and the declaration, and nothing
// about waveforms.
//
// THE CONTRACT (decisions 0053, 0056, 0057)
//
//   1. Position must be DECLARED. Until then nothing advances, whatever
//      arrives. The operator's declaration is truth -- and, being truth, it is
//      falsifiable at the very next magnet.
//   2. Exactly ONE target per event: nextMarker(navMm, navDir). Never a list,
//      never a candidate, never a search.
//   3. Identity is POLARITY against that one target. It is the only test here,
//      and the only one that can tell a WRONG magnet from a NON-magnet.
//   4. Pass -> navMm advances by exactly one. advance() is the only place in
//      this program that writes navMm.
//   5. Fail -> navMm does not move, the reason is published, and the
//      locomotive stops. ONE STRIKE (operator, 2026-08-29).
//   6. Nothing else exists. No offsets, no scoring, no adoption, no
//      quarantine, no recovery, no velocity model, no dead reckoning, and no
//      "silent magnet" -- the term and the concept are banned.
//
// WHAT STOPS THE LOCOMOTIVE, AND WHAT DOES NOT
//
//   An IDENTITY failure stops it. The map and the world disagree, and under
//   decision 0056 the navigator's whole job at that moment is to stop saying
//   where it is. It cannot tell a missed magnet from a moved magnet from a bad
//   declaration -- all three need a person.
//
//   A RECOGNIZER refusal does not. That is the filter working before identity
//   is asked: navMm was not written, position is not in doubt, and on the
//   surveyed circuit the recognizer refused 156 non-primaries and zero real
//   magnets. Stopping for correct behaviour teaches an operator to ignore
//   stops.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include "MagnetRecognizer.h"
#include "RouteMap.h"

namespace navi_one {

enum class NavState : uint8_t { Unset = 0, Declared };

enum class Ruling : uint8_t {
  Advanced = 0,     // identity confirmed, navMm moved exactly one
  NoPosition,       // nothing declared; event recorded and discarded
  NotAMagnet,       // recognizer refused; no stop
  WrongMagnet,      // POLARITY disagreed -- one strike, stop
};

inline const char* rulingName(Ruling r) {
  switch (r) {
    case Ruling::Advanced:    return "ADVANCED";
    case Ruling::NoPosition:  return "NO_POSITION";
    case Ruling::NotAMagnet:  return "NOT_A_MAGNET";
    default:                  return "WRONG_MAGNET";
  }
}

// Confidence in the declaration. DECLARED is the operator's word; PROVEN is the
// track's. Ten consecutive polarity readings have exactly one place on this
// route they can come from, so the sequence either confirms the declaration or
// names where the pattern says the locomotive really is.
enum class Trust : uint8_t { Declared = 0, Proven, Contradicted };

inline const char* trustName(Trust t) {
  switch (t) {
    case Trust::Proven:       return "PROVEN";
    case Trust::Contradicted: return "CONTRADICTED";
    default:                  return "DECLARED";
  }
}

static constexpr uint8_t SEQ_N = 10;   // every window of 10 is unique

struct NavStatus {
  NavState state   = NavState::Unset;
  uint8_t  navMm   = 0;
  int8_t   navDir  = 0;          // +1 CW, -1 CCW, 0 unset
  uint8_t  target  = 0;
  Trust    trust   = Trust::Declared;
  uint8_t  seqAt   = 0;          // where the ten-magnet word says it is
  uint32_t advances = 0, refusals = 0, notMagnets = 0;
  Ruling   lastRuling = Ruling::NoPosition;
  Outcome  lastOutcome = Outcome::Magnet;
};

class Navigator {
 public:
  explicit Navigator(MagnetRecognizer& rec) : rec_(rec) {}

  const NavStatus& status() const { return s_; }
  bool positionKnown() const { return s_.state == NavState::Declared && s_.navDir != 0; }

  // Rule 1. Seeds position and direction and NOTHING else: no guard is armed
  // (a declaration is not a detected passage, so it has no rebound to reject),
  // no interval is started, and the sequence witness starts empty.
  void declare(uint8_t mm, int8_t dir) {
    s_.navMm = mm % ROUTE_N;
    s_.navDir = dir;
    s_.state = NavState::Declared;
    s_.target = nextMarker(s_.navMm, s_.navDir);
    s_.trust = Trust::Declared;
    s_.advances = s_.refusals = s_.notMagnets = 0;
    seqLen_ = 0;
    rec_.reset();          // gain and guard describe a frame that has ended
  }

  // A direction change invalidates the same things a declaration does:
  // readings taken one way cannot testify about the other. Position survives,
  // stepped back one so the standard advance lands on the marker the
  // locomotive is about to meet again.
  void setDirection(int8_t dir) {
    if (dir == s_.navDir || dir == 0) return;
    if (s_.state == NavState::Declared && s_.navDir != 0)
      s_.navMm = routeMod((int32_t)s_.navMm + s_.navDir);
    s_.navDir = dir;
    s_.target = nextMarker(s_.navMm, s_.navDir);
    s_.trust = Trust::Declared;
    seqLen_ = 0;
    rec_.reset();
  }

  // The whole decision. One passage in, one ruling out.
  Ruling judge(const Passage& p, const Verdict& v) {
    s_.lastOutcome = v.outcome;
    if (!positionKnown()) { s_.lastRuling = Ruling::NoPosition; return s_.lastRuling; }
    if (!v.isMagnet)      { s_.notMagnets++; s_.lastRuling = Ruling::NotAMagnet; return s_.lastRuling; }

    s_.target = nextMarker(s_.navMm, s_.navDir);
    if (p.polarity != polarityAt(s_.target)) {       // rule 3
      s_.refusals++;
      s_.lastRuling = Ruling::WrongMagnet;           // rule 5 -- one strike
      return s_.lastRuling;
    }
    advance(p.polarity);                             // rule 4
    s_.lastRuling = Ruling::Advanced;
    return s_.lastRuling;
  }

 private:
  // THE ONLY WRITE TO navMm IN THE PROGRAM.
  void advance(uint8_t polarity) {
    s_.navMm = nextMarker(s_.navMm, s_.navDir);
    s_.target = nextMarker(s_.navMm, s_.navDir);
    s_.advances++;
    seqPush(polarity);
    verifySequence();
  }

  void seqPush(uint8_t pol) {
    if (seqLen_ < SEQ_N) { seq_[seqLen_++] = pol; return; }
    for (uint8_t i = 1; i < SEQ_N; ++i) seq_[i - 1] = seq_[i];
    seq_[SEQ_N - 1] = pol;
  }

  // Secondary verification. It REPORTS and never acts: it cannot move navMm,
  // cannot refuse an event, and cannot stop the locomotive. Its job is to say
  // whether the position being claimed is the one the track spells out.
  void verifySequence() {
    if (seqLen_ < SEQ_N) return;
    // The newest reading sits at navMm; reading i belongs to
    // navMm - navDir*(SEQ_N-1-i).
    bool here = true;
    for (uint8_t i = 0; i < SEQ_N && here; ++i) {
      uint8_t mm = routeMod((int32_t)s_.navMm - (int32_t)s_.navDir * (int32_t)(SEQ_N - 1 - i));
      if (polarityAt(mm) != seq_[i]) here = false;
    }
    if (here) { s_.trust = Trust::Proven; s_.seqAt = s_.navMm; return; }
    // Where DOES the word fit? Unique if it fits anywhere.
    uint8_t found = 0; int hits = 0;
    for (uint8_t start = 0; start < ROUTE_N; ++start) {
      bool ok = true;
      for (uint8_t i = 0; i < SEQ_N && ok; ++i) {
        uint8_t mm = routeMod((int32_t)start - (int32_t)s_.navDir * (int32_t)(SEQ_N - 1 - i));
        if (polarityAt(mm) != seq_[i]) ok = false;
      }
      if (ok) { found = start; if (++hits > 1) break; }
    }
    s_.trust = Trust::Contradicted;
    s_.seqAt = (hits == 1) ? found : s_.navMm;
  }

  MagnetRecognizer& rec_;
  NavStatus s_;
  uint8_t seq_[SEQ_N] = {};
  uint8_t seqLen_ = 0;
};

}  // namespace navi_one
