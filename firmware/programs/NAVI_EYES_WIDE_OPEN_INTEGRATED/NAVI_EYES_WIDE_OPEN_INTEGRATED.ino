/* EWO integrated working-sketch candidate. NOT FIELD ACCEPTED.
 * Type-6 pulse evidence is observation-only; Type-5/control unchanged.
 * Spec: docs/NAVI_PULSE_EVENT_PHYSICAL_SPEED_OBSERVATION_20261006.md.
 * PWM-zero movement requires operator declaration, 2026-10-02 (0120 supersedes 0119).
 * Rollback: ab0938b (R2_FT2 flashed to Otto); earlier c3c925a, d0185be.
 * No flashing/deployment authorized by this change; await David/Sam review.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <Preferences.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <limits.h>
#include <atomic>

// EWO hardware target: Otto's reviewed X22 profile. The profile supplies
// LOCO_ID, pins, motor/PWM, direction, IR pairing and battery configuration.
// Its legacy polarity macro is intentionally not a NAVI authority here.
#include "../NAVI_ONE/variants/NAVI_ONE_X22/LocoConfig.h"
#ifdef HALL_POLARITY_INVERTED
#undef HALL_POLARITY_INVERTED
#endif
#ifndef NAVI_MAX_OPERATING_PWM
#define NAVI_MAX_OPERATING_PWM NORMAL_PWM
#endif
#include "credentials.h"
#include "../NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/Ops.h"
#include "../NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/Stations.h"
#include "EwoStationStop.h"
#include "NaviIntegratedCore.h"
#include "NaviEstop.h"
#include "NaviCompatibility.h"
#include "NaviPulseTelemetry.h"
#include "../IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX/ReliablePulseTransport.h"

static portMUX_TYPE recorderMux = portMUX_INITIALIZER_UNLOCKED;
#define NAVI_SYNC_ENTER_CRITICAL() portENTER_CRITICAL(&recorderMux)
#define NAVI_SYNC_EXIT_CRITICAL() portEXIT_CRITICAL(&recorderMux)
#include "NaviSyncRecorder.h"

using namespace navi_one;
using namespace navi_eyes;

static constexpr char SKETCH_NAME[] = "NAVI_EYES_WIDE_OPEN_INTEGRATED_IR_RELIABLE_RX_TEST";
static constexpr char BUILD_CLASS[] = "INTEGRATION_CANDIDATE_NOT_FIELD_ACCEPTED";
static constexpr uint8_t HALL_PIN = 33;
static constexpr uint8_t I2C_SDA = 21, I2C_SCL = 22;
static constexpr uint16_t AUTO_STEP_UP_MS = 62, AUTO_STEP_DOWN_MS = 31;
static constexpr uint16_t MANUAL_STEP_UP_MS = 150;
static constexpr uint16_t BRAKE_STEP_COAST_MS = 400, BRAKE_STEP_HARD_MS = 15;
static constexpr uint16_t NAVI_SYNC_PORT = 47620;
static constexpr char NAVI_SYNC_HOST[] = "192.168.68.142";
static constexpr char MQTT_BROKER[] = "192.168.68.142";
// Each PubMsg is 1273 bytes. A 48-entry queue exhausted Otto's usable heap
// before Wi-Fi association; 16 connected, but startup drops were seen in the
// reviewed build and sustained-load/reconnect behavior remains unverified.
static constexpr uint8_t PUB_QUEUE_DEPTH = 16;

struct IrRx { uint8_t mac[6]; uint64_t receivedUs; uint16_t length;
  uint8_t pwmAtReceive, commandedAtReceive; navi_sync::Context contextAtReceive;
  uint8_t bytes[110]; };
static_assert(sizeof(PulseEventPacket)==61,"Type-6 contract changed");
static_assert(sizeof(PulseEventPacket)<=sizeof(IrRx::bytes),"Type-6 exceeds legacy ingress buffer");
static_assert(sizeof(navi_pulse::Rx::bytes)==sizeof(PulseEventPacket),"pulse ingress size");
struct PubMsg { char topic[72]; char payload[1200]; bool retain; };
struct CmdMsg { char topic[72]; char payload[64]; uint64_t order; uint64_t receivedUs; };

static NaviIntegratedCore navi;
static StationMachine stationMachine;
static EwoStationStopProfile measuredStationStop;
// The recorder owns large fixed native Hall/IR/NAVI rings. Keep the pointer
// small in internal DRAM so ESP-IDF can create the Arduino app task; allocate
// the unchanged recorder after setup() begins.
static navi_sync::Recorder* recorder = nullptr;
static QueueHandle_t hallQ = nullptr, irQ = nullptr, pubQ = nullptr, cmdQ = nullptr;
// Experimental loss never enters irQueueDrops/serviceObservationLoss or NAVI.
static QueueHandle_t pulseQ=nullptr,pulseStatusQ=nullptr;
static navi_pulse::Observation pulseObservation;
static navi_pulse::TransportObservation pulseTransportObservation;
struct LinkRx { uint64_t receivedUs; uint16_t length; uint8_t bytes[sizeof(ir_link::Status)]; };
struct LinkOutput { uint8_t mac[6]; uint16_t length; uint8_t bytes[sizeof(ir_link::Ack)]; };
static_assert(sizeof(ir_link::Event)<=sizeof(LinkRx::bytes),"link ingress size");
static_assert(sizeof(ir_link::Hello)<=sizeof(LinkOutput::bytes),"link egress size");
static ir_link::Receiver reliablePulses;
static QueueHandle_t linkAckQ=nullptr,linkHelloQ=nullptr,linkSummaryQ=nullptr;
static std::atomic<uint32_t> linkUnknown{0},linkRxLoss{0},linkAckLoss{0},linkMacOK{0},linkMacFail{0},linkTimeouts{0};
static portMUX_TYPE pairMux=portMUX_INITIALIZER_UNLOCKED;
static uint8_t admittedIrMac[6]{};
static bool linkPairChanged=false;
static uint64_t linkSession=0;
struct LinkSummary {
  ir_link::Status tx{};
  uint64_t session=0,boot=0,received=0,missing=0,highest=0,priorMissing=0;
  uint64_t receivedUs=0,lowerUs=0,upperUs=0,acceptanceLowerUs=0,statusUs=0;
  uint64_t previousBoot=0;uint32_t previousUnresolved=0;bool previousStatusKnown=false;
  uint32_t duplicates=0,historical=0,invalid=0,stale=0,boots=0,statusGaps=0,statusOrder=0;
};
static std::atomic<uint32_t> pulseMqttDrops{0};
static volatile uint32_t hallQueueDrops = 0, irQueueDrops = 0;
static uint32_t reportedHallDrops = 0, reportedIrDrops = 0;
static uint32_t irPacketInvalid = 0, pubDrops = 0, cmdDrops = 0;
static volatile uint32_t nextHallSerial = 0;
static uint64_t bootId = 0;
static uint32_t naviSyncSession = 0, naviSyncUdpFailures = 0, naviSyncDatagrams = 0;
static WiFiUDP naviSyncUdp;
static IPAddress naviSyncDest;
static bool naviSyncDestValid = false;
static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);
static Preferences pairing;
static uint8_t pairedIrMac[6]{};
static uint32_t seenIrFrames = 0;
static std::atomic<bool> radioReady{false};
static bool irCarCoupled = false;
static const uint8_t IR_LINK_BROADCAST[6]={0xff,0xff,0xff,0xff,0xff,0xff};
static uint32_t irLinkHelloSequence=0,irLinkHelloAt=0;
static bool haveNetReport = false, lastWifiConnected = false, lastMqttConnected = false;
static int lastMqttState = 0;
static uint32_t lastConnectivityReportMs = 0;

static void onWifiDisconnect(WiFiEvent_t, WiFiEventInfo_t info) {
  Serial.printf("[NET] wifi_disconnect_reason=%u\n",
                static_cast<unsigned>(info.wifi_sta_disconnected.reason));
}

static volatile int actualPwm = 0, commandedPwm = 0;
static int rampTarget = 0;
static uint16_t rampUpMs = MANUAL_STEP_UP_MS, rampDownMs = 0;
static uint32_t lastRampStepMs = 0;
static uint8_t brakeValue = 0, motorDirection = 1;
static volatile int8_t sessionDir = 0;
static bool autoEnrolled = false, autoRunning = false;
static bool estopped = false, lowVoltage = false;
static std::atomic<bool> estopAsserted{false};
static portMUX_TYPE estopMux = portMUX_INITIALIZER_UNLOCKED;
static OrderedEstop orderedEstop;
static Adafruit_INA219 ina219;
static bool inaReady = false;
static float busV = 0, busA = 0, busW = 0;
static uint8_t lowVoltCount = 0;
static bool warnSticky = false;

static portMUX_TYPE contextMux = portMUX_INITIALIZER_UNLOCKED;
static navi_sync::Context publishedContext;
static uint64_t activeCommandOrder = 0, activeCommandReceivedUs = 0;

static navi_sync::Context recorderContext() {
  portENTER_CRITICAL(&contextMux);
  const navi_sync::Context c = publishedContext;
  portEXIT_CRITICAL(&contextMux);
  return c;
}
static void refreshRecorderContext() {
  navi_sync::Context c;
  c.navMm = navi.declared() && navi.positionReliable() ? navi.mm() : navi_sync::MM_NA;
  c.navDir = navi.direction();
  c.stationPhase = static_cast<uint8_t>(stationMachine.phase());
  uint8_t flags = 0;
  if (navi.declared() && navi.positionReliable()) flags |= navi_sync::CTX_NAV_KNOWN;
  if (autoEnrolled) flags |= navi_sync::CTX_AUTO_ENROLLED;
  if (autoRunning) flags |= navi_sync::CTX_AUTO_RUNNING;
  if (estopped || estopAsserted) flags |= navi_sync::CTX_ESTOP;
  if (lowVoltage) flags |= navi_sync::CTX_LOW_VOLT;
  if (actualPwm == 0) flags |= navi_sync::CTX_HOLD;
  c.flags = flags;
  portENTER_CRITICAL(&contextMux);
  publishedContext = c;
  portEXIT_CRITICAL(&contextMux);
}

static void recordInput(navi_sync::InputKind kind, uint64_t judgmentUs,
                        uint64_t observationUs, uint32_t serial, uint8_t pwm,
                        uint8_t commanded, int8_t direction) {
  refreshRecorderContext();
  const auto c = recorderContext();
  navi_sync::ConsumptionItem item{};
  item.id = navi.consumptionId() + 1;
  item.decisionUs = judgmentUs; item.observationUs = observationUs; item.serial = serial;
  item.kind = uint8_t(kind); item.pwm = pwm; item.commandedPwm = commanded;
  item.direction = direction; item.navMm = c.navMm; item.target = navi.target().sequence;
  item.stationPhase = c.stationPhase; item.contextFlags = c.flags;
  recorder->addConsumption(item, c);
}
// Loop-task only. CommandReceived is recorded separately in the callback and
// deliberately has no NAVI consumption ID: receipt is not consumption.
static void recordAction(navi_sync::ActionKind kind, const char* topic = "",
                         const char* payload = "") {
  navi_sync::ActionSnapshot a{};
  a.tUs = esp_timer_get_time(); a.lastConsumptionId = navi.consumptionId();
  a.commandOrder = activeCommandOrder; a.commandReceivedUs = activeCommandReceivedUs;
  a.kind = uint8_t(kind); a.pwm = actualPwm; a.targetPwm = commandedPwm;
  a.rampUpMs = rampUpMs; a.rampDownMs = rampDownMs;
  strlcpy(a.topic, topic, sizeof(a.topic)); strlcpy(a.payload, payload, sizeof(a.payload));
  refreshRecorderContext(); recorder->addAction(a, recorderContext());
}
static void serviceObservationLoss() {
  const uint32_t hallLost = hallQueueDrops, irLost = irQueueDrops;
  if (hallLost == reportedHallDrops && irLost == reportedIrDrops) return;
  reportedHallDrops = hallLost; reportedIrDrops = irLost;
  const uint64_t now = esp_timer_get_time();
  recordInput(navi_sync::InputKind::Loss, now, now, hallLost, actualPwm, commandedPwm, navi.direction());
  navi.noteObservationLoss(hallLost, irLost, now);
}

static bool pub(const char* leaf, const char* payload, bool retain = false) {
  if (!pubQ) return false;
  PubMsg m{};
  snprintf(m.topic, sizeof(m.topic), "ngr/loco/%s/%s", LOCO_NAME, leaf);
  if (strlen(payload) >= sizeof(m.payload)) { ++pubDrops; return false; }
  strlcpy(m.payload, payload, sizeof(m.payload));
  m.retain = retain;
  if (xQueueSend(pubQ, &m, 0) != pdTRUE) { ++pubDrops; return false; }
  return true;
}
static void warn(const char* message, bool sticky = false) {
  if (sticky) warnSticky = true;
  pub("state/warning", message, true);
  Serial.printf("[WARN] %s\n", message);
}
static void clearWarning() {
  if (!warnSticky) pub("state/warning", "", true);
}
static void writePwm(int value) {
  ledcWrite(MOTOR_PWM_PIN, value);
  static int lastRecorded = -1;
  if (value != lastRecorded) {
    lastRecorded = value;
    recordAction(navi_sync::ActionKind::AppliedPwm);
  }
}
static uint16_t brakeStepMs() {
  return uint16_t(BRAKE_STEP_COAST_MS -
    (uint32_t(BRAKE_STEP_COAST_MS - BRAKE_STEP_HARD_MS) * brakeValue) / 255U);
}
static void requestPwm(int target, uint16_t up, uint16_t down,
                       bool manual = false) {
  target = constrain(target, 0, manual ? 255 : int(NAVI_MAX_OPERATING_PWM));
  const bool changed = target != rampTarget || up != rampUpMs || down != rampDownMs;
  rampTarget = target;
  commandedPwm = target;
  rampUpMs = up;
  rampDownMs = down;
  if (changed) recordAction(navi_sync::ActionKind::RequestedPwm);
}
static void serviceRamp() {
  digitalWrite(MOTOR_DIR_PIN, motorDirection ? HIGH : LOW);
  if (estopped || estopAsserted) {
    estopped = true;
    autoRunning = false;
    actualPwm = 0; commandedPwm = 0; rampTarget = 0;
    writePwm(0);
    return;
  }
  if (lowVoltage && rampTarget != 0) {
    rampTarget = 0; commandedPwm = 0;
    rampDownMs = AUTO_STEP_DOWN_MS;
  }
  if (actualPwm == rampTarget) return;
  const bool down = rampTarget < actualPwm;
  const uint16_t step = down ? (rampDownMs ? rampDownMs : brakeStepMs())
                             : (rampUpMs ? rampUpMs : 1);
  const uint32_t now = millis();
  if (now - lastRampStepMs < step) return;
  lastRampStepMs = now;
  actualPwm += down ? -1 : 1;
  writePwm(actualPwm);
}
static void withdraw(const char* why) {
  autoRunning = autoEnrolled = false;
  requestPwm(0, 0, AUTO_STEP_DOWN_MS);
  pub("state/auto", "0", true);
  pub("state/nav_ready", "0", true);
  warn(why, true);
}

// Check the durable NAVI latch, not its finite event queue. Withdraw once for
// each new movement count and refuse any AUTO restart until declaration.
// Manual positioning remains available after withdrawal.
static void servicePwmZeroMovementHold() {
  static uint32_t reportedDisplacements = 0;
  if (!navi.pwmZeroMovementRequiresDeclaration()) return;
  if (reportedDisplacements != navi.pwmZeroDisplacements() || autoEnrolled || autoRunning) {
    reportedDisplacements = navi.pwmZeroDisplacements();
    withdraw(kPwmZeroMovementWarning);
  }
}

// CRC/length checks are wire acquisition facts, not movement qualification.
static uint16_t movementCrc(const uint8_t* bytes, size_t length) {
  uint16_t crc = 0xffff;
  while (length--) {
    crc ^= uint16_t(*bytes++) << 8;
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = crc & 0x8000 ? uint16_t((crc << 1) ^ 0x1021) : uint16_t(crc << 1);
  }
  return crc;
}
static void hallTask(void*) {
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    navi_sync::HallSample wire{};
    const uint64_t firstUs = esp_timer_get_time();
    for (unsigned i = 0; i < 5; ++i) {
      HallSample observation{};
      nextHallSerial = nextHallSerial + 1;
      observation.sampleSerial = nextHallSerial;
      observation.timestampUs = esp_timer_get_time();
      observation.raw = analogRead(HALL_PIN);  // ONE ADC conversion = ONE observation
      observation.pwm = static_cast<uint8_t>(actualPwm);
      const int8_t routeDir = motorDirection ? sessionDir : -sessionDir;
      observation.direction = routeDir > 0 ? 1 : routeDir < 0 ? 2 : 0;
      wire.raw[i] = static_cast<uint16_t>(observation.raw);
      if (hallQ && xQueueSend(hallQ, &observation, 0) != pdTRUE)
        hallQueueDrops = hallQueueDrops + 1;
      navi_sync::NativeHallItem item{};
      item.serial = observation.sampleSerial;
      item.tUs = observation.timestampUs;
      item.raw = observation.raw;
      item.pwm = observation.pwm;
      item.direction = observation.direction;
      recorder->addNativeHall(item, recorderContext());
    }
    wire.median = INT16_MIN;  // no acquisition median; field is legacy NSR1 layout
    wire.pwmActual = static_cast<uint8_t>(actualPwm);
    wire.pwmCommanded = static_cast<uint8_t>(commandedPwm);
    wire.flags = motorDirection ? navi_sync::HALL_F_DIR_FWD : 0;
    if (actualPwm == 0) wire.flags |= navi_sync::HALL_F_HOLD;
      recorder->addHall(firstUs, uint32_t(firstUs / 1000), wire, recorderContext());
    vTaskDelayUntil(&wake, 1);
  }
}
static void onIr(const esp_now_recv_info_t* info, const uint8_t* bytes,
                 int length) {
  if (!info || !bytes) return;
  // Reliable and legacy experimental frames never enter operational Type-5.
  if((length>=4 && bytes[0]==0x52 && bytes[1]==0x49 && bytes[3]>=6 && bytes[3]<=10) ||
     length==int(sizeof(PulseEventPacket)) || length==int(sizeof(PulseTransportStatusPacket))){
    if(!ir_input::Wireless)return;
    uint8_t allowed[6];portENTER_CRITICAL(&pairMux);memcpy(allowed,admittedIrMac,6);portEXIT_CRITICAL(&pairMux);
    if(!ir_link::configured(allowed) || !ir_link::sameMac(allowed,info->src_addr)){++linkUnknown;return;}
    LinkRx item{};item.receivedUs=esp_timer_get_time();item.length=length;
    if(length>0 && length<=int(sizeof(item.bytes)))memcpy(item.bytes,bytes,length);
    if(!pulseQ || xQueueSend(pulseQ,&item,0)!=pdTRUE)++linkRxLoss;
    return;
  }
  // All other traffic retains the existing Type-5 acquisition path.
  IrRx rx{};
  memcpy(rx.mac, info->src_addr, 6);
  rx.receivedUs = esp_timer_get_time();
  rx.length = length < 0 ? 0 : static_cast<uint16_t>(length);
  rx.pwmAtReceive = static_cast<uint8_t>(actualPwm);
  rx.commandedAtReceive = static_cast<uint8_t>(commandedPwm);
  rx.contextAtReceive = recorderContext();
  if (length > 0 && length <= int(sizeof(rx.bytes))) memcpy(rx.bytes, bytes, length);
  if (irQ && xQueueSend(irQ, &rx, 0) != pdTRUE)
    irQueueDrops = irQueueDrops + 1;
}
static void serviceIrIngress() {
  IrRx rx;
  while (irQ && xQueueReceive(irQ, &rx, 0) == pdTRUE) {
    ++seenIrFrames;
    if (rx.length != sizeof(ir_movement::WireSnapshot)) {
      ++irPacketInvalid;
      continue;
    }
    ir_movement::WireSnapshot wire;
    memcpy(&wire, rx.bytes, sizeof(wire));
    if (wire.magic != 0x4952 || wire.version != 1 || wire.type != 5 ||
        !wire.bootId || wire.distanceValidated ||
        wire.opticalReason > ir_movement::TRACKING ||
        wire.completedPulses > wire.observedRises ||
        !ir_movement::validConfiguredDistance(wire) ||
        movementCrc(rx.bytes, offsetof(ir_movement::WireSnapshot, crc)) != wire.crc) {
      ++irPacketInvalid;
      continue;
    }
    serviceObservationLoss();
    const uint64_t judgedUs = esp_timer_get_time();
    recordInput(navi_sync::InputKind::Ir, judgedUs, rx.receivedUs, wire.sequence,
                rx.pwmAtReceive, rx.commandedAtReceive, rx.contextAtReceive.navDir);
    navi.observeIr(wire, rx.receivedUs, rx.pwmAtReceive, rx.mac, judgedUs);
    recorder->addIr(rx.receivedUs, rx.mac, 1, wire,
                   navi.irHealthFault(), navi.irReadiness(),
                   rx.pwmAtReceive, rx.commandedAtReceive, rx.contextAtReceive);
  }
}

// Loop-owned observation and comparison snapshots. Only read-only NAVI access.
static navi_pulse::Report pulseReport() {
  navi_pulse::Report r{}; r.pulse=pulseObservation.state();
  r.transport=pulseTransportObservation.state();
  r.comparedUs=esp_timer_get_time(); r.legacyValid=navi.irSpeedAvailable(r.comparedUs);
  r.legacyPkph=r.legacyValid?navi.irSpeedMmS()/EWO_PKPH_MM_PER_SEC:0;
  r.legacyBoot=navi.latestIr().bootId; r.coupled=irCarCoupled;
  r.legacySameSource=r.pulse.have && r.legacyBoot==r.pulse.event.bootId &&
    memcmp(navi.latestIrMac(),r.pulse.mac,6)==0;
  return r;
}
static void servicePulseIngress() {
  if(!ir_input::Wireless)return; // DIRECT producer is intentionally absent in this revision.
  if(linkPairChanged){
    linkPairChanged=false;linkSession=(uint64_t(esp_random())<<32)|esp_random();if(!linkSession)linkSession=1;
    reliablePulses.start(linkSession);pulseObservation=navi_pulse::Observation{};
  }
  ir_link::Ack ack{};ack.session=linkSession;ack.txBoot=reliablePulses.boot;
  LinkRx rx{};
  // Bounded diagnostic work AFTER control; no radio, serial or MQTT wait.
  for(unsigned i=0;i<4 && pulseQ && xQueueReceive(pulseQ,&rx,0)==pdTRUE;++i){
    if(rx.length==sizeof(ir_link::Join)){
      ir_link::Join j{};memcpy(&j,rx.bytes,sizeof(j));
      if(ir_link::valid(j,10) && j.session==linkSession && j.txBoot && j.challenge==irLinkHelloSequence){
        if(j.txBoot!=reliablePulses.boot){
          reliablePulses.bind(j.txBoot);pulseObservation=navi_pulse::Observation{};
          ack.count=0;ack.txBoot=j.txBoot;
        }
        ++irLinkHelloSequence;irLinkHelloAt=millis()-1000; // Consume this challenge, reject stale joins.
      } else ++reliablePulses.invalid;
    } else if(rx.length==sizeof(ir_link::Event)){
      ir_link::Event e{};memcpy(&e,rx.bytes,sizeof(e));ir_link::AckItem item{};
      const int result=reliablePulses.accept(e,rx.receivedUs,esp_timer_get_time(),item);
      if(result){ack.items[ack.count++]=item;}
      if(result==1){ // Historical recovery and duplicates never update the current pulse observation.
        ir_input::Arrival native{};memcpy(native.sourceMac,pairedIrMac,6);native.receivedUs=rx.receivedUs;
        native.queueDrops=linkRxLoss.load();
        native.evidence=e.pulse; // Immutable common evidence, no delivery envelope downstream.
        pulseObservation.receiveNative(native);
      }
    } else if(rx.length==sizeof(ir_link::Status)){
      ir_link::Status st{};memcpy(&st,rx.bytes,sizeof(st));reliablePulses.observeStatus(st,rx.receivedUs);
    } else ++reliablePulses.invalid;
  }
  if(ack.count){
    ir_link::seal(ack);LinkOutput out{};memcpy(out.mac,pairedIrMac,6);out.length=sizeof(ack);memcpy(out.bytes,&ack,sizeof(ack));
    if(xQueueSend(linkAckQ,&out,0)!=pdTRUE)++linkAckLoss; // Sender retains evidence and will retry.
  }
  static uint32_t lastStatus=0;
  if(millis()-lastStatus>=1000){
    lastStatus=millis();const auto& r=reliablePulses;LinkSummary s{};
    s.tx=r.status;s.session=r.session;s.boot=r.boot;s.received=r.received;s.missing=r.missing;s.highest=r.highest;
    s.priorMissing=r.priorMissing;s.receivedUs=r.currentReceivedUs;s.lowerUs=r.currentLowerUs;s.upperUs=r.currentUpperUs;
    s.acceptanceLowerUs=r.acceptanceLowerUs;
    s.statusUs=r.lastStatusUs;s.duplicates=r.duplicates;s.historical=r.historical;s.invalid=r.invalid;s.stale=r.stale;
    s.previousBoot=r.previousBoot;s.previousUnresolved=r.previousUnresolved;s.previousStatusKnown=r.previousStatusKnown;
    s.boots=r.bootChanges;s.statusGaps=r.statusGaps;s.statusOrder=r.statusOrder;
    xQueueOverwrite(linkSummaryQ,&s);
    auto report=pulseReport();
    // Keep the existing comparison topic as a periodic summary only.
    // Its current-valid flag cannot treat a newly arrived historical pulse as fresh.
    const uint64_t now=esp_timer_get_time();
    const uint64_t age=now>=r.currentReceivedUs?now-r.currentReceivedUs:0;
    const bool timely=r.currentUpperUs && r.currentUpperUs<=ir_link::DiagnosticUs &&
      age<=ir_link::DiagnosticUs-r.currentUpperUs;
    report.transportFresh=timely;
    xQueueOverwrite(pulseStatusQ,&report);
  }
}

static const char* eventName(EwoEventKind kind) {
  switch (kind) {
    case EwoEventKind::Declared: return "DECLARED";
    case EwoEventKind::Reversed: return "REVERSED";
    case EwoEventKind::HallSupport: return "HALL_SUPPORT";
    case EwoEventKind::TargetConfirmed: return "TARGET_CONFIRMED";
    case EwoEventKind::MissedMagnet: return "MISSED_MAGNET";
    case EwoEventKind::ReferenceReady: return "INITIAL_REFERENCE";
    case EwoEventKind::SpatialClearance: return "SPATIAL_CLEARANCE";
    case EwoEventKind::SpatialCollect: return "SPATIAL_COLLECT";
    case EwoEventKind::SpatialReady: return "SPATIAL_REFERENCE";
    case EwoEventKind::SpatialEmpty: return "SPATIAL_EMPTY_RETAINED_REFERENCE";
    case EwoEventKind::IrDistanceHold: return "IR_DISTANCE_HOLD";
    case EwoEventKind::IrDistanceReady: return "IR_DISTANCE_READY";
    case EwoEventKind::PwmZeroDisplacement: return "PWM_ZERO_IR_DISPLACEMENT";
    case EwoEventKind::ObservationLoss: return "OBSERVATION_LOSS";
    case EwoEventKind::SpatialInvalidated: return "SPATIAL_INVALIDATED";
    default: return "NONE";
  }
}
static void publishEvents() {
  EwoEvent e;
  while (navi.takeEvent(e)) {
    char payload[480];
    snprintf(payload, sizeof(payload),
      "{\"event\":\"%s\",\"hall_serial\":%lu,\"mm\":%u,\"target\":%u,"
      "\"dir\":%d,\"ir_um\":%llu,\"median5\":%d,\"reference\":%d,"
      "\"ir_distance_holding\":%u,\"opening_serial\":%lu,\"opening_ir_um\":%llu,"
      "\"position_reliable\":%u,\"spatial_phase\":%u,\"decision_us\":%llu,"
      "\"consumption_id\":%llu,\"ir_seq\":%lu}",
      eventName(e.kind), (unsigned long)e.hallSerial, e.mm, e.target, e.direction,
      (unsigned long long)e.irUm, e.median, e.reference, e.distanceHolding ? 1 : 0,
      (unsigned long)e.openingSerial, (unsigned long long)e.openingIrUm,
      e.positionReliable ? 1 : 0, e.spatialPhase, (unsigned long long)e.timestampUs,
      (unsigned long long)e.consumptionId, (unsigned long)e.irSequence);
    pub("nav/evidence", payload);
    navi_sync::NaviSnapshot recorded{};
    recorded.tUs = e.timestampUs;
    recorded.irUm = e.irUm;
    recorded.openingIrUm = e.openingIrUm;
    recorded.hallSerial = e.hallSerial;
    recorded.openingSerial = e.openingSerial;
    recorded.hallQueueDrops = e.hallQueueDrops;
    recorded.irQueueDrops = e.irQueueDrops;
    recorded.median = e.median;
    recorded.reference = e.reference;
    recorded.kind = static_cast<uint8_t>(e.kind);
    recorded.mm = e.mm;
    recorded.target = e.target;
    recorded.direction = e.direction;
    recorded.degraded = e.distanceHolding; // NSR1 v2 slot retained for compatibility
    recorded.positionReliable = e.positionReliable;
    recorded.spatialPhase = e.spatialPhase;
    recorded.consumptionId = e.consumptionId;
    recorded.irSequence = e.irSequence;
    recorder->addNavi(recorded, recorderContext());
    if (e.kind == EwoEventKind::TargetConfirmed)
      measuredStationStop.noteAcceptedHall(e.mm, e.openingIrUm,
                                           navi.latestIr().pitchUm);
    if (e.kind == EwoEventKind::TargetConfirmed || e.kind == EwoEventKind::MissedMagnet)
      pub("mm/marker", payload);
    if (e.kind == EwoEventKind::PwmZeroDisplacement && navi.pwmZeroMovementRequiresDeclaration())
      warn(kPwmZeroMovementWarning, true);
  }
}

static int8_t travelDirection() {
  if (!sessionDir) return 0;
  return motorDirection ? sessionDir : -sessionDir;
}
static Ops opsNow() {
  Ops o;
  o.positionKnown = navi.declared() && navi.positionReliable() &&
                    !navi.pwmZeroMovementRequiresDeclaration();
  o.enrolled = autoEnrolled; o.running = autoRunning;
  o.estopped = estopped; o.lowVoltage = lowVoltage;
  o.forward = motorDirection; o.actualPwm = actualPwm;
  o.commandedPwm = commandedPwm; o.sessionDir = sessionDir;
  o.safeDirPwm = SAFE_DIRECTION_CHANGE_PWM;
  return o;
}
static void declarePosition(uint8_t mm, int8_t direction, const char* interval) {
  const uint64_t now = esp_timer_get_time();
  recordInput(navi_sync::InputKind::Declaration, now, activeCommandReceivedUs, mm,
              actualPwm, commandedPwm, direction);
  navi.declare(mm, direction, now);
  stationMachine.reset();
  measuredStationStop.reset();
  warnSticky = false; pub("state/warning", "", true);
  char value[20]; snprintf(value, sizeof(value), "%u", mm);
  pub("state/start_mm", value, true);
  if (interval && *interval) pub("state/start_interval", interval, true);
  else {
    char range[16];
    snprintf(range, sizeof(range), "%03u-%03u",
             direction > 0 ? mm : nextMarker(mm, -1),
             direction > 0 ? nextMarker(mm, 1) : mm);
    pub("state/start_interval", range, true);
  }
  pub("state/nav_ready", "1", true);
}
static void reversePosition(int8_t direction) {
  const uint64_t now = esp_timer_get_time();
  recordInput(navi_sync::InputKind::Reversal, now, activeCommandReceivedUs,
              direction > 0 ? 1 : 2, actualPwm, commandedPwm, direction);
  navi.reverse(direction, now);
  stationMachine.reset();
  measuredStationStop.reset();
}
static void onMqtt(char* topic, byte* payload, unsigned length) {
  CmdMsg command{};
  command.receivedUs = esp_timer_get_time();
  strlcpy(command.topic, topic, sizeof(command.topic));
  const unsigned count = length < sizeof(command.payload) - 1
                             ? length : sizeof(command.payload) - 1;
  memcpy(command.payload, payload, count);
  command.payload[count] = 0;
  const char* leaf = strrchr(command.topic, '/');
  leaf = leaf ? leaf + 1 : command.topic;
  bool assertion = false;
  if (!strcmp(leaf, "estop")) {
    bool stop = true;
    parseEstop(command.payload, stop);
    assertion = stop;
  }
  portENTER_CRITICAL(&estopMux);
  command.order = orderedEstop.received(assertion);
  estopAsserted = orderedEstop.asserted(); // independent of queue insertion
  portEXIT_CRITICAL(&estopMux);
  navi_sync::ActionSnapshot arrival{};
  arrival.tUs = command.receivedUs; arrival.commandOrder = command.order;
  arrival.commandReceivedUs = command.receivedUs;
  arrival.kind = uint8_t(navi_sync::ActionKind::CommandReceived);
  arrival.pwm = actualPwm; arrival.targetPwm = commandedPwm;
  strlcpy(arrival.topic, command.topic, sizeof(arrival.topic));
  strlcpy(arrival.payload, command.payload, sizeof(arrival.payload));
  recorder->addAction(arrival, recorderContext());
  if (cmdQ && xQueueSend(cmdQ, &command, 0) != pdTRUE) ++cmdDrops;
}
static void handleCommand(const CmdMsg& command) {
  activeCommandOrder = command.order; activeCommandReceivedUs = command.receivedUs;
  recordAction(navi_sync::ActionKind::CommandConsumed, command.topic, command.payload);
  const char* leaf = strrchr(command.topic, '/');
  leaf = leaf ? leaf + 1 : command.topic;
  const bool dispatcher = strstr(command.topic, "/dispatcher/") != nullptr;
  const Ops o = opsNow();
  if (!strcmp(leaf, "ir_pair")) {
    if (Refusal reason = admitDeclaration(o)) { warn(reason); return; }
    unsigned b[6]; char extra;
    if (sscanf(command.payload, "%2x:%2x:%2x:%2x:%2x:%2x%c",
               &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &extra) != 6) {
      warn("IR PAIR: expected six-byte MAC"); return;
    }
    uint8_t proposedMac[6];for(unsigned i=0;i<6;++i)proposedMac[i]=b[i];
    if(!ir_link::configured(proposedMac)){warn("IR PAIR: expected a unicast MAC");return;}
    const bool changed=!ir_link::sameMac(proposedMac,pairedIrMac);
    memcpy(pairedIrMac,proposedMac,6);
    pairing.putBytes("ir_mac", pairedIrMac, 6);
    portENTER_CRITICAL(&pairMux);memcpy(admittedIrMac,pairedIrMac,6);portEXIT_CRITICAL(&pairMux);
    linkPairChanged=linkPairChanged || changed;
    warn("Type-6/7 source admission updated; Type-5 processing unchanged");
  } else if (!strcmp(leaf, "ir_coupled")) {
    if (strcmp(command.payload, "0") && strcmp(command.payload, "1")) {
      warn("IR COUPLED: expected 0 or 1"); return;
    }
    irCarCoupled = command.payload[0] == '1';
  } else if (!strcmp(leaf, "estop")) {
    bool stop = true;
    const bool understood = parseEstop(command.payload, stop);
    portENTER_CRITICAL(&estopMux);
    estopped = orderedEstop.apply(command.order, stop);
    estopAsserted = orderedEstop.asserted();
    portEXIT_CRITICAL(&estopMux);
    if (estopped) { autoRunning = false; requestPwm(0, 0, 1); }
    if (!understood) warn("ESTOP: unreadable payload, assumed STOP", true);
    else if (estopped) warn(stop ? "ESTOP" : "ESTOP: older release ignored", true);
    else clearWarning();
    pub("state/estop", estopped ? "1" : "0", true);
  } else if (!strcmp(leaf, "session_direction")) {
    int8_t direction;
    if (!parseSessionDir(command.payload, direction)) { warn("SESSION_DIRECTION: expected CW or CCW"); return; }
    if (Refusal reason = admitDeclaration(o)) { warn(reason); return; }
    sessionDir = direction;
    pub("state/session_direction", direction > 0 ? "CW" : "CCW", true);
    if (navi.declared() && travelDirection() != navi.direction()) {
      reversePosition(travelDirection());
    }
  } else if (!strcmp(leaf, "start_interval")) {
    if (Refusal reason = admitStartMarker(o)) { warn(reason); return; }
    int a, b;
    if (!parseInterval(command.payload, a, b) || a < 0 || b < 0 ||
        a >= ROUTE_N || b >= ROUTE_N || nextMarker(a, 1) != b) {
      warn("START_INTERVAL REFUSED: expected adjacent AAA-BBB"); return;
    }
    declarePosition(travelDirection() > 0 ? a : b, travelDirection(), command.payload);
  } else if (!strcmp(leaf, "start_mm")) {
    if (Refusal reason = admitStartMarker(o)) { warn(reason); return; }
    int value;
    if (!parseInt(command.payload, value) || value < 0 || value >= ROUTE_N) {
      warn("START_MM REFUSED: invalid marker"); return;
    }
    declarePosition(value, travelDirection(), nullptr);
  } else if (!strcmp(leaf, "brake")) {
    int value;
    if (!parseInt(command.payload, value)) { warn("BRAKE REFUSED: invalid value"); return; }
    brakeValue = constrain(value, 0, 255);
    char text[8]; snprintf(text, sizeof(text), "%u", brakeValue);
    pub("state/brake", text, true);
  } else if (!strcmp(leaf, "dispatcher_release")) {
    autoRunning = autoEnrolled = false;
    requestPwm(0, 0, AUTO_STEP_DOWN_MS);
    pub("state/auto", "0", true);
    warn("RELEASED by dispatcher");
  } else if (!strcmp(leaf, "auto")) {
    bool want;
    if (!parseBool(command.payload, want)) { warn("AUTO REFUSED: expected 0 or 1"); return; }
    if (!want) { autoRunning = autoEnrolled = false; requestPwm(0, 0, AUTO_STEP_DOWN_MS); }
    else if (Refusal reason = admitAuto(o)) { warn(reason); return; }
    else autoEnrolled = true;
    pub("state/auto", autoEnrolled ? "1" : "0", true);
  } else if (!strcmp(leaf, "go") || (dispatcher && strstr(command.topic, "/go/"))) {
    if (Refusal reason = admitGo(o)) { warn(reason); return; }
    autoRunning = true;
    clearWarning();
  } else if (!strcmp(leaf, "stop") || (dispatcher && strstr(command.topic, "/stop/"))) {
    autoRunning = false;
    requestPwm(0, 0, AUTO_STEP_DOWN_MS);
  } else if (!strcmp(leaf, "throttle")) {
    if (Refusal reason = admitThrottle(o)) { warn(reason); return; }
    int value;
    if (!parseInt(command.payload, value)) { warn("THROTTLE REFUSED: invalid value"); return; }
    requestPwm(value, MANUAL_STEP_UP_MS, 0, true);
  } else if (!strcmp(leaf, "direction")) {
    bool forward;
    if (!parseMotorDir(command.payload, forward)) { warn("DIRECTION REFUSED: expected 0 or 2"); return; }
    if (Refusal reason = admitMotorDirection(o)) { warn(reason); return; }
    if (actualPwm || commandedPwm) { warn("DIRECTION REFUSED: stop fully first"); return; }
    motorDirection = forward ? 1 : 0;
    if (navi.declared() && travelDirection() != navi.direction()) {
      reversePosition(travelDirection());
    }
  }
}

static void serviceBattery() {
  static uint32_t last = 0;
  if (!inaReady || millis() - last < 5000) return;
  last = millis();
  busV = ina219.getBusVoltage_V();
  busA = ina219.getCurrent_mA() / 1000.0f;
  busW = ina219.getPower_mW() / 1000.0f;
  if (busV < DISCONNECTED_VOLTAGE_THRESHOLD) lowVoltCount = 0;
  else if (busV < SHUTDOWN_VOLTAGE) {
    if (lowVoltCount < VOLTAGE_COUNTER_LIMIT) ++lowVoltCount;
  } else lowVoltCount = 0;
  if (!lowVoltage && lowVoltCount >= VOLTAGE_COUNTER_LIMIT) {
    lowVoltage = true;
    autoRunning = false;
    requestPwm(0, 0, AUTO_STEP_DOWN_MS);
    warn("LOW VOLTAGE: controlled stop", true);
  } else if (lowVoltage && busV >= RECOVERY_VOLTAGE) {
    lowVoltage = false;
    lowVoltCount = 0;
  }
  pub("state/lowvolt", lowVoltage ? "1" : "0", true);
  char text[24];
  snprintf(text, sizeof(text), "%.2f", double(busV)); pub("telem/voltage", text, true);
  snprintf(text, sizeof(text), "%.2f", double(busA)); pub("telem/current", text, true);
  snprintf(text, sizeof(text), "%.2f", double(busW)); pub("telem/power", text, true);
}

static void publishStationEvent(const char* event, const char* station,
                                int16_t offset, const EwoStationStopDemand& demand,
                                uint8_t pwm) {
  char payload[900];
  snprintf(payload, sizeof(payload),
    "{\"event\":\"%s\",\"station\":\"%s\",\"phase\":\"%s\","
    "\"brake_phase\":\"%s\",\"off\":%d,\"pwm\":%u,"
    "\"actual_pwm\":%d,\"commanded_pwm\":%d,\"target_pkph\":%.3f,"
    "\"measured_pkph\":%.3f,\"distance_mm\":%.3f,"
    "\"ir_pulses\":%llu,\"reference_ir_um\":%llu,"
    "\"nominal_down_ms\":%.1f,\"applied_down_ms\":%.1f,"
    "\"rate_factor\":%.2f,\"speed_drop_per_pwm\":%.5f,"
    "\"projected_stop_mm\":%.3f,\"target_stop_mm\":%.3f,"
    "\"judgment\":\"%s\",\"ir_unavailable\":%u}",
    event, station ? station : "", stPhaseName(stationMachine.phase()),
    brakePhaseName(demand.phase), offset, unsigned(pwm), actualPwm, commandedPwm,
    demand.targetPkph, demand.measuredPkph, demand.distanceMm,
    (unsigned long long)demand.irPulses,
    (unsigned long long)demand.referenceIrUm, demand.nominalDownMs,
    demand.appliedDownMs, demand.rateFactor, demand.speedDropPerPwm,
    demand.projectedStopMm, demand.targetStopMm,
    brakeJudgmentName(demand.judgment), demand.irUnavailable ? 1 : 0);
  pub("state/station", payload);
  recordAction(navi_sync::ActionKind::StationOrder, station ? station : "", event);
}

static void serviceStation() {
  static bool irUnavailableReported = false;
  const uint32_t now = millis();
  stationMachine.setRunning(autoRunning, now);
  if (!autoRunning) { irUnavailableReported = false; return; }
  if (!navi.declared() || !navi.positionReliable()) {
    withdraw("Position relationship unreliable; AUTO withdrawn. Manual available.");
    return;
  }

  const uint8_t currentMm = navi.mm();
  const int8_t direction = navi.direction();
  const uint8_t cruise = cruisePwmAt(currentMm, direction, NAVI_AUTO_CRUISE_PWM);
  const StPhase phaseBefore = stationMachine.phase();

  // The legacy station machine remains responsible for identifying the visit,
  // dwell timing and ordinary departure. Its ZERO_RAMP order is deliberately
  // made non-authoritative here: a fake nonzero actuator value prevents that
  // phase from entering DWELL, and its PWM=0 order is never applied.
  const uint8_t lifecyclePwm = phaseBefore == StPhase::Ramp ? 1 :
                               static_cast<uint8_t>(actualPwm);
  StationOrder order = stationMachine.tick(currentMm, direction, lifecyclePwm,
                                            cruise, now);
  if (order.event && (!strcmp(order.event, "MISSED") ||
                      !strcmp(order.event, "PHASE_TIMEOUT"))) {
    withdraw("Station approach failed: controlled stop; Manual available.");
    return;
  }

  const int8_t stationIndex = stationMachine.stationIdx();
  if (stationMachine.phase() == StPhase::Idle || stationIndex < 0) {
    if (order.event && strcmp(order.event, "ZERO_RAMP")) {
      EwoStationStopDemand noDemand;
      publishStationEvent(order.event, order.station, order.offset, noDemand,
                          order.pwm);
    }
    measuredStationStop.reset();
    irUnavailableReported = false;
    if (rampTarget != cruise) requestPwm(cruise, AUTO_STEP_UP_MS, AUTO_STEP_DOWN_MS);
    return;
  }

  const StationDefinition& station = STATIONS[stationIndex];
  const uint64_t nowUs = esp_timer_get_time();
  const auto latestIr = navi.latestIr();
  const bool irDistanceValid = navi.irApplicable(nowUs);
  const bool irSpeedValid = navi.irSpeedAvailable(nowUs);
  if (!measuredStationStop.activeFor(station.centre, direction)) {
    measuredStationStop.begin(station.centre, direction, currentMm,
                              latestIr.nominalUm, latestIr.completedPulses,
                              actualPwm,
                              irSpeedValid ? navi.irSpeedMmS() / EWO_PKPH_MM_PER_SEC : 0.0,
                              BRAKE_STEP_COAST_MS, stationPwm(station, direction),
                              STATION_STOP_STEP_MS, latestIr.capturedUs);
  }

  // Once the measured stop has entered DWELL or DEPART, preserve the existing
  // lifecycle behavior. The measured profile owns only approach and stopping.
  if (stationMachine.phase() == StPhase::Dwell ||
      stationMachine.phase() == StPhase::Depart) {
    if (order.setThrottle)
      requestPwm(order.pwm, order.stepMs ? order.stepMs : AUTO_STEP_UP_MS,
                 order.stepMs ? order.stepMs : AUTO_STEP_DOWN_MS);
    if (order.event && strcmp(order.event, "ZERO_RAMP")) {
      EwoStationStopDemand noDemand;
      publishStationEvent(order.event, station.name, order.offset, noDemand,
                          order.pwm);
    }
    if (stationMachine.phase() == StPhase::Idle) measuredStationStop.reset();
    return;
  }

  EwoStationStopDemand demand = measuredStationStop.demand(
      currentMm, latestIr.nominalUm, latestIr.completedPulses, latestIr.capturedUs,
      irDistanceValid, irSpeedValid, navi.irSpeedMmS(), actualPwm);
  if (!demand.available) {
    const char* reason = !strcmp(demand.reason, "STATION_APPROACH_IR_REQUIRED")
        ? "Station approach IR reference unavailable; AUTO withdrawn."
        : !strcmp(demand.reason, "STATION_APPROACH_ENTRY_MISSED")
            ? "Station approach entry missed; AUTO withdrawn."
            : demand.referenceRequired
                ? "Station 0 Hall reference unavailable; AUTO withdrawn. No later-marker fallback."
                : "Station adaptive stop coordinate unavailable; AUTO withdrawn. No PWM/Hall fallback.";
    publishStationEvent(demand.reason, station.name,
                        ewoStationOffsetToCentre(currentMm, direction, station.centre),
                        demand, static_cast<uint8_t>(actualPwm));
    withdraw(reason);
    return;
  }

  if (demand.irUnavailable != irUnavailableReported) {
    irUnavailableReported = demand.irUnavailable;
    publishStationEvent(demand.reason, station.name,
                        ewoStationOffsetToCentre(currentMm, direction, station.centre),
                        demand, static_cast<uint8_t>(demand.pwmTarget));
  }

  // During approach/final braking the controller changes only the requested
  // down-ramp timing. It never converts a speed target directly to PWM.
  requestPwm(demand.pwmTarget, demand.pwmUpMs, demand.pwmDownMs);

  // Suppress the old station-offset ZERO_RAMP publication. The final-stop
  // event is emitted only after the candidate's accepted Station 0 reference.
  if (order.event && strcmp(order.event, "ZERO_RAMP"))
    publishStationEvent(order.event, station.name, order.offset, demand,
                        static_cast<uint8_t>(demand.pwmTarget));
  if (measuredStationStop.takeFinalStart())
    publishStationEvent("FINAL_IR_RAMP", station.name,
                        ewoStationOffsetToCentre(currentMm, direction, station.centre),
                        demand, static_cast<uint8_t>(demand.pwmTarget));
  if (demand.newIrObservation)
    publishStationEvent("BRAKE_OBSERVATION", station.name,
                        ewoStationOffsetToCentre(currentMm, direction, station.centre),
                        demand, static_cast<uint8_t>(demand.pwmTarget));

  if (demand.stopReached) {
    // Physical stop is allowed with residual actuator PWM. Remove actuator
    // output and release the retained lifecycle into DWELL immediately; do
    // not wait for target distance or for actualPwm to reach zero first.
    requestPwm(0, 0, demand.pwmDownMs);
    order = stationMachine.tick(currentMm, direction, 0, cruise, now);
    if (order.event && (!strcmp(order.event, "MISSED") ||
                        !strcmp(order.event, "PHASE_TIMEOUT"))) {
      withdraw("Station approach failed: controlled stop; Manual available.");
      return;
    }
    if (order.event && strcmp(order.event, "ZERO_RAMP"))
      publishStationEvent(order.event, station.name, order.offset, demand, 0);
    if (measuredStationStop.takeStopped())
      publishStationEvent("BRAKE_STOP", station.name, order.offset, demand, 0);
  }
}

static bool syncSend(const void* bytes, size_t length) {
  if (!naviSyncUdp.beginPacket(naviSyncDest, NAVI_SYNC_PORT) ||
      naviSyncUdp.write(reinterpret_cast<const uint8_t*>(bytes), length) != length ||
      !naviSyncUdp.endPacket()) { ++naviSyncUdpFailures; return false; }
  ++naviSyncDatagrams;
  return true;
}
static void syncDrain() {
  if (!naviSyncDestValid || WiFi.status() != WL_CONNECTED) return;
  for (unsigned i = 0; i < 20; ++i) {
    bool sent = false;
    // Network-task-owned scratch buffers keep larger batches off its stack.
    static navi_sync::ConsumptionWire consumed;
    static navi_sync::ActionWire action;
    if (recorder->popConsumption(consumed)) { syncSend(&consumed, sizeof(consumed)); sent = true; }
    if (recorder->popAction(action)) { syncSend(&action, sizeof(action)); sent = true; }
    if (i % 4 == 0) {
      navi_sync::NativeHallWire wire;
      if (recorder->popNativeHall(wire)) { syncSend(&wire, sizeof(wire)); sent = true; }
    } else if (i % 4 == 1) {
      navi_sync::NaviWire wire;
      if (recorder->popNavi(wire)) { syncSend(&wire, sizeof(wire)); sent = true; }
    } else if (i % 4 == 2) {
      navi_sync::IrWire wire;
      if (recorder->popIr(wire)) { syncSend(&wire, sizeof(wire)); sent = true; }
    } else {
      navi_sync::HallWire wire;
      if (recorder->popHall(wire)) { syncSend(&wire, sizeof(wire)); sent = true; }
    }
    if (!sent) {
      navi_sync::HallWire hall;
      navi_sync::IrWire ir;
      navi_sync::NaviWire nav;
      navi_sync::NativeHallWire native;
      if (recorder->popNativeHall(native)) { syncSend(&native, sizeof(native)); sent = true; }
      else if (recorder->popHall(hall)) { syncSend(&hall, sizeof(hall)); sent = true; }
      else if (recorder->popIr(ir)) { syncSend(&ir, sizeof(ir)); sent = true; }
      else if (recorder->popNavi(nav)) { syncSend(&nav, sizeof(nav)); sent = true; }
    }
    if (!sent) break;
  }
  static uint32_t lastStatus = 0;
  if (millis() - lastStatus < 1000) return;
  lastStatus = millis();
  navi_sync::Status status{};
  status.tMs = lastStatus;
  status.tUs = esp_timer_get_time();
  status.hallSamples = recorder->hallSamples();
  status.hallRingDrops = recorder->hallDrops();
  status.irAccepted = recorder->irAccepted();
  status.irRingDrops = recorder->irDrops();
  status.irInputQueueDrops = irQueueDrops;
  status.udpFailures = naviSyncUdpFailures;
  status.datagramsSent = naviSyncDatagrams;
  status.maxHallGapUs = recorder->maxHallGapUs();
  status.hallHighWater = recorder->hallHighWater();
  status.irHighWater = recorder->irHighWater();
  status.wifiConnected = WiFi.status() == WL_CONNECTED;
  status.mqttConnected = mqtt.connected();
  const navi_sync::StatusWire wire = recorder->makeStatus(
      lastStatus, status.tUs, recorderContext(), status);
  syncSend(&wire, sizeof(wire));
}
// Network-task only. Compact queues keep experimental traffic out of pubQ.
// Periodic observation summaries; no per-event MQTT or broker-delivery promise.
static void servicePulseTelemetry() {
  if(!ir_input::Wireless)return;
  LinkSummary s{};
  if(!linkSummaryQ || xQueueReceive(linkSummaryQ,&s,0)!=pdTRUE)return;
  const uint64_t now=esp_timer_get_time();
  const uint64_t receiveAge=s.receivedUs && now>=s.receivedUs?now-s.receivedUs:0;
  const uint64_t ageLower=s.lowerUs>UINT64_MAX-receiveAge?UINT64_MAX:s.lowerUs+receiveAge;
  const auto delivered=ir_link::timing(s.acceptanceLowerUs,s.upperUs);
  // Timely acceptance is retrospective. Current evidence ages independently.
  const auto currentTiming=ir_link::timing(ageLower,s.upperUs && s.upperUs<=UINT64_MAX-receiveAge?s.upperUs+receiveAge:0);
  uint8_t channel=0;wifi_second_chan_t secondary{};
  const bool channelKnown=esp_wifi_get_channel(&channel,&secondary)==ESP_OK && WiFi.status()==WL_CONNECTED;
  const char* channelState=!channelKnown?"unknown":channel==ir_link::Channel?"compatible":"mismatch";
  char payload[2048],topic[72];
  const int size=snprintf(payload,sizeof(payload),
    "{\"authority\":\"OBSERVATION_ONLY\",\"session\":\"%016llx\",\"boot\":\"%016llx\","
    "\"received\":%llu,\"missing_since_tx_boot\":%llu,\"last_count\":%llu,\"duplicate\":%lu,\"out_of_order\":%lu,"
    "\"invalid\":%lu,\"stale\":%lu,\"source_reject\":%lu,\"rx_drop\":%lu,\"ack_queue_drop\":%lu,"
    "\"tx_valid\":%u,\"tx_status_age_ms\":%llu,\"tx_generated\":%lu,\"tx_attempts\":%lu,\"tx_retries\":%lu,"
    "\"tx_mac_ok\":%lu,\"tx_mac_fail\":%lu,\"tx_ack\":%lu,\"tx_overflow\":%lu,\"tx_unresolved\":%lu,"
    "\"tx_high\":%lu,\"oldest_us\":%llu,\"ack_age_max_us\":%llu,\"ack_timely\":%lu,\"ack_uncertain\":%lu,"
    "\"receive_age_us\":%llu,\"evidence_age_lower_us\":%llu,\"acceptance_lower_us\":%llu,\"acceptance_upper_us\":%llu,"
    "\"delivery_timing\":\"%s\",\"current_timing\":\"%s\",\"status_gaps\":%lu,\"status_order\":%lu,"
    "\"tx_boot_changes\":%lu,\"prior_missing\":%llu,\"receiver_session_changes\":%lu,\"prior_unresolved\":%lu,"
    "\"previous_tx_boot\":\"%016llx\",\"previous_unresolved\":%lu,\"previous_status_known\":%u,\"receiver_ram_only\":1,"
    "\"ack_mac_ok\":%lu,\"ack_mac_fail\":%lu,\"radio_timeout\":%lu,\"mqtt_drop\":%lu,"
    "\"navi_channel\":%u,\"ir_channel\":11,\"channel_state\":\"%s\",\"field_channel_ready\":%u}",
    (unsigned long long)s.session,(unsigned long long)s.boot,(unsigned long long)s.received,
    (unsigned long long)s.missing,(unsigned long long)s.highest,(unsigned long)s.duplicates,(unsigned long)s.historical,
    (unsigned long)s.invalid,(unsigned long)s.stale,(unsigned long)linkUnknown.load(),(unsigned long)linkRxLoss.load(),
    (unsigned long)linkAckLoss.load(),unsigned(s.statusUs && now>=s.statusUs && now-s.statusUs<=1000000),
    (unsigned long long)(s.statusUs && now>=s.statusUs?(now-s.statusUs)/1000:0),
    (unsigned long)s.tx.generated,(unsigned long)s.tx.attempts,(unsigned long)s.tx.retries,
    (unsigned long)s.tx.macOK,(unsigned long)s.tx.macFail,(unsigned long)s.tx.acknowledged,
    (unsigned long)s.tx.overflow,(unsigned long)s.tx.unresolved,(unsigned long)s.tx.high,
    (unsigned long long)s.tx.oldestUs,(unsigned long long)s.tx.ackAgeMaxUs,
    (unsigned long)s.tx.ackTimely,(unsigned long)s.tx.ackUncertain,(unsigned long long)receiveAge,
    (unsigned long long)ageLower,(unsigned long long)s.acceptanceLowerUs,(unsigned long long)s.upperUs,
    s.receivedUs?ir_link::name(delivered):"uncertain",s.receivedUs?ir_link::name(currentTiming):"uncertain",
    (unsigned long)s.statusGaps,(unsigned long)s.statusOrder,(unsigned long)s.boots,(unsigned long long)s.priorMissing,
    (unsigned long)s.tx.sessionChanges,(unsigned long)s.tx.priorUnresolved,
    (unsigned long long)s.previousBoot,(unsigned long)s.previousUnresolved,unsigned(s.previousStatusKnown),
    (unsigned long)linkMacOK.load(),(unsigned long)linkMacFail.load(),(unsigned long)linkTimeouts.load(),
    (unsigned long)pulseMqttDrops.load(),unsigned(channel),channelState,unsigned(channelKnown && channel==11));
  snprintf(topic,sizeof(topic),"ngr/loco/%s/telem/ir_link",LOCO_NAME);
  if(size<=0 || size>=int(sizeof(payload)) || !mqtt.connected() || !mqtt.publish(topic,payload,false))++pulseMqttDrops;
  navi_pulse::Report report{};
  if(xQueueReceive(pulseStatusQ,&report,0)==pdTRUE){
    const int bytes=navi_pulse::format(payload,sizeof(payload),report,now,
      NaviIntegratedCore::kIrFreshUs,EWO_PKPH_MM_PER_SEC,false,linkRxLoss.load(),0,pulseMqttDrops.load());
    snprintf(topic,sizeof(topic),"ngr/loco/%s/telem/ir_pulse",LOCO_NAME);
    if(bytes<=0 || bytes>=int(sizeof(payload)) || !mqtt.connected() || !mqtt.publish(topic,payload,false))++pulseMqttDrops;
  }
  Serial.printf("[IR_LINK] channel=%u expected=11 state=%s unresolved=%lu rx_drop=%lu authority=OBSERVATION_ONLY\n",
    unsigned(channel),channelState,(unsigned long)s.tx.unresolved,(unsigned long)linkRxLoss.load());
}

// Loop-owned discovery snapshots. ACK transmission runs independently of MQTT.
static void serviceIrLinkHello(){
  if(!ir_input::Wireless)return;
  if(!radioReady || !linkSession)return;
  const uint32_t now=millis();
  if(uint32_t(now-irLinkHelloAt)<1000)return;
  irLinkHelloAt=now;
  ir_link::Hello hello{};hello.session=linkSession;hello.txBoot=reliablePulses.boot;
  hello.sequence=++irLinkHelloSequence;memcpy(hello.pairedMac,pairedIrMac,6);
  wifi_second_chan_t secondary{};esp_wifi_get_channel(&hello.channel,&secondary);
  ir_link::seal(hello);
  LinkOutput out{};memcpy(out.mac,IR_LINK_BROADCAST,6);out.length=sizeof(hello);memcpy(out.bytes,&hello,sizeof(hello));
  xQueueOverwrite(linkHelloQ,&out);
}

static std::atomic<bool> linkSendDone{true};
static std::atomic<bool> linkSendOK{false};
static void onLinkSent(const wifi_tx_info_t*,esp_now_send_status_t result){
  linkSendOK=result==ESP_NOW_SEND_SUCCESS;linkSendDone=true;
}
static void linkRadioTask(void*){
  bool inFlight=false,timeoutReported=false;uint32_t started=0;
  uint8_t installedMac[6]{};bool ackInFlight=false;
  for(;;){
    if(inFlight && linkSendDone.load()){
      if(ackInFlight){if(linkSendOK.load())++linkMacOK;else ++linkMacFail;}
      inFlight=false;
    }
    if(inFlight && !timeoutReported && uint32_t(millis()-started)>=100){
      ++linkTimeouts;timeoutReported=true;
    }
    LinkOutput out{};
    if(ir_input::Wireless && radioReady && !inFlight){
      bool isAck=xQueueReceive(linkAckQ,&out,0)==pdTRUE;
      bool have=isAck || xQueueReceive(linkHelloQ,&out,0)==pdTRUE;
      if(have){
        if(isAck && !ir_link::sameMac(installedMac,out.mac)){
          if(ir_link::configured(installedMac))esp_now_del_peer(installedMac);
          memset(installedMac,0,6);
          esp_now_peer_info_t peer{};memcpy(peer.peer_addr,out.mac,6);peer.channel=0;peer.encrypt=false;
          const esp_err_t result=esp_now_add_peer(&peer);
          if(result!=ESP_OK && result!=ESP_ERR_ESPNOW_EXIST){++linkMacFail;have=false;}
          else memcpy(installedMac,out.mac,6);
        }
        if(have){
          linkSendDone=false;
          if(esp_now_send(out.mac,out.bytes,out.length)==ESP_OK){
            inFlight=true;ackInFlight=isAck;started=millis();timeoutReported=false;
          } else {linkSendDone=true;if(isAck)++linkMacFail;}
        }
      }
    }
    vTaskDelay(1);
  }
}
static void networkTask(void*) {
  uint32_t nextConnect = 0, wifiDownSince = 0, nextIrInitAttempt = 0;
  for (;;) {
    const uint32_t now = millis();
    const bool wifiConnected = WiFi.status() == WL_CONNECTED;
    if (!wifiConnected) {
      if (!wifiDownSince) wifiDownSince = now;
      else if (now - wifiDownSince > 15000) {
        WiFi.disconnect(); WiFi.begin(WIFI_SSID, WIFI_PASS); wifiDownSince = now;
      }
    } else wifiDownSince = 0;
    if (wifiConnected && !radioReady && now >= nextIrInitAttempt) {
      nextIrInitAttempt = now + 15000;
      const esp_err_t initResult = esp_now_init();
      const esp_err_t callbackResult = initResult == ESP_OK
          ? esp_now_register_recv_cb(onIr) : initResult;
      esp_err_t peerResult=callbackResult;
      if(callbackResult==ESP_OK && ir_input::Wireless){
        esp_now_peer_info_t peer{};
        memcpy(peer.peer_addr,IR_LINK_BROADCAST,6);peer.channel=0;peer.encrypt=false;
        peerResult=esp_now_add_peer(&peer);
        if(peerResult==ESP_ERR_ESPNOW_EXIST)peerResult=ESP_OK;
        if(peerResult==ESP_OK)peerResult=esp_now_register_send_cb(onLinkSent);
      }
      radioReady = callbackResult == ESP_OK && peerResult==ESP_OK;
      if (initResult == ESP_OK && !radioReady) esp_now_deinit();
      Serial.printf("[IR] radio=%s init_error=%d callback_error=%d peer_error=%d\n",
                    radioReady ? "READY" : "FAILED", initResult, callbackResult, peerResult);
    }
    // Discovery and ACK scheduling are independent of networkTask/MQTT.
    if (wifiConnected && !mqtt.connected() && now >= nextConnect) {
      wifiClient.setConnectionTimeout(3000);
      nextConnect = now + 2000;
      char id[48], online[72];
      snprintf(id, sizeof(id), "EWO_%s", LOCO_NAME);
      snprintf(online, sizeof(online), "ngr/loco/%s/online", LOCO_NAME);
      if (mqtt.connect(id, online, 0, true, "0")) {
        mqtt.publish(online, "1", true);
        const char* commands[] = {"auto", "estop", "throttle", "direction",
          "session_direction", "start_mm", "start_interval", "dispatcher_release",
          "brake", "ir_pair", "ir_coupled"};
        for (const char* command : commands) {
          char topic[72];
          snprintf(topic, sizeof(topic), "ngr/loco/%s/cmd/%s", LOCO_NAME, command);
          mqtt.subscribe(topic);
        }
        char topic[72];
        snprintf(topic, sizeof(topic), "ngr/dispatcher/cmd/go/%s", LOCO_NAME);
        mqtt.subscribe(topic);
        snprintf(topic, sizeof(topic), "ngr/dispatcher/cmd/stop/%s", LOCO_NAME);
        mqtt.subscribe(topic);
        mqtt.subscribe("ngr/dispatcher/cmd/estop");
        Serial.printf("[MQTT] connected broker=%s client=%s\n", MQTT_BROKER, id);
      }
      else Serial.printf("[MQTT] connect failed state=%d broker=%s\n", mqtt.state(), MQTT_BROKER);
    }
    mqtt.loop();
    const bool mqttConnected = mqtt.connected();
    if (wifiConnected != lastWifiConnected ||
        mqttConnected != lastMqttConnected ||
        mqtt.state() != lastMqttState || !haveNetReport ||
        now - lastConnectivityReportMs >= 5000) {
      lastWifiConnected = wifiConnected;
      lastMqttConnected = mqttConnected;
      lastMqttState = mqtt.state();
      haveNetReport = true;
      lastConnectivityReportMs = now;
      if (wifiConnected) {
        IPAddress ip = WiFi.localIP();
        Serial.printf("[NET] wifi=CONNECTED ip=%u.%u.%u.%u rssi=%d mqtt=%s mqtt_state=%d\n",
                      ip[0], ip[1], ip[2], ip[3], WiFi.RSSI(),
                      mqttConnected ? "CONNECTED" : "DISCONNECTED", mqtt.state());
      } else {
        Serial.printf("[NET] wifi=DISCONNECTED wifi_status=%d mqtt=%s mqtt_state=%d\n",
                      static_cast<int>(WiFi.status()),
                      mqttConnected ? "CONNECTED" : "DISCONNECTED", mqtt.state());
      }
      if (mqttConnected) {
        IPAddress ip = WiFi.localIP();
        char connectivity[520], topic[72];
        snprintf(topic, sizeof(topic), "ngr/loco/%s/state/connectivity", LOCO_NAME);
        snprintf(connectivity, sizeof(connectivity),
          "{\"wifi\":%u,\"ip\":\"%u.%u.%u.%u\",\"rssi\":%d,"
          "\"mqtt\":1,\"mqtt_state\":%d,\"broker\":\"%s\","
          "\"ir_radio\":%u,\"ir_seen\":%lu,\"ir_coupled\":%u,"
          "\"navi_sync\":%u,\"udp_failures\":%lu}",
          wifiConnected, ip[0], ip[1], ip[2], ip[3], WiFi.RSSI(), mqtt.state(),
          MQTT_BROKER, radioReady.load(), (unsigned long)seenIrFrames, irCarCoupled,
          naviSyncDestValid, (unsigned long)naviSyncUdpFailures);
        mqtt.publish(topic, connectivity, true);
      }
    }
    syncDrain();
    if (mqtt.connected()) {
      PubMsg message;
      while (pubQ && xQueueReceive(pubQ, &message, 0) == pdTRUE)
        if (!mqtt.publish(message.topic, message.payload, message.retain)) ++pubDrops;
    }
    servicePulseTelemetry(); // Existing MQTT/command/NSR work gets first service.
    vTaskDelay(10);
  }
}

static void serviceStatus() {
  static uint32_t last = 0;
  if (millis() - last < 1000) return;
  last = millis();
  const uint64_t nowUs = esp_timer_get_time();
  const auto ir = navi.latestIr();
  char payload[1100];
  const int size = snprintf(payload, sizeof(payload),
    "{\"build\":\"%s\",\"mm\":%u,\"target\":%u,\"dir\":%d,"
    "\"target_distance_mm\":%lu,\"target_polarity\":%d,"
    "\"position_reliable\":%u,\"reference_ready\":%u,\"reference\":%d,"
    "\"median5\":%d,\"hall_support\":%u,\"spatial_phase\":%u,"
    "\"ir_applicable\":%u,\"ir_distance_state\":\"%s\",\"ir_health_fault\":%u,"
    "\"ir_readiness\":%u,\"ir_boot\":\"%016llX\","
    "\"ir_epoch\":%llu,\"ir_epoch_active\":%u,"
    "\"ir_seq\":%lu,\"ir_pulses\":%llu,\"ir_reason\":%u,"
    "\"ir_age_ms\":%llu,\"ir_span\":%u,\"ir_gap\":%llu,"
    "\"ir_sat\":%llu,\"ir_abort\":%llu,\"ir_inferred_added\":%llu,"
    "\"ir_inferred_removed\":%llu,\"ir_calibration\":%lu,"
    "\"hall_seen\":%llu,\"ir_seen\":%llu,\"hall_q_drop\":%lu,"
    "\"ir_q_drop\":%lu,\"ir_invalid\":%lu,\"nsr_hall_drop\":%lu,"
    "\"nsr_ir_drop\":%lu,\"nsr_navi_drop\":%lu,\"nsr_native_drop\":%lu,"
    "\"event_drop\":%lu,\"pwm0_ir_motion\":%lu,"
    "\"confirmed\":%lu,\"missed\":%lu,\"pwm\":%d,\"auto\":%u,"
    "\"running\":%u,\"estop\":%u,\"lowvolt\":%u,"
    "\"ir_stationary_pwm_warning\":%u,\"pub_drop\":%lu}",
    SKETCH_NAME, navi.mm(), unsigned(navi.target().sequence), navi.direction(),
    (unsigned long)navi.target().distanceMm, int(navi.target().polarity),
    navi.positionReliable(), navi.initialReferenceReady(), navi.activeReference(),
    navi.hallMedian(), navi.hallSupport(), navi.spatialPhase(),
    navi.irApplicable(nowUs), navi.irDistanceState(nowUs), navi.irHealthFault(),
    navi.irReadiness(), (unsigned long long)ir.bootId,
    (unsigned long long)navi.irMeasurementEpochId(), navi.irMeasurementEpochActive(),
    (unsigned long)ir.sequence, (unsigned long long)ir.completedPulses,
    ir.opticalReason,
    (unsigned long long)(nowUs >= navi.latestIrReceivedUs() ?
      (nowUs - navi.latestIrReceivedUs()) / 1000 : 0), ir.span,
    (unsigned long long)ir.sampleGaps,
    (unsigned long long)ir.saturatedSamples,
    (unsigned long long)ir.openAborts,
    (unsigned long long)ir.inferredAdded,
    (unsigned long long)ir.inferredRemoved,
    (unsigned long)ir.calibrationId,
    (unsigned long long)navi.hallObservationCount(),
    (unsigned long long)navi.irObservationCount(), (unsigned long)hallQueueDrops,
    (unsigned long)irQueueDrops, (unsigned long)irPacketInvalid,
    (unsigned long)recorder->hallDrops(), (unsigned long)recorder->irDrops(),
    (unsigned long)recorder->naviDrops(),
    (unsigned long)recorder->nativeDrops(),
    (unsigned long)navi.eventLoss(), (unsigned long)navi.pwmZeroDisplacements(),
    (unsigned long)navi.confirmedCount(), (unsigned long)navi.missedCount(),
    int(actualPwm), autoEnrolled, autoRunning, estopped, lowVoltage,
    actualPwm > 60 && navi.irSpeedAvailable(nowUs) && navi.irSpeedMmS() == 0,
    (unsigned long)pubDrops);
  if (size < 0 || size >= int(sizeof(payload))) {
    ++pubDrops;
    pub("state/warning", "EWO status JSON overflow");
  } else pub("state/loopstat", payload);
  char traceStatus[160];
  snprintf(traceStatus, sizeof(traceStatus),
    "{\"consumption_drop\":%lu,\"action_drop\":%lu,\"command_queue_drop\":%lu,\"lossless\":false}",
    (unsigned long)recorder->consumptionDrops(), (unsigned long)recorder->actionDrops(),
    (unsigned long)cmdDrops);
  pub("state/trace", traceStatus);
  char consolePayload[384];
  int consoleSize = formatConsoleNav(consolePayload, sizeof(consolePayload), navi, sessionDir, nowUs);
  if (consoleSize > 0 && consoleSize < int(sizeof(consolePayload)))
    pub("state/nav", consolePayload, true);
  else ++pubDrops;
  consoleSize = formatConsoleIr(consolePayload, sizeof(consolePayload), navi, nowUs, irCarCoupled);
  if (consoleSize > 0 && consoleSize < int(sizeof(consolePayload))) {
    pub("telem/ir", consolePayload);
    pub("telem/speed", consolePayload);
  } else ++pubDrops;
  const uint8_t* sourceMac = navi.latestIrMac();
  char link[320];
  snprintf(link, sizeof(link),
    "{\"radio_ready\":%u,\"seen\":%lu,\"source_mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\","
    "\"paired_mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\",\"paired_for_display_only\":1,"
    "\"coupled\":%u,\"invalid_wire\":%lu,\"queue_drop\":%lu}",
    radioReady.load(), (unsigned long)seenIrFrames,
    sourceMac[0], sourceMac[1], sourceMac[2], sourceMac[3], sourceMac[4], sourceMac[5],
    pairedIrMac[0], pairedIrMac[1], pairedIrMac[2], pairedIrMac[3],
    pairedIrMac[4], pairedIrMac[5], irCarCoupled,
    (unsigned long)irPacketInvalid, (unsigned long)irQueueDrops);
  pub("diag/ir_link", link);
  char value[16];
  snprintf(value, sizeof(value), "%d", int(actualPwm)); pub("state/throttle", value, true);
  pub("state/direction", motorDirection ? "2" : "0", true);
  pub("state/auto", autoEnrolled ? "1" : "0", true);
  pub("state/estop", estopped || estopAsserted ? "1" : "0", true);
  pub("state/nav_ready", opsNow().positionKnown ? "1" : "0", true);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(MOTOR_PWM_PIN, OUTPUT);
  digitalWrite(MOTOR_PWM_PIN, LOW);
  recorder = new (std::nothrow) navi_sync::Recorder();
  if (!recorder || !recorder->storageReady()) {
    // No PWM peripheral or control task has been enabled yet.
    pinMode(MOTOR_PWM_PIN, OUTPUT); digitalWrite(MOTOR_PWM_PIN, LOW);
    Serial.println("[BOOT] FATAL: bounded evidence storage allocation failed");
    for (;;) delay(1000);
  }
  bootId = (uint64_t(esp_random()) << 32) | esp_random();
  naviSyncSession = esp_random();
  if (!naviSyncSession) naviSyncSession = 1;
  recorder->begin(LOCO_ID, naviSyncSession, bootId);
  naviSyncDestValid = naviSyncDest.fromString(NAVI_SYNC_HOST);
  analogReadResolution(12);
  pinMode(HALL_PIN, INPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT); pinMode(MOTOR_PWM_PIN, OUTPUT);
  digitalWrite(MOTOR_DIR_PIN, HIGH);
  ledcAttach(MOTOR_PWM_PIN, PWM_FREQUENCY, PWM_RESOLUTION);
  writePwm(0);
  Wire.begin(I2C_SDA, I2C_SCL);
  inaReady = ina219.begin();
  hallQ = xQueueCreate(256, sizeof(HallSample));
  irQ = xQueueCreate(32, sizeof(IrRx));
  pubQ = xQueueCreate(PUB_QUEUE_DEPTH, sizeof(PubMsg));
  cmdQ = xQueueCreate(16, sizeof(CmdMsg));
  pulseQ=xQueueCreate(32,sizeof(LinkRx));
  linkAckQ=xQueueCreate(16,sizeof(LinkOutput));linkHelloQ=xQueueCreate(1,sizeof(LinkOutput));
  linkSummaryQ=xQueueCreate(1,sizeof(LinkSummary));
  pulseStatusQ=xQueueCreate(1,sizeof(navi_pulse::Report));
  Serial.printf("[IR_PULSE] queues rx=%u ack=%u status=%u authority=OBSERVATION_ONLY\n",
    pulseQ!=nullptr,linkAckQ!=nullptr,pulseStatusQ!=nullptr);
  Serial.printf("[BOOT] queues hall=%u ir=%u pub=%u cmd=%u pub_depth=%u\n",
                hallQ != nullptr, irQ != nullptr, pubQ != nullptr, cmdQ != nullptr,
                PUB_QUEUE_DEPTH);
  if (!hallQ || !irQ || !pubQ || !cmdQ || !pulseQ ||
      !pulseStatusQ || !linkAckQ || !linkHelloQ || !linkSummaryQ) {
    Serial.println("[BOOT] FATAL: queue allocation failed");
    writePwm(0); for (;;) delay(1000);
  }
  pairing.begin("ngr-nav", false);
  if (pairing.getBytesLength("ir_mac") == 6)
    pairing.getBytes("ir_mac", pairedIrMac, 6);
  memcpy(admittedIrMac,pairedIrMac,6);linkPairChanged=true;
  Serial.printf("[BOOT] profile loco=%s id=%lu mqtt_broker=%s sync_host=%s ir=ALL_VALID_TYPE5_TO_NAVI\n",
                LOCO_NAME, (unsigned long)LOCO_ID, MQTT_BROKER, NAVI_SYNC_HOST);
  Serial.printf("[BOOT] ir_radio=WAITING_FOR_WIFI paired_mac=%02X:%02X:%02X:%02X:%02X:%02X type5_display_only=1 type6_admission=1\n",
                pairedIrMac[0], pairedIrMac[1],
                pairedIrMac[2], pairedIrMac[3], pairedIrMac[4], pairedIrMac[5]);
  refreshRecorderContext();
  if (xTaskCreatePinnedToCore(hallTask, "hall", 4096, nullptr, 3, nullptr, 0) != pdPASS) {
    Serial.println("[BOOT] FATAL: Hall task failed");
    writePwm(0); for (;;) delay(1000);
  }
  // Let Wi-Fi select the AP channel before starting ESP-NOW reception.
  WiFi.onEvent(onWifiDisconnect, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("[NET] wifi=CONNECTING mqtt_broker=%s\n", MQTT_BROKER);
  mqtt.setServer(MQTT_BROKER, 1883);
  mqtt.setCallback(onMqtt);
  mqtt.setBufferSize(2304);
  Serial.printf("[IR_INPUT] path=%s direct_hardware=NOT_IMPLEMENTED authority=OBSERVATION_ONLY\n",ir_input::Name);
  if(ir_input::Wireless && xTaskCreatePinnedToCore(linkRadioTask,"ir-ack",4096,nullptr,1,nullptr,1)!=pdPASS)
    Serial.println("[IR_LINK] ACK task unavailable; transport cannot complete delivery");
  if (xTaskCreatePinnedToCore(networkTask, "net", 8192, nullptr, 1, nullptr, 1) != pdPASS)
    Serial.println("[BOOT] WARNING: network task failed");
  char payload[420];
  snprintf(payload, sizeof(payload),
    "{\"sketch\":\"%s\",\"class\":\"%s\",\"boot_id\":\"%016llX\","
    "\"field_accepted\":0,\"hall\":\"RAW_ADC_TO_NAVI\","
    "\"ir\":\"ALL_TYPE5_TO_NAVI\",\"median\":\"NAVI_ONLY\","
    "\"polarity\":\"COMMON_MEASURED_TOBY_OTTO\",\"legacy_otto_inversion\":\"IGNORED\"}",
    SKETCH_NAME, BUILD_CLASS, (unsigned long long)bootId);
  pub("state/bootid", payload, true);
  if (!inaReady) warn("INA219 NOT FOUND — no battery protection this session", true);
  Serial.printf("[BOOT] %s — integration candidate, not field accepted\n", SKETCH_NAME);
}

void loop() {
  serviceRamp();  // e-stop asserted by MQTT callback pre-empts evidence backlog
  serviceIrIngress(); // declaration must see IR already queued before it
  servicePwmZeroMovementHold(); // withdraw before queued AUTO/GO commands or station departure
  CmdMsg command;
  while (cmdQ && xQueueReceive(cmdQ, &command, 0) == pdTRUE) handleCommand(command);
  HallSample observation;
  for (unsigned processed = 0; processed < 128 && hallQ &&
       xQueueReceive(hallQ, &observation, 0) == pdTRUE; ++processed) {
    serviceObservationLoss();
    const uint64_t now = esp_timer_get_time();
    recordInput(navi_sync::InputKind::Hall, now, observation.timestampUs, observation.sampleSerial,
                observation.pwm, commandedPwm, observation.direction);
    navi.observeHall(observation, now);
    if (estopAsserted) serviceRamp();
  }
  serviceObservationLoss();
  publishEvents();
  serviceStation();
  refreshRecorderContext();
  serviceRamp();
  serviceBattery();
  serviceStatus();
  servicePulseIngress();
  serviceIrLinkHello();
  delay(1);
}
