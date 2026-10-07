#!/usr/bin/env python3
"""Exercise the actual asynchronous radio functions and selective-ACK core."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SKETCH = ROOT / "firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX"
SOURCE = (SKETCH / "IR_SCOPE_ESPNOW_TX.ino").read_text()

def function(source, name):
    start = source.rfind("static ", 0, source.index(name))
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]

def compile_run(source, include=SKETCH):
    with tempfile.TemporaryDirectory(prefix="ir-reliable-test-") as tmp:
        cpp, exe = Path(tmp) / "test.cpp", Path(tmp) / "test"
        cpp.write_text(source)
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-I", str(include), str(cpp),
                        "-o", str(exe)], check=True)
        return subprocess.check_output([str(exe)], text=True)

class TransportTests(unittest.TestCase):
    def test_input_configuration_rejects_invalid_selection(self):
        result=subprocess.run(['c++','-std=c++17','-DNGR_IR_INPUT_PATH=2',
            '-I',str(ROOT/'firmware/common'),'-x','c++','-fsyntax-only','-'],
            input='#include "IrPulseInput.h"\n',text=True,capture_output=True)
        self.assertNotEqual(result.returncode,0)
        self.assertIn('NGR_IR_INPUT_PATH must be',result.stderr)

    def test_actual_peer_discovery_and_ack_source(self):
        harness = r'''
#include <atomic>
#include <cassert>
#include <cstring>
#include <deque>
#include <vector>
#include "ReliablePulseTransport.h"
struct esp_now_recv_info_t {uint8_t src_addr[6];};
struct LinkPeerUpdate {uint8_t mac[6];ir_link::Hello hello;};
struct PulseAckRx {uint8_t mac[6];ir_link::Ack ack;};
struct Queue {size_t size;std::deque<std::vector<uint8_t>> items;};
using QueueHandle_t=Queue*;
static Queue peers{sizeof(LinkPeerUpdate),{}},acks{sizeof(PulseAckRx),{}};
static QueueHandle_t linkPeerQueue=&peers,pulseAckQueue=&acks;
constexpr int pdTRUE=1,ESP_OK=0;using esp_err_t=int;
static int xQueueSend(Queue* q,const void* data,unsigned wait){assert(!wait);auto* p=(const uint8_t*)data;q->items.emplace_back(p,p+q->size);return 1;}
static int xQueueReceive(Queue* q,void* out,unsigned wait){assert(!wait);if(q->items.empty())return 0;std::memcpy(out,q->items.front().data(),q->size);q->items.pop_front();return 1;}
static void xQueueOverwrite(Queue* q,const void* data){q->items.clear();xQueueSend(q,data,0);}
static uint8_t selfMac[6]={2,1,2,3,4,5},naviPeerMac[6]{};
static constexpr uint32_t NAVI_TARGET_ID=9950011;static constexpr uint8_t CHANNEL=11;
static bool radioInFlight=false,naviPeerReady=false,naviAccepted=false,joinPending=false,installed=false;
static uint64_t retiredSession=0,naviHelloBoot=0,movementBoot=42;
static uint32_t naviHelloSequence=0;
static std::atomic<uint32_t> pulseAckDrops{0},linkHelloAt{0},pulseOutstanding{0},pulseAckCount{0};
static ir_link::Transmitter pulseTransport;
static uint32_t millis(){return 100;}
static uint64_t esp_timer_get_time(){return 100000;}
struct esp_now_peer_info_t {uint8_t peer_addr[6],channel;bool encrypt;};
static bool esp_now_is_peer_exist(const uint8_t*){return installed;}
static int esp_now_add_peer(const esp_now_peer_info_t* p){assert(p->channel==11 && !p->encrypt);installed=true;return 0;}
'''
        receive = function(SOURCE, 'onReceive(')
        harness += receive[:receive.index('  if(len==(int)sizeof(FusionAckPacket))')] + '}\n'
        harness += '\n'.join(function(SOURCE, name) for name in ('serviceNaviPeer(', 'servicePulseAcks('))
        harness += r'''
int main(){
 esp_now_recv_info_t info{{4,1,2,3,4,5}};
 ir_link::Hello h{};h.session=99;h.sequence=1;h.channel=11;std::memcpy(h.pairedMac,selfMac,6);ir_link::seal(h);
 if(!ir_input::Wireless){
  onReceive(&info,(uint8_t*)&h,sizeof(h));serviceNaviPeer();
  ir_link::Ack ack{};onReceive(&info,(uint8_t*)&ack,sizeof(ack));servicePulseAcks();
  assert(!naviPeerReady && !installed && peers.items.empty() && acks.items.empty());return 0;
 }
 auto wrong=h;wrong.loco=9950012;ir_link::seal(wrong);onReceive(&info,(uint8_t*)&wrong,sizeof(wrong));serviceNaviPeer();assert(!naviPeerReady);
 wrong=h;wrong.channel=6;ir_link::seal(wrong);onReceive(&info,(uint8_t*)&wrong,sizeof(wrong));serviceNaviPeer();assert(!naviPeerReady);
 wrong=h;wrong.pairedMac[5]=9;ir_link::seal(wrong);onReceive(&info,(uint8_t*)&wrong,sizeof(wrong));serviceNaviPeer();assert(!naviPeerReady);
 onReceive(&info,(uint8_t*)&h,sizeof(h));serviceNaviPeer();assert(naviPeerReady && joinPending && !naviAccepted);
 ++h.sequence;h.txBoot=42;ir_link::seal(h);onReceive(&info,(uint8_t*)&h,sizeof(h));serviceNaviPeer();assert(naviAccepted);
 // Same MAC receiver reboot changes session. Delayed old-session hello cannot restore it.
 auto old=h;h.session=100;h.sequence=1;ir_link::seal(h);onReceive(&info,(uint8_t*)&h,sizeof(h));serviceNaviPeer();assert(naviHelloBoot==100);
 onReceive(&info,(uint8_t*)&old,sizeof(old));serviceNaviPeer();assert(naviHelloBoot==100);
 info.src_addr[0]=6;++h.sequence;ir_link::seal(h);onReceive(&info,(uint8_t*)&h,sizeof(h));serviceNaviPeer();assert(naviPeerMac[0]==4);
 PulseEventPacket p{};p.bootId=42;p.sequence=1;p.completedPulses=1;p.completedUs=1;
 pulseTransport.enqueue(p);pulseTransport.submit(0,10);pulseOutstanding=1;
 ir_link::Ack ack{};ack.session=100;ack.txBoot=42;ack.count=1;ack.items[0]={1,1,0};ir_link::seal(ack);
 onReceive(&info,(uint8_t*)&ack,sizeof(ack));servicePulseAcks();assert(pulseOutstanding==1);
 info.src_addr[0]=4;onReceive(&info,(uint8_t*)&ack,sizeof(ack));servicePulseAcks();assert(!pulseOutstanding && pulseAckCount==1);
}
'''
        compile_run(harness)
        compile_run('#define NGR_IR_INPUT_PATH NGR_IR_INPUT_DIRECT\n' + harness)

    def test_selective_ack_faults(self):
        print(compile_run('#include "' + str(ROOT / "tools/test_ir_reliable_transport.cpp") + '"'))

    def test_actual_asynchronous_radio_and_late_callback(self):
        harness = r'''
#include <atomic>
#include <cassert>
#include <cstring>
#include "ReliablePulseTransport.h"
using esp_err_t=int;using esp_now_send_status_t=int;
constexpr int ESP_OK=0,ESP_NOW_SEND_SUCCESS=0,ESP_NOW_SEND_FAIL=1;
struct wifi_tx_info_t {};
static std::atomic<bool> sendDone{true};
static std::atomic<uint32_t> pulseSendLagMaxUs{0};
static int sendStatus=ESP_NOW_SEND_FAIL;
static bool radioInFlight=false,radioTimeoutReported=false;
static uint8_t radioKind=0;
static uint32_t now=0,radioStartedMs=0,calls=0;
static uint32_t sendErrors=0,pulseEventFailed=0,pulseEventSent=0,radioTimeouts=0,sent=0,observationSent=0,fusionSent=0;
static int immediate=ESP_OK;
static uint8_t destination[6]{};
static const uint8_t BROADCAST[6]={255,255,255,255,255,255};
static uint32_t millis(){return now;}
static int esp_now_send(const uint8_t* mac,const uint8_t*,size_t){++calls;std::memcpy(destination,mac,6);return immediate;}
'''
        harness += "\n".join(function(SOURCE, n) for n in
                             ("onSent(", "radioSendTo(", "radioSend(", "serviceRadioCompletion("))
        harness += r'''
int main(){
  uint8_t packet[4]={0x52,0x49,2,6},peer[6]={2,1,2,3,4,5};
  if(!ir_input::Wireless){
    for(unsigned type=6;type<=10;++type){packet[3]=type;assert(!radioSendTo(peer,packet,4));}
    assert(!calls);packet[3]=5;assert(radioSend(packet,4) && calls==1);return 0;
  }
  assert(radioSendTo(peer,packet,4) && now==0 && calls==1);
  assert(!radioSend(packet,4) && calls==1);
  now=101;serviceRadioCompletion();assert(radioTimeouts==1 && radioInFlight);
  for(unsigned i=0;i<100;++i){serviceRadioCompletion();assert(!radioSend(packet,4));}
  assert(calls==1 && radioTimeouts==1); // callback remains owned, task never waits
  onSent(nullptr,ESP_NOW_SEND_SUCCESS);serviceRadioCompletion();assert(pulseEventSent==1);
  assert(radioSendTo(peer,packet,4));onSent(nullptr,ESP_NOW_SEND_FAIL);serviceRadioCompletion();
  assert(pulseEventFailed==1 && pulseEventSent==1);
  immediate=1;assert(!radioSendTo(peer,packet,4));assert(!radioInFlight && pulseEventFailed==2);
  immediate=0;packet[3]=5;assert(radioSend(packet,4));assert(std::memcmp(destination,BROADCAST,6)==0);
  onSent(nullptr,ESP_NOW_SEND_SUCCESS);serviceRadioCompletion();
}
'''
        compile_run(harness)
        compile_run('#define NGR_IR_INPUT_PATH NGR_IR_INPUT_DIRECT\n' + harness)

    def test_physical_and_operational_source_isolation(self):
        base = "83462fee6ff56ff85caa3a9d9d6e8a8d84c50925"
        paths = ["firmware/common/IrMovementDetector.h", "firmware/common/IrMovementContract.h",
                 "firmware/common/IrMovementWire.h",
                 str(SKETCH.relative_to(ROOT) / "PulseEventEvidence.h")]
        for path in paths:
            self.assertEqual((ROOT/path).read_bytes(), subprocess.check_output(["git","show",base+":"+path],cwd=ROOT))
        old = subprocess.check_output(["git","show",base+":"+str(SKETCH.relative_to(ROOT)/"IR_SCOPE_ESPNOW_TX.ino")],cwd=ROOT,text=True)
        # Exactly one sampling-path change: zero-wait admission with retained-event credits.
        sampler = function(SOURCE,"sampler(").replace("log.queued=enqueuePulse(e);",
                    "log.queued=xQueueSend(pulseEventQueue,&e,0)==pdTRUE;").replace(
                    "if(ir_input::Wireless && !log.queued)", "if(!log.queued)")
        self.assertEqual(sampler,function(old,"sampler("))
        navi_path = "firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/NAVI_EYES_WIDE_OPEN_INTEGRATED.ino"
        navi = (ROOT/navi_path).read_text()
        old = subprocess.check_output(["git","show",base+":"+navi_path],cwd=ROOT,text=True)
        for name in ("serviceIrIngress(", "serviceRamp(", "requestPwm(", "serviceStation(",
                     "onMqtt(", "hallTask(", "serviceObservationLoss(", "servicePwmZeroMovementHold("):
            self.assertEqual(function(navi,name),function(old,name),name)
        # The only command-handler change is the explicitly approved ir_pair branch.
        self.assertEqual(function(navi,'handleCommand(').split('} else if (!strcmp(leaf, "ir_coupled"))')[1],
                         function(old,'handleCommand(').split('} else if (!strcmp(leaf, "ir_coupled"))')[1])
        for name in ("radioSendTo(", "serviceNaviPeer(", "servicePulseAcks("):
            self.assertNotIn("vTaskDelay",function(SOURCE,name))
        self.assertNotIn("mqtt",function(navi,"linkRadioTask(").lower())
        self.assertNotIn("esp_now_send",function(navi,"servicePulseIngress("))
        self.assertNotIn("esp_wifi_set_channel",navi)
        self.assertIn('if(ir_input::Wireless && xTaskCreatePinnedToCore(linkRadioTask',navi)
        self.assertIn('if(callbackResult==ESP_OK && ir_input::Wireless)',navi)
        self.assertIn('if(!ir_input::Wireless)return false;',function(SOURCE,'enqueuePulse('))

if __name__ == "__main__":
    unittest.main()
