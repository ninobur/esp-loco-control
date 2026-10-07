"""Actual NAVI sketch ingress isolation and actual telemetry serialization."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SKETCH = ROOT / 'firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED'


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

    def test_actual_callback_ledger_ack_channel_and_mqtt_isolation(self):
        source = (SKETCH/'NAVI_EYES_WIDE_OPEN_INTEGRATED.ino').read_text()
        def function(name):
            return re.search(r'^static [^\n]+\b' + name + r'\([^\{]*\{.*?^\}',
                             source, re.M | re.S).group()
        ir_rx = re.search(r'struct IrRx \{.*?\};', source, re.S).group()
        link_globals = source[source.index('struct LinkRx'):source.index('static volatile uint32_t hallQueueDrops')]
        cpp = r'''
#include "NaviIntegratedCore.h"
#include "NaviPulseTelemetry.h"
#include "EwoStationStop.h"
#include "../IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX/ReliablePulseTransport.h"
#include <atomic>
#include <cassert>
#include <deque>
#include <vector>
using namespace navi_eyes;
namespace navi_sync { struct Context {int8_t navDir=1;}; enum class InputKind {Ir}; }
struct esp_now_recv_info_t {uint8_t src_addr[6]={2,1,2,3,4,5};};
struct Queue {size_t size,capacity;std::deque<std::vector<uint8_t>> entries;};
using QueueHandle_t=Queue*;
constexpr int pdTRUE=1;
static int xQueueSend(Queue* q,const void* item,unsigned wait){
 assert(!wait);if(q->entries.size()==q->capacity)return 0;
 auto* p=static_cast<const uint8_t*>(item);q->entries.emplace_back(p,p+q->size);return 1;
}
static int xQueueReceive(Queue* q,void* item,unsigned wait){
 assert(!wait);if(q->entries.empty())return 0;
 std::memcpy(item,q->entries.front().data(),q->size);q->entries.pop_front();return 1;
}
static void xQueueOverwrite(Queue* q,const void* item){assert(q->capacity==1);q->entries.clear();xQueueSend(q,item,0);}
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) (void)(p)
#define portEXIT_CRITICAL(p) (void)(p)
static uint64_t clockUs=1000000;
static uint64_t esp_timer_get_time(){return clockUs;}
static uint32_t millis(){return uint32_t(clockUs/1000);}
static uint32_t esp_random(){static uint32_t random=10;return ++random;}
static navi_sync::Context recorderContext(){return {};}
static unsigned actualPwm=40,commandedPwm=40;
static uint32_t irQueueDrops=0,seenIrFrames=0,irPacketInvalid=0,inputs=0,lossCalls=0;
static NaviIntegratedCore navi;
static bool irCarCoupled=false;
static navi_pulse::Observation pulseObservation;
static navi_pulse::TransportObservation pulseTransportObservation;
static void serviceObservationLoss(){++lossCalls;}
template<class... Args> static void recordInput(Args...){++inputs;}
struct Recorder {unsigned ir=0;template<class... Args>void addIr(Args...){++ir;}};
static Recorder recording;static Recorder* recorder=&recording;
static uint8_t pairedIrMac[6]={2,1,2,3,4,5};
static uint32_t irLinkHelloSequence=0,irLinkHelloAt=0;
static bool radioReady=true;
static const uint8_t IR_LINK_BROADCAST[6]={255,255,255,255,255,255};
using wifi_second_chan_t=int;
static uint8_t wifiChannel=11;
constexpr int ESP_OK=0,WL_CONNECTED=3;
static int esp_wifi_get_channel(uint8_t* ch,int*){*ch=wifiChannel;return 0;}
struct WifiMock {int status(){return WL_CONNECTED;}} WiFi;
#define LOCO_NAME "9950011"
struct MqttMock {
 bool online=false;
 bool connected(){return online;}
 bool publish(const char* topic,const char* data,bool){
   assert(online);
   if(std::strstr(topic,"ir_link"))std::puts(data);
   return true;
 }
} mqtt;
struct SerialMock {template<class... Args>void printf(const char*,Args...){}} Serial;
'''
        cpp += ir_rx + '\n' + link_globals + r'''
static Queue operational{sizeof(IrRx),32,{}},pulses{sizeof(LinkRx),2,{}},
 statuses{sizeof(navi_pulse::Report),1,{}},acks{sizeof(LinkOutput),2,{}},
 hellos{sizeof(LinkOutput),1,{}},summaries{sizeof(LinkSummary),1,{}};
static QueueHandle_t irQ=&operational,pulseQ=&pulses,pulseStatusQ=&statuses;
'''
        cpp += '\n'.join(function(name) for name in
                         ('movementCrc','onIr','serviceIrIngress','pulseReport','servicePulseIngress',
                          'serviceIrLinkHello','servicePulseTelemetry'))
        cpp += r'''
static esp_now_recv_info_t info;
static void type5(unsigned seq){
 ir_movement::WireSnapshot w;w.bootId=42;w.sequence=seq;w.capturedUs=seq*100000;
 w.completedPulses=w.observedRises=seq;w.nominalUm=seq*w.pitchUm;
 w.crc=movementCrc((uint8_t*)&w,offsetof(ir_movement::WireSnapshot,crc));
 onIr(&info,(uint8_t*)&w,sizeof(w));serviceIrIngress();
}
static ir_link::Event event(unsigned seq){
 ir_link::Event e{};e.session=99;e.slot=seq%256;e.generation=seq/256+1;e.attempt=1;e.ageUs=1;
 auto& p=e.pulse;p.magic=0x4952;p.version=1;p.type=6;p.sid=42;p.bootId=42;
 p.sequence=seq;p.completedPulses=seq;p.completedUs=uint64_t(seq)*40000;p.intervalUs=seq==1?0:40000;
 p.pitchUm=9652;p.nominalUm=uint64_t(seq)*9652;p.opticalReason=ir_movement::TRACKING;
 ir_link::seal(p);ir_link::seal(e);return e;
}
static void ingest(const ir_link::Event& e){onIr(&info,(const uint8_t*)&e,sizeof(e));}
int main(){
 linkAckQ=&acks;linkHelloQ=&hellos;linkSummaryQ=&summaries;linkSession=99;
 std::memcpy(admittedIrMac,pairedIrMac,6);reliablePulses.start(99);
 if(!ir_input::Wireless){
  // DIRECT has no hardware producer, discovery, experimental ingress or ACK output.
  serviceIrLinkHello();ingest(event(1));servicePulseIngress();servicePulseTelemetry();
  assert(pulses.entries.empty() && acks.entries.empty() && hellos.entries.empty());
  assert(!reliablePulses.received && !pulseObservation.state().have && !pulseMqttDrops);
  // Existing Type-5/NAVI input remains active without a wireless pulse transport.
  type5(1);assert(inputs==1 && recording.ir==1);
  const auto before=navi.latestIr();
  // A future wired producer hands off exactly the same native representation.
  ir_input::Arrival direct{};direct.evidence=event(1).pulse;direct.receivedUs=clockUs;
  navi_pulse::Observation wired,wireless;
  wired.receiveNative(direct);
  auto radio=direct;std::memcpy(radio.sourceMac,pairedIrMac,6);wireless.receiveNative(radio);
  assert(std::memcmp(&wired.state().event,&wireless.state().event,sizeof(PulseEventPacket))==0);
  assert(std::memcmp(&wired.state().event,&direct.evidence,sizeof(PulseEventPacket))==0);
  assert(wired.state().eventValid==wireless.state().eventValid);
  const auto after=navi.latestIr();assert(std::memcmp(&before,&after,sizeof(before))==0);
  return 0;
 }
 // Only current discovery challenge binds the IR boot.
 serviceIrLinkHello();ir_link::Join join{};join.session=99;join.txBoot=42;join.challenge=irLinkHelloSequence;
 ir_link::seal(join);onIr(&info,(uint8_t*)&join,sizeof(join));servicePulseIngress();
 assert(reliablePulses.boot==42);const auto challenges=irLinkHelloSequence;
 onIr(&info,(uint8_t*)&join,sizeof(join));servicePulseIngress();assert(irLinkHelloSequence==challenges);
 type5(1);navi.declare(0,1,clockUs+1);const auto before=navi.latestIr();
 const auto consumed=navi.consumptionId(),epoch=navi.irMeasurementEpochId();
 // Receiver ingress saturation loses no transmitter-owned evidence: no ACK for rejected item.
 auto e1=event(1),e2=event(2),e3=event(3);
 ingest(e1);ingest(e3);ingest(e2);assert(linkRxLoss==1);servicePulseIngress();
 assert(reliablePulses.received==2 && reliablePulses.missing==1);
 LinkOutput out{};assert(xQueueReceive(linkAckQ,&out,0));ir_link::Ack ack{};
 std::memcpy(&ack,out.bytes,sizeof(ack));assert(ir_link::valid(ack,9) && ack.count==2);
 assert(ack.items[0].sequence==1 && ack.items[1].sequence==3);
 ingest(e2);servicePulseIngress();assert(reliablePulses.historical==1 && !reliablePulses.missing);
 assert(reliablePulses.current.sequence==3);
 ingest(e2);servicePulseIngress();assert(reliablePulses.received==3 && reliablePulses.duplicates==1);
 // ACK queue congestion causes visible failure; accepted evidence remains deduplicated.
 ingest(e2);servicePulseIngress();assert(linkAckLoss>0);
 info.src_addr[0]=4;ingest(event(4));servicePulseIngress();assert(linkUnknown==1 && reliablePulses.received==3);
 info.src_addr[0]=2;
 // MQTT remains disconnected while acceptance and ACK recovery continue.
 for(unsigned n=4;n<=400;++n){
  acks.entries.clear();clockUs+=40000;ingest(event(n));servicePulseIngress();servicePulseTelemetry();
  assert(reliablePulses.received==n && !acks.entries.empty());
 }
 assert(pulseMqttDrops>0);
 assert(navi.consumptionId()==consumed && navi.irMeasurementEpochId()==epoch);
 const auto after=navi.latestIr();assert(std::memcmp(&after,&before,sizeof(after))==0);
 assert(irQueueDrops==0 && inputs==1 && lossCalls==1 && recording.ir==1);
 type5(2);assert(inputs==2 && recording.ir==2);
 // Channel changes are reported without setting channel or touching Wi-Fi config.
 mqtt.online=true;wifiChannel=6;clockUs+=1000000;servicePulseIngress();servicePulseTelemetry();
 wifiChannel=11;clockUs+=1000000;servicePulseIngress();servicePulseTelemetry();
 LinkSummary maximum{};std::memset(&maximum.tx,0xff,sizeof(maximum.tx));
 maximum.received=maximum.missing=maximum.highest=maximum.priorMissing=maximum.receivedUs=
 maximum.lowerUs=maximum.upperUs=maximum.statusUs=maximum.previousBoot=UINT64_MAX;
 maximum.duplicates=maximum.historical=maximum.invalid=maximum.stale=maximum.boots=
 maximum.statusGaps=maximum.statusOrder=maximum.previousUnresolved=UINT32_MAX;
 maximum.previousStatusKnown=true;
 linkUnknown=linkRxLoss=linkAckLoss=linkMacOK=linkMacFail=linkTimeouts=pulseMqttDrops=UINT32_MAX;
 xQueueOverwrite(linkSummaryQ,&maximum);servicePulseTelemetry();
 // Pairing replaces the receiver session; previously queued source/session cannot be admitted.
 ingest(event(401));linkPairChanged=true;servicePulseIngress();assert(!reliablePulses.received && linkSession!=99);
}
'''
        lines = self.compile_run(cpp).splitlines()
        self.assertEqual(self.compile_run('#define NGR_IR_INPUT_PATH NGR_IR_INPUT_DIRECT\n' + cpp), '')
        rows = [json.loads(line, parse_constant=lambda value: self.fail(value)) for line in lines]
        self.assertEqual([r['channel_state'] for r in rows], ['mismatch', 'compatible', 'compatible'])
        self.assertEqual([r['field_channel_ready'] for r in rows], [0, 1, 1])
        self.assertTrue(all(r['authority']=='OBSERVATION_ONLY' for r in rows))
        self.assertLess(max(map(len, lines)), 2048)
        print('link JSON maximum exercised bytes:', max(map(len, lines)))


if __name__ == '__main__':
    unittest.main()
