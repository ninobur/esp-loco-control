#pragma once
#include <stdint.h>
#include <stddef.h>
#include "NaviDartOperations.h"
#include "NaviIrCredibility.h"
namespace navi_eyes {
struct DartIr {
  uint64_t um=0, receivedUs=0, capturedUs=0, pulses=0, epoch=0, boot=0;
  uint32_t sequence=0;
  bool applicable=false, powered=false;
};
struct DartOpening {
  uint64_t timeUs=0, firstTickUs=0;
  uint64_t manualPauseUs=0;
  DartIr ir;
  uint32_t id=0, serial=0, referenceVersion=0;
  int16_t median=0, reference=0;
  int8_t pole=0;
  uint8_t pwm=0, direction=0;
  bool timingApplicable=true;
};
enum class DartReason : uint8_t {
  Opening, Inapplicable, WrongPolarity, NoCoordinate, Candidate, Replaced,
  Eliminated, Tie, Selected, Boundary, Incomplete, AnchorMismatch,
  MetricChanged, StopRequested, CredibilityEvidence
};
struct DartRecord {
  DartOpening opening;
  uint64_t decisionUs=0, anchorUm=0, errorUm=0, boundaryUm=0, intervalUs=0, departureUs=0;
  int64_t residualUm=0, cumulativeUm=0, priorResidualUm=0;
  uint32_t partnerId=0, generation=0;
  uint8_t target=0, previousMm=0, context=0;
  DartReason reason=DartReason::Opening;
  bool inferred=false, intervalValid=false, residualValid=false;
  DartMetric metric=DartMetric::Distance;
  DartStop stop=DartStop::None;
  IrIntervalClassification credibility=IrIntervalClassification::Unavailable;
  IrGeographicAuthority authority=IrGeographicAuthority::Retained;
  IrCredibilityTransition transition=IrCredibilityTransition::None;
  DartIr intervalStart;
  uint32_t intervalStartId=0, restorationStartId=0;
  uint64_t measuredIntervalUm=0, expectedIntervalUm=0;
  bool anchorAvailable=false, independentIdentity=false;
  uint64_t expected=0, successorExpected=0, metricCoordinate=0, timingOriginUs=0;
  uint32_t timingFromId=0, timingToId=0;
  uint32_t timingLap=0;
  int8_t expectedPole=0;
  uint8_t successorTarget=0;
  // Additive timing diagnostic; the original DART JSON contract stays intact.
  uint64_t fromUs=0,runningIntervalUs=0,manualStopUs=0,departureOffsetUs=0;
  uint32_t manualStops=0,completedLaps=0;
  uint8_t fromContext=0;
  bool timingObserved=false,timingSuitable=false,geographicallyResolved=false;
  bool reassessmentArmed=false,provisionalIr=false,timingStartup=false;
};
// Diagnostic ring only: overflow never enters navigation or measurement state.
struct DartTiming {
  uint64_t fromUs=0, toUs=0, departureUs=0;
  uint32_t fromId=0, toId=0;
  uint8_t fromMm=0, toMm=0, context=0;
  int8_t direction=0;
  bool valid=false;
};
}
