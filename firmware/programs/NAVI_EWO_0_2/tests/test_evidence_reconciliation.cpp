#include "../NaviIntegratedCore.h"
#include <cassert>

using namespace navi_eyes;

static ir_movement::WireSnapshot ir(uint32_t sequence, uint64_t pulses, uint64_t us) {
  ir_movement::WireSnapshot w{}; w.magic=0x4952; w.version=1; w.type=5;
  w.bootId=7; w.sequence=sequence; w.capturedUs=us; w.completedPulses=pulses;
  w.observedRises=pulses; w.pitchUm=ir_movement::kInstalledPitchUm;
  w.nominalUm=pulses*uint64_t(w.pitchUm); return w;
}
static HallSample hall(uint32_t serial,uint64_t us,int16_t raw,uint8_t pwm=90) {
  HallSample h{};h.sampleSerial=serial;h.timestampUs=us;h.raw=raw;h.pwm=pwm;h.direction=1;return h;
}
int main() {
  NaviIntegratedCore n;
  n.observeHall(hall(1,1000,100));                 // provisional reference
  n.observeIr(ir(1,0,1000),2000,90);
  n.declare(0,1,3000);
  n.observeIr(ir(2,1,20000),21000,90);
  n.observeHall(hall(2,22000,171));
  n.observeHall(hall(3,23000,171));                 // 70x2 encounter
  EwoEvent e{}; bool saw=false; while(n.takeEvent(e)) saw|=e.kind==EwoEventKind::HallEncounter;
  assert(saw);
  // A fixed-PWM long pulse followed by normal timing is recorded as two
  // suspected equivalents and excludes IR for this interval without repair.
  const uint8_t mac[6]={1,2,3,4,5,6};
  n.observePulse(100000,1,7,mac,90,100000);
  n.observePulse(120000,2,7,mac,90,120000);
  n.observePulse(160000,3,7,mac,90,160000);
  n.observePulse(180000,4,7,mac,90,180000);
  n.observePulse(220000,5,7,mac,90,220000);
  n.observePulse(240000,6,7,mac,90,240000);
  assert(n.irQuestionable());
  return 0;
}
