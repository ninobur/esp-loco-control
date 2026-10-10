#include "../NaviSyncRecorder.h"
#include <cstdio>
#include <cstring>
using namespace navi_sync;
template<class T> static void emit(const T& wire) {
  const uint32_t length=sizeof(wire);
  std::fwrite(&length,sizeof(length),1,stdout); std::fwrite(&wire,sizeof(wire),1,stdout);
}
int main() {
  Recorder r; if(!r.storageReady()) return 1;
  r.begin(9950012,7,42);
  Context received; received.navMm=12; received.navDir=1; received.flags=CTX_HOLD;
  ir_movement::WireSnapshot w; w.bootId=99; w.sequence=10;
  r.addIr(100000,nullptr,1,w,0,0,0,0,received);
  IrWire iw; if(!r.popIr(iw)) return 2; emit(iw);
  for(unsigned i=0;i<CONSUMPTION_ITEMS;++i) {
    ConsumptionItem item{}; item.id=100+i; item.decisionUs=200000+i;
    item.observationUs=100000+i; item.serial=10+i;
    item.kind=uint8_t(i?InputKind::Hall:InputKind::Ir); item.pwm=i?40:0;
    item.navMm=12; item.target=13; r.addConsumption(item,received);
  }
  ConsumptionWire cw; if(!r.popConsumption(cw)) return 3; emit(cw);
  NaviSnapshot n{}; n.tUs=200031; n.consumptionId=131; n.irSequence=10;
  n.hallSerial=41; n.kind=4; n.mm=13; n.target=13;
  r.addNavi(n,received); NaviWire nw; if(!r.popNavi(nw)) return 4; emit(nw);
  ActionSnapshot a{}; a.tUs=200100; a.lastConsumptionId=131; a.commandOrder=2;
  a.commandReceivedUs=90000; a.kind=uint8_t(ActionKind::RequestedPwm); a.targetPwm=60;
  std::strcpy(a.topic,"station"); std::strcpy(a.payload,"ZONE");
  r.addAction(a,received); ActionWire aw; if(!r.popAction(aw)) return 5; emit(aw);
  // Every bounded new ring exposes overflow independently.
  for(unsigned i=0;i<(CONSUMPTION_BATCHES+1)*CONSUMPTION_ITEMS;++i) {
    ConsumptionItem item{}; item.id=i; r.addConsumption(item,received);
  }
  for(unsigned i=0;i<ACTION_RECORDS+1;++i) r.addAction(a,received);
  return r.consumptionDrops()==1 && r.actionDrops()==1 ? 0 : 6;
}
