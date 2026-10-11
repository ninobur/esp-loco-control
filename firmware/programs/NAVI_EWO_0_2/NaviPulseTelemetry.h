#pragma once
#include <cstdio>
#include "NaviPulseObservation.h"

namespace navi_pulse {
// 1200-byte MQTT payload ceiling is unchanged. Check snprintf before publishing.
inline int format(char* out,size_t size,const Report& r,uint64_t nowUs,
                  uint64_t freshUs,double mmPerSecPerPkph,bool event,
                  uint32_t rxDrops,uint32_t logDrops,uint32_t mqttDrops) {
  const auto& s=r.pulse;const auto& e=s.event;
  const bool fresh=s.have && nowUs>=s.receivedUs && nowUs-s.receivedUs<=freshUs;
  const bool valid=s.eventValid && fresh && rxDrops==s.queueDrops;
  char mmps[32]="null",pkph[32]="null",legacy[32]="null",age[32]="null";
  if(s.eventValid){std::snprintf(mmps,sizeof(mmps),"%.3f",s.mmps);
    std::snprintf(pkph,sizeof(pkph),"%.3f",s.mmps/mmPerSecPerPkph);}
  if(r.legacyValid)std::snprintf(legacy,sizeof(legacy),"%.3f",r.legacyPkph);
  if(s.have && nowUs>=s.receivedUs)std::snprintf(age,sizeof(age),"%llu",(unsigned long long)((nowUs-s.receivedUs)/1000));
  return std::snprintf(out,size,
    "{\"kind\":\"%s\",\"valid\":%u,\"event_valid\":%u,\"fresh\":%u,\"seq\":%lu,"
    "\"completed\":%llu,\"completed_us\":%llu,\"interval_us\":%llu,\"mmps\":%s,\"pkph\":%s,"
    "\"legacy_valid\":%u,\"legacy_pkph\":%s,\"compared_us\":%llu,\"legacy_boot\":\"%016llx\","
    "\"same_source\":%u,\"coupled\":%u,\"age_ms\":%s,\"received_us\":%llu,"
    "\"boot\":\"%016llx\",\"sid\":%lu,\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\","
    "\"reason\":%u,\"span\":%u,\"flags\":%lu,\"received\":%lu,\"accepted\":%lu,\"invalid\":%lu,"
    "\"discontinuities\":%lu,\"seq_breaks\":%lu,\"pulse_breaks\":%lu,\"time_breaks\":%lu,"
    "\"interval_breaks\":%lu,\"source_changes\":%lu,\"boot_changes\":%lu,\"rx_drop\":%lu,"
    "\"log_drop\":%lu,\"mqtt_drop\":%lu,\"authority\":\"OBSERVATION_ONLY\"}",
    event?"EVENT":"STATUS",unsigned(valid),unsigned(s.eventValid),unsigned(fresh),(unsigned long)e.sequence,
    (unsigned long long)e.completedPulses,(unsigned long long)e.completedUs,(unsigned long long)e.intervalUs,mmps,pkph,
    unsigned(r.legacyValid),legacy,(unsigned long long)r.comparedUs,(unsigned long long)r.legacyBoot,
    unsigned(r.legacySameSource),unsigned(r.coupled),age,(unsigned long long)s.receivedUs,
    (unsigned long long)e.bootId,(unsigned long)e.sid,s.mac[0],s.mac[1],s.mac[2],s.mac[3],s.mac[4],s.mac[5],
    unsigned(e.opticalReason),unsigned(e.span),(unsigned long)s.flags,(unsigned long)s.received,
    (unsigned long)s.accepted,(unsigned long)s.invalid,(unsigned long)s.discontinuities,
    (unsigned long)s.sequenceBreaks,(unsigned long)s.pulseBreaks,(unsigned long)s.timeBreaks,
    (unsigned long)s.intervalBreaks,(unsigned long)s.sourceChanges,(unsigned long)s.bootChanges,
    (unsigned long)rxDrops,(unsigned long)logDrops,(unsigned long)mqttDrops);
}
} // namespace navi_pulse
