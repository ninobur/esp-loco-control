#!/usr/bin/env python3
"""Exercise actual submitted ingress/service functions and original NAVI types.

Run: python3 tests/test_ir_sync.py [output-directory]
RTOS queue/clock and Serial/recorder sinks are mocked. This proves deterministic
software isolation, not RF capacity, real-time scheduling or field acceptance.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

variant = Path(__file__).resolve().parents[1]
baseline = variant.parent / 'NAVI_COHERENCE_0_6_IR_HEALTH'
# Staged build dependencies may resolve to the original historical checkout.
if not baseline.exists():
    baseline = Path('/Users/davidbrown/esp-loco-control/firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH')
out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(tempfile.mkdtemp(prefix='ir-sync-tests-'))
out.mkdir(parents=True, exist_ok=True)
original = (baseline / 'NAVI_COHERENCE_0_6_IR_HEALTH.ino').read_text()
corrected = (variant / 'NAVI_COHERENCE_0_6_IR_SYNC_TEST.ino').read_text()

def between(s, a, b):
    return s[s.index(a):s.index(b, s.index(a))]

# Source-equivalence checks establish the unchanged control call order as well
# as byte-identical algorithm headers, not just identical names or signatures.
headers = list(baseline.glob('*.h'))
for p in headers:
    assert (variant / p.name).read_bytes() == p.read_bytes(), p.name
assert between(original, 'static void hallTask(', 'static void onIr(') == between(corrected, 'static void hallTask(', 'static void onIr(')
assert between(original, 'struct Judged {', 'struct HallDecisionMsg') == between(corrected, 'struct Judged {', 'struct HallDecisionMsg')
old_loop = original[original.index('void loop(){'):]
new_loop = corrected[corrected.index('void loop(){'):]
for addition in (
    '  const uint64_t syncDrainAfterUs=esp_timer_get_time();\n',
    '    irSync.hall(j.serial,j.capturedUs); // Record only; never supply NAVI evidence.\n',
    '  serviceIrSync(syncDrainAfterUs);\n',
):
    assert addition in new_loop
    new_loop = new_loop.replace(addition, '', 1)
assert new_loop == old_loop, 'navigation/control loop changed beyond diagnostic taps'
old_movement = between(original, 'static void serviceMovement(){', 'static void onMqtt(').strip()
new_movement = between(corrected, 'static void serviceMovement(){', '// Runs after the original navigation/control services.').strip()
tap = ('      const auto& syncWire=movement.latest();\n'
       '      irSync.type5(packet.receivedUs,syncWire.completedPulses,syncWire.bootId,syncWire.sequence);\n')
assert tap in new_movement
assert new_movement.replace(tap, '', 1) == old_movement
# Everything between MQTT ingress and setup is identical except source reset.
reset = '    irSync.pair(mac,esp_timer_get_time());\n'
assert between(corrected, 'static void onMqtt(', 'void setup(){').replace(reset, '', 1) == between(original, 'static void onMqtt(', 'void setup(){')
print(f'PASS source equivalence: {len(headers)} historical headers, Hall acquisition, Type-5 processing, MQTT/motor/station services, navigation/control loop', flush=True)

prefix = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <deque>
#include <vector>
#include <tuple>
#include <new>
#include "IrHealthMonitor.h"
#include "Navigator.h"
#include "IrSyncDiagnostics.h"
static uint64_t clockUs=1000000;
static uint64_t esp_timer_get_time(){return clockUs;}
struct esp_now_recv_info_t {uint8_t src_addr[6];};
struct Queue {size_t size,capacity;std::deque<std::vector<uint8_t>> values;};
using QueueHandle_t=Queue*;
constexpr int pdTRUE=1;
static int xQueueSend(Queue* q,const void* p,int){
  if(q->values.size()==q->capacity)return 0;
  const auto* b=static_cast<const uint8_t*>(p);q->values.emplace_back(b,b+q->size);return 1;
}
static int xQueuePeek(Queue* q,void* p,int){if(q->values.empty())return 0;memcpy(p,q->values.front().data(),q->size);return 1;}
static int xQueueReceive(Queue* q,void* p,int){if(!xQueuePeek(q,p,0))return 0;q->values.pop_front();return 1;}
static unsigned uxQueueMessagesWaiting(Queue* q){return unsigned(q->values.size());}
struct SerialSink {std::vector<std::string> lines;void print(const char* s){lines.emplace_back(s);}};
struct Recorder {
  std::vector<ir_movement::WireSnapshot> wires;
  void addIr(uint64_t,const uint8_t*,int,const ir_movement::WireSnapshot& w,uint8_t,uint8_t,uint8_t,uint8_t,int){wires.push_back(w);}
};
static int naviSyncContext(){return 0;}
static const uint8_t MAC[6]={2,3,4,5,6,7},FOREIGN[6]={2,3,4,5,6,8};
'''.replace('#include <tuple>', '#include <tuple>\n#include <string>')
globals_ = r'''
static Queue q5{sizeof(IrRx),24,{}};
static QueueHandle_t irQ=&q5;
static ngr_nav::MovementSource movement;
static ngr_nav::IrHealthMonitor irHealth;
static navi_one::Navigator navigator;
static uint32_t irQueueDrops=0,handledIrDrops=0,seenFrames=0;
static uint8_t lastSeenMac[6]{},actualPwm=90,commandedPwm=90;
static Recorder naviSyncRecorder;
static SerialSink Serial;
'''
pulse_globals = r'''
static Queue q6{sizeof(IrPulseRx),32,{}};
static QueueHandle_t irPulseQ=&q6;
static uint32_t irPulseQueueDrops=0,handledPulseDrops=0;
static ir_sync::Diagnostics irSync;
'''
parts = [prefix]
for name, source in [('before', original), ('after', corrected)]:
    types = re.search(r'struct IrRx \{[^\n]+', source).group(0) + '\n'
    if name == 'after':
        types += re.search(r'struct IrPulseRx \{[^\n]+', source).group(0) + '\n'
    functions = between(source, 'static void onIr(', 'static void onMqtt(')
    parts.append('namespace ' + name + '{\n' + types + globals_ + (pulse_globals if name == 'after' else '') + functions + '\n}\n')

parts.append(r'''
static ir_movement::WireSnapshot wire5(uint32_t sequence,uint64_t pulses){
  ir_movement::WireSnapshot w{};w.sequence=sequence;w.bootId=7;w.capturedUs=clockUs-100;
  w.observedRises=w.completedPulses=pulses;w.nominalUm=pulses*w.pitchUm;w.opticalReason=ir_movement::TRACKING;
  w.crc=ir_sync::crc(reinterpret_cast<const uint8_t*>(&w),108);return w;
}
static ir_sync::PulseWire wire6(uint32_t seq,uint64_t pulses,uint64_t boot=7){
  ir_sync::PulseWire w{};w.magic=0x4952;w.version=1;w.type=6;w.sequence=seq;w.bootId=boot;
  w.completedPulses=pulses;w.completedUs=clockUs-100;w.intervalUs=10000;w.pitchUm=9652;
  w.nominalUm=pulses*w.pitchUm;w.opticalReason=ir_movement::TRACKING;
  w.crc=ir_sync::crc(reinterpret_cast<const uint8_t*>(&w),59);return w;
}
static void send5(ir_movement::WireSnapshot w){
  esp_now_recv_info_t info{};memcpy(info.src_addr,MAC,6);
  before::onIr(&info,reinterpret_cast<const uint8_t*>(&w),sizeof(w));
  after::onIr(&info,reinterpret_cast<const uint8_t*>(&w),sizeof(w));
}
static void send6(ir_sync::PulseWire w,const uint8_t* mac=MAC,int size=61){
  esp_now_recv_info_t info{};memcpy(info.src_addr,mac,6);
  before::onIr(&info,reinterpret_cast<const uint8_t*>(&w),size);
  after::onIr(&info,reinterpret_cast<const uint8_t*>(&w),size);
}
static auto status(const navi_one::Navigator& n){
  const auto& s=n.status();return std::make_tuple(s.state,s.navMm,s.target,s.navDir,s.trust,s.corrections,
    s.evidence,s.advances,s.refusals,s.irWaits,s.unresolvedCount,s.sequenceLength,s.sequenceMatches,
    s.missedSinceLast,s.distanceConfirmed,s.distanceAssessable,s.distance,s.sansMmConsecutive,s.sansMmTotal,
    n.haveReference(),n.referenceMm(),n.positionKnown(),n.autoLandmarkLimitReached());
}
static void equal(){
  assert(before::irQueueDrops==after::irQueueDrops);
  assert(before::seenFrames==after::seenFrames);
  assert(before::movement.frame()==after::movement.frame());
  assert(before::movement.accepted==after::movement.accepted);
  assert(before::movement.rejected==after::movement.rejected);
  assert(before::movement.duplicates==after::movement.duplicates);
  assert(before::movement.reboots==after::movement.reboots);
  assert(!memcmp(&before::movement.latest(),&after::movement.latest(),sizeof(ir_movement::WireSnapshot)));
  char b[1200],a[1200];
  before::irHealth.format(b,sizeof(b),clockUs,123);after::irHealth.format(a,sizeof(a),clockUs,123);
  assert(!strcmp(b,a));
  const auto bp=before::movement.at(clockUs,clockUs),ap=after::movement.at(clockUs,clockUs);
  assert(bp.issue==ap.issue && bp.frame==ap.frame && bp.arrivalUs==ap.arrivalUs && bp.alignmentUs==ap.alignmentUs);
  assert(!memcmp(&bp.wire,&ap.wire,sizeof(bp.wire)));
  assert(status(before::navigator)==status(after::navigator));
  assert(before::naviSyncRecorder.wires.size()==after::naviSyncRecorder.wires.size());
  for(size_t i=0;i<before::naviSyncRecorder.wires.size();++i)
    assert(!memcmp(&before::naviSyncRecorder.wires[i],&after::naviSyncRecorder.wires[i],sizeof(ir_movement::WireSnapshot)));
}
static void reset(){
  before::q5.values.clear();after::q5.values.clear();after::q6.values.clear();after::irPulseQ=&after::q6;
  before::movement=ngr_nav::MovementSource{};after::movement=ngr_nav::MovementSource{};
  before::irHealth.~IrHealthMonitor();new (&before::irHealth) ngr_nav::IrHealthMonitor{};
  after::irHealth.~IrHealthMonitor();new (&after::irHealth) ngr_nav::IrHealthMonitor{};
  before::navigator=navi_one::Navigator{};after::navigator=navi_one::Navigator{};
  before::irQueueDrops=before::handledIrDrops=before::seenFrames=0;
  after::irQueueDrops=after::handledIrDrops=after::seenFrames=0;
  after::irPulseQueueDrops=after::handledPulseDrops=0;after::irSync=ir_sync::Diagnostics{};
  before::naviSyncRecorder.wires.clear();after::naviSyncRecorder.wires.clear();after::Serial.lines.clear();
  clockUs=1000000;
  before::movement.pair(MAC);after::movement.pair(MAC);
  before::irHealth.pair(MAC,0);after::irHealth.pair(MAC,0);after::irSync.pair(MAC,0);
  before::navigator.declare(40,1,1000);after::navigator.declare(40,1,1000);
}
static void judge(uint32_t serial){
  navi_one::NavObservation b,a;
  b.openedAtMs=a.openedAtMs=uint32_t(clockUs/1000);
  b.polarity=a.polarity=navi_one::polarityAt(before::navigator.status().target);
  b.odometry=before::irHealth.at(clockUs,clockUs);a.odometry=after::irHealth.at(clockUs,clockUs);
  const auto br=before::navigator.judge(b),ar=after::navigator.judge(a);assert(br==ar);
  if(br==navi_one::Ruling::Advanced || br==navi_one::Ruling::AdvancedWithDiscrepancy){
    before::irHealth.acceptedMm(before::navigator.status().navMm,uint32_t(clockUs/1000),clockUs,serial,clockUs);
    after::irHealth.acceptedMm(after::navigator.status().navMm,uint32_t(clockUs/1000),clockUs,serial,clockUs);
  }
  before::navigator.traverse(before::irHealth.odometry().point());
  after::navigator.traverse(after::irHealth.odometry().point());
}
static void isolation(){
  // 10 failure/delivery patterns, with actual NAVI judgments and IR references.
  for(unsigned mode=0;mode<10;++mode){
    reset();uint64_t pulses=10;
    for(unsigned i=1;i<=24;++i){
      clockUs=1000000ULL+i*1000000ULL;
      if(i>1)pulses+=(navi_one::spanMm(before::navigator.status().navMm,1)*1000ULL+4826)/9652;
      send5(wire5(i,pulses));
      for(unsigned k=1;k<=40;++k){
        auto w=wire6(i*100+k,pulses+k);
        if(mode==1)w.crc^=1;                       // corrupt CRC
        if(mode==2){w=wire6(1,1);}                // duplicates
        if(mode==3)w=wire6(10000-i*100-k,pulses+k);// reordered sequences
        if(mode==4)w=wire6(k,pulses+k,i+k);       // transmitter reboot churn
        if(mode==5){send6(w,FOREIGN);continue;}  // another transmitter
        if(mode==6){send6(w,MAC,60);continue;}   // malformed length
        if(mode==7){after::irPulseQ=nullptr;}    // unavailable diagnostic queue
        if(mode==8)continue;                    // missing Type-6 entirely
        if(mode==9){w.version=2;}                // unsupported version
        send6(w);
      }
      assert(after::q5.values.size()==1);assert(before::q5.values.size()==1);
      before::serviceMovement();after::serviceMovement();equal();
      after::irSync.hall(i,clockUs);judge(i);equal();
      after::serviceIrSync(clockUs+1);equal();
      assert(after::irQueueDrops==0);
      assert(after::irHealth.reference().validFor(after::irHealth.odometry()));
    }
    assert(after::navigator.status().advances==24);
    if(mode!=6 && mode!=8)assert(after::irPulseQueueDrops>0);
  }
  // Genuine Type-5 overflow retains the original reset behavior.
  reset();for(unsigned i=1;i<=25;++i){++clockUs;send5(wire5(i,i));}
  before::serviceMovement();after::serviceMovement();equal();assert(after::irQueueDrops==1);
  std::puts("PASS Type-6 isolation: 240 real NAVI judgments across 10 patterns; Type-5 state, health/MM reference, recorder output and full NavStatus agree with original; genuine Type-5 overflow retained");
}
static void observations(){
  reset();send5(wire5(1,10));send6(wire6(1,10));before::serviceMovement();after::serviceMovement();after::serviceIrSync(clockUs);
  clockUs=1090000;send5(wire5(2,11));send6(wire6(2,11));
  after::irSync.hall(1,1100000); // arrivals are queued when Hall occurs
  clockUs=1110000;send5(wire5(3,12));send6(wire6(3,12)); // must not replace pre-Hall values
  after::serviceIrSync(1105000);assert(after::Serial.lines.empty()); // Type-5 backlog defers only output
  before::serviceMovement();after::serviceMovement();after::serviceIrSync(1120000);
  assert(after::Serial.lines.size()==1);const auto& line=after::Serial.lines.back();
  assert(line.find("t5=11 t5_age_ms=10")!=std::string::npos);
  assert(line.find("t6=11 t6_age_ms=10")!=std::string::npos);
  assert(line.find("compare_ok=1")!=std::string::npos);

  clockUs=1200000;send6(wire6(5,14));after::serviceIrSync(clockUs);
  after::irSync.hall(2,clockUs);after::serviceIrSync(clockUs+1);
  const auto gap=after::Serial.lines.back();
  assert(gap.find("t6=14 t6_age_ms=0 t6_valid=1")!=std::string::npos);
  assert(gap.find("t6_cont=0 same_boot=1 compare_ok=0 t6_minus_t5=2")!=std::string::npos);
  assert(after::irSync.gaps==1);
  clockUs+=100;send6(wire6(6,15),FOREIGN);after::serviceIrSync(clockUs);
  assert(after::irSync.type6At(clockUs).point.pulses==14 && after::irSync.wrongSource==1);

  // Sequence wrap remains adjacent; re-pairing clears old evidence and rejects queued old arrivals.
  ir_sync::Diagnostics d;d.pair(MAC,0);
  auto w=wire6(UINT32_MAX,100);assert(d.type6(MAC,(const uint8_t*)&w,61,1000));
  w=wire6(0,101);assert(d.type6(MAC,(const uint8_t*)&w,61,1100));
  assert(d.type6At(1100).point.continuous);
  assert(!d.type6(MAC,(const uint8_t*)&w,61,1200));
  w=wire6(UINT32_MAX,100);assert(!d.type6(MAC,(const uint8_t*)&w,61,1201));
  d.pair(FOREIGN,1300);assert(!d.type6At(2000).point.valid);
  w=wire6(1,1);assert(!d.type6(FOREIGN,(const uint8_t*)&w,61,1299));
  assert(!d.type6(MAC,(const uint8_t*)&w,61,1400));
  assert(d.type6(FOREIGN,(const uint8_t*)&w,61,1400));assert(!d.type6At(1400).point.continuous);
  // Bounded history and Hall queue overflow are explicit, never NAVI loss.
  for(unsigned i=1;i<=65;++i)d.type5(1500+i,i,7,i);
  assert(d.type5At(1501).historyLost && !d.type5At(1501).point.valid);
  assert(d.type5At(1565).point.pulses==65);
  for(unsigned i=0;i<17;++i)d.hall(i,2000+i);
  assert(d.hallDrops==1);
  char small[12];assert(d.format(small,sizeof(small),{2100,1},0,0)>=int(sizeof(small)));
  std::puts("PASS causal Hall selection, future exclusion, deferred backlog, post-gap raw counts, foreign source, sequence wrap/duplicate/reorder, pairing reset, history/diagnostic queue bounds, formatting bounds");
}
int main(){isolation();observations();}
''')
cpp=out/'ir_sync_regression.cpp'
cpp.write_text(''.join(parts))
subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-unused-variable','-Wno-unused-function','-fsanitize=address,undefined','-g','-I'+str(variant),str(cpp),'-o',str(out/'ir_sync_regression')],check=True)
subprocess.run([str(out/'ir_sync_regression')],check=True)
