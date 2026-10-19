/* ============================================================================
 * NAVI_RVM_SHAPE_MANUAL — manual locomotive, observation-only shape navigator
 * ============================================================================
 * FIELD-TEST CANDIDATE FOR REVIEW. NOT FIELD ACCEPTED. NOT FLASHED.
 * Otto (9950011) only.
 *
 * The operator drives. Throttle, direction, brake, STOP, E-STOP and the battery
 * protection are the manual controller of NAVI_RVM / Otto EWO_0_6 with the
 * automatic chamber deleted (as in MANUAL_1_1): no AUTO, GO, station, dispatcher
 * or CTO path exists, so no navigation-originated motor write exists.
 *
 * Navigation watches. NAVI_RVM statistical shape recognition (Model A, the
 * general composite) reads the 20 ms Hall tracing, recognizes complete magnetic
 * ensembles against the local baseline that precedes them, and the known route
 * polarity turns each completed MM -> BL -> MM piece into progression.
 * STOP/GO: while STOPPED (applied PWM < 15) the recognition waveform is paused
 * (RvmPause.h): the unfinished ensemble, its baseline and the held candidate
 * survive the stop; after GO the waveform resumes only when the Hall pattern
 * leaves the stationary state observed at the stop, and the stationary arrival
 * blocks held back since the operator commanded the stop are dropped. PWM and
 * the commanded target do nothing else. A
 * complete ensemble of the wrong polarity is a CONTRADICTION: reported, the
 * waveform preserved on the Pi, and no geographic claim until the operator
 * declares again. Nothing in navigation can reach the motor.
 *
 * Every motor write in this binary is in writePwm(), called only by
 * serviceRamp() (operator ramps, E-STOP clamp, low-voltage stop), setup() and
 * haltStartup().
 *
 * Docs: docs/NAVI_RVM_SHAPE_MANUAL_FIELD_CANDIDATE_20261018.md
 * ========================================================================== */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <esp_timer.h>
#include <atomic>

#include "LocoConfig.h"
#include "credentials.h"
#include "RvmOps.h"
#include "NaviEstop.h"
#include "RouteMap.h"
#include "RvmTracing.h"
#include "RvmPause.h"
#include "RvmShape.h"
#include "RvmNavigator.h"

static portMUX_TYPE recorderMux = portMUX_INITIALIZER_UNLOCKED;
#define NAVI_SYNC_ENTER_CRITICAL() portENTER_CRITICAL(&recorderMux)
#define NAVI_SYNC_EXIT_CRITICAL() portEXIT_CRITICAL(&recorderMux)
#include "NaviSyncRecorder.h"

using namespace navi_one;
using navi_eyes::OrderedEstop;

static constexpr char SKETCH_NAME[] = "NAVI_RVM_SHAPE_MANUAL";
static constexpr char BUILD_CLASS[] = "MANUAL_OBSERVATION_ONLY_NOT_FIELD_ACCEPTED";
static constexpr uint8_t HALL_PIN = 33;
static constexpr uint8_t I2C_SDA = 21, I2C_SCL = 22;
static constexpr uint16_t MANUAL_STEP_UP_MS = 150;                 // ms per PWM count
static constexpr uint16_t BRAKE_STEP_COAST_MS = 400, BRAKE_STEP_HARD_MS = 15;
static constexpr uint16_t STOP_STEP_DOWN_MS = 31;                  // cmd/stop and low-voltage stop
static constexpr uint16_t NAVI_SYNC_PORT = 47620;
static constexpr char NAVI_SYNC_HOST[] = "192.168.68.142";
static constexpr char MQTT_BROKER[] = "192.168.68.142";
static constexpr uint8_t PUB_QUEUE_DEPTH = 24;

struct PubMsg { char topic[72]; char payload[700]; bool retain; };
struct CmdMsg { char topic[72]; char payload[64]; uint64_t order; uint64_t receivedUs; };

// ---------------------------------------------------------------- state --
static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);
static WiFiUDP naviSyncUdp;
static IPAddress naviSyncDest;
static bool naviSyncDestValid = false;
static uint32_t naviSyncUdpFailures = 0, naviSyncDatagrams = 0;
static navi_sync::Recorder* recorder = nullptr;
static QueueHandle_t pubQ = nullptr, cmdQ = nullptr, blockQ = nullptr, ctlQ = nullptr, navEvQ = nullptr;
static uint32_t pubDrops = 0, cmdDrops = 0;
static std::atomic<uint32_t> blockQueueDrops{0}, navEventDrops{0}, ctlDrops{0};
static uint64_t bootId = 0;
static uint32_t lastMqttReportMs = 0;

static volatile int actualPwm = 0, commandedPwm = 0;
static int rampTarget = 0;
static uint16_t rampUpMs = MANUAL_STEP_UP_MS, rampDownMs = 0;
static uint32_t lastRampStepMs = 0;
static uint8_t brakeValue = 0;
static volatile uint8_t motorDirection = 1;          // 1 = forward (console "2")
static volatile int8_t sessionDir = 0;               // +1 CW, -1 CCW, 0 unset
static bool estopped = false, lowVoltage = false;
static std::atomic<bool> estopAsserted{false};
static portMUX_TYPE estopMux = portMUX_INITIALIZER_UNLOCKED;
static OrderedEstop orderedEstop;
static Adafruit_INA219 ina219;
static bool inaReady = false;
static float busV = 0, busA = 0, busW = 0;
static uint8_t lowVoltCount = 0;
static char warning[200] = "";
static uint64_t activeCommandOrder = 0, activeCommandReceivedUs = 0;

