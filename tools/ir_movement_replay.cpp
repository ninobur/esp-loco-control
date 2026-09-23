#include "IrMovementDetector.h"
#include <cstdio>
#include <cstdint>

// Input: boot-id sample-index raw-ADC. Output: per-boot completed/rise counts.
int main(int argc, char**) {
  const bool retainPhase=argc>1;
  ir_movement::Detector detector(retainPhase);
  unsigned long long boot,seq,lastBoot=0;
  unsigned raw;
  bool have=false;
  uint64_t samples=0,tracking=0;
  auto report=[&]() {
    if(have) std::printf("boot=%llu samples=%llu completed=%llu rises=%llu gaps=%llu tracking=%llu\n",
      lastBoot,(unsigned long long)samples,(unsigned long long)detector.completed,
      (unsigned long long)detector.rises,(unsigned long long)detector.gaps,
      (unsigned long long)tracking);
  };
  while(std::scanf("%llu %llu %u",&boot,&seq,&raw)==3) {
    if(raw>4095) return 2;
    if(!have || boot!=lastBoot) {
      report(); detector=ir_movement::Detector(retainPhase); samples=tracking=0;
      lastBoot=boot; have=true;
    }
    detector.sample(seq*1000,raw); ++samples;
    if(detector.reason==ir_movement::TRACKING) ++tracking;
  }
  report();
}
