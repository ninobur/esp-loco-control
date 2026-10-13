
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
struct SyncPoint { uint64_t receivedUs=0, pulses=0, boot=0; uint32_t seq=0; bool valid=false,continuous=false; };
static portMUX_TYPE syncMux=portMUX_INITIALIZER_UNLOCKED;
static SyncPoint syncType5{},syncType6{};
static uint32_t syncType6Gap=0,syncType6Bad=0;
struct __attribute__((packed)) SyncPulseWire {
  uint16_t magic; uint8_t version,type; uint32_t sid,sequence;
  uint64_t bootId,completedPulses,completedUs,intervalUs,nominalUm;
  uint32_t pitchUm; uint8_t opticalReason; uint16_t span,crc;
};
static_assert(sizeof(SyncPulseWire)==61,"Type-6 contract mismatch");
static uint16_t syncCrc(const uint8_t* data,size_t n){
  uint16_t c=0xffff;
  while(n--){c^=uint16_t(*data++)<<8;
    for(int i=0;i<8;++i)c=(c&0x8000)?uint16_t((c<<1)^0x1021):uint16_t(c<<1);}
  return c;
}
struct IrRx { uint8_t mac[6]; uint64_t receivedUs; uint8_t bytes[110]; uint16_t length; };

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
static void onIr(const esp_now_recv_info_t* info,const uint8_t* bytes,int length){
  if(!info || !bytes || length<4 || length>110 ||
     bytes[0]!=0x52 || bytes[1]!=0x49 || bytes[2]!=1 ||
     !((bytes[3]==5 && length==110) || (bytes[3]==6 && length==61)))return;
  IrRx packet;memcpy(packet.mac,info->src_addr,6);packet.receivedUs=esp_timer_get_time();
  memcpy(packet.bytes,bytes,length);packet.length=length;
  if(irQ && xQueueSend(irQ,&packet,0)!=pdTRUE)irQueueDrops=irQueueDrops+1;
}
static void serviceMovement(){
  if(irQueueDrops!=handledIrDrops){
    movement.transportGap();
    // Exclude pre-gap queued packets from starting a shadow epoch across loss.
    irHealth.queueGap(esp_timer_get_time());
    handledIrDrops=irQueueDrops;
  }
  IrRx packet;
  while(irQ && xQueueReceive(irQ,&packet,0)==pdTRUE){
    if(packet.length==sizeof(SyncPulseWire) && packet.bytes[3]==6){
      SyncPulseWire e{};memcpy(&e,packet.bytes,sizeof(e));
      if(e.magic!=0x4952 || e.version!=1 || e.type!=6 || !e.bootId ||
         !e.completedPulses || !e.completedUs || !e.pitchUm ||
         e.completedPulses>UINT64_MAX/e.pitchUm ||
         e.nominalUm!=e.completedPulses*uint64_t(e.pitchUm) ||
         syncCrc(packet.bytes,59)!=e.crc){++syncType6Bad;continue;}
      portENTER_CRITICAL(&syncMux);
      const SyncPoint old=syncType6;
      const bool same=old.valid && old.boot==e.bootId;
      const uint32_t step=e.sequence-old.seq;
      if(same && (step==0 || step>=0x80000000u ||
                  e.completedPulses<=old.pulses)){
        ++syncType6Bad;
      }else{
        if(same && (step!=1 || e.completedPulses-old.pulses!=1))++syncType6Gap;
        syncType6={packet.receivedUs,e.completedPulses,e.bootId,e.sequence,true,
                   same && step==1 && e.completedPulses-old.pulses==1};
      }
      portEXIT_CRITICAL(&syncMux);
      continue; // Never feed Type-6 to the legacy Type-5 odometer.
    }
    if(ngr_nav::movementCrc(packet.bytes,108)==uint16_t(packet.bytes[108]|uint16_t(packet.bytes[109])<<8)){
      memcpy(lastSeenMac,packet.mac,6);++seenFrames;
    }
    const ngr_nav::RxResult movementResult =
      movement.receive(packet.mac,packet.bytes,110,packet.receivedUs);
    irHealth.receive(packet.mac,packet.bytes,110,packet.receivedUs);
    if (movementResult == ngr_nav::RxResult::Accepted ||
        movementResult == ngr_nav::RxResult::Reset) {
      // This is the actual accepted ~100 ms IR stream, not serviceIr()'s 1 Hz
      // dashboard/status publication. The snapshot remains byte-for-byte
      // available inside wire; recorder output cannot influence either
      // movement or health admission.
      const auto& latest=movement.latest();
      portENTER_CRITICAL(&syncMux);
      syncType5={packet.receivedUs,latest.completedPulses,latest.bootId,
                 latest.sequence,true,true};
      portEXIT_CRITICAL(&syncMux);
      naviSyncRecorder.addIr(
        packet.receivedUs,packet.mac,
        movementResult == ngr_nav::RxResult::Reset ? 2 : 1,
        movement.latest(),
        (uint8_t)irHealth.health().fault,
        (uint8_t)irHealth.health().readiness,
        (uint8_t)actualPwm,(uint8_t)commandedPwm,naviSyncContext());
    }
  }
  irHealth.tick(esp_timer_get_time());
}


static CapturedHall capture(){CapturedHall pending{};
      pending.capturedUs=esp_timer_get_time();
      portENTER_CRITICAL(&syncMux);
      pending.type5AtOpening=syncType5;
      pending.type6AtOpening=syncType6;
      portEXIT_CRITICAL(&syncMux);
return pending;}
static void report(const CapturedHall& j){
    const auto a=j.type5AtOpening,b=j.type6AtOpening;
    const bool aOk=a.valid && a.receivedUs<=j.capturedUs;
    const bool bOk=b.valid && b.continuous && b.receivedUs<=j.capturedUs &&
                   (!aOk || a.boot==b.boot);
    Serial.printf("[IR_SYNC] hall=%lu t_us=%llu t5=%llu t5_age_ms=%lld "
                  "t6=%llu t6_age_ms=%lld t6_minus_t5=%lld gaps=%lu bad=%lu "
                  "authority=OBSERVE_ONLY\n",
                  (unsigned long)j.serial,(unsigned long long)j.capturedUs,
                  (unsigned long long)(aOk?a.pulses:0),
                  aOk?(long long)((j.capturedUs-a.receivedUs)/1000):-1LL,
                  (unsigned long long)(bOk?b.pulses:0),
                  bOk?(long long)((j.capturedUs-b.receivedUs)/1000):-1LL,
                  aOk&&bOk?(long long)b.pulses-(long long)a.pulses:0LL,
                  (unsigned long)syncType6Gap,(unsigned long)syncType6Bad);
}

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
