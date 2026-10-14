#pragma once
#include <stdint.h>
#include <stddef.h>
namespace navi_eyes {
enum class DartMetric : uint8_t { Distance, Time, None };
enum class DartStop : uint8_t {
  None, NoApplicableReference, NoTimingHistory, IncompleteEvidence,
  MissedTarget, NoHallBySuccessor, TimingContextChanged,
  FinePositionUnavailable, MetricDisagreement
};
inline const char* dartStopName(DartStop reason) {
  switch(reason) {
    case DartStop::None:return "NONE";
    case DartStop::NoApplicableReference:return "NO_APPLICABLE_REFERENCE";
    case DartStop::NoTimingHistory:return "NO_APPLICABLE_TIMING_HISTORY";
    case DartStop::IncompleteEvidence:return "INCOMPLETE_ACQUISITION";
    case DartStop::MissedTarget:return "MISSED_TARGET_SUCCESSOR_OBSERVED";
    case DartStop::NoHallBySuccessor:return "NO_TARGET_BY_EXPECTED_SUCCESSOR";
    case DartStop::TimingContextChanged:return "TIMING_CONTEXT_CHANGED";
    case DartStop::FinePositionUnavailable:return "STOP_OVERLAY_FINE_POSITION_UNAVAILABLE";
    case DartStop::MetricDisagreement:return "DISTANCE_TIME_IDENTITY_DISAGREEMENT";
  }
  return "UNKNOWN";
}
// These states describe evidence, NOT calibrated physical error limits.
enum class DartCredibility : uint8_t {
  NominalUnverified, ExactAgreement, IsolatedResidual, RepeatedResidualUnresolved,
  CompensatingResiduals, Unavailable, RestorationUnproven
};
struct DartTravel {
  uint32_t intervalUs=0, departureUs=0, fromId=0, toId=0;
  uint16_t observations=0;
  uint8_t context=0;
  bool valid=false;
};
// Latest completed traversal for every directed interval. No heap, eviction by
// diagnostic consumers, PWM integration, map mutation, or fabricated timing.
class DartTravelHistory {
 public:
  const DartTravel& get(uint8_t mm,int8_t direction) const {
    return records_[size_t(direction<0)*171+mm%171];
  }
  void save(uint8_t mm,int8_t direction,uint64_t interval,uint64_t departure,
            uint8_t context,uint32_t fromId,uint32_t toId) {
    if(mm>=171 || (direction!=1 && direction!=-1) || !interval ||
       interval>UINT32_MAX || departure>UINT32_MAX || !context) return;
    auto& r=records_[size_t(direction<0)*171+mm];
    if(r.observations<UINT16_MAX) ++r.observations;
    r.intervalUs=uint32_t(interval); r.departureUs=uint32_t(departure);
    r.fromId=fromId;r.toId=toId;r.context=context;r.valid=true;
  }
 private:
  DartTravel records_[342]{};
};
}