// ------------------------------------------------- navigation (observer) --
// Owned by rvmTask. The loop sends declarations / suspensions (ctlQ) and
// receives events (navEvQ). Nothing here is read by the motor path.
struct NavCtl { uint8_t kind; uint8_t last, next; int8_t dir; };      // 1 declare, 2 suspend
enum : uint8_t { CTL_DECLARE = 1, CTL_SUSPEND = 2 };
struct NavEvent {
  uint8_t kind;                    // 1 declared, 2 suspended, 3 ensemble
  uint8_t result;                  // rvm_shape::NavResult for ensembles
  uint8_t mm, expected, last, next; int8_t dir;
  rvm_shape::Ensemble e;
  uint32_t blocks;
  // STOP/GO transitions only
  bool candidateHeld; uint32_t baselineStartId, stationaryBlocks, withheld, arrivalTrimmed; float stationaryLow, stationaryHigh;
};
enum : uint8_t { EV_DECLARED = 1, EV_SUSPENDED = 2, EV_ENSEMBLE = 3, EV_STOPPED = 4, EV_RESUMED = 5 };
struct NavSnapshot {
  uint8_t state = 0, last = 0, next = 0, contradicted = 0xFF; int8_t dir = 0;
  uint32_t recognized = 0, advanced = 0, contradictions = 0, blocks = 0, blStart = 0, windows = 0, outDrops = 0, pausedBlocks = 0;
  bool holding = false, active = false, paused = false;
  uint32_t maxPushUs = 0;
};
static rvm_shape::Recognizer recognizer;           // ~68 KB, static
static rvm_shape::Navigator navigator;
static rvm_shape::WaveformPause waveformPause;      // owned by rvmTask
static uint32_t pausedBlocks = 0;
static NavSnapshot navShared;
static portMUX_TYPE navMux = portMUX_INITIALIZER_UNLOCKED;
static std::atomic<uint64_t> firstBlockUs{0};
static std::atomic<uint32_t> rvmStackFree{0};

static NavSnapshot navSnapshot() {
  portENTER_CRITICAL(&navMux); NavSnapshot s = navShared; portEXIT_CRITICAL(&navMux); return s;
}
static uint64_t blockUs(uint32_t id) {             // centre of tracing block `id` on the boot clock
  return firstBlockUs.load() + uint64_t(id) * rvm_shape::Tracing::BLOCK_US + rvm_shape::Tracing::BLOCK_US / 2;
}

// ------------------------------------------------------------ recorder --
static navi_sync::Context recorderContext() {
  const NavSnapshot s = navSnapshot();
  navi_sync::Context c;
  const bool tracking = s.state == uint8_t(rvm_shape::NavState::Tracking);
  c.navMm = tracking ? s.last : navi_sync::MM_NA;
  c.navDir = s.dir; c.stationPhase = 0;
  uint8_t f = 0;
  if (tracking) f |= navi_sync::CTX_NAV_KNOWN;
  if (estopped || estopAsserted) f |= navi_sync::CTX_ESTOP;
  if (lowVoltage) f |= navi_sync::CTX_LOW_VOLT;
  if (actualPwm == 0) f |= navi_sync::CTX_HOLD;
  c.flags = f;
  return c;
}
static void recordAction(navi_sync::ActionKind kind, const char* topic = "", const char* payload = "") {
  navi_sync::ActionSnapshot a{};
  a.tUs = esp_timer_get_time(); a.commandOrder = activeCommandOrder; a.commandReceivedUs = activeCommandReceivedUs;
  a.kind = uint8_t(kind); a.pwm = actualPwm; a.targetPwm = commandedPwm; a.rampUpMs = rampUpMs; a.rampDownMs = rampDownMs;
  strlcpy(a.topic, topic, sizeof(a.topic)); strlcpy(a.payload, payload, sizeof(a.payload));
  recorder->addAction(a, recorderContext());
}
static void recordDeclaration(navi_sync::InputKind kind, uint32_t value, int8_t dir) {
  static uint64_t id = 0;
  navi_sync::ConsumptionItem item{};
  const uint64_t now = esp_timer_get_time();
  item.id = ++id; item.decisionUs = now; item.observationUs = activeCommandReceivedUs; item.serial = value;
  item.kind = uint8_t(kind); item.pwm = actualPwm; item.commandedPwm = commandedPwm; item.direction = dir;
  const auto c = recorderContext(); item.navMm = c.navMm; item.contextFlags = c.flags;
  recorder->addConsumption(item, c);
}

// --------------------------------------------------------------- MQTT out --
static bool pub(const char* leaf, const char* payload, bool retain = false) {
  if (!pubQ) return false;
  PubMsg m{};
  snprintf(m.topic, sizeof(m.topic), "ngr/loco/%s/%s", LOCO_NAME, leaf);
  if (strlen(payload) >= sizeof(m.payload)) { ++pubDrops; return false; }
  strlcpy(m.payload, payload, sizeof(m.payload)); m.retain = retain;
  if (xQueueSend(pubQ, &m, 0) != pdTRUE) { ++pubDrops; return false; }
  return true;
}
static void warn(const char* message) {
  strlcpy(warning, message, sizeof(warning));
  pub("state/warning", warning, true);
  Serial.printf("[WARN] %s\n", message);
}
static void clearWarning() { warning[0] = 0; pub("state/warning", "", true); }

