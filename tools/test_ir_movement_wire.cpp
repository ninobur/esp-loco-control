#include "IrMovementWire.h"
#include <cstdio>
#include <cassert>
static uint16_t crc16(const uint8_t *p,size_t n) {
  uint16_t c=0xffff;
  while(n--){c^=uint16_t(*p++)<<8;for(int i=0;i<8;i++)c=(c&0x8000)?(c<<1)^0x1021:c<<1;}
  return c;
}
int main(){
  ir_movement::Snapshot s;s.bootId=123;s.capturedUs=987654321;
  s.completedPulses=42;s.observedRises=43;s.mmPerPulse=9.652;
  auto w=ir_movement::encode(s,17,420);
  assert(w.nominalUm==405384 && w.distanceValidated==0);
  w.crc=crc16(reinterpret_cast<const uint8_t*>(&w),sizeof(w)-2);
  for(auto c:reinterpret_cast<const uint8_t(&)[sizeof(w)]>(w))std::printf("%02x",c);
  std::puts("");
}
