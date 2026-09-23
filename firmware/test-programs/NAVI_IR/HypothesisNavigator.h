#pragma once

#include <stddef.h>
#include <stdint.h>

#include "RouteMap.h"
#include "MovementEvidence.h"

namespace navi_hypothesis {

static constexpr uint8_t MAX_HYPOTHESES = 8;
static constexpr uint32_t LEGACY_MIN_MARKER_MS = 500; // temporary timing reality gate retained in 0.4

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
  ngr_nav::MotionPoint movement{};
};

struct Hypothesis {
  uint8_t mm = 0;              // Last interpreted physical marker.
  uint32_t lastRealMs = 0;     // Time of the observation interpreted as mm.
  uint8_t faults = 0;
  uint8_t causes = CAUSE_NONE;
  uint8_t clean = 0;
  bool distanceAgrees = false;
  ngr_nav::MotionIssue distanceIssue = ngr_nav::MotionIssue::NoSource;
  ngr_nav::MotionPoint movement{};
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

struct DistanceChoice {
  uint8_t closest=0;       // 0,1,2 mapped intervals; 3 means exact tie
  bool tied=false;
  double error=0;
};


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

    // Storage belongs to the object; do not put many wire snapshots on an
    // ESP task stack. Exhaustion is reported as LOST, never silently pruned.
    overflow_=false;
    uint8_t candidateCount = 0;

    for (uint8_t i = 0; i < count_; ++i) {
      const Hypothesis& source = hypotheses_[i];
      const uint8_t next = navi_one::nextMarker(source.mm, direction_);
      const bool disagreement = observation.windowValid &&
                                observation.windowPolarity != observation.openingPolarity;
      const bool nextSupported = supports(observation, navi_one::polarityAt(next));
      const bool timingTooSoon = source.lastRealMs &&
          uint32_t(observation.atMs - source.lastRealMs) < LEGACY_MIN_MARKER_MS;
      const bool nextReachable = !timingTooSoon && reachable(source, observation.atMs, 1);
      const auto movement=ngr_nav::between(source.movement,observation.movement);
      const DistanceChoice distanceChoice = movement.usable() ?
          classifyDistance(source.mm,movement.nominalMm) : DistanceChoice{1,false,0};
      const uint8_t closest=distanceChoice.closest;
      // Unvalidated nominal travel may reveal another interpretation, but
      // cannot eliminate a physically possible one or assign its identity.
      const bool irAlternative=movement.usable() && closest!=1;
      // 0.4 reality gate: when valid wheel travel is still closer to ZERO
      // mapped intervals than to the next marker, the Hall event is physically
      // too early to advance NAV. This is deliberately a midpoint gate, not a
      // fabricated +/- percentage accuracy claim.
      const bool irTooEarly = movement.usable() && closest==0;

      // Interpret this observation as the next marker. A polarity contradiction
      // is recorded as fault evidence; disagreement retains its provenance
      // but does not choose one of the two Hall votes. There is no ONE STRIKE
      // terminal rule in 0.4; later evidence may restore certainty.
      if (nextReachable && !irTooEarly) {
        const uint8_t extra = nextSupported ? 0 : 1;
        {
          Hypothesis advanced = source;
          advanced.mm = next;
          advanced.lastRealMs = observation.atMs;
          advanced.movement = observation.movement;
          advanced.distanceAgrees=movement.usable() && closest==1;
          advanced.distanceIssue=movement.issue;
          advanced.faults += extra;
          if (extra) advanced.causes |= CAUSE_POLARITY;
          if (disagreement) advanced.causes |= CAUSE_DISAGREEMENT;
          advanced.clean = extra || disagreement || !advanced.distanceAgrees ? 0 :
              uint8_t(source.clean<10 ? source.clean+1:10);
          addCandidate(candidates_, candidateCount, advanced);
        }
      }

      // A missed-marker branch is created only when the ordinary next-marker
      // interpretation is contradicted or the Hall votes disagree. Creating it
      // on every clean event would make clean running permanently ambiguous.
      if (!nextSupported || disagreement || (irAlternative && closest>=2)) {
        const uint8_t skipped = navi_one::nextMarker(next, direction_);
        if (reachable(source, observation.atMs, 2) &&
            supports(observation, navi_one::polarityAt(skipped))) {
          Hypothesis missed = source;
          missed.mm = skipped;
          missed.lastRealMs = observation.atMs;
          missed.movement=observation.movement;missed.clean=0;
          missed.distanceAgrees=movement.usable() && closest==2;
          missed.distanceIssue=movement.issue;
          ++missed.faults;
          missed.causes |= CAUSE_MISSED_MARKER;
          if (disagreement) missed.causes |= CAUSE_DISAGREEMENT;
          addCandidate(candidates_, candidateCount, missed);
        }
      }

      // Preserve the observation as false only when it carries an explicit
      // reason for doubt. A physically plausible, matching, single-polarity
      // false event is unobservable with Hall alone and cannot be forked on
      // every clean event without destroying useful position certainty.
      const bool suspect = disagreement || !nextSupported || !nextReachable || irAlternative || irTooEarly;
      if (suspect) {
        Hypothesis ignored = source;
        ++ignored.faults;
        ignored.clean=0;
        ignored.distanceAgrees=movement.usable() && closest==0;
        ignored.distanceIssue=movement.issue;
        ignored.causes |= CAUSE_FALSE_OBSERVATION;
        if (disagreement) ignored.causes |= CAUSE_DISAGREEMENT;
        addCandidate(candidates_, candidateCount, ignored);
      }
    }