// ------------------------------------------------------------------ motor --
static void writePwm(int value) {
  ledcWrite(MOTOR_PWM_PIN, value);
  static int lastRecorded = -1;
  if (value != lastRecorded) { lastRecorded = value; if (recorder) recordAction(navi_sync::ActionKind::AppliedPwm); }
}
static uint16_t brakeStepMs() {
  return uint16_t(BRAKE_STEP_COAST_MS - (uint32_t(BRAKE_STEP_COAST_MS - BRAKE_STEP_HARD_MS) * brakeValue) / 255U);
}
static void requestPwm(int target, uint16_t up, uint16_t down) {
  target = constrain(target, 0, 255);
  const bool changed = target != rampTarget || up != rampUpMs || down != rampDownMs;
  rampTarget = target; commandedPwm = target; rampUpMs = up; rampDownMs = down;
  if (changed) recordAction(navi_sync::ActionKind::RequestedPwm);
}
static void serviceRamp() {
  digitalWrite(MOTOR_DIR_PIN, motorDirection ? HIGH : LOW);
  if (estopped || estopAsserted) {
    estopped = true; actualPwm = 0; commandedPwm = 0; rampTarget = 0; writePwm(0); return;
  }
  if (lowVoltage && rampTarget != 0) { rampTarget = 0; commandedPwm = 0; rampDownMs = STOP_STEP_DOWN_MS; }
  if (actualPwm == rampTarget) return;
  const bool down = rampTarget < actualPwm;
  const uint16_t step = down ? (rampDownMs ? rampDownMs : brakeStepMs()) : (rampUpMs ? rampUpMs : 1);
  const uint32_t now = millis();
  if (now - lastRampStepMs < step) return;
  lastRampStepMs = now;
  actualPwm += down ? -1 : 1;
  writePwm(actualPwm);
}
static int8_t travelDirection() {
  if (!sessionDir) return 0;
  return motorDirection ? sessionDir : int8_t(-sessionDir);
}
static Ops opsNow() {
  Ops o; o.estopped = estopped || estopAsserted; o.lowVoltage = lowVoltage;
  o.actualPwm = actualPwm; o.commandedPwm = commandedPwm; o.sessionDir = sessionDir;
  o.safeDirPwm = SAFE_DIRECTION_CHANGE_PWM; return o;
}

// ------------------------------------------------------------- Hall task --
// 5 kHz native reads (5 per 1 ms tick), unchanged from NAVI_RVM: native and
// legacy NSR1 records for the Pi, plus the 20 ms tracing for recognition.
static void hallTask(void*) {
  static rvm_shape::Tracing tracing;
  static uint32_t serial = 0;
  TickType_t wake = xTaskGetTickCount();
  auto emit = [](const rvm_shape::Block& b) {
    if (!blockQ || xQueueSend(blockQ, &b, 0) != pdTRUE) ++blockQueueDrops;
  };
  for (;;) {
    navi_sync::HallSample wire{};
    const uint64_t firstUs = esp_timer_get_time();
    const int8_t routeDir = travelDirection();
    const uint8_t dirByte = routeDir > 0 ? 1 : routeDir < 0 ? 2 : 0;
    const auto ctx = recorderContext();
    for (unsigned i = 0; i < 5; ++i) {
      ++serial;
      const uint64_t us = esp_timer_get_time();
      const int raw = analogRead(HALL_PIN);
      wire.raw[i] = uint16_t(raw);
      navi_sync::NativeHallItem item{};
      item.serial = serial; item.tUs = us; item.raw = int16_t(raw); item.pwm = uint8_t(actualPwm); item.direction = dirByte;
      recorder->addNativeHall(item, ctx);
      tracing.add(us, uint16_t(raw), dirByte, serial, uint8_t(actualPwm), uint8_t(commandedPwm), emit);
      if (!firstBlockUs.load()) firstBlockUs.store(tracing.firstBlockUs());
    }
    wire.median = INT16_MIN;
    wire.pwmActual = uint8_t(actualPwm); wire.pwmCommanded = uint8_t(commandedPwm);
    wire.flags = motorDirection ? navi_sync::HALL_F_DIR_FWD : 0;
    if (actualPwm == 0) wire.flags |= navi_sync::HALL_F_HOLD;
    recorder->addHall(firstUs, uint32_t(firstUs / 1000), wire, ctx);
    vTaskDelayUntil(&wake, 1);
  }
}

// -------------------------------------------------------------- RVM task --
static void publishSnapshot() {
  NavSnapshot s;
  s.state = uint8_t(navigator.state()); s.last = navigator.lastMm(); s.next = navigator.nextMm();
  s.dir = navigator.dir(); s.contradicted = navigator.contradictedMm();
  s.blocks = recognizer.blocks(); s.blStart = recognizer.baselineStartId(); s.holding = recognizer.holding();
  s.paused = waveformPause.paused(); s.pausedBlocks = pausedBlocks;
  s.active = recognizer.active(); s.windows = recognizer.windowsScored(); s.outDrops = recognizer.outputDrops();
  portENTER_CRITICAL(&navMux);
  s.recognized = navShared.recognized; s.advanced = navShared.advanced; s.contradictions = navShared.contradictions;
  s.maxPushUs = navShared.maxPushUs;
  navShared = s;
  portEXIT_CRITICAL(&navMux);
}
static void postEvent(const NavEvent& ev) { if (!navEvQ || xQueueSend(navEvQ, &ev, 0) != pdTRUE) ++navEventDrops; }
// STOP/GO transitions of the recognition waveform, for the Pi record (observation only).
static void postPause(uint8_t kind, uint32_t blockId) {
  NavEvent ev{}; ev.kind = kind; ev.blocks = blockId;
  ev.last = navigator.lastMm(); ev.next = navigator.nextMm(); ev.dir = navigator.dir();
  ev.candidateHeld = recognizer.holding(); ev.baselineStartId = recognizer.baselineStartId();
  ev.stationaryLow = waveformPause.stationaryLow(); ev.stationaryHigh = waveformPause.stationaryHigh();
  ev.stationaryBlocks = waveformPause.stationaryBlocks(); ev.withheld = waveformPause.withheldAfterGo();
  ev.arrivalTrimmed = waveformPause.lastArrivalTrim();
  postEvent(ev);
}
// Append one block to the recognition waveform and hand any completed ensemble
// to the navigator (observation only).
static void feedRecognizer(const rvm_shape::Block& a) {
  recognizer.push(a.hall, a.dir == 2, a.index);
  rvm_shape::Ensemble e;
  while (recognizer.take(e)) {
    NavEvent ev{}; ev.kind = EV_ENSEMBLE; ev.e = e; ev.expected = navigator.nextMm(); ev.blocks = recognizer.blocks();
    const auto r = navigator.onEnsemble(e.pol);
    ev.result = uint8_t(r); ev.mm = navigator.lastMm(); ev.last = navigator.lastMm(); ev.next = navigator.nextMm(); ev.dir = navigator.dir();
    portENTER_CRITICAL(&navMux);
    ++navShared.recognized;
    if (r == rvm_shape::NavResult::Advanced) ++navShared.advanced;
    if (r == rvm_shape::NavResult::Contradiction) ++navShared.contradictions;
    portEXIT_CRITICAL(&navMux);
    postEvent(ev);
  }
}
static void applyControl() {
  NavCtl c;
  while (ctlQ && xQueueReceive(ctlQ, &c, 0) == pdTRUE) {
    NavEvent ev{};
    if (c.kind == CTL_DECLARE) {
      // Anything still held back from a stop was traversed before it: release it
      // into the old context first. Then the startup baseline: the Hall sensor is
      // declared clear of a magnet, standing still; the tracing from the next
      // block on is the first baseline (BL0).
      waveformPause.declared(feedRecognizer);
      recognizer.startBaseline(recognizer.blocks());
      navigator.declare(c.last, c.next, c.dir);
      ev.kind = EV_DECLARED;
    } else {
      navigator.suspend();
      ev.kind = EV_SUSPENDED;
    }
    ev.blocks = recognizer.blocks();
    ev.last = navigator.lastMm(); ev.next = navigator.nextMm(); ev.dir = navigator.dir();
    publishSnapshot(); postEvent(ev);
  }
}
static void rvmTask(void*) {
  for (;;) {
    rvm_shape::Block b;
    const bool got = blockQ && xQueueReceive(blockQ, &b, pdMS_TO_TICKS(100)) == pdTRUE;
    applyControl();                                   // declarations take effect at a block boundary
    if (!got) { publishSnapshot(); continue; }
    // STOP/GO: a STOPPED block (applied PWM < 15) is not appended; after GO,
    // blocks that repeat the stationary state observed at the stop are withheld
    // until the Hall pattern resumes developing (RvmPause.h). Everything already
    // in the waveform, including an unfinished ensemble, is preserved.
    const bool wasPaused = waveformPause.paused();
    const uint64_t t0 = esp_timer_get_time();
    int na = 0; uint32_t firstId = b.index;
    waveformPause.admit(b, [&](const rvm_shape::Block& a) { if (!na++) firstId = a.index; feedRecognizer(a); });
    const uint32_t took = uint32_t(esp_timer_get_time() - t0);
    if (!wasPaused && waveformPause.paused()) postPause(EV_STOPPED, b.index);
    if (wasPaused && !waveformPause.paused()) postPause(EV_RESUMED, firstId);
    if (!na) { ++pausedBlocks; publishSnapshot(); continue; }
    portENTER_CRITICAL(&navMux); if (took > navShared.maxPushUs) navShared.maxPushUs = took; portEXIT_CRITICAL(&navMux);
    publishSnapshot();
    rvmStackFree.store(uxTaskGetStackHighWaterMark(nullptr));
  }
}

