"""Compile review probes from the submitted functions without editing firmware.

This mocks only RTOS queue operations, clocks, locks, and the recorder sink.
MovementSource and IrHealthMonitor are the real historical headers. The probes
demonstrate deterministic software behavior, not field frequency or RF behavior.
"""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent
REPO = Path('/Users/davidbrown/esp-loco-control')
BASE = REPO / 'firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH'
SOURCE = BASE.parent / 'NAVI_COHERENCE_0_6_IR_SYNC_TEST.ino'
source = SOURCE.read_text()
snapshot_types = source[source.index('struct SyncPoint {'):source.index('struct HallDecisionMsg')]
rx_type = re.search(r'struct IrRx \{[^\n]+', source).group(0)
receive_functions = source[source.index('static void onIr('):source.index('static void onMqtt(')]
hall_capture = source[source.index('      pending.capturedUs=esp_timer_get_time();'):source.index('      pending.raw=opening.rawAtDetect;')]
diagnostic = source[source.index('    const auto a=j.type5AtOpening,b=j.type6AtOpening;'):source.index('    const auto point=movement.at(j.capturedUs,esp_timer_get_time());')]

prefix = r'''
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <deque>
#include <new>
#include "IrHealthMonitor.h"
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
static uint64_t clockUs;
static uint64_t esp_timer_get_time() { return clockUs; }
struct esp_now_recv_info_t { uint8_t src_addr[6]; };
'''
stubs = r'''
struct Queue { std::deque<IrRx> items; } queue;
static Queue* irQ=&queue;
static const int pdTRUE=1;
static int xQueueSend(Queue* q,const IrRx* p,int) {
  if(q->items.size()==24)return 0;
  q->items.push_back(*p);return pdTRUE;
}
static int xQueueReceive(Queue* q,IrRx* p,int) {
  if(q->items.empty())return 0;
  *p=q->items.front();q->items.pop_front();return pdTRUE;
}
static ngr_nav::MovementSource movement;
static ngr_nav::IrHealthMonitor irHealth;
static uint32_t irQueueDrops=0,handledIrDrops=0,seenFrames=0;
static uint8_t lastSeenMac[6],actualPwm=90,commandedPwm=90;
struct Recorder { template<class... T> void addIr(T...){} } naviSyncRecorder;
static int naviSyncContext(){return 0;}
struct CapturedHall { uint32_t serial=1; uint64_t capturedUs=0; SyncPoint type5AtOpening{},type6AtOpening{}; };
struct SerialSink {
  char line[512]{};
  template<class... T> void printf(const char* fmt,T... args){std::snprintf(line,sizeof(line),fmt,args...);}
} Serial;
'''
helpers = r'''
static const uint8_t MAC[6]={2,3,4,5,6,7};
static const uint8_t FOREIGN[6]={2,3,4,5,6,8};
static void reset() {
  queue.items.clear();irQueueDrops=handledIrDrops=seenFrames=0;
  syncType5={};syncType6={};syncType6Gap=syncType6Bad=0;
  movement=ngr_nav::MovementSource{};
  irHealth.~IrHealthMonitor();new (&irHealth) ngr_nav::IrHealthMonitor{};
  movement.pair(MAC);irHealth.pair(MAC,0);clockUs=1000000;
}
static void send5(uint32_t seq,uint64_t pulses) {
  ir_movement::WireSnapshot w{};
  w.bootId=7;w.sequence=seq;w.capturedUs=clockUs-100;
  w.completedPulses=w.observedRises=pulses;
  w.nominalUm=pulses*w.pitchUm;w.opticalReason=ir_movement::TRACKING;
  w.crc=ngr_nav::movementCrc(reinterpret_cast<const uint8_t*>(&w),108);
  esp_now_recv_info_t info{};std::memcpy(info.src_addr,MAC,6);
  onIr(&info,reinterpret_cast<const uint8_t*>(&w),sizeof(w));
}
static void send6(uint32_t seq,uint64_t pulses,const uint8_t* mac=MAC,uint64_t boot=7) {
  SyncPulseWire w{};w.magic=0x4952;w.version=1;w.type=6;w.sid=123;
  w.sequence=seq;w.bootId=boot;w.completedPulses=pulses;
  w.completedUs=clockUs-100;w.intervalUs=10000;w.pitchUm=9652;
  w.nominalUm=pulses*w.pitchUm;w.opticalReason=ir_movement::TRACKING;
  w.crc=syncCrc(reinterpret_cast<const uint8_t*>(&w),59);
  esp_now_recv_info_t info{};std::memcpy(info.src_addr,mac,6);
  onIr(&info,reinterpret_cast<const uint8_t*>(&w),sizeof(w));
}
'''
main = r'''
int main() {
  reset();send5(1,10);serviceMovement();
  assert(irHealth.odometry().epochActive());
  assert(irHealth.acceptedMm(40,1000,clockUs,1,clockUs));
  const auto frame=movement.frame();
  for(unsigned n=1;n<=25;++n){clockUs+=1000;send6(n,10+n);}
  assert(irQueueDrops==1);
  serviceMovement();
  assert(movement.frame()==frame+1);
  assert(!irHealth.odometry().epochActive());
  assert(!irHealth.reference().validFor(irHealth.odometry()));
  std::puts("REPRODUCED: dropping only Type-6 resets Type-5 history and invalidates the active IR/MM reference.");

  reset();send5(1,10);serviceMovement();
  clockUs=1050000;send6(1,10);serviceMovement();
  clockUs=1090000;send5(2,11);send6(2,11);
  clockUs=1100000;auto hall=capture();
  assert(hall.type5AtOpening.pulses==10 && hall.type6AtOpening.pulses==10);
  serviceMovement();
  assert(syncType5.pulses==11 && syncType6.pulses==11);
  assert(syncType5.receivedUs<hall.capturedUs && syncType6.receivedUs<hall.capturedUs);
  std::puts("REPRODUCED: packets arriving 10 ms before Hall are omitted when still queued at Hall capture.");

  reset();send5(1,10);serviceMovement();
  clockUs+=1000;send6(1,10);serviceMovement();
  clockUs+=1000;send6(2,11);serviceMovement();
  assert(syncType6.continuous && syncType6.boot==7);
  clockUs+=1000;send6(1,100,FOREIGN,99);serviceMovement();
  assert(syncType6.boot==99 && syncType6.pulses==100);
  std::puts("REPRODUCED: a different MAC/boot replaces the paired transmitter's Type-6 snapshot.");

  reset();send5(1,10);serviceMovement();
  clockUs+=1000;send6(1,10);serviceMovement();
  clockUs+=1000;send6(3,12);serviceMovement();
  assert(syncType6.valid && !syncType6.continuous && syncType6.pulses==12);
  assert(syncType6Gap==1);clockUs+=1000;report(capture());
  assert(std::strstr(Serial.line,"t6=0 t6_age_ms=-1 t6_minus_t5=0"));
  std::puts("REPRODUCED: the first valid cumulative event after a sequence gap is hidden in the Hall log.");
  std::fputs(Serial.line,stdout);
}
'''
text = (prefix + snapshot_types + rx_type + '\n' + stubs + receive_functions +
        '\nstatic CapturedHall capture(){CapturedHall pending{};\n' + hall_capture +
        'return pending;}\nstatic void report(const CapturedHall& j){\n' + diagnostic +
        '}\n' + helpers + main)
cpp = ROOT / 'review_probes.cpp'
cpp.write_text(text)
command = ['clang++', '-std=c++17', '-fsanitize=address,undefined', '-g',
           '-I' + str(BASE), str(cpp), '-o', str(ROOT / 'review_probes')]
subprocess.run(command, check=True)
result = subprocess.run([str(ROOT / 'review_probes')], check=True, text=True, capture_output=True)
(ROOT / 'probe-results.txt').write_text(result.stdout + result.stderr)
print(result.stdout, end='')
