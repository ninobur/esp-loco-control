#pragma once

#include <cstdio>
#include "NaviPulseObservation.h"

namespace navi_pulse {
inline int format(char* out,size_t size,const Report& r,uint64_t nowUs,
                  uint64_t freshUs,double mmPerSecPerPkph,uint32_t mqttDrops) {
  const auto& s=r.pulse;const auto& e=s.event;
  const bool fresh=s.have&&nowUs>=s.receivedUs&&nowUs-s.receivedUs<=freshUs;
  char mmps[32]="null",pkph[32]="null",legacy[32]="null",age[32]="null";
  if(s.speedValid){std::snprintf(mmps,sizeof(mmps),"%.3f",s.averageMmps);
    std::snprintf(pkph,sizeof(pkph),"%.3f",s.averageMmps/mmPerSecPerPkph);}
  if(r.legacyValid)std::snprintf(legacy,sizeof(legacy),"%.3f",r.legacyPkph);
  if(s.have&&nowUs>=s.receivedUs)std::snprintf(age,sizeof(age),"%llu",
    (unsigned long long)((nowUs-s.receivedUs)/1000));
  return std::snprintf(out,size,
    "{\"valid\":%u,\"fresh\":%u,\"seq\":%lu,\"completed\":%llu,"
    "\"completed_us\":%llu,\"distance_advanced_um\":%llu,\"physical_interval_us\":%llu,"
    "\"mmps\":%s,\"pkph\":%s,\"legacy_valid\":%u,\"legacy_pkph\":%s,"
    "\"same_source\":%u,\"coupled\":%u,\"age_ms\":%s,\"boot\":\"%016llx\","
    "\"flags\":%lu,\"received\":%lu,\"accepted\":%lu,\"invalid\":%lu,"
    "\"discontinuities\":%lu,\"missing\":%lu,\"source_mismatches\":%lu,"
    "\"boot_changes\":%lu,\"order_faults\":%lu,\"queue_replacements\":%lu,"
    "\"mqtt_drop\":%lu,\"authority\":\"OBSERVATION_ONLY\"}",
    unsigned(s.speedValid&&fresh),unsigned(fresh),(unsigned long)e.sequence,
    (unsigned long long)e.completedPulses,(unsigned long long)e.completedUs,
    (unsigned long long)s.distanceAdvancedUm,(unsigned long long)s.physicalIntervalUs,
    mmps,pkph,unsigned(r.legacyValid),legacy,unsigned(r.legacySameSource),unsigned(r.coupled),age,
    (unsigned long long)e.bootId,(unsigned long)s.flags,(unsigned long)s.received,
    (unsigned long)s.accepted,(unsigned long)s.invalid,(unsigned long)s.discontinuities,
    (unsigned long)s.missing,(unsigned long)s.sourceMismatches,(unsigned long)s.bootChanges,
    (unsigned long)s.orderFaults,(unsigned long)s.queueReplacements,(unsigned long)mqttDrops);
}
} // namespace navi_pulse
