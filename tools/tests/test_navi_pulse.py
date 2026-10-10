"""Actual NAVI sketch ingress isolation and actual telemetry serialization."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SKETCH = ROOT / 'firmware/programs/NAVI_EWO_0_1_MM045_STOP'


class PulseIntegration(unittest.TestCase):
    def compile_run(self, text, args=()):
        with tempfile.TemporaryDirectory(prefix='navi-pulse-') as tmp:
            cpp, exe = Path(tmp)/'test.cpp', Path(tmp)/'test'
            cpp.write_text(text)
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=address,undefined', '-I', str(SKETCH),
                            str(cpp), '-o', str(exe)], check=True)
            return subprocess.check_output([str(exe), *args], text=True)

    def test_actual_json_validity_age_and_worst_case_capacity(self):
        cpp = '#include "' + str(SKETCH/'tests/test_pulse_observation.cpp') + '"\n'
        lines = self.compile_run(cpp, ['json']).splitlines()
        rows = [json.loads(line, parse_constant=lambda s: self.fail(s)) for line in lines]
        self.assertEqual(len(rows), 4)
        self.assertIsNone(rows[0]['mmps'])
        self.assertIsNone(rows[0]['legacy_pkph'])
        self.assertIsNone(rows[0]['age_ms'])
        self.assertEqual(rows[1]['valid'], 1)
        self.assertEqual(rows[1]['legacy_pkph'], 45)
        self.assertEqual(rows[2]['valid'], 0)
        self.assertEqual(rows[2]['event_valid'], 1)
        self.assertEqual(rows[2]['age_ms'], 1000)
        for line, row in zip(lines, rows):
            self.assertLess(len(line.encode()), 1200)
            self.assertEqual(row['authority'], 'OBSERVATION_ONLY')
        print('pulse JSON maximum exercised bytes:', max(map(len, lines)))

    def test_actual_callback_and_queues_cannot_feed_pulse_loss_to_navi(self):
        source = (SKETCH/'NAVI_EWO_0_1_MM045_STOP.ino').read_text()
        def function(name):
            return re.search(r'^static [^\n]+\b' + name + r'\([^\{]*\{.*?^\}',
                             source, re.M | re.S).group()
        ir_rx = re.search(r'struct IrRx \{.*?\};', source, re.S).group()
        cpp = r'''
#include "NaviIntegratedCore.h"
#include "NaviPulseTelemetry.h"
#include "EwoStationStop.h"
#include <atomic>
#include <cassert>
#include <deque>
#include <vector>
using namespace navi_eyes;
namespace navi_sync { struct Context {int8_t navDir=1;}; enum class InputKind {Ir}; }
struct esp_now_recv_info_t {uint8_t src_addr[6]={1,2,3,4,5,6};};
struct Queue {size_t size, capacity;std::deque<std::vector<uint8_t>> entries;};
using QueueHandle_t=Queue*;
constexpr int pdTRUE=1;
static int xQueueSend(Queue* q,const void* item,unsigned wait){
  assert(!wait);if(q->entries.size()==q->capacity)return 0;
  auto* p=static_cast<const uint8_t*>(item);q->entries.emplace_back(p,p+q->size);return pdTRUE;
}
static int xQueueReceive(Queue* q,void* item,unsigned wait){
  assert(!wait);if(q->entries.empty())return 0;
  std::memcpy(item,q->entries.front().data(),q->size);q->entries.pop_front();return pdTRUE;
}
static void xQueueOverwrite(Queue* q,const void* item){
  assert(q->capacity==1);q->entries.clear();xQueueSend(q,item,0);
}
static uint64_t clockUs=1000000;
static uint64_t esp_timer_get_time(){return clockUs;}
static uint32_t millis(){return uint32_t(clockUs/1000);}
static navi_sync::Context recorderContext(){return {};}
static unsigned actualPwm=40,commandedPwm=40;
static uint32_t irQueueDrops=0,seenIrFrames=0,irPacketInvalid=0,inputs=0,lossCalls=0;
static std::atomic<uint32_t> pulseRxDrops{0},pulseLogDrops{0};
static NaviIntegratedCore navi;
static bool irCarCoupled=false;
static navi_pulse::Observation pulseObservation;
static void serviceObservationLoss(){++lossCalls;}
template<class... Args> static void recordInput(Args...){++inputs;}
struct Recorder { unsigned ir=0;template<class... Args>void addIr(Args...){++ir;} };
static Recorder recording;static Recorder* recorder=&recording;
'''
        cpp += ir_rx + r'''
static Queue operational{sizeof(IrRx),32,{}}, pulses{sizeof(navi_pulse::Rx),32,{}},
  logs{sizeof(navi_pulse::Report),16,{}}, status{sizeof(navi_pulse::Report),1,{}};
static QueueHandle_t irQ=&operational,pulseQ=&pulses,pulseLogQ=&logs,pulseStatusQ=&status;
'''
        cpp += '\n'.join(function(name) for name in
                         ('movementCrc', 'onIr', 'serviceIrIngress', 'pulseReport', 'servicePulseIngress'))
        cpp += r'''
static esp_now_recv_info_t info;
static void type5(unsigned seq){
  ir_movement::WireSnapshot w;w.bootId=42;w.sequence=seq;w.capturedUs=seq*100000;
  w.completedPulses=w.observedRises=seq;w.nominalUm=seq*w.pitchUm;
  w.crc=movementCrc((uint8_t*)&w,offsetof(ir_movement::WireSnapshot,crc));
  clockUs+=100000;onIr(&info,(uint8_t*)&w,sizeof(w));serviceIrIngress();
}
static void type6(unsigned seq,int length=61){
  PulseEventPacket e{};e.magic=0x4952;e.version=1;e.type=6;e.sid=42;e.bootId=42;
  e.sequence=seq;e.completedPulses=seq;e.completedUs=seq*40000;e.intervalUs=40000;
  e.pitchUm=9652;e.nominalUm=seq*9652;e.opticalReason=ir_movement::TRACKING;
  e.crc=movementCrc((uint8_t*)&e,offsetof(PulseEventPacket,crc));
  uint8_t buffer[110]{};std::memcpy(buffer,&e,sizeof(e));clockUs+=40000;
  onIr(&info,buffer,length);
}
int main(){
  type5(1);navi.declare(0,1,clockUs+1);
  const auto epoch=navi.irMeasurementEpochId(),consumed=navi.consumptionId();
  const auto latest=navi.latestIr();const auto mm=navi.mm();
  for(unsigned i=1;i<=64;++i)type6(i);
  assert(pulseRxDrops==32 && irQueueDrops==0 && operational.entries.empty());
  assert(navi.irObservationCount()==1 && navi.consumptionId()==consumed);
  for(unsigned i=0;i<8;++i)servicePulseIngress();
  assert(pulseObservation.state().accepted==32 && pulseLogDrops==16);
  assert(pulseObservation.state().eventValid); // Queued pre-loss evidence remains historical truth.
  type6(65);servicePulseIngress();
  assert(!pulseObservation.state().eventValid && (pulseObservation.state().flags&navi_pulse::QUEUE_LOSS));
  type6(66);servicePulseIngress();assert(pulseObservation.state().eventValid);
  type6(67,110);servicePulseIngress(); // Recognizable Type-6 fault cannot reach operational queue.
  assert(pulseObservation.state().invalid==1);
  type6(68);servicePulseIngress();assert(!pulseObservation.state().eventValid);
  info.src_addr[0]=2;type6(69);servicePulseIngress();assert(!pulseObservation.state().eventValid);
  serviceIrIngress();assert(lossCalls==1 && inputs==1 && recording.ir==1);
  assert(irQueueDrops==0 && irPacketInvalid==0 && seenIrFrames==1);
  assert(navi.irMeasurementEpochId()==epoch && navi.consumptionId()==consumed);
  assert(navi.mm()==mm && navi.positionReliable() && !navi.pwmZeroMovementRequiresDeclaration());
  const auto after=navi.latestIr();assert(std::memcmp(&after,&latest,sizeof(after))==0);
  info.src_addr[0]=1;type5(2);
  assert(navi.irObservationCount()==2 && recording.ir==2 && inputs==2);
  assert(navi.irMeasurementEpochId()==epoch && navi.positionReliable());
}
'''
        self.compile_run(cpp)


if __name__ == '__main__':
    unittest.main()
