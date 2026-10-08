#pragma once
#include <cstdio>
#include "NaviPulseObservation.h"
namespace navi_pulse {
inline int format(char* out,size_t size,const State& s,uint64_t nowUs,uint64_t freshUs) {
  const bool fresh=s.have&&nowUs>=s.receivedUs&&nowUs-s.receivedUs<=freshUs;
  char age[32]="null",speed[32]="null";
  if(s.have&&nowUs>=s.receivedUs)std::snprintf(age,sizeof(age),"%llu",(unsigned long long)((nowUs-s.receivedUs)/1000));
  if(s.speedValid)std::snprintf(speed,sizeof(speed),"%.3f",s.averageMmps);
  return std::snprintf(out,size,
    "{\"accepted\":%lu,\"current\":%u,\"fresh\":%u,\"boot\":\"%016llx\",\"sid\":%lu,\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"seq\":%lu,"
    "\"completed\":%llu,\"nominal_um\":%llu,\"completed_us\":%llu,\"distance_advanced_um\":%llu,"
    "\"physical_interval_us\":%llu,\"average_mmps\":%s,\"age_ms\":%s,\"reason\":%u,\"missing\":%lu,"
    "\"discontinuities\":%lu,\"invalid\":%lu,\"source_mismatches\":%lu,\"boot_changes\":%lu,\"order_faults\":%lu,\"flags\":%lu,\"authority\":\"OBSERVATION_ONLY\"}",
    (unsigned long)s.accepted,unsigned(s.current),unsigned(fresh),(unsigned long long)s.event.bootId,
    (unsigned long)s.event.sid,s.mac[0],s.mac[1],s.mac[2],s.mac[3],s.mac[4],s.mac[5],
    (unsigned long)s.event.sequence,(unsigned long long)s.event.completedPulses,(unsigned long long)s.event.nominalUm,
    (unsigned long long)s.event.completedUs,(unsigned long long)s.distanceAdvancedUm,
    (unsigned long long)s.physicalIntervalUs,speed,age,unsigned(s.event.opticalReason),(unsigned long)s.missing,
    (unsigned long)s.discontinuities,(unsigned long)s.invalid,(unsigned long)s.sourceMismatches,
    (unsigned long)s.bootChanges,(unsigned long)s.orderFaults,(unsigned long)s.flags);
}
} // namespace navi_pulse