// ---------------------------------------------------------- nav reporting --
static const char* stateName(uint8_t s) {
  return s == uint8_t(rvm_shape::NavState::Tracking) ? "TRACKING" : s == uint8_t(rvm_shape::NavState::Contradiction) ? "CONTRADICTION" : "UNSET";
}
static char lastNavEvent[24] = "NONE";
static void publishNavState() {
  const NavSnapshot s = navSnapshot();
  char contradicted[8] = "null";
  if (s.contradicted != 0xFF) snprintf(contradicted, sizeof(contradicted), "%u", s.contradicted);
  char payload[520];
  snprintf(payload, sizeof(payload),
    "{\"state\":\"%s\",\"mm\":%u,\"target\":%u,\"dir\":%d,\"session_dir\":\"%s\","
    "\"position_reliable\":%u,\"event\":\"%s\",\"contradicted_mm\":%s,"
    "\"recognized\":%lu,\"advanced\":%lu,\"contradictions\":%lu,"
    "\"recognizer_active\":%u,\"holding_candidate\":%u,\"waveform_paused\":%u,\"baseline_start_us\":%llu,"
    "\"authority\":\"NAVI_RVM_SHAPE_OBSERVATION_ONLY\"}",
    stateName(s.state), s.last, s.next, s.dir, sessionDir > 0 ? "CW" : sessionDir < 0 ? "CCW" : "UNSET",
    s.state == uint8_t(rvm_shape::NavState::Tracking), lastNavEvent, contradicted,
    (unsigned long)s.recognized, (unsigned long)s.advanced, (unsigned long)s.contradictions,
    s.active, s.holding, s.paused, (unsigned long long)(s.active ? blockUs(s.blStart) : 0));
  pub("state/nav", payload, true);
  pub("state/nav_ready", s.state == uint8_t(rvm_shape::NavState::Tracking) ? "1" : "0", true);
}
static void serviceNavEvents() {
  NavEvent ev;
  for (unsigned k = 0; k < 4 && navEvQ && xQueueReceive(navEvQ, &ev, 0) == pdTRUE; ++k) {
    if (ev.kind == EV_DECLARED) {
      strlcpy(lastNavEvent, "DECLARED", sizeof(lastNavEvent));
      Serial.printf("[RVM] DECLARED last=MM%03u next=MM%03u dir=%d baseline_block=%lu\n", ev.last, ev.next, ev.dir, (unsigned long)ev.blocks);
    } else if (ev.kind == EV_SUSPENDED) {
      strlcpy(lastNavEvent, "SUSPENDED", sizeof(lastNavEvent));
    } else if (ev.kind == EV_STOPPED || ev.kind == EV_RESUMED) {
      // STOP: waveform paused with the unfinished ensemble preserved.
      // RESUMED: the Hall pattern left the stationary state observed at the stop.
      char row[420];
      snprintf(row, sizeof(row),
        "{\"boot\":\"%016llX\",\"transition\":\"%s\",\"block_us\":%llu,\"last_mm\":%u,\"expected_mm\":%u,"
        "\"candidate_held\":%u,\"baseline_start_us\":%llu,\"stationary_blocks\":%lu,"
        "\"stationary_low\":%.1f,\"stationary_high\":%.1f,\"withheld_after_go_total\":%lu,\"arrival_blocks_trimmed\":%lu,\"authority\":\"OBSERVATION_ONLY\"}",
        (unsigned long long)bootId, ev.kind == EV_STOPPED ? "STOPPED_WAVEFORM_PAUSED" : "PATTERN_RESUMED",
        (unsigned long long)blockUs(ev.blocks), ev.last, ev.next, unsigned(ev.candidateHeld),
        (unsigned long long)blockUs(ev.baselineStartId), (unsigned long)ev.stationaryBlocks,
        ev.kind == EV_STOPPED ? 0.0 : double(ev.stationaryLow), ev.kind == EV_STOPPED ? 0.0 : double(ev.stationaryHigh),
        (unsigned long)ev.withheld, (unsigned long)(ev.kind == EV_STOPPED ? 0 : ev.arrivalTrimmed));
      pub("diag/rvm_stopgo", row);
      Serial.printf("[RVM] %s\n", row);
      strlcpy(lastNavEvent, ev.kind == EV_STOPPED ? "STOPPED" : "RESUMED", sizeof(lastNavEvent));
    } else {
      const auto r = rvm_shape::NavResult(ev.result);
      const char* result = r == rvm_shape::NavResult::Advanced ? "MM" : r == rvm_shape::NavResult::Contradiction ? "CONTRADICTION" : "RECOGNIZED_NO_CLAIM";
      strlcpy(lastNavEvent, result, sizeof(lastNavEvent));
      char mm[8] = "null", expected[8] = "null";
      if (r == rvm_shape::NavResult::Advanced) snprintf(mm, sizeof(mm), "%u", ev.mm);
      if (r != rvm_shape::NavResult::NoClaim) snprintf(expected, sizeof(expected), "%u", ev.expected);
      const auto& e = ev.e;
      char row[620];
      snprintf(row, sizeof(row),
        "{\"boot\":\"%016llX\",\"seq\":%lu,\"result\":\"%s\",\"mm\":%s,\"expected_mm\":%s,"
        "\"expected_polarity\":\"%s\",\"polarity\":\"%s\",\"dir\":%d,"
        "\"midpoint_us\":%llu,\"start_us\":%llu,\"end_us\":%llu,\"waveform_window_ms\":%.0f,"
        "\"decided_us\":%llu,\"baseline_start_us\":%llu,\"llr\":%.3f,\"amplitude\":%.1f,"
        "\"baseline_level\":%.1f,\"baseline_noise\":%.2f,\"authority\":\"OBSERVATION_ONLY\"}",
        (unsigned long long)bootId, (unsigned long)e.seq, result, mm, expected,
        r == rvm_shape::NavResult::NoClaim ? "NA" : rvm_shape::expectedHallSign(ev.expected) > 0 ? "N" : "S",
        e.pol > 0 ? "N" : "S", ev.dir,
        (unsigned long long)blockUs(e.cId), (unsigned long long)blockUs(e.sId), (unsigned long long)blockUs(e.eId),
        e.w * rvm_shape::Tracing::BLOCK_US / 1000.0, (unsigned long long)blockUs(e.decidedId),
        (unsigned long long)blockUs(e.blStartId), double(e.llr), double(e.amp), double(e.level), double(e.noise));
      pub("diag/rvm_ensemble", row);
      Serial.printf("[RVM] %s\n", row);
      if (r == rvm_shape::NavResult::Advanced) pub("mm/marker", row);
      if (r == rvm_shape::NavResult::Contradiction) {
        char m[200];
        snprintf(m, sizeof(m), "CONTRADICTION: expected MM%03u (%s), recognized %s ensemble. No geographic claim until declared again.",
                 ev.expected, rvm_shape::expectedHallSign(ev.expected) > 0 ? "N" : "S", e.pol > 0 ? "N" : "S");
        warn(m);
      }
    }
    publishNavState();
  }
}
static void navControl(uint8_t kind, uint8_t last = 0, uint8_t next = 0, int8_t dir = 0) {
  NavCtl c{kind, last, next, dir};
  if (!ctlQ || xQueueSend(ctlQ, &c, 0) != pdTRUE) { ++ctlDrops; warn("NAVIGATION CONTROL QUEUE FULL: declaration not applied"); }
}
static void declare(uint8_t last, uint8_t next, int8_t dir, const char* interval) {
  recordDeclaration(navi_sync::InputKind::Declaration, last, dir);
  navControl(CTL_DECLARE, last, next, dir);
  clearWarning();
  char value[8]; snprintf(value, sizeof(value), "%u", last);
  pub("state/start_mm", value, true);
  pub("state/start_interval", interval, true);
}
static void suspendIfDeclared(const char* why) {
  const NavSnapshot s = navSnapshot();
  if (s.state == uint8_t(rvm_shape::NavState::Unset)) return;
  recordDeclaration(navi_sync::InputKind::Reversal, uint32_t(travelDirection() > 0 ? 1 : 2), travelDirection());
  navControl(CTL_SUSPEND);
  warn(why);
}

