#include "IrMovementDetector.h"
#include <cassert>
#include <cstdio>
using namespace ir_movement;
struct Rig {
  Detector d{false,true};uint64_t us=0;
  void hold(unsigned raw,unsigned ms) {
    for(unsigned i=0;i<ms;++i){us+=1000;d.sample(us,raw);}
  }
  void prime() {
    for(unsigned i=0;i<20;++i){hold(1000,100);hold(2000,100);}
    hold(1000,100);assert(d.reason==TRACKING);
  }
};
int main() {
  for(unsigned high=0;high<2;++high) {
    Rig r;r.prime();if(high)r.hold(2000,100);
    const auto count=r.d.completed,aborts=r.d.aborts;
    r.hold(high?2000:1000,120000);
    assert(r.d.completed==count && r.d.aborts==aborts);
    assert(r.d.reason==SIGNAL_STALE);
    if(high){r.hold(1000,100);assert(r.d.completed==count+1);}
    else {r.hold(2000,100);r.hold(1000,100);assert(r.d.completed==count+1);}
    const auto start=r.d.completed;
    for(unsigned n=0;n<10;++n){r.hold(2000,4000);r.hold(1000,4000);}
    assert(r.d.completed==start+10 && r.d.aborts==aborts);
  }
  Rig cold;cold.hold(1000,10000);
  assert(cold.d.completed==0 && cold.d.reason==INADEQUATE_CONTRAST);
  Rig noise;for(unsigned i=0;i<10000;++i)noise.hold(1000+i%15,1);
  assert(noise.d.completed==0 && noise.d.reason==INADEQUATE_CONTRAST);
  Rig mid;mid.prime();mid.hold(1500,1500);
  assert(mid.d.reason==INADEQUATE_CONTRAST);
  Rig shifted;shifted.prime();shifted.hold(3000,1500);
  assert(shifted.d.reason==INADEQUATE_CONTRAST);
  Rig sat;sat.prime();sat.hold(4095,1);assert(sat.d.reason==SATURATION);
  sat.hold(1000,2000);assert(sat.d.reason==INADEQUATE_CONTRAST);
  Rig gap;gap.prime();gap.us+=5000;gap.hold(1000,1000);
  assert(gap.d.gaps==1 && gap.d.reason==INADEQUATE_CONTRAST);
  // Electrical faults mimicking a learned optical plateau are indistinguishable
  // from a stationary wheel with this single channel. No universal fault claim.
  std::puts("PASS learned low/high 120s holds, 4s half-cycles, exact restart counts, startup/noise/midband/shift/saturation/gap");
}
