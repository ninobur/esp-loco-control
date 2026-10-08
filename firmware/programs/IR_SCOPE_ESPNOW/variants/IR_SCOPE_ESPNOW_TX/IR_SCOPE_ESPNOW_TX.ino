/*
 * IR_SCOPE_ESPNOW_CUMULATIVE_TX — 1 kHz IR waveform transmitter plus
 * read-only QUORUM/TEMPLATES CtoPeerPacket v3 receiver and onboard interval
 * comparison. Samples GPIO34; no local navigation or motor authority.
 * NAVI consumes its evidence, so detector changes can affect navigation.
 * R2: quiet-only retention, agreeing-cycle reference, visible continuity loss.
 * 1.7: additive type-6 completed-pulse evidence; no detector/filter changes.
 * Latest-value Type-6 delivery; no history recovery or application ACKs.
 * Spec: docs/NAVI_IR_PULSE_EVENT_PHYSICAL_EVIDENCE_20261006.md.
 * Built/reviewed diagnostic candidate; hardware acceptance pending. No flash
 * or NAV/AUTO authority follows from this instrumentation experiment.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "PulseEventEvidence.h"
#include <atomic>

static const uint8_t CHANNEL=11;
static const int SENSOR_PIN=34;
static const uint16_t MAGIC=0x4952;
static const uint8_t VERSION=1;
static const uint16_t FLAG_INPULSE=0x1000, FLAG_RISE=0x2000, FLAG_FALL=0x4000, FLAG_CONTRAST=0x8000;
// A 512 ms envelope follows shade/sun boundaries without attenuating the
// valid wheel modulation seen in the 2026-09-19 track replay.
static const int ENV_N=512, PRIME_N=128, MIN_SPAN=120, BATCH_N=96;
static const uint32_t ENV_UPDATE_MS=50;
static const uint32_t DEBOUNCE_US=15000, LATCH_MS=2500;
// Fixed Otto receiver, verified from the supplied locomotive configuration.
static const uint8_t OTTO_MAC[6]={0x68,0x09,0x47,0xAC,0x4E,0xB4};
static const uint8_t CTO_MAGIC=0xC4, CTO_VERSION=3, CTO_ECHO_MAGIC=0xC5;
static const uint32_t TOBY_ID=9950012UL;
static const uint8_t ACK_TYPE=4;
static const size_t RETAINED_INTERVALS=192;

struct __attribute__((packed)) Packet {
  uint16_t magic; uint8_t version, type;
  uint32_t sid, batchSeq, firstSample;
  uint16_t count, missedBefore;
  int16_t runMin,runMax,thrHigh,thrLow;
  uint32_t lateTotal,missedTotal,queueDrops,sendErrors,pulses,latch,contrastLoss;
  uint16_t sample[BATCH_N];
  uint16_t crc;
};
static_assert(sizeof(Packet)<=250,"ESP-NOW packet too large");
struct __attribute__((packed)) CtoPeerPacket {
  uint8_t magic,version; uint32_t senderId,sequence;
  uint8_t hallMm,frontBoundaryMm,rearBoundaryMm; int8_t mapDir;
  uint8_t autoMode,running,motionState,rampPwm,speedValid;
  uint16_t lastMoveAgeDs,speedX10;
  uint8_t frontOffset,rearOffset,truthSource,stationPhase,trafficPhase,mustHoldEligible;
  uint32_t trafficStopForId,senderRxAccepted,senderTxAttempts,senderTxImmediateErrors;
};
static_assert(sizeof(CtoPeerPacket)==45,"CTO v3 wire size changed");
static_assert(offsetof(CtoPeerPacket,senderId)==2,"CTO senderId offset changed");
static_assert(offsetof(CtoPeerPacket,senderTxAttempts)==37,"CTO txAttempts offset changed");

struct CtoRxItem {
  CtoPeerPacket packet; int8_t rssi; uint16_t opticalSpan;
  uint32_t receivedMs,localSample,localPulses,localSaturated;
};

// IR observation of one Toby status packet. New IR-family type 2; it does not
// consume 0xC6, which is already assigned to IrSpeedPacketV1 in the repo.
struct __attribute__((packed)) CtoObservationPacket {
  uint16_t magic; uint8_t version,type;
  uint32_t sid,reportSequence,receivedMs,localSample,localPulses;
  uint32_t senderId,senderSequence,senderTxAttempts,senderTxImmediateErrors;
  uint32_t rxAccepted,rxMissing,rxReboots,rxQueueDrops,sampleMissed,late;
  int8_t rssi; uint8_t hallMm; int8_t mapDir;
  uint8_t truthSource,motionState,rampPwm;
  uint16_t crc;
};
static_assert(sizeof(CtoObservationPacket)==72,"observation packet wire size changed");

enum FusionFlags : uint8_t {
  FUSION_STRUCTURALLY_VALID=0x01, FUSION_CTO_GAP=0x02,
  FUSION_SAMPLE_MISS=0x04, FUSION_ZERO_PULSES=0x08,
  FUSION_DIRECTION_CHANGE=0x10, FUSION_HALL_JUMP=0x20,
  FUSION_TOBY_REBOOT=0x40
};

// One report per observed Hall-MM change. Values are measurements, not a
// navigation verdict; flags name structural defects without a heuristic score.
struct __attribute__((packed)) FusionIntervalPacket {
  uint16_t magic; uint8_t version,type;
  uint32_t sid,reportSequence,senderSequence,receivedMs;
  uint32_t startMs,endMs,startPulses,endPulses,pulseDelta,sampleDelta;
  uint32_t senderTxAttempts,rxAccepted,rxMissing,sampleMissed,late,saturated,pulseRateX100;
  uint16_t opticalSpan,pulsesPerMarkerX100;
  uint8_t previousMm,currentMm,markerDelta; int8_t mapDir;
  uint8_t previousTruth,currentTruth,flags,reserved;
  uint16_t crc;
};
static_assert(sizeof(FusionIntervalPacket)==86,"fusion packet wire size changed");

struct __attribute__((packed)) FusionAckPacket {
  uint16_t magic; uint8_t version,type;
  uint32_t sid,reportSequence;
  uint16_t crc;
};
static_assert(sizeof(FusionAckPacket)==14,"fusion ACK wire size changed");

struct RetainedFusion {
  FusionIntervalPacket packet;
  uint32_t nextSendMs;
  uint16_t attempts;
  bool occupied;
};

static QueueHandle_t q,ctoRxQueue,observationQueue,fusionQueue,fusionAckQueue;
static uint32_t sid, sampleSeq=0, batchSeq=0, pendingMiss=0;
static volatile uint32_t lateTotal=0,missedTotal=0,queueDrops=0,pulses=0,latches=0,closs=0,saturatedSamples=0,sent=0;
static std::atomic<uint32_t> sendErrors{0};
static volatile uint32_t ctoCallbackAccepted=0,ctoRxQueueDrops=0,ctoBadLength=0,ctoBadVersion=0,ctoBadSender=0,ctoEchoes=0;
static uint32_t ctoAccepted=0,ctoMissing=0,ctoReboots=0,ctoReportSequence=0,observationDrops=0,observationSent=0;
static uint32_t fusionReportSequence=0,fusionDrops=0,fusionSent=0,fusionAcked=0,fusionRetries=0,fusionEvicted=0;
static RetainedFusion retained[RETAINED_INTERVALS];
static bool haveCto=false; static uint32_t lastCtoSequence=0,lastCtoTxAttempts=0;
static bool haveAnchor=false; static uint8_t anchorMm=0,anchorTruth=0; static int8_t anchorDir=0;
static uint32_t anchorMs=0,anchorSample=0,anchorPulses=0,anchorMissing=0,anchorReboots=0,anchorSampleMissed=0;
static std::atomic<bool> sendDone{true};
static esp_now_send_status_t sendStatus=ESP_NOW_SEND_FAIL;
static std::atomic<uint32_t> radioTimeouts{0},radioBusyDrops{0};
static int runMin=0,runMax=0;
// Delivery retains only current cumulative knowledge.  Replacing an unsent
// completion cannot lose cumulative distance: the replacement carries it.
static constexpr unsigned PULSE_QUEUE_DEPTH=1;
static QueueHandle_t movementQueue,pulseEventQueue;
static std::atomic<uint32_t> pulseEventGenerated{0},pulseEventSent{0};
static std::atomic<uint32_t> pulseEventFailed{0},pulseQueueHigh{0};
static std::atomic<uint32_t> pulseSendLagMaxUs{0};
static portMUX_TYPE movementMux=portMUX_INITIALIZER_UNLOCKED;
static ir_movement::WireSnapshot latestMovement;
static uint64_t movementBoot;

static uint16_t crc16(const uint8_t *p,size_t n){uint16_t c=0xffff;while(n--){c^=(uint16_t)*p++<<8;for(int i=0;i<8;i++)c=(c&0x8000)?(c<<1)^0x1021:c<<1;}return c;}
static void onSent(const wifi_tx_info_t*,esp_now_send_status_t s){sendStatus=s;sendDone=true;}

static void onReceive(const esp_now_recv_info_t *info,const uint8_t *data,int len){
  if(len==(int)sizeof(FusionAckPacket)){
    FusionAckPacket ack{};memcpy(&ack,data,sizeof(ack));uint16_t wireCrc=ack.crc;ack.crc=0;
    if(ack.magic==MAGIC&&ack.version==VERSION&&ack.type==ACK_TYPE&&wireCrc==crc16((uint8_t*)&ack,sizeof(ack)-2)&&ack.sid==sid){
      xQueueSend(fusionAckQueue,&ack.reportSequence,0);
      return;
    }
  }
  if(len<1)return;
  if(data[0]==CTO_ECHO_MAGIC){ctoEchoes++;return;}
  if(data[0]!=CTO_MAGIC)return;
  if(len!=(int)sizeof(CtoPeerPacket)){ctoBadLength++;return;}
  CtoRxItem item{}; memcpy(&item.packet,data,sizeof(item.packet));
  if(item.packet.version!=CTO_VERSION){ctoBadVersion++;return;}
  if(item.packet.senderId!=TOBY_ID){ctoBadSender++;return;}
  item.rssi=info&&info->rx_ctrl?info->rx_ctrl->rssi:0;
  item.receivedMs=millis(); item.localSample=sampleSeq; item.localPulses=pulses;
  item.localSaturated=saturatedSamples; item.opticalSpan=(uint16_t)max(0,runMax-runMin);
  if(xQueueSend(ctoRxQueue,&item,0)!=pdTRUE)ctoRxQueueDrops++; else ctoCallbackAccepted++;
}

static void sampler(void*) {
  Packet p{}; p.magic=MAGIC;p.version=VERSION;p.type=1;p.sid=sid;
  // Installed 9.652-mm pitch is settled; legacy calibration ID remains zero.
  ir_movement::Measurement measurement(movementBoot,0,9.652,true);
  uint64_t nextReport=0; uint32_t movementSequence=0;
  PulseEventEvidence pulseEvidence;
  ir_movement::Reason lastReason=ir_movement::PRIMING;
  TickType_t wake=xTaskGetTickCount(); uint64_t t0=esp_timer_get_time();
  for(;;){
    uint64_t us=esp_timer_get_time(); int64_t late=(int64_t)(us-t0)-(int64_t)sampleSeq*1000;
    if(late>=1000){uint32_t m=late/1000; sampleSeq+=m;missedTotal+=m;pendingMiss+=m;wake=xTaskGetTickCount();late-=m*1000;}
    if(late>250)lateTotal++;
    int raw=analogRead(SENSOR_PIN);
    measurement.sample(us,raw);
    const auto& detector=measurement.detector();
    const auto snapshot=measurement.snapshot();
    PulseEventPacket event{};
    if(pulseEvidence.observe(snapshot,detector,sid,event)){
      auto& e=event;
      e.crc=crc16((const uint8_t*)&e,offsetof(PulseEventPacket,crc));
      pulseEventGenerated++;
      xQueueOverwrite(pulseEventQueue,&e);
      const uint32_t depth=uxQueueMessagesWaiting(pulseEventQueue);
      if(depth>pulseQueueHigh.load())pulseQueueHigh=depth;
    }
    runMin=detector.low; runMax=detector.high;
    // Legacy type-1/fusion counters retain observed-rise semantics.
    pulses=(uint32_t)detector.rises; latches=(uint32_t)detector.aborts;
    saturatedSamples=(uint32_t)detector.saturated;
    if(snapshot.reason==ir_movement::INADEQUATE_CONTRAST && lastReason!=snapshot.reason)closs++;
    lastReason=snapshot.reason;
    int span=runMax-runMin,th=runMin+span*2/3,tl=runMin+span/3;uint16_t flags=0;
    if(span>=MIN_SPAN)flags|=FLAG_CONTRAST;
    if(detector.rise)flags|=FLAG_RISE;
    if(detector.fall)flags|=FLAG_FALL;
    if(detector.inPulse())flags|=FLAG_INPULSE;
    if(us>=nextReport){
      nextReport=us+100000;
      auto report=ir_movement::encode(snapshot,++movementSequence,span);
      report.crc=crc16((uint8_t*)&report,sizeof(report)-2);
      portENTER_CRITICAL(&movementMux);latestMovement=report;portEXIT_CRITICAL(&movementMux);
      xQueueOverwrite(movementQueue,&report);
    }
    if(p.count==0){p.batchSeq=batchSeq++;p.firstSample=sampleSeq;p.missedBefore=(uint16_t)min(pendingMiss,(uint32_t)65535);pendingMiss=0;p.runMin=runMin;p.runMax=runMax;p.thrHigh=th;p.thrLow=tl;}
    p.sample[p.count++]=(raw&0xfff)|flags;sampleSeq++;
    if(p.count==BATCH_N){p.lateTotal=lateTotal;p.missedTotal=missedTotal;p.queueDrops=queueDrops;p.sendErrors=sendErrors;p.pulses=pulses;p.latch=latches;p.contrastLoss=closs;p.crc=0;p.crc=crc16((uint8_t*)&p,sizeof(p)-2);if(xQueueSend(q,&p,0)!=pdTRUE)queueDrops++;p.count=0;}
    vTaskDelayUntil(&wake,pdMS_TO_TICKS(1));
  }
}

static bool radioSend(const uint8_t *data,size_t len){
  // A timed-out accepted send still owns the next callback. Never submit
  // another frame until it arrives; otherwise its result could be misattributed.
  if(!sendDone.load()){radioBusyDrops++;sendErrors++;return false;}
  sendDone=false;esp_err_t e=esp_now_send(OTTO_MAC,data,len);
  if(e!=ESP_OK){sendErrors++;sendDone=true;return false;}
  uint32_t start=millis();while(!sendDone&&millis()-start<100)vTaskDelay(pdMS_TO_TICKS(1));
  if(!sendDone.load()){radioTimeouts++;sendErrors++;return false;}
  if(sendStatus!=ESP_NOW_SEND_SUCCESS){sendErrors++;return false;}return true;
}

static void radio(void*){
  Packet p; CtoObservationPacket o; FusionIntervalPacket f;
  for(;;){
    uint32_t ackSequence;
    ir_movement::WireSnapshot movement; PulseEventPacket pulseEvent;
    // Preserve the established periodic Type-5 service: a pending Type-6
    // submission never gets ahead of the newest Type-5 operational snapshot.
    if(xQueueReceive(movementQueue,&movement,0)==pdTRUE)
      radioSend((uint8_t*)&movement,sizeof(movement));
    if(xQueueReceive(pulseEventQueue,&pulseEvent,0)==pdTRUE){
      const uint64_t lag=uint64_t(esp_timer_get_time())-pulseEvent.completedUs;
      const uint32_t boundedLag=lag>UINT32_MAX?UINT32_MAX:uint32_t(lag);
      if(boundedLag>pulseSendLagMaxUs.load())pulseSendLagMaxUs=boundedLag;
      if(radioSend((uint8_t*)&pulseEvent,sizeof(pulseEvent)))pulseEventSent++;
      else pulseEventFailed++;
      // One pulse per pass gives Type-5 and existing diagnostics service too.
    }
    while(xQueueReceive(fusionAckQueue,&ackSequence,0)==pdTRUE){
      for(size_t i=0;i<RETAINED_INTERVALS;i++)if(retained[i].occupied&&retained[i].packet.reportSequence==ackSequence){retained[i].occupied=false;fusionAcked++;break;}
    }
    while(xQueueReceive(fusionQueue,&f,0)==pdTRUE){
      int slot=-1;for(size_t i=0;i<RETAINED_INTERVALS;i++)if(!retained[i].occupied){slot=(int)i;break;}
      if(slot<0){uint32_t oldest=UINT32_MAX;for(size_t i=0;i<RETAINED_INTERVALS;i++)if(retained[i].packet.reportSequence<oldest){oldest=retained[i].packet.reportSequence;slot=(int)i;}fusionEvicted++;}
      retained[slot].packet=f;retained[slot].nextSendMs=0;retained[slot].attempts=0;retained[slot].occupied=true;
    }
    uint32_t now=millis();int due=-1;
    for(size_t i=0;i<RETAINED_INTERVALS;i++)if(retained[i].occupied&&(int32_t)(now-retained[i].nextSendMs)>=0){due=(int)i;break;}
    if(due>=0){
      RetainedFusion &r=retained[due];if(r.attempts)fusionRetries++;
      if(radioSend((uint8_t*)&r.packet,sizeof(r.packet)))fusionSent++;
      r.attempts++;r.nextSendMs=millis()+(r.attempts<4?250UL*r.attempts:5000UL);continue;
    }
    if(xQueueReceive(observationQueue,&o,0)==pdTRUE){if(radioSend((uint8_t*)&o,sizeof(o)))observationSent++;continue;}
    if(xQueueReceive(q,&p,0)==pdTRUE){if(radioSend((uint8_t*)&p,sizeof(p)))sent++;}
    vTaskDelay(pdMS_TO_TICKS(1)); // Yield only; pulse production is event-driven.
  }
}

void setup(){
  Serial.begin(115200);delay(300);analogReadResolution(12);pinMode(SENSOR_PIN,INPUT);sid=esp_random();q=xQueueCreate(30,sizeof(Packet));ctoRxQueue=xQueueCreate(16,sizeof(CtoRxItem));observationQueue=xQueueCreate(16,sizeof(CtoObservationPacket));fusionQueue=xQueueCreate(32,sizeof(FusionIntervalPacket));fusionAckQueue=xQueueCreate(32,sizeof(uint32_t));if(!q||!ctoRxQueue||!observationQueue||!fusionQueue||!fusionAckQueue){Serial.println("FATAL queue");while(1)delay(1000);}
  movementBoot=((uint64_t)esp_random()<<32)|sid;if(!movementBoot)movementBoot=1;
  movementQueue=xQueueCreate(1,sizeof(ir_movement::WireSnapshot));pulseEventQueue=xQueueCreate(PULSE_QUEUE_DEPTH,sizeof(PulseEventPacket));
  if(!movementQueue||!pulseEventQueue){Serial.println("FATAL movement/pulse queue");while(1)delay(1000);}
  WiFi.mode(WIFI_STA);WiFi.disconnect(false,true);esp_wifi_set_channel(CHANNEL,WIFI_SECOND_CHAN_NONE);
  if(esp_now_init()!=ESP_OK){Serial.println("FATAL esp_now_init");while(1)delay(1000);}esp_now_register_send_cb(onSent);esp_now_register_recv_cb(onReceive);
  esp_now_peer_info_t peer{};memcpy(peer.peer_addr,OTTO_MAC,6);peer.channel=CHANNEL;peer.encrypt=false;if(esp_now_add_peer(&peer)!=ESP_OK){Serial.println("FATAL add_peer");while(1)delay(1000);}
  Serial.printf("READY IR_SCOPE_ESPNOW_CUMULATIVE_TX sid=%08lx pin=%d rate=1000 env=%d update=%lu prime=%d mingate=%d channel=%u raw=%u cto=%u obs=%u fusion=%u retained=%u target=%lu mac=%s\n",(unsigned long)sid,SENSOR_PIN,ENV_N,(unsigned long)ENV_UPDATE_MS,PRIME_N,MIN_SPAN,CHANNEL,(unsigned)sizeof(Packet),(unsigned)sizeof(CtoPeerPacket),(unsigned)sizeof(CtoObservationPacket),(unsigned)sizeof(FusionIntervalPacket),(unsigned)RETAINED_INTERVALS,(unsigned long)TOBY_ID,WiFi.macAddress().c_str());
  Serial.printf("PULSE FORMAT boot=%016llx type=6 version=1 bytes=%u pitch_um=%lu latest_capacity=%u interval_zero=unavailable\n",(unsigned long long)movementBoot,(unsigned)sizeof(PulseEventPacket),(unsigned long)ir_movement::kInstalledPitchUm,PULSE_QUEUE_DEPTH);
  xTaskCreatePinnedToCore(sampler,"sample",4096,nullptr,2,nullptr,0);xTaskCreatePinnedToCore(radio,"radio",4096,nullptr,1,nullptr,1);
}
static uint32_t statusAt = 0;
void loop(){
  static uint32_t movementStatusAt=0;
  if(millis()-movementStatusAt>=5000){
    movementStatusAt=millis();ir_movement::WireSnapshot m;
    portENTER_CRITICAL(&movementMux);m=latestMovement;portEXIT_CRITICAL(&movementMux);
    Serial.printf("MOVE CUMULATIVE_TX boot=%016llx t_us=%llu rises=%llu completed=%llu nominal_um=%llu reason=%u span=%u unreliable=%llu gaps=%llu sat=%llu distance_valid=0\n",
      (unsigned long long)m.bootId,(unsigned long long)m.capturedUs,
      (unsigned long long)m.observedRises,(unsigned long long)m.completedPulses,
      (unsigned long long)m.nominalUm,m.opticalReason,m.span,
      (unsigned long long)m.unreliableSamples,(unsigned long long)m.sampleGaps,
      (unsigned long long)m.saturatedSamples);
  }
  static uint32_t pulseStatusAt=0;
  if(millis()-pulseStatusAt>=1000){
    pulseStatusAt=millis();
    Serial.printf("PULSE_STAT boot=%016llx generated=%lu sent=%lu fail=%lu q=%u qhigh=%lu lag_max_us=%lu timeout=%lu busy_drop=%lu\n",
      (unsigned long long)movementBoot,(unsigned long)pulseEventGenerated.load(),
      (unsigned long)pulseEventSent.load(),(unsigned long)pulseEventFailed.load(),
      (unsigned)uxQueueMessagesWaiting(pulseEventQueue),(unsigned long)pulseQueueHigh.load(),
      (unsigned long)pulseSendLagMaxUs.load(),
      (unsigned long)radioTimeouts.load(),(unsigned long)radioBusyDrops.load());
  }
  CtoRxItem item;
  while(xQueueReceive(ctoRxQueue,&item,0)==pdTRUE){
    const CtoPeerPacket& p=item.packet;
    if(haveCto){
      if(p.sequence<lastCtoSequence || p.senderTxAttempts<lastCtoTxAttempts){ctoReboots++;}
      else if(p.sequence>lastCtoSequence){ctoMissing+=p.sequence-lastCtoSequence-1;}
    }
    haveCto=true;lastCtoSequence=p.sequence;lastCtoTxAttempts=p.senderTxAttempts;ctoAccepted++;
    CtoObservationPacket o{};o.magic=MAGIC;o.version=VERSION;o.type=2;o.sid=sid;o.reportSequence=++ctoReportSequence;
    o.receivedMs=item.receivedMs;o.localSample=item.localSample;o.localPulses=item.localPulses;
    o.senderId=p.senderId;o.senderSequence=p.sequence;o.senderTxAttempts=p.senderTxAttempts;o.senderTxImmediateErrors=p.senderTxImmediateErrors;
    o.rxAccepted=ctoAccepted;o.rxMissing=ctoMissing;o.rxReboots=ctoReboots;o.rxQueueDrops=ctoRxQueueDrops;o.sampleMissed=missedTotal;o.late=lateTotal;
    o.rssi=item.rssi;o.hallMm=p.hallMm;o.mapDir=p.mapDir;o.truthSource=p.truthSource;o.motionState=p.motionState;o.rampPwm=p.rampPwm;
    o.crc=0;o.crc=crc16((uint8_t*)&o,sizeof(o)-2);if(xQueueSend(observationQueue,&o,0)!=pdTRUE)observationDrops++;

    if(!haveAnchor && p.mapDir!=0){
      haveAnchor=true;anchorMm=p.hallMm;anchorTruth=p.truthSource;anchorDir=p.mapDir;
      anchorMs=item.receivedMs;anchorSample=item.localSample;anchorPulses=item.localPulses;
      anchorMissing=ctoMissing;anchorReboots=ctoReboots;anchorSampleMissed=missedTotal;
    }else if(haveAnchor && p.hallMm!=anchorMm){
      FusionIntervalPacket f{};f.magic=MAGIC;f.version=VERSION;f.type=3;f.sid=sid;f.reportSequence=++fusionReportSequence;
      f.senderSequence=p.sequence;f.receivedMs=item.receivedMs;f.startMs=anchorMs;f.endMs=item.receivedMs;
      f.startPulses=anchorPulses;f.endPulses=item.localPulses;f.pulseDelta=item.localPulses-anchorPulses;
      f.sampleDelta=item.localSample-anchorSample;f.senderTxAttempts=p.senderTxAttempts;f.rxAccepted=ctoAccepted;
      f.rxMissing=ctoMissing;f.sampleMissed=missedTotal;f.late=lateTotal;f.saturated=item.localSaturated;
      f.opticalSpan=item.opticalSpan;f.previousMm=anchorMm;f.currentMm=p.hallMm;f.mapDir=p.mapDir;
      f.previousTruth=anchorTruth;f.currentTruth=p.truthSource;
      if(anchorDir==1&&p.mapDir==1)f.markerDelta=(uint8_t)((p.hallMm+171-anchorMm)%171);
      else if(anchorDir==-1&&p.mapDir==-1)f.markerDelta=(uint8_t)((anchorMm+171-p.hallMm)%171);
      else f.flags|=FUSION_DIRECTION_CHANGE;
      if(ctoMissing!=anchorMissing)f.flags|=FUSION_CTO_GAP;
      if(missedTotal!=anchorSampleMissed)f.flags|=FUSION_SAMPLE_MISS;
      if(f.pulseDelta==0)f.flags|=FUSION_ZERO_PULSES;
      if(ctoReboots!=anchorReboots)f.flags|=FUSION_TOBY_REBOOT;
      if(f.markerDelta==0||f.markerDelta>10)f.flags|=FUSION_HALL_JUMP;
      if(anchorTruth==2&&p.truthSource==2&&anchorDir==p.mapDir&&p.mapDir!=0&&f.markerDelta>=1&&f.markerDelta<=10&&!(f.flags&(FUSION_SAMPLE_MISS|FUSION_TOBY_REBOOT)))f.flags|=FUSION_STRUCTURALLY_VALID;
      if(f.markerDelta){uint32_t ppm=f.pulseDelta*100UL/f.markerDelta;f.pulsesPerMarkerX100=(uint16_t)min(ppm,65535UL);}
      {uint32_t dt=item.receivedMs-anchorMs;if(dt)f.pulseRateX100=(uint32_t)((uint64_t)f.pulseDelta*100000ULL/dt);}
      f.crc=0;f.crc=crc16((uint8_t*)&f,sizeof(f)-2);if(xQueueSend(fusionQueue,&f,0)!=pdTRUE)fusionDrops++;
      anchorMm=p.hallMm;anchorTruth=p.truthSource;anchorDir=p.mapDir;anchorMs=item.receivedMs;
      anchorSample=item.localSample;anchorPulses=item.localPulses;anchorMissing=ctoMissing;
      anchorReboots=ctoReboots;anchorSampleMissed=missedTotal;
    }
  }
  if(millis()-statusAt>=5000){statusAt=millis();uint32_t retainedNow=0;for(size_t i=0;i<RETAINED_INTERVALS;i++)if(retained[i].occupied)retainedNow++;Serial.printf("STAT samples=%lu rawsent=%lu obssent=%lu fsent=%lu fack=%lu fretry=%lu fretained=%lu fevict=%lu qdrop=%lu odrop=%lu fdrop=%lu senderr=%lu missed=%lu late=%lu pulses=%lu sat=%lu cto=%lu cmiss=%lu reboot=%lu crxdrop=%lu echo=%lu bad=%lu/%lu/%lu q=%u/%u/%u/%u\n",(unsigned long)sampleSeq,(unsigned long)sent,(unsigned long)observationSent,(unsigned long)fusionSent,(unsigned long)fusionAcked,(unsigned long)fusionRetries,(unsigned long)retainedNow,(unsigned long)fusionEvicted,(unsigned long)queueDrops,(unsigned long)observationDrops,(unsigned long)fusionDrops,(unsigned long)sendErrors,(unsigned long)missedTotal,(unsigned long)lateTotal,(unsigned long)pulses,(unsigned long)saturatedSamples,(unsigned long)ctoAccepted,(unsigned long)ctoMissing,(unsigned long)ctoReboots,(unsigned long)ctoRxQueueDrops,(unsigned long)ctoEchoes,(unsigned long)ctoBadLength,(unsigned long)ctoBadVersion,(unsigned long)ctoBadSender,(unsigned)uxQueueMessagesWaiting(q),(unsigned)uxQueueMessagesWaiting(ctoRxQueue),(unsigned)uxQueueMessagesWaiting(observationQueue),(unsigned)uxQueueMessagesWaiting(fusionQueue));}
  delay(5);
}
