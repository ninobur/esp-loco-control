#include "IrMovementDetector.h"
#include <cassert>
#include <cstdio>
int main() {
  ir_movement::Detector d(true);
  for(unsigned i=0;i<20000;++i)d.sample(i*1000ULL,i%2000<1000?1000:2000);
  auto before=d.completed;
  for(unsigned i=20000;i<40000;++i)d.sample(i*1000ULL,i%2000<1000?1000:2000);
  assert(d.completed-before==10);
  // The identical ADC history could be light modulation on a stopped wheel.
  // The algorithm cannot claim wheel-motion truth from this test alone.
  before=d.completed;
  for(unsigned i=40000;i<50000;++i)d.sample(i*1000ULL,2000);
  assert(d.completed==before);
  assert(d.reason!=ir_movement::TRACKING);
  ir_movement::Detector noise(true);
  for(unsigned i=0;i<10000;++i)noise.sample(i*1000ULL,2646+(i%15)-7);
  assert(noise.completed==0);
  std::puts("PASS slow cycles retained; stopped plateau and stationary noise do not grow counts");
}