// --------------------------------------------------------------- commands --
static void onMqtt(char* topic, byte* payload, unsigned length) {
  CmdMsg command{};
  command.receivedUs = esp_timer_get_time();
  strlcpy(command.topic, topic, sizeof(command.topic));
  const unsigned count = length < sizeof(command.payload) - 1 ? length : sizeof(command.payload) - 1;
  memcpy(command.payload, payload, count); command.payload[count] = 0;
  const char* leaf = strrchr(command.topic, '/'); leaf = leaf ? leaf + 1 : command.topic;
  bool assertion = false;
  if (!strcmp(leaf, "estop")) { bool stop = true; parseEstop(command.payload, stop); assertion = stop; }
  portENTER_CRITICAL(&estopMux);
  command.order = orderedEstop.received(assertion);
  estopAsserted = orderedEstop.asserted();          // E-STOP asserted here, before any queue
  portEXIT_CRITICAL(&estopMux);
  navi_sync::ActionSnapshot arrival{};
  arrival.tUs = command.receivedUs; arrival.commandOrder = command.order; arrival.commandReceivedUs = command.receivedUs;
  arrival.kind = uint8_t(navi_sync::ActionKind::CommandReceived); arrival.pwm = actualPwm; arrival.targetPwm = commandedPwm;
  strlcpy(arrival.topic, command.topic, sizeof(arrival.topic)); strlcpy(arrival.payload, command.payload, sizeof(arrival.payload));
  recorder->addAction(arrival, recorderContext());
  if (cmdQ && xQueueSend(cmdQ, &command, 0) != pdTRUE) ++cmdDrops;
}
static void handleCommand(const CmdMsg& command) {
  activeCommandOrder = command.order; activeCommandReceivedUs = command.receivedUs;
  recordAction(navi_sync::ActionKind::CommandConsumed, command.topic, command.payload);
  const char* leaf = strrchr(command.topic, '/'); leaf = leaf ? leaf + 1 : command.topic;
  const Ops o = opsNow();
  if (!strcmp(leaf, "estop")) {
    bool stop = true;
    const bool understood = parseEstop(command.payload, stop);
    portENTER_CRITICAL(&estopMux);
    estopped = orderedEstop.apply(command.order, stop);
    estopAsserted = orderedEstop.asserted();
    portEXIT_CRITICAL(&estopMux);
    if (estopped) requestPwm(0, 0, 1);
    if (!understood) warn("ESTOP: unreadable payload, assumed STOP");
    else if (estopped) warn(stop ? "ESTOP" : "ESTOP: older release ignored");
    else clearWarning();
    pub("state/estop", estopped ? "1" : "0", true);
  } else if (!strcmp(leaf, "throttle")) {
    if (Refusal reason = admitThrottle(o)) { warn(reason); return; }
    int value;
    if (!parseInt(command.payload, value)) { warn("THROTTLE REFUSED: invalid value"); return; }
    requestPwm(value, MANUAL_STEP_UP_MS, 0);
  } else if (!strcmp(leaf, "stop")) {
    requestPwm(0, 0, STOP_STEP_DOWN_MS);
  } else if (!strcmp(leaf, "brake")) {
    int value;
    if (!parseInt(command.payload, value)) { warn("BRAKE REFUSED: invalid value"); return; }
    brakeValue = constrain(value, 0, 255);
    char text[8]; snprintf(text, sizeof(text), "%u", brakeValue); pub("state/brake", text, true);
  } else if (!strcmp(leaf, "direction")) {
    bool forward;
    if (!parseMotorDir(command.payload, forward)) { warn("DIRECTION REFUSED: expected 0 or 2"); return; }
    if (Refusal reason = admitMotorDirection(o)) { warn(reason); return; }
    if (actualPwm || commandedPwm) { warn("DIRECTION REFUSED: stop fully first"); return; }
    const int8_t before = travelDirection();
    motorDirection = forward ? 1 : 0;
    pub("state/direction", motorDirection ? "2" : "0", true);
    if (travelDirection() != before)
      suspendIfDeclared("DIRECTION CHANGED: navigation suspended. Declare the position again.");
  } else if (!strcmp(leaf, "session_direction")) {
    int8_t direction;
    if (!parseSessionDir(command.payload, direction)) { warn("SESSION_DIRECTION: expected CW or CCW"); return; }
    if (Refusal reason = admitDeclaration(o)) { warn(reason); return; }
    const int8_t before = travelDirection();
    sessionDir = direction;
    pub("state/session_direction", direction > 0 ? "CW" : "CCW", true);
    if (travelDirection() != before)
      suspendIfDeclared("SESSION DIRECTION CHANGED: navigation suspended. Declare the position again.");
  } else if (!strcmp(leaf, "start_interval")) {
    if (Refusal reason = admitStartMarker(o)) { warn(reason); return; }
    int a, b;
    if (!parseInterval(command.payload, a, b) || a < 0 || b < 0 || a >= ROUTE_N || b >= ROUTE_N || nextMarker(a, 1) != b) {
      warn("START_INTERVAL REFUSED: expected adjacent AAA-BBB"); return;
    }
    const int8_t d = travelDirection();
    declare(uint8_t(d > 0 ? a : b), uint8_t(d > 0 ? b : a), d, command.payload);
  } else if (!strcmp(leaf, "start_mm")) {
    if (Refusal reason = admitStartMarker(o)) { warn(reason); return; }
    int value;
    if (!parseInt(command.payload, value) || value < 0 || value >= ROUTE_N) { warn("START_MM REFUSED: invalid marker"); return; }
    const int8_t d = travelDirection();
    char interval[16];
    snprintf(interval, sizeof(interval), "%03u-%03u", d > 0 ? value : nextMarker(value, -1), d > 0 ? nextMarker(value, 1) : value);
    declare(uint8_t(value), nextMarker(uint8_t(value), d), d, interval);
  }
}

