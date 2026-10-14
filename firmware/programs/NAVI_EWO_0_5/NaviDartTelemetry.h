#pragma once
#include <stdio.h>
#include "NaviDartEvidence.h"
#include "../../common/IrMovementWire.h"
namespace navi_eyes {
inline int formatDart(char* out,size_t size,const DartRecord& r,uint64_t boot) {
  const auto& o=r.opening;
  const int base=snprintf(out,size,
    "{\"v\":2,\"boot\":\"%016llX\",\"reason\":%u,\"id\":%lu,\"serial\":%lu,"
    "\"open_us\":%llu,\"first_tick_us\":%llu,\"decision_us\":%llu,\"gen\":%lu,"
    "\"target\":%u,\"previous_mm\":%u,\"pole\":%d,\"median\":%d,\"ref\":%d,\"ref_v\":%lu,"
    "\"pwm\":%u,\"dir\":%u,\"ir_um\":%llu,\"rx_us\":%llu,\"tx_us\":%llu,"
    "\"ir_seq\":%lu,\"pulses\":%llu,\"epoch\":%llu,\"ir_boot\":%llu,\"applicable\":%u,"
    "\"anchor_um\":%llu,\"error_um\":%llu,\"boundary_um\":%llu,\"partner\":%lu,"
    "\"inferred\":%u,\"residual_valid\":%u,\"residual_um\":%lld,\"prior_residual_um\":%lld,\"cumulative_um\":%lld,"
    "\"interval_us\":%llu,\"departure_us\":%llu,\"interval_valid\":%u,\"context\":%u}",
    (unsigned long long)boot,unsigned(r.reason),(unsigned long)o.id,(unsigned long)o.serial,
    (unsigned long long)o.timeUs,(unsigned long long)o.firstTickUs,(unsigned long long)r.decisionUs,
    (unsigned long)r.generation,unsigned(r.target),unsigned(r.previousMm),int(o.pole),int(o.median),
    int(o.reference),(unsigned long)o.referenceVersion,unsigned(o.pwm),unsigned(o.direction),
    (unsigned long long)o.ir.um,(unsigned long long)o.ir.receivedUs,(unsigned long long)o.ir.capturedUs,
    (unsigned long)o.ir.sequence,(unsigned long long)o.ir.pulses,(unsigned long long)o.ir.epoch,(unsigned long long)o.ir.boot,
    unsigned(o.ir.applicable),(unsigned long long)r.anchorUm,(unsigned long long)(r.metric==DartMetric::Distance?r.errorUm:0),
    (unsigned long long)(r.metric==DartMetric::Distance?r.boundaryUm:0),(unsigned long)r.partnerId,unsigned(r.inferred),unsigned(r.residualValid),
    (long long)r.residualUm,(long long)r.priorResidualUm,(long long)r.cumulativeUm,
    (unsigned long long)r.intervalUs,(unsigned long long)r.departureUs,unsigned(r.intervalValid),unsigned(r.context));
  if(base<=0 || size_t(base)>=size) return base;
  const auto& a=r.intervalStart;
  const int added=snprintf(out+base-1,size-size_t(base)+1,
    ",\"method\":%u,\"unit\":\"%s\",\"expected\":%llu,\"successor_expected\":%llu,"
    "\"coordinate\":%llu,\"anchor_available\":%u,\"ir_authority\":%u,\"ir_class\":%u,"
    "\"transition\":%u,\"stop\":%u,\"expected_pole\":%d,\"measured_interval_um\":%llu,"
    "\"expected_interval_um\":%llu,\"absolute_discrepancy_um\":%llu,\"discrepancy_pulses\":%.6f,"
    "\"restore_ref\":%lu,\"independent_identity\":%u,\"timing_origin_us\":%llu,\"timing_lap\":%lu,"
    "\"timing_from_id\":%lu,\"timing_to_id\":%lu,\"from_id\":%lu,\"from_um\":%llu,"
    "\"from_pulses\":%llu,\"from_rx_us\":%llu,\"from_tx_us\":%llu,\"from_seq\":%lu,"
    "\"from_epoch\":%llu,\"from_boot\":%llu,\"from_applicable\":%u,\"successor_target\":%u,\"error\":%llu,\"boundary\":%llu}",
    unsigned(r.metric),r.metric==DartMetric::Time?"us":"um",
    (unsigned long long)r.expected,(unsigned long long)r.successorExpected,
    (unsigned long long)r.metricCoordinate,unsigned(r.anchorAvailable),unsigned(r.authority),unsigned(r.credibility),
    unsigned(r.transition),unsigned(r.stop),int(r.expectedPole),(unsigned long long)r.measuredIntervalUm,
    (unsigned long long)r.expectedIntervalUm,(unsigned long long)(r.residualUm<0?uint64_t(-(r.residualUm+1))+1:uint64_t(r.residualUm)),
    double(r.residualUm)/ir_movement::kInstalledPitchUm,(unsigned long)r.restorationStartId,unsigned(r.independentIdentity),
    (unsigned long long)r.timingOriginUs,(unsigned long)r.timingLap,
    (unsigned long)r.timingFromId,(unsigned long)r.timingToId,(unsigned long)r.intervalStartId,
    (unsigned long long)a.um,(unsigned long long)a.pulses,(unsigned long long)a.receivedUs,
    (unsigned long long)a.capturedUs,(unsigned long)a.sequence,(unsigned long long)a.epoch,
    (unsigned long long)a.boot,unsigned(a.applicable),unsigned(r.successorTarget),
    (unsigned long long)r.errorUm,(unsigned long long)r.boundaryUm);
  return added<0?added:base-1+added;
}
}
