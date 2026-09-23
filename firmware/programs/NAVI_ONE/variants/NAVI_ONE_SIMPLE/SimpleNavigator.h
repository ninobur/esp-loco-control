#pragma once
#include <stdint.h>
#include "RouteMap.h"

namespace navi_one {

// The Hall detector decides whether a sustained magnetic departure occurred.
// These small records preserve the existing telemetry handoff to the loop task.
enum class Outcome : uint8_t { Magnet = 0 };
inline const char* outcomeName(Outcome) { return "MAGNET"; }
struct Passage {
  uint32_t openedAtMs = 0, closedAtMs = 0;
  uint16_t peakCounts = 0;
  uint8_t polarity = 0;
};
struct Verdict {
  Outcome outcome = Outcome::Magnet;
  bool isMagnet = true;
  bool postStopSuccessor = false;
  float amplitudeRatio = 0;
  uint32_t gapMs = 0;
  uint16_t gain = 0;
};

enum class NavState : uint8_t { Unset, Declared, Struck };
inline const char* navStateName(NavState n) {
  switch (n) {
    case NavState::Declared: return "DECLARED";
    case NavState::Struck: return "STRUCK";
    default: return "UNSET";
  }
}
enum class Ruling : uint8_t { Advanced, NoPosition, NotAMagnet, WrongMagnet };
inline const char* rulingName(Ruling r) {
  switch (r) {
    case Ruling::Advanced: return "ADVANCED";
    case Ruling::NotAMagnet: return "NOT_A_MAGNET";
    case Ruling::WrongMagnet: return "WRONG_MAGNET";
    default: return "NO_POSITION";
  }
}
enum class Trust : uint8_t { Declared };
inline const char* trustName(Trust) { return "DECLARED"; }

struct NavStatus {
  NavState state = NavState::Unset;
  uint8_t navMm = 0, target = 0;
  int8_t navDir = 0;
  Trust trust = Trust::Declared;
  uint8_t seqAt = 0;
  bool seqNamed = false;
  uint32_t advances = 0, refusals = 0, notMagnets = 0;
  Ruling lastRuling = Ruling::NoPosition;
  Outcome lastOutcome = Outcome::Magnet;
};

class Navigator {
 public:
  const NavStatus& status() const { return s_; }
  bool positionKnown() const {
    return s_.state == NavState::Declared && s_.navDir != 0;
  }
  bool takeResetRequest() {
    bool requested = resetPending_;
    resetPending_ = false;
    return requested;
  }
  void declare(uint8_t mm, int8_t dir) {
    s_.navMm = mm % ROUTE_N;
    s_.navDir = dir;
    s_.state = NavState::Declared;
    s_.target = nextMarker(s_.navMm,dir);
    s_.advances = s_.refusals = s_.notMagnets = 0;
    resetPending_ = true;
  }
  void setDirection(int8_t dir) {
    if (dir == 0 || dir == s_.navDir) return;
    if (positionKnown()) s_.navMm = routeMod((int32_t)s_.navMm + s_.navDir);
    s_.navDir = dir;
    s_.target = nextMarker(s_.navMm,dir);
    s_.advances = s_.refusals = s_.notMagnets = 0;
    resetPending_ = true;
  }
  Ruling judge(const Passage& p, const Verdict& v) {
    s_.lastOutcome = v.outcome;
    if (!positionKnown()) return s_.lastRuling = Ruling::NoPosition;
    if (!v.isMagnet) {
      ++s_.notMagnets;
      return s_.lastRuling = Ruling::NotAMagnet;
    }
    s_.target = nextMarker(s_.navMm,s_.navDir);
    if (p.polarity != polarityAt(s_.target)) {
      ++s_.refusals;
      s_.state = NavState::Struck;
      return s_.lastRuling = Ruling::WrongMagnet;
    }
    s_.navMm = s_.target;
    s_.target = nextMarker(s_.navMm,s_.navDir);
    ++s_.advances;
    return s_.lastRuling = Ruling::Advanced;
  }
 private:
  NavStatus s_{};
  bool resetPending_ = false;
};
}
