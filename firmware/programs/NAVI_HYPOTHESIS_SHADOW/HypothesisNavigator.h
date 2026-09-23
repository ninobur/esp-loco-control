#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../NAVI_ONE/variants/NAVI_ONE/RouteMap.h"

namespace navi_hypothesis {

static constexpr uint8_t MAX_HYPOTHESES = 8;
static constexpr uint8_t MAX_FAULTS = 1;

enum Cause : uint8_t {
  CAUSE_NONE = 0,
  CAUSE_POLARITY = 1,
  CAUSE_FALSE_OBSERVATION = 2,
  CAUSE_MISSED_MARKER = 4,
  CAUSE_DISAGREEMENT = 8,
};

struct Observation {
  uint32_t atMs = 0;
  uint8_t openingPolarity = 0;
  uint8_t windowPolarity = 0;
  bool windowValid = false;
  bool motionPermitsAdvance = true;
};

struct Hypothesis {
  uint8_t mm = 0;              // Last interpreted physical marker.
  uint32_t lastRealMs = 0;     // Time of the observation interpreted as mm.
  uint8_t faults = 0;
  uint8_t causes = CAUSE_NONE;
};

enum class Result : uint8_t { Ignored, Tracking, Ambiguous, Lost };

inline const char* resultName(Result result) {
  switch (result) {
    case Result::Ignored: return "IGNORED";
    case Result::Tracking: return "TRACKING";
    case Result::Ambiguous: return "AMBIGUOUS";
    default: return "LOST";
  }
}

class HypothesisNavigator {
 public:
  void declare(uint8_t mm, int8_t direction, uint32_t nowMs) {
    direction_ = direction > 0 ? 1 : (direction < 0 ? -1 : 0);
    count_ = direction_ ? 1 : 0;
    if (count_) hypotheses_[0] = {static_cast<uint8_t>(mm % navi_one::ROUTE_N), nowMs, 0, CAUSE_NONE};
    result_ = count_ ? Result::Tracking : Result::Lost;
  }

  Result observe(const Observation& observation) {
    if (!count_ || !direction_) return result_ = Result::Lost;
    if (!observation.motionPermitsAdvance) return result_ = Result::Ignored;

    Hypothesis candidates[MAX_HYPOTHESES * 3]{};
    uint8_t candidateCount = 0;

    for (uint8_t i = 0; i < count_; ++i) {
      const Hypothesis& source = hypotheses_[i];
      const uint8_t next = navi_one::nextMarker(source.mm, direction_);
      const bool disagreement = observation.windowValid &&
                                observation.windowPolarity != observation.openingPolarity;
      const bool nextSupported = supports(observation, navi_one::polarityAt(next));
      const bool nextReachable = reachable(source, observation.atMs, 1);

      // Interpret this observation as the next marker. A polarity contradiction
      // consumes the one-fault allowance; disagreement retains its provenance
      // but does not choose one of the two Hall votes.
      if (nextReachable) {
        const uint8_t extra = nextSupported ? 0 : 1;
        if (source.faults + extra <= MAX_FAULTS) {
          Hypothesis advanced = source;
          advanced.mm = next;
          advanced.lastRealMs = observation.atMs;
          advanced.faults += extra;
          if (extra) advanced.causes |= CAUSE_POLARITY;
          if (disagreement) advanced.causes |= CAUSE_DISAGREEMENT;
          addCandidate(candidates, candidateCount, advanced);
        }
      }

      // A missed-marker branch is created only when the ordinary next-marker
      // interpretation is contradicted or the Hall votes disagree. Creating it
      // on every clean event would make clean running permanently ambiguous.
      if (source.faults < MAX_FAULTS && (!nextSupported || disagreement)) {
        const uint8_t skipped = navi_one::nextMarker(next, direction_);
        if (reachable(source, observation.atMs, 2) &&
            supports(observation, navi_one::polarityAt(skipped))) {
          Hypothesis missed = source;
          missed.mm = skipped;
          missed.lastRealMs = observation.atMs;
          ++missed.faults;
          missed.causes |= CAUSE_MISSED_MARKER;
          if (disagreement) missed.causes |= CAUSE_DISAGREEMENT;
          addCandidate(candidates, candidateCount, missed);
        }
      }

      // Preserve the observation as false only when it carries an explicit
      // reason for doubt. A physically plausible, matching, single-polarity
      // false event is unobservable with Hall alone and cannot be forked on
      // every clean event without destroying useful position certainty.
      const bool suspect = disagreement || !nextSupported || !nextReachable;
      if (source.faults < MAX_FAULTS && suspect) {
        Hypothesis ignored = source;
        ++ignored.faults;
        ignored.causes |= CAUSE_FALSE_OBSERVATION;
        if (disagreement) ignored.causes |= CAUSE_DISAGREEMENT;
        addCandidate(candidates, candidateCount, ignored);
      }
    }

    count_ = candidateCount;
    for (uint8_t i = 0; i < count_; ++i) hypotheses_[i] = candidates[i];
    if (!count_) return result_ = Result::Lost;
    return result_ = distinctPositions() == 1 ? Result::Tracking : Result::Ambiguous;
  }

