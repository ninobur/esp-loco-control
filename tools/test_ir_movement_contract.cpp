#include "IrMovementContract.h"
#include <cassert>
#include <cstdio>
using namespace ir_movement;
int main() {
  Snapshot a,b;
  a.bootId=b.bootId=7; a.calibrationId=b.calibrationId=1;
  a.mmPerPulse=b.mmPerPulse=9.652; a.reason=b.reason=TRACKING;
  a.capturedUs=1000; b.capturedUs=1001000;
  b.completedPulses=b.observedRises=30;
  assert(!between(a,b).bounded());
  assert(isinf(between(a,b).maxMm));
  // Synthetic budget only: not evidence for the physical sensor.
  ErrorBudget e; e.evidenceId=1; e.calibrationId=1; e.maxIntervalUs=2000000;
  e.maxObservedPulses=100; e.endpointPhaseMm=9.652; e.maxMissed=1; e.maxExtra=1;
  auto r=between(a,b,e); assert(r.bounded());
  assert(r.minMm<289.56 && r.maxMm>289.56);
  assert(!couldReach(r,0,20)); assert(couldReach(r,280,310));
  assert(!couldReach(r,580,620));
  b.unreliableSamples=1; assert(between(a,b,e).issues&INTERRUPTED);
  assert(couldReach(between(a,b,e),0,20));
  b.unreliableSamples=0; b.bootId=8; assert(between(a,b,e).issues&RESET);
  b.bootId=7; b.capturedUs=0; assert(between(a,b,e).issues&TIME_ORDER);
  b.capturedUs=1001000; b.reason=SIGNAL_STALE;
  assert(between(a,b,e).issues&OPTICAL_INVALID);
  Measurement m(9,1,9.652);
  for(unsigned i=0;i<3000;++i) m.sample(i*1000ULL,i%100<50?1000:2000);
  auto before=m.snapshot(); m.sample(3000000,4095);
  for(unsigned i=3001;i<6000;++i) m.sample(i*1000ULL,i%100<50?1000:2000);
  auto after=m.snapshot(); assert(after.reason==TRACKING);
  assert(between(before,after,e).issues&INTERRUPTED);
  assert(after.inferredAdded==0 && after.inferredRemoved==0);
  puts("PASS unvalidated bounds, synthetic marker windows, resets, silence and hidden failure");
}
