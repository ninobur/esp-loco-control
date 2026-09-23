#include "IrMovementDetector.h"
#include <cassert>
#include <cstdio>
using ir_movement::Detector;
int main() {
  Detector stationary;
  for(unsigned i=0;i<10000;++i) stationary.sample(i*1000ULL,2646+(i%15)-7);
  assert(stationary.completed==0);
  assert(stationary.reason==ir_movement::INADEQUATE_CONTRAST);
  Detector wheel;
  for(unsigned i=0;i<10000;++i) wheel.sample(i*1000ULL,i%100<50?1000:2000);
  assert(wheel.completed>=97 && wheel.completed<=99);
  auto count=wheel.completed;
  for(unsigned i=10000;i<20000;++i) wheel.sample(i*1000ULL,2000);
  assert(wheel.completed==count);
  assert(wheel.reason!=ir_movement::TRACKING);
  wheel.sample(21000000,4095);
  assert(wheel.reason==ir_movement::SATURATION && wheel.gaps==1);
  assert(wheel.completed==count);
  std::puts("PASS stationary, completed cycles, plateau silence, saturation and gap");
}