// ---------------------------------------------------------------- battery --
static void serviceBattery() {
  static uint32_t last = 0;
  if (!inaReady || millis() - last < 5000) return;
  last = millis();
  busV = ina219.getBusVoltage_V(); busA = ina219.getCurrent_mA() / 1000.0f; busW = ina219.getPower_mW() / 1000.0f;
  if (busV < DISCONNECTED_VOLTAGE_THRESHOLD) lowVoltCount = 0;
  else if (busV < SHUTDOWN_VOLTAGE) { if (lowVoltCount < VOLTAGE_COUNTER_LIMIT) ++lowVoltCount; }
  else lowVoltCount = 0;
  if (!lowVoltage && lowVoltCount >= VOLTAGE_COUNTER_LIMIT) {
    lowVoltage = true; requestPwm(0, 0, STOP_STEP_DOWN_MS); warn("LOW VOLTAGE: controlled stop");
  } else if (lowVoltage && busV >= RECOVERY_VOLTAGE) { lowVoltage = false; lowVoltCount = 0; }
  pub("state/lowvolt", lowVoltage ? "1" : "0", true);
  char text[24];
  snprintf(text, sizeof(text), "%.2f", double(busV)); pub("telem/voltage", text, true);
  snprintf(text, sizeof(text), "%.2f", double(busA)); pub("telem/current", text, true);
  snprintf(text, sizeof(text), "%.2f", double(busW)); pub("telem/power", text, true);
}