    count_ = overflow_ ? 0 : candidateCount;
    for (uint8_t i = 0; i < count_; ++i) hypotheses_[i] = candidates_[i];
    if (!count_) return result_ = Result::Lost;
    // Ten subsequent clean observations remain a map-pattern backstop after
    // uncertainty. They are not required at startup after operator declaration.
    // On a resolved track they clear accumulated fault provenance.
    if(distinctPositions()==1) {
      bool renewed=true;
      for(uint8_t i=0;i<count_;++i)if(hypotheses_[i].clean<10)renewed=false;
      if(renewed)for(uint8_t i=0;i<count_;++i){hypotheses_[i].faults=0;hypotheses_[i].causes=CAUSE_NONE;}
    }
    return result_ = distinctPositions() == 1 ? Result::Tracking : Result::Ambiguous;
  }

  // Movement-assisted recovery. NAVI remains the sole decider: wheel travel
  // narrows the map neighborhood from the last confirmed landmark, while the
  // current Hall observation must corroborate candidate polarity. Whole laps
  // are retained explicitly and the remainder is searched around the route.
  bool reseedFromMovement(uint8_t anchorMm, const ngr_nav::MotionPoint& anchorMovement,
                          const Observation& observation, uint8_t maxCandidates=2) {
    if (!direction_ || maxCandidates==0) return false;
    const auto interval=ngr_nav::between(anchorMovement, observation.movement);
    if(!interval.usable()) return false;

    uint32_t lapMm=0; uint8_t lapCursor=anchorMm;
    for(uint16_t step=0;step<navi_one::ROUTE_N;++step){
      lapMm += navi_one::spanMm(lapCursor,direction_);
      lapCursor=navi_one::nextMarker(lapCursor,direction_);
    }
    if(!lapMm)return false;

    const uint32_t wholeLaps=uint32_t(interval.nominalMm/double(lapMm));
    double remainder=interval.nominalMm-double(wholeLaps)*double(lapMm);
    if(remainder<0)remainder=0;

    struct Pick { uint8_t mm=0; double error=1e30; uint32_t distance=0; };
    Pick best[2];
    uint8_t cursor=anchorMm; uint32_t cumulative=0;
    for(uint16_t step=1; step<=navi_one::ROUTE_N; ++step){
      cumulative += navi_one::spanMm(cursor,direction_);
      cursor=navi_one::nextMarker(cursor,direction_);
      double e=remainder-double(cumulative); if(e<0)e=-e;
      // A remainder near zero is legitimately closest to the anchor after one
      // or more complete circuits; the step==ROUTE_N candidate represents it.
      if(e<best[0].error){best[1]=best[0];best[0]={cursor,e,cumulative};}
      else if(e<best[1].error)best[1]={cursor,e,cumulative};
    }

    count_=0; overflow_=false;
    for(uint8_t i=0;i<maxCandidates && i<2;++i){
      if(best[i].error>=1e29)continue;
      if(!supports(observation,navi_one::polarityAt(best[i].mm)))continue;
      Hypothesis h{}; h.mm=best[i].mm; h.lastRealMs=observation.atMs;
      h.movement=observation.movement; h.distanceAgrees=true;
      h.distanceIssue=ngr_nav::MotionIssue::None; h.clean=0;
      addCandidate(hypotheses_,count_,h);
    }
    result_=count_?(distinctPositions()==1?Result::Tracking:Result::Ambiguous):Result::Lost;
    return count_!=0;
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

  DistanceChoice classifyDistance(uint8_t from,double nominalMm) const {
    DistanceChoice out; out.closest=0; out.error=nominalMm;
    for(uint8_t step=1;step<=2;++step) {
      double d=nominalMm-distanceMm(from,step); if(d<0)d=-d;
      if(d<out.error){out.error=d;out.closest=step;out.tied=false;}
      else if(d==out.error){out.tied=true;}
    }
    if(out.tied)out.closest=3;
    return out;
  }

  bool reachable(const Hypothesis& hypothesis, uint32_t atMs, uint8_t steps) const {
    if (!physicalVmaxMmS_) return true;
    const uint32_t elapsed = atMs - hypothesis.lastRealMs;
    return static_cast<uint64_t>(elapsed) * physicalVmaxMmS_ >=
           static_cast<uint64_t>(distanceMm(hypothesis.mm, steps)) * 1000ULL;
  }

  void addCandidate(Hypothesis* candidates, uint8_t& count,
                           const Hypothesis& candidate) {
    for (uint8_t i = 0; i < count; ++i) {
      if (candidates[i].mm == candidate.mm &&
          candidates[i].lastRealMs == candidate.lastRealMs) {
        const bool distanceAgrees=candidates[i].distanceAgrees && candidate.distanceAgrees;
        const auto distanceIssue=candidates[i].distanceIssue!=ngr_nav::MotionIssue::None ?
            candidates[i].distanceIssue:candidate.distanceIssue;
        if (candidate.faults < candidates[i].faults) candidates[i] = candidate;
        else { candidates[i].causes |= candidate.causes;
          if(candidate.clean<candidates[i].clean)candidates[i].clean=candidate.clean; }
        candidates[i].distanceAgrees=distanceAgrees;
        candidates[i].distanceIssue=distanceIssue;
        return;
      }
    }
    if (count < MAX_HYPOTHESES * 3) candidates[count++] = candidate;
    else overflow_=true;
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
  Hypothesis candidates_[MAX_HYPOTHESES * 3]{};
  bool overflow_=false;
  uint8_t count_ = 0;
  int8_t direction_ = 0;
  uint32_t physicalVmaxMmS_ = 0;
  Result result_ = Result::Lost;
};

}  // namespace navi_hypothesis
