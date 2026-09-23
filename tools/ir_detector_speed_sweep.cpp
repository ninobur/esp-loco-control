#include "IrMovementDetector.h"
#include <cstdio>
#include <initializer_list>
int main(int argc, char**) {
  std::puts("period_ms,duty_percent,expected,completed,tracking_samples,total_samples");
  for(unsigned period: {20U,40U,100U,250U,500U,1000U,2000U,4000U}) {
    for(unsigned duty: {25U,50U,75U}) {
      ir_movement::Detector d(argc>1);
      unsigned tracking=0,expected=0;
      const unsigned warm=period*(1000/period+5), end=warm+period*20;
      uint64_t start=0;
      for(unsigned i=0;i<end;++i) {
        const bool high=i%period>=period*(100-duty)/100;
        d.sample(i*1000ULL,high?2000:1000);
        if(i==warm)start=d.completed;
        if(i>warm && i%period==0)++expected;
        if(i>warm && d.reason==ir_movement::TRACKING)++tracking;
      }
      std::printf("%u,%u,%u,%llu,%u,%u\n",period,duty,expected,
        (unsigned long long)(d.completed-start),tracking,end-warm-1);
    }
  }
}