// ----------------------------------------------------------------- status --
static void serviceStatus() {
  static uint32_t last = 0;
  if (millis() - last < 1000) return;
  last = millis();
  const NavSnapshot s = navSnapshot();
  char payload[600];
  snprintf(payload, sizeof(payload),
    "{\"build\":\"%s\",\"class\":\"%s\",\"model\":\"%s\",\"pwm\":%d,\"commanded\":%d,\"estop\":%u,\"lowvolt\":%u,"
    "\"nav\":\"%s\",\"blocks\":%lu,\"paused_blocks\":%lu,\"block_q_drop\":%lu,\"nav_event_drop\":%lu,\"ctl_drop\":%lu,"
    "\"windows_scored\":%lu,\"rvm_out_drop\":%lu,\"rvm_push_max_us\":%lu,\"rvm_stack_free\":%lu,"
    "\"nsr_hall_drop\":%lu,\"nsr_native_drop\":%lu,\"nsr_udp_fail\":%lu,\"pub_drop\":%lu,\"cmd_drop\":%lu,"
    "\"heap_free\":%u,\"heap_min\":%u}",
    SKETCH_NAME, BUILD_CLASS, rvm_shape::MODEL_SHA256, int(actualPwm), int(commandedPwm), unsigned(estopped || estopAsserted), unsigned(lowVoltage),
    stateName(s.state), (unsigned long)s.blocks, (unsigned long)s.pausedBlocks, (unsigned long)blockQueueDrops.load(), (unsigned long)navEventDrops.load(),
    (unsigned long)ctlDrops.load(), (unsigned long)s.windows, (unsigned long)s.outDrops, (unsigned long)s.maxPushUs,
    (unsigned long)rvmStackFree.load(), (unsigned long)recorder->hallDrops(), (unsigned long)recorder->nativeDrops(),
    (unsigned long)naviSyncUdpFailures, (unsigned long)pubDrops, (unsigned long)cmdDrops,
    unsigned(ESP.getFreeHeap()), unsigned(ESP.getMinFreeHeap()));
  pub("state/loopstat", payload);
  publishNavState();
  char value[16];
  snprintf(value, sizeof(value), "%d", int(actualPwm)); pub("state/throttle", value, true);
  pub("state/direction", motorDirection ? "2" : "0", true);
  pub("state/auto", "0", true);                      // no automatic operation exists in this build
  pub("state/estop", estopped || estopAsserted ? "1" : "0", true);
}

// ---------------------------------------------------------------- network --
static bool syncSend(const void* bytes, size_t length) {
  if (!naviSyncUdp.beginPacket(naviSyncDest, NAVI_SYNC_PORT) ||
      naviSyncUdp.write(reinterpret_cast<const uint8_t*>(bytes), length) != length ||
      !naviSyncUdp.endPacket()) { ++naviSyncUdpFailures; return false; }
  ++naviSyncDatagrams; return true;
}
static void syncDrain() {
  if (!naviSyncDestValid || WiFi.status() != WL_CONNECTED) return;
  static navi_sync::ConsumptionWire consumed;
  static navi_sync::ActionWire action;
  static navi_sync::NativeHallWire native;
  static navi_sync::HallWire hall;
  for (unsigned i = 0; i < 20; ++i) {
    bool sent = false;
    if (recorder->popConsumption(consumed)) { syncSend(&consumed, sizeof(consumed)); sent = true; }
    if (recorder->popAction(action)) { syncSend(&action, sizeof(action)); sent = true; }
    if (recorder->popNativeHall(native)) { syncSend(&native, sizeof(native)); sent = true; }
    if (i % 2 == 0 && recorder->popHall(hall)) { syncSend(&hall, sizeof(hall)); sent = true; }
    if (!sent) break;
  }
  static uint32_t lastStatus = 0;
  if (millis() - lastStatus < 1000) return;
  lastStatus = millis();
  navi_sync::Status status{};
  status.tMs = lastStatus; status.tUs = esp_timer_get_time();
  status.hallSamples = recorder->hallSamples(); status.hallRingDrops = recorder->hallDrops();
  status.udpFailures = naviSyncUdpFailures; status.datagramsSent = naviSyncDatagrams;
  status.maxHallGapUs = recorder->maxHallGapUs(); status.hallHighWater = recorder->hallHighWater();
  status.wifiConnected = WiFi.status() == WL_CONNECTED; status.mqttConnected = mqtt.connected();
  const navi_sync::StatusWire wire = recorder->makeStatus(lastStatus, status.tUs, recorderContext(), status);
  syncSend(&wire, sizeof(wire));
}
static void networkTask(void*) {
  uint32_t nextConnect = 0, wifiDownSince = 0;
  for (;;) {
    const uint32_t now = millis();
    const bool wifiConnected = WiFi.status() == WL_CONNECTED;
    if (!wifiConnected) {
      if (!wifiDownSince) wifiDownSince = now;
      else if (now - wifiDownSince > 15000) { WiFi.disconnect(); WiFi.begin(WIFI_SSID, WIFI_PASS); wifiDownSince = now; }
    } else wifiDownSince = 0;
    if (wifiConnected && !mqtt.connected() && now >= nextConnect) {
      wifiClient.setConnectionTimeout(3000);
      nextConnect = now + 2000;
      char id[48], online[72];
      snprintf(id, sizeof(id), "RVM_%s", LOCO_NAME);
      snprintf(online, sizeof(online), "ngr/loco/%s/online", LOCO_NAME);
      if (mqtt.connect(id, online, 0, true, "0")) {
        mqtt.publish(online, "1", true);
        // MANUAL only: no auto, go, stop-dispatcher, dispatcher_release, IR subscriptions.
        const char* commands[] = {"estop", "throttle", "direction", "brake", "stop",
                                  "session_direction", "start_mm", "start_interval"};
        for (const char* c : commands) {
          char topic[72]; snprintf(topic, sizeof(topic), "ngr/loco/%s/cmd/%s", LOCO_NAME, c); mqtt.subscribe(topic);
        }
        mqtt.subscribe("ngr/dispatcher/cmd/estop");   // fleet E-STOP is a safety channel, kept
        char boot[400];
        snprintf(boot, sizeof(boot),
          "{\"sketch\":\"%s\",\"class\":\"%s\",\"boot_id\":\"%016llX\",\"field_accepted\":0,"
          "\"navigation\":\"OBSERVATION_ONLY\",\"recognizer\":\"NAVI_RVM_SHAPE_MODEL_A_LOCAL_BASELINE\","
          "\"model\":\"%s\",\"auto\":\"ABSENT\"}",
          SKETCH_NAME, BUILD_CLASS, (unsigned long long)bootId, rvm_shape::MODEL_SHA256);
        char topic[72]; snprintf(topic, sizeof(topic), "ngr/loco/%s/state/bootid", LOCO_NAME);
        mqtt.publish(topic, boot, true);
        Serial.printf("[MQTT] connected broker=%s client=%s\n", MQTT_BROKER, id);
      } else Serial.printf("[MQTT] connect failed state=%d\n", mqtt.state());
    }
    mqtt.loop();
    if (mqtt.connected() && now - lastMqttReportMs >= 5000) {
      lastMqttReportMs = now;
      IPAddress ip = WiFi.localIP();
      char c[200], topic[72];
      snprintf(topic, sizeof(topic), "ngr/loco/%s/state/connectivity", LOCO_NAME);
      snprintf(c, sizeof(c), "{\"wifi\":1,\"ip\":\"%u.%u.%u.%u\",\"rssi\":%d,\"mqtt\":1,\"broker\":\"%s\",\"navi_sync\":%u,\"udp_failures\":%lu}",
               ip[0], ip[1], ip[2], ip[3], WiFi.RSSI(), MQTT_BROKER, naviSyncDestValid, (unsigned long)naviSyncUdpFailures);
      mqtt.publish(topic, c, true);
    }
    syncDrain();
    if (mqtt.connected()) {
      PubMsg m;
      while (pubQ && xQueueReceive(pubQ, &m, 0) == pdTRUE) if (!mqtt.publish(m.topic, m.payload, m.retain)) ++pubDrops;
    }
    vTaskDelay(10);
  }
}

