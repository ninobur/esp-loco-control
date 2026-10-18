#pragma once
#include <stdint.h>
#include <math.h>
#include "IrMovementDetector.h"

namespace ir_movement {
enum RangeIssue : uint32_t {
  RESET=1, TIME_ORDER=2, COUNTER_ORDER=4, OPTICAL_INVALID=8,
  INTERRUPTED=16, UNVALIDATED=32, CALIBRATION=64, OUTSIDE_ENVELOPE=128
};
struct Snapshot {
  uint64_t bootId=0, capturedUs=0, observedRises=0, completedPulses=0;
  uint64_t inferredAdded=0, inferredRemoved=0, unreliableSamples=0;
  uint64_t saturatedSamples=0, sampleGaps=0, openAborts=0;
  uint32_t calibrationId=0;
  double mmPerPulse=0;
  Reason reason=PRIMING;
  double nominalDistanceMm() const { return completedPulses*mmPerPulse; }
};

// Only a separately measured and approved operating envelope may populate this.
// No production validated budget is provided by this implementation.
struct ErrorBudget {
  uint32_t evidenceId=0, calibrationId=0;
  uint64_t maxIntervalUs=0, maxObservedPulses=0;
  uint32_t maxMissed=0, maxExtra=0;
  double endpointPhaseMm=0, scaleErrorFraction=0;
};
struct DistanceRange {
  double nominalMm=0, minMm=0, maxMm=INFINITY;
  uint32_t issues=UNVALIDATED;
  bool bounded() const { return issues==0; }
};

inline DistanceRange between(const Snapshot& a, const Snapshot& b,
                             const ErrorBudget& budget=ErrorBudget{}) {
  DistanceRange r;
  r.issues=0;
  if (!a.bootId || a.bootId!=b.bootId) r.issues|=RESET;
  if (b.capturedUs<=a.capturedUs) r.issues|=TIME_ORDER;
  if (b.completedPulses<a.completedPulses || b.observedRises<a.observedRises)
    r.issues|=COUNTER_ORDER;
  if (a.reason!=TRACKING || b.reason!=TRACKING) r.issues|=OPTICAL_INVALID;
  if (a.unreliableSamples!=b.unreliableSamples || a.sampleGaps!=b.sampleGaps ||
      a.saturatedSamples!=b.saturatedSamples || a.openAborts!=b.openAborts)
    r.issues|=INTERRUPTED;
  if (!a.calibrationId || a.calibrationId!=b.calibrationId ||
      !isfinite(a.mmPerPulse) || a.mmPerPulse<=0 || a.mmPerPulse!=b.mmPerPulse)
    r.issues|=CALIBRATION;
  if (!budget.evidenceId || budget.calibrationId!=a.calibrationId ||
      a.inferredAdded || b.inferredAdded || a.inferredRemoved || b.inferredRemoved)
    r.issues|=UNVALIDATED;
  const uint64_t n=b.completedPulses>=a.completedPulses ? b.completedPulses-a.completedPulses : 0;
  if (!(r.issues&(RESET|COUNTER_ORDER|CALIBRATION))) r.nominalMm=n*a.mmPerPulse;
  if (b.capturedUs-a.capturedUs>budget.maxIntervalUs || n>budget.maxObservedPulses ||
      !isfinite(budget.endpointPhaseMm) || budget.endpointPhaseMm<a.mmPerPulse ||
      !isfinite(budget.scaleErrorFraction) || budget.scaleErrorFraction<0 ||
      budget.scaleErrorFraction>=1) r.issues|=OUTSIDE_ENVELOPE;
  if (r.issues) return r;
  const double lowerPulses=n>budget.maxExtra ? double(n-budget.maxExtra) : 0;
  const double lower=lowerPulses*a.mmPerPulse*(1-budget.scaleErrorFraction)-budget.endpointPhaseMm;
  r.minMm=lower>0?lower:0;
  r.maxMm=(double(n)+budget.maxMissed)*a.mmPerPulse*(1+budget.scaleErrorFraction)+budget.endpointPhaseMm;
  return r;
}

// Route code supplies travel windows, including Hall sensing extent and direction.
// This predicate only rejects impossible distances; it never assigns identity.
inline bool couldReach(const DistanceRange& r, double routeMinMm, double routeMaxMm) {
  if (!r.bounded() || !isfinite(routeMinMm) || !isfinite(routeMaxMm) ||
      routeMinMm<0 || routeMaxMm<routeMinMm) return true;
  return r.maxMm>=routeMinMm && r.minMm<=routeMaxMm;
}

class Measurement {
 public:
  Measurement(uint64_t boot, uint32_t calibration, double pitch,bool retainStationary=false)
    : detector_(false,retainStationary) {
    state_.bootId=boot; state_.calibrationId=calibration; state_.mmPerPulse=pitch;
  }
  void sample(uint64_t us, uint16_t raw) {
    detector_.sample(us,raw);
    state_.capturedUs=us; state_.reason=detector_.reason;
    state_.observedRises=detector_.rises; state_.completedPulses=detector_.completed;
    state_.saturatedSamples=detector_.saturated; state_.sampleGaps=detector_.gaps;
    state_.openAborts=detector_.aborts;
    if (state_.reason!=TRACKING) ++state_.unreliableSamples;
  }
  Snapshot snapshot() const { return state_; }
  const Detector& detector() const { return detector_; }
 private:
  Detector detector_;
  Snapshot state_;
};
} // namespace ir_movement
