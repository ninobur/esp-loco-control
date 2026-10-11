#include "../NaviIrEvidence.h"
#include <cstdio>
#include <cinttypes>
int main(){
  ngr_evidence::IrPulseEvidence monitor;
  uint64_t serial,time,boot,priorBoot=0;unsigned pwm,valid;
  uint64_t events=0,normal=0,anomalies=0,compatible=0,gaps=0;
  while(std::scanf("%" SCNu64 " %" SCNu64 " %" SCNu64 " %u %u",&boot,&serial,&time,&pwm,&valid)==5){
    ++events;
    if(boot!=priorBoot || !valid){monitor.reset();priorBoot=boot;++gaps;if(!valid)continue;}
    const auto a=monitor.observe(serial,time,pwm);
    using F=ngr_evidence::PulseFinding;
    normal+=a.finding==F::Normal;
    anomalies+=a.finding==F::TimingAnomaly;
    compatible+=a.finding==F::BracketedMissingCompatible;
    gaps+=a.finding==F::ObservationGap;
  }
  // No field log here distinguishes wheel slowdown from uncounted cycles.
  std::printf("{\"events\":%" PRIu64 ",\"normal\":%" PRIu64 ",\"timing_questions\":%" PRIu64 ",\"missing_compatible\":%" PRIu64 ",\"observation_gaps\":%" PRIu64 ",\"proven_physical_missed_pulses\":0,\"dna_replay\":false}\n",events,normal,anomalies,compatible,gaps);
}