// ------------------------------------------------------------------ setup --
static void haltStartup(const char* why, bool pwmAttached) {
  if (pwmAttached) writePwm(0); else { pinMode(MOTOR_PWM_PIN, OUTPUT); digitalWrite(MOTOR_PWM_PIN, LOW); }
  for (;;) { Serial.printf("[BOOT] FATAL %s; PWM=0; restart required\n", why); delay(5000); }
}

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_PWM_PIN, OUTPUT); digitalWrite(MOTOR_PWM_PIN, LOW);
  delay(300);
  recorder = new (std::nothrow) navi_sync::Recorder();
  if (!recorder || !recorder->storageReady()) haltStartup("recorder storage", false);
  // The 8 KB arrival hold-back buffer lives on the heap (the static DRAM segment is full).
  if (!waveformPause.begin(new (std::nothrow) rvm_shape::Block[rvm_shape::HOLD_MAX])) haltStartup("hold-back storage", false);
  bootId = (uint64_t(esp_random()) << 32) | esp_random();
  uint32_t session = esp_random(); if (!session) session = 1;
  recorder->begin(LOCO_ID, session, bootId);
  naviSyncDestValid = naviSyncDest.fromString(NAVI_SYNC_HOST);
  analogReadResolution(12);
  pinMode(HALL_PIN, INPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT); digitalWrite(MOTOR_DIR_PIN, HIGH);
  ledcAttach(MOTOR_PWM_PIN, PWM_FREQUENCY, PWM_RESOLUTION);
  writePwm(0);
  Wire.begin(I2C_SDA, I2C_SCL);
  inaReady = ina219.begin();
  pubQ = xQueueCreate(PUB_QUEUE_DEPTH, sizeof(PubMsg));
  cmdQ = xQueueCreate(16, sizeof(CmdMsg));
  blockQ = xQueueCreate(128, sizeof(rvm_shape::Block));       // 2.5 s of blocks: covers a re-score burst
  ctlQ = xQueueCreate(8, sizeof(NavCtl));
  navEvQ = xQueueCreate(16, sizeof(NavEvent));
  if (!pubQ || !cmdQ || !blockQ || !ctlQ || !navEvQ) haltStartup("queues", true);
  // core 0: Hall (prio 3) above recognition (prio 1); core 1: loop and network.
  if (xTaskCreatePinnedToCore(hallTask, "hall", 4096, nullptr, 3, nullptr, 0) != pdPASS) haltStartup("hall task", true);
  if (xTaskCreatePinnedToCore(rvmTask, "rvm", 8192, nullptr, 1, nullptr, 0) != pdPASS) haltStartup("rvm task", true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  mqtt.setServer(MQTT_BROKER, 1883);
  mqtt.setCallback(onMqtt);
  if (!mqtt.setBufferSize(1024)) haltStartup("mqtt buffer", true);
  if (xTaskCreatePinnedToCore(networkTask, "net", 8192, nullptr, 1, nullptr, 1) != pdPASS) haltStartup("network task", true);
  if (!inaReady) warn("INA219 NOT FOUND — no battery protection this session");
  Serial.printf("[BOOT] %s %s loco=%s model=%s — manual, navigation observation only, not field accepted\n",
                SKETCH_NAME, BUILD_CLASS, LOCO_NAME, rvm_shape::MODEL_SHA256);
}

void loop() {
  serviceRamp();                                       // E-STOP asserted by the MQTT callback acts here first
  CmdMsg command;
  while (cmdQ && xQueueReceive(cmdQ, &command, 0) == pdTRUE) { handleCommand(command); serviceRamp(); }
  serviceNavEvents();
  serviceRamp();
  serviceBattery();
  serviceStatus();
  delay(1);
}
