#!/usr/bin/env python3
"""Compile the sketch's actual CRC/callback/send functions against a host radio.

This is a deterministic host regression, not an ESP32 firmware compile.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SKETCH = ROOT / "firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX"
source = (SKETCH / "IR_SCOPE_ESPNOW_TX.ino").read_text()


def function(name):
    start = source.index(name)
    start = source.rfind("static ", 0, start)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def unicast_receive_branch():
    start = source.index("if(len==(int)sizeof(IrNaviLinkHelloPacket)")
    end = source.index("if(len==(int)sizeof(FusionAckPacket)", start)
    return "static void unicastReceive(const esp_now_recv_info_t *info,const uint8_t *data,int len){" + source[start:end] + "}"


harness = r'''
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstring>
#include "PulseEventEvidence.h"
#include "PulseTransportStatus.h"
using esp_err_t=int;
constexpr int ESP_OK=0, ESP_NOW_SEND_SUCCESS=0, ESP_NOW_SEND_FAIL=1;
using esp_now_send_status_t=int;
struct wifi_tx_info_t {};
struct esp_now_recv_info_t { uint8_t src_addr[6]; };
static const uint8_t BROADCAST[6]={255,255,255,255,255,255};
static std::atomic<bool> sendDone{true};
static esp_now_send_status_t sendStatus=ESP_NOW_SEND_FAIL;
static uint32_t sendErrors=0;
static std::atomic<uint32_t> radioTimeouts{0},radioBusyDrops{0};
static uint32_t now=0,calls=0,due=0;
static int immediate=ESP_OK, callback=ESP_NOW_SEND_SUCCESS;
static bool pending=false, automatic=true;
static uint8_t lastDestination[6]{};
struct LinkPeerUpdate { uint8_t mac[6]; uint32_t helloSequence; uint64_t naviBootId; };
using QueueHandle_t=LinkPeerUpdate*;
static LinkPeerUpdate peerUpdate{};
static bool peerUpdatePending=false;
static QueueHandle_t linkPeerQueue=&peerUpdate;
static uint8_t naviPeerMac[6]{};
static bool naviPeerReady=false;
static uint32_t naviHelloSequence=0;
static uint64_t naviHelloBoot=0;
struct esp_now_peer_info_t { uint8_t peer_addr[6]; uint8_t channel; bool encrypt; };
static bool peerInstalled=false;
static int xQueueOverwrite(QueueHandle_t, const void* item){std::memcpy(&peerUpdate,item,sizeof(peerUpdate));peerUpdatePending=true;return 1;}
static int xQueueReceive(QueueHandle_t, void* item, unsigned){if(!peerUpdatePending)return 0;std::memcpy(item,&peerUpdate,sizeof(peerUpdate));peerUpdatePending=false;return 1;}
static bool esp_now_is_peer_exist(const uint8_t*){return peerInstalled;}
static int esp_now_del_peer(const uint8_t*){peerInstalled=false;return ESP_OK;}
static int esp_now_add_peer(const esp_now_peer_info_t*){peerInstalled=true;return ESP_OK;}
static constexpr int ESP_ERR_ESPNOW_EXIST=123;
static constexpr int pdTRUE=1;
static constexpr int CHANNEL=11;
static const uint16_t MAGIC=0x4952;static const uint8_t VERSION=1,LINK_HELLO_TYPE=8;
static const uint32_t NAVI_TARGET_ID=9950011UL;
struct SerialMock { template<typename... T> void printf(const char*,T...){ } };
static SerialMock Serial;
static uint32_t millis(){return now;}
static unsigned pdMS_TO_TICKS(unsigned ms){return ms;}
static void onSent(const wifi_tx_info_t*,esp_now_send_status_t);
static void vTaskDelay(unsigned ms){
  now+=ms;
  if(automatic && pending && now>=due){pending=false;onSent(nullptr,callback);}
}
static esp_err_t esp_now_send(const uint8_t* destination,const uint8_t*,size_t){
  ++calls;
  std::memcpy(lastDestination,destination,sizeof(lastDestination));
  if(immediate!=ESP_OK)return immediate;
  assert(!pending);pending=true;due=now+2;return ESP_OK;
}
'''
harness += "\n".join([function(n) for n in ("crc16(", "onSent(", "radioSendTo(", "radioSend(", "serviceNaviPeer(")] + [unicast_receive_branch()])
harness += r'''
int main(){
  const uint8_t bytes[]={1,2,3};
  assert(radioSend(bytes,sizeof(bytes)) && calls==1 && sendErrors==0);
  assert(std::memcmp(lastDestination,BROADCAST,6)==0);
  IrNaviLinkHelloPacket hello{};hello.magic=0x4952;hello.version=1;hello.type=8;
  hello.locoId=9950011UL;hello.sequence=17;hello.naviBootId=99;
  hello.crc=crc16((const uint8_t*)&hello,sizeof(hello)-2);
  esp_now_recv_info_t info{{1,2,3,4,5,6}};
  unicastReceive(&info,(const uint8_t*)&hello,sizeof(hello));serviceNaviPeer();
  assert(naviPeerReady && peerInstalled && std::memcmp(naviPeerMac,info.src_addr,6)==0);
  assert(naviHelloSequence==17 && naviHelloBoot==99);
  const uint8_t wrongMac[6]={6,5,4,3,2,1};
  hello.locoId=9950012UL;hello.crc=crc16((const uint8_t*)&hello,sizeof(hello)-2);
  esp_now_recv_info_t wrong{{6,5,4,3,2,1}};
  unicastReceive(&wrong,(const uint8_t*)&hello,sizeof(hello));serviceNaviPeer();
  assert(std::memcmp(naviPeerMac,info.src_addr,6)==0 && std::memcmp(naviPeerMac,wrongMac,6)!=0);
  assert(radioSendTo(naviPeerMac,bytes,sizeof(bytes)) && std::memcmp(lastDestination,info.src_addr,6)==0);
  callback=ESP_NOW_SEND_FAIL;
  assert(!radioSend(bytes,sizeof(bytes)) && sendErrors==1);
  immediate=1;
  assert(!radioSend(bytes,sizeof(bytes)) && sendErrors==2 && sendDone);
  immediate=ESP_OK;automatic=false;
  assert(!radioSend(bytes,sizeof(bytes)) && radioTimeouts==1 && pending);
  const auto before=calls;
  for(unsigned i=0;i<3;++i)assert(!radioSend(bytes,sizeof(bytes)));
  assert(calls==before && radioBusyDrops==3); // No overlapping accepted sends.
  pending=false;onSent(nullptr,ESP_NOW_SEND_SUCCESS); // Late completion.
  automatic=true;callback=ESP_NOW_SEND_FAIL;
  assert(!radioSend(bytes,sizeof(bytes))); // Late success cannot mask new failure.
  callback=ESP_NOW_SEND_SUCCESS;assert(radioSend(bytes,sizeof(bytes)));
  assert(crc16((const uint8_t*)"123456789",9)==0x29b1); // CCITT-FALSE check value.
  PulseEventPacket e{};e.magic=0x4952;e.version=1;e.type=6;
  e.sid=42;e.sequence=7;e.bootId=123;e.completedPulses=7;
  e.completedUs=UINT64_C(0x100000000)+1000;e.intervalUs=40000;
  e.nominalUm=7*9652;e.pitchUm=9652;e.opticalReason=ir_movement::TRACKING;e.span=1007;
  e.crc=crc16((const uint8_t*)&e,offsetof(PulseEventPacket,crc));
  assert(e.crc==crc16((const uint8_t*)&e,sizeof(e)-2));
  // Every protected field is in CRC coverage (single-bit changes cannot pass).
  auto* p=(uint8_t*)&e;
  for(size_t i=0;i<offsetof(PulseEventPacket,crc);++i){
    p[i]^=1;assert(e.crc!=crc16(p,offsetof(PulseEventPacket,crc)));p[i]^=1;
  }
  std::puts("PASS broadcast compatibility, addressed NAVI peer selection, unicast delivery, immediate/callback failure, timeout, late callback isolation, recovery and CRC coverage");
}
'''
with tempfile.TemporaryDirectory(prefix="ir-pulse-transport-") as tmp:
    cpp = Path(tmp) / "test.cpp"
    exe = Path(tmp) / "test"
    cpp.write_text(harness)
    subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-I", str(SKETCH), str(cpp),
                    "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