  Result result() const { return result_; }
  int8_t direction() const { return direction_; }
  uint8_t count() const { return count_; }
  const Hypothesis& hypothesis(uint8_t index) const { return hypotheses_[index]; }
  bool positionCertain() const { return count_ && distinctPositions() == 1; }
  uint8_t position() const { return hypotheses_[0].mm; }

 private:
  static bool supports(const Observation& observation, uint8_t polarity) {
    return observation.openingPolarity == polarity ||
           (observation.windowValid && observation.windowPolarity == polarity);
  }

  uint16_t distanceMm(uint8_t from, uint8_t steps) const {
    uint16_t distance = 0;
    uint8_t cursor = from;
    for (uint8_t i = 0; i < steps; ++i) {
      distance = static_cast<uint16_t>(distance + navi_one::spanMm(cursor, direction_));
      cursor = navi_one::nextMarker(cursor, direction_);
    }
    return distance;
  }

  bool reachable(const Hypothesis& hypothesis, uint32_t atMs, uint8_t steps) const {
    if (!physicalVmaxMmS_) return true;
    const uint32_t elapsed = atMs - hypothesis.lastRealMs;
    return static_cast<uint64_t>(elapsed) * physicalVmaxMmS_ >=
           static_cast<uint64_t>(distanceMm(hypothesis.mm, steps)) * 1000ULL;
  }

  static void addCandidate(Hypothesis* candidates, uint8_t& count,
                           const Hypothesis& candidate) {
    for (uint8_t i = 0; i < count; ++i) {
      if (candidates[i].mm == candidate.mm &&
          candidates[i].lastRealMs == candidate.lastRealMs) {
        if (candidate.faults < candidates[i].faults) candidates[i] = candidate;
        else candidates[i].causes |= candidate.causes;
        return;
      }
    }
    if (count < MAX_HYPOTHESES * 3) candidates[count++] = candidate;
  }

  uint8_t distinctPositions() const {
    uint8_t distinct = 0;
    for (uint8_t i = 0; i < count_; ++i) {
      bool seen = false;
      for (uint8_t j = 0; j < i; ++j) {
        if (hypotheses_[j].mm == hypotheses_[i].mm) { seen = true; break; }
      }
      if (!seen) ++distinct;
    }
    return distinct;
  }

 public:
  void setPhysicalVmax(uint32_t mmPerSecond) { physicalVmaxMmS_ = mmPerSecond; }

 private:
  Hypothesis hypotheses_[MAX_HYPOTHESES * 3]{};
  uint8_t count_ = 0;
  int8_t direction_ = 0;
  uint32_t physicalVmaxMmS_ = 1000;
  Result result_ = Result::Lost;
};

}  // namespace navi_hypothesis
