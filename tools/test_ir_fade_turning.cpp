// Contrast fading toward the bright plateau while the wheel keeps turning
// (a model of a sun/shade compression). Runs against whichever detector the
// build stages: R2 (firmware/common) or the R3 review proposal. Reports how
// many 10 Hz snapshots claim a stationary wheel (SIGNAL_STALE) while turning.
//
// Hard gate for both: no such snapshot while the remaining modulation is at
// least 70 counts. Below that, one optical channel cannot separate a turning
// wheel from a still one; R2 happens to refuse (it erases its reference while
// fading), the R3 proposal does not. See the review document.
#include "IrMovementDetector.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
using namespace ir_movement;
int main(){
  int failures=0; const double PI=3.14159265358979323846;
  for(double endAmp: {400.0,100.0,70.0,40.0,20.0}){
    Detector d(false,true); uint64_t us=0; uint32_t rng=12345; double ph=0; unsigned staleSnaps=0;
    auto s=[&](double lvl,bool turning){ us+=1000; rng=rng*1664525u+1013904223u;
      int n=int(rng>>28)-8; if((rng>>8)%1000<3) n+=((rng>>20)&1)?260:-260;
      d.sample(us,(uint16_t)std::lround(lvl+n)); if(turning&&us%100000==0&&d.reason==SIGNAL_STALE)++staleSnaps; };
    for(int i=0;i<4000;++i){ ph+=2*PI/150; s(1500-500*std::cos(ph),false); }
    for(unsigned i=0;i<6000;++i){ const double a=500-(500-endAmp/2)*std::min(1.0,i/4000.0); ph+=2*PI/200; s(2000-a*(1+std::cos(ph)),true); }
    const bool hard=endAmp>=70, ok=staleSnaps==0;
    std::printf("%s fade to %3g counts at 5 Hz: %u stationary-ready snapshots while turning\n",ok?"PASS":hard?"FAIL":"INFO",endAmp,staleSnaps);
    if(hard&&!ok)++failures;
  }
  return failures?1:0;
}
