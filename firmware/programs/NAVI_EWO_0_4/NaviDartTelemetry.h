#pragma once
#include <stdio.h>
#include "NaviDartEvidence.h"
namespace navi_eyes {
inline int formatDart(char* out,size_t size,const DartRecord& r,uint64_t boot) {
  const auto& o=r.opening;
  return snprintf(out,size,
    "{\"v\":1,\"boot\":\"%016llX\",\"reason\":%u,\"id\":%lu,\"serial\":%lu,"
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
    unsigned(o.ir.applicable),(unsigned long long)r.anchorUm,(unsigned long long)r.errorUm,
    (unsigned long long)r.boundaryUm,(unsigned long)r.partnerId,unsigned(r.inferred),unsigned(r.residualValid),
    (long long)r.residualUm,(long long)r.priorResidualUm,(long long)r.cumulativeUm,
    (unsigned long long)r.intervalUs,(unsigned long long)r.departureUs,unsigned(r.intervalValid),unsigned(r.context));
}
}
