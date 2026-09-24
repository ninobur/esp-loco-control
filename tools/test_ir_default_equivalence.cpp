// Supply -DIR_BASELINE_HEADER='"/absolute/path/to/pre-change/header.h"'.
// Baseline used for this revision: 1de06ae^:firmware/common/IrMovementDetector.h.
#define ir_movement ir_movement_baseline
#include IR_BASELINE_HEADER
#undef ir_movement
#include "IrMovementDetector.h"
#include <cassert>
#include <cstdio>
int main() {
  for(unsigned phase=0;phase<2;++phase) {
    ir_movement_baseline::Detector old(phase);
    ir_movement::Detector now(phase,false);
    uint32_t random=12345;uint64_t us=0;
    for(unsigned i=0;i<2000000;++i) {
      random=random*1664525U+1013904223U;
      us+=(random%10007==0?6000:1000);
      unsigned raw=((i/100)%2?2000:1000)+(random%31);
      if(i%100000>=50000)raw=1000+random%15;
      if(i%500000>=400000)raw=random%4096;
      if(random%9991==0)raw=4095;
      old.sample(us,raw);now.sample(us,raw);
      assert(old.completed==now.completed && old.rises==now.rises);
      assert(old.aborts==now.aborts && old.gaps==now.gaps && old.saturated==now.saturated);
      assert(old.low==now.low && old.high==now.high);
      assert(old.rise==now.rise && old.fall==now.fall && old.inPulse()==now.inPulse());
      assert(unsigned(old.reason)==unsigned(now.reason));
    }
  }
  std::puts("PASS 4000000 differential samples, both retainPhase modes, default stationary retention disabled");
}
