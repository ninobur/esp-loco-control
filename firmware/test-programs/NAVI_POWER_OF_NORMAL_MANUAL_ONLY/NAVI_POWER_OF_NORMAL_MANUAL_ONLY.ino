/* ============================================================================
 * NAVI_POWER_OF_NORMAL_MANUAL_ONLY — EXPERIMENTAL. Clean-sheet NAVI built
 * directly from the "Power of Normal" conference record
 * (docs/navi/NAVI_CONFERENCE_POWER_OF_NORMAL_RECORD_20261024.md), per David's
 * authorization of the same date. Not based on RvmShape.h / RvmContinuity.h /
 * RvmNavigator.h: those are discarded entirely, per the independent audits
 * (Claude and Codex, 2026-10-24) that found their active logic in direct
 * conflict with the agreed design. The 2026-10-24 replacement (NaviCandidate.h,
 * NaviEncounter.h, NaviTarget.h, NaviTimingPredictor.h) is itself retired by
 * the 2026-10-27 waveform recognizer; see the NAVIGATION paragraph below.
 *
 * REUSED UNCHANGED from NAVI_RVM_AUTO_STATIONS_BUBBLE r5.1 (commit 5b7a518b,
 * the last-flashed, field-run build): RouteMap.h, RvmStations.h, RvmOps.h,
 * NaviEstop.h, NaviWordStorage.h, NaviSyncRecorder.h, LocoConfig.h, both
 * LL_LocoConfig_*.h. Motor ramp, MQTT, E-stop, battery protection, peer/
 * bubble/station wiring below are carried over from that sketch unchanged;
 * only the navigation call sites are new.
 *
 * MANUAL ONLY. AUTO is hard-refused in handleCommand() below
 * (navi_power::AUTO_GO_PERMITTED is false). The arrival-boundary protective
 * stop (Proposition 6B) was removed 2026-10-24 by David's instruction:
 * "missing a magnet is not our problem; correctly recognizing it is." There
 * is no automatic-stop mechanism in this build at all now, and this gate
 * stays false regardless — this instruction did not authorize enabling AUTO.
 * Manual throttle/direction/declare/E-STOP/battery protection all work
 * normally; GO and AUTO do not.
 *
 * NAVIGATION (new, 2026-10-27): a single-scale matched-filter waveform
 * recognizer (NaviMatchedFilter.h — one fixed 240 ms template, level-only
 * correlation, r >= 0.7, |A| >= 70, 120 ms NMS, causal declaration at
 * passage + 359 ms) over the continuous Hall signal, fed by 1 ms tick
 * medians (hallTask) and correlated in rvmTask, plus a simple next-target
 * DNA matcher (NaviWaveformMatcher.h): confirm and advance on polarity
 * agreement, record the observation and the discrepancy on contradiction,
 * position and target unchanged. No timing gate, no interval scaling, no
 * IR, no sequence re-alignment, no rearview mirror, no competing
 * hypotheses. The 70x2 onset detector (NaviCandidate.h) stays only as a
 * diagnostic cross-check, fed by a trailing median level; it never vetoes
 * or modifies the recognizer. The frozen-median baseline, the PWM-90
 * timing gate, the encounter packaging and the 20 ms tracing of the
 * 2026-10-26 revision are retired; see docs/navi/
 * NAVI_WAVEFORM_RECOGNIZER_IMPLEMENTATION_PLAN_20261027.md.
 * ========================================================================== */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <esp_timer.h>
#include <atomic>
#include <esp_now.h>
#include <esp_wifi.h>

#include "LocoConfig.h"
#include "credentials.h"
#include "RvmOps.h"
#include "NaviEstop.h"
#include "RouteMap.h"
#include "NaviCandidate.h"
#include "NaviWaveformTemplate.h"
#include "NaviMatchedFilter.h"
#include "NaviWaveformMatcher.h"
#include "NaviDiagnosticLevel.h"
#include "RvmStations.h"

namespace navi_power {
// AUTO/GO hard gate, independent of any specific mechanism file. False until
// a protective scheme for AUTO is proposed and approved — removing the old
// arrival-boundary stop (2026-10-24) does not lift this; it was never tied
// to that mechanism's internals, only to the fact that nothing has been
// approved for AUTO yet.
static constexpr bool AUTO_GO_PERMITTED = false;
}  // namespace navi_power

static portMUX_TYPE recorderMux = portMUX_INITIALIZER_UNLOCKED;
#define NAVI_SYNC_ENTER_CRITICAL() portENTER_CRITICAL(&recorderMux)
#define NAVI_SYNC_EXIT_CRITICAL() portEXIT_CRITICAL(&recorderMux)
#include "NaviSyncRecorder.h"

using namespace navi_one;
using navi_eyes::OrderedEstop;

static constexpr char SKETCH_NAME[] = "NAVI_POWER_OF_NORMAL_MANUAL_ONLY";
static constexpr char BUILD_CLASS[] = "EXPERIMENTAL_POWER_OF_NORMAL_MANUAL_ONLY_AUTO_LOCKED_NOT_FIELD_ACCEPTED";
static constexpr uint8_t HALL_PIN = 33;
static constexpr uint8_t I2C_SDA = 21, I2C_SCL = 22;
static constexpr uint16_t MANUAL_STEP_UP_MS = 150;                 // ms per PWM count
static constexpr uint16_t BRAKE_STEP_COAST_MS = 400, BRAKE_STEP_HARD_MS = 15;
static constexpr uint16_t STOP_STEP_DOWN_MS = 31;                  // cmd/stop and low-voltage stop
static constexpr uint16_t AUTO_STEP_UP_MS = 150, AUTO_STEP_DOWN_MS = 31, STOP_FINAL_STEP_DOWN_MS = 300, STOP_RESTART_STEP_UP_MS = 150;
static constexpr uint8_t NAVI_BASE_CRUISE_PWM = 90, NAVI_MAX_AUTO_PWM = 120;
static constexpr uint32_t PEER_TX_MS = 250;
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
static QueueHandle_t pubQ = nullptr, cmdQ = nullptr, medianQ = nullptr, ctlQ = nullptr, navEvQ = nullptr;
static uint32_t pubDrops = 0, cmdDrops = 0;
static std::atomic<uint32_t> medianQueueDrops{0}, navEventDrops{0}, ctlDrops{0};
// 70x2 diagnostic onsets (hallTask only). A count, not a gate: the diagnostic
// never vetoes or modifies the recognizer.
static std::atomic<uint32_t> diagOnsets{0};
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
static bool autoEnrolled = false, autoRunning = false;
static uint64_t activeCommandOrder = 0, activeCommandReceivedUs = 0;

// ------------------------------------------------- navigation (observer) --
// Owned by rvmTask (core 0) except the tick-median producer and the 70x2
// diagnostic, which are local to hallTask. The recognizer core and the
// matcher live in rvmTask. The loop (core 1) sends declarations /
// suspensions via ctlQ and receives events via navEvQ. Nothing here is
// read by the motor path except the NavSnapshot below.
// One 1 ms tick median, hallTask -> rvmTask (medianQ, depth 256 = 256 ms of
// slack; rvmTask drains it every iteration).
struct TickMedian { int16_t median; uint64_t tUs; };
struct NavCtl { uint8_t kind; uint8_t last, next; int8_t dir; };      // 1 declare, 2 suspend
enum : uint8_t { CTL_DECLARE = 1, CTL_SUSPEND = 2 };
// One judged observation, rvmTask -> loop (navEvQ). `e` is the matcher's
// Observation record for the event, plus the PWM at recognition time.
struct NavEventData {
  uint32_t seq = 0;
  uint64_t passageUs = 0, recognizedUs = 0;
  float r = 0, amplitude = 0;
  int8_t pol = 0, expectedPol = 0;
  uint8_t expectedMm = 0;
  uint8_t outcome = 0;  // MatcherOutcome
  uint8_t pwmAtRecognition = 0;
};
struct NavEvent {
  uint8_t kind;      // 1 declared, 2 suspended, 3 confirmed, 4 nonqualifying (polarity discrepancy), 5 abandoned (unused, retained), 6 duplicate-guard, 7 observation (no target)
  uint8_t mm, expected, last, next; int8_t dir;
  NavEventData e;
};
enum : uint8_t { EV_DECLARED = 1, EV_SUSPENDED = 2, EV_CONFIRMED = 3, EV_NONQUALIFYING = 4, EV_ABANDONED = 5, EV_DUPLICATE_GUARD = 6, EV_OBSERVATION = 7 };
struct NavSnapshot {
  uint8_t state = 0, last = 0, next = 0; int8_t dir = 0;
  // confirmed = advances; nonqualifying = recorded polarity discrepancies
  // (observations that did not confirm the target); abandoned is always 0
  // in this build (no encounter packaging); duplicateGuards counts advances
  // refused by the provisional 400 ms guard.
  uint32_t confirmed = 0, nonqualifying = 0, abandoned = 0, duplicateGuards = 0;
  bool encounterActive = false;
};
static NaviMatchedFilter recognizer;   // rvmTask only — the waveform recognizer core
static navi_power::WaveformMatcher matcher;  // rvmTask only — the simple DNA matcher
// ------------------------------------------------------- AUTO and the bubble --
static rvm_ops::StationDriver stationDriver;
static rvm_ops::DriveOutput lastDrive;
static portMUX_TYPE peerMux = portMUX_INITIALIZER_UNLOCKED;
static rvm_ops::PeerPacket peerRx; static uint32_t peerRxMs = 0; static bool peerSeen = false;
static uint32_t peerRxCount = 0, peerRxRejected = 0, peerTxFail = 0, peerTxSeq = 0;
static volatile bool espNowReady = false;
static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static NavSnapshot navShared;
static portMUX_TYPE navMux = portMUX_INITIALIZER_UNLOCKED;
static std::atomic<uint32_t> rvmStackFree{0};

static NavSnapshot navSnapshot() {
  portENTER_CRITICAL(&navMux); NavSnapshot s = navShared; portEXIT_CRITICAL(&navMux); return s;
}

// ------------------------------------------------------------ recorder --
static navi_sync::Context recorderContext() {
  const NavSnapshot s = navSnapshot();
  navi_sync::Context c;
  const bool tracking = s.state == uint8_t(navi_power::MatcherState::Tracking);
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
static void requestPwm(int target_, uint16_t up, uint16_t down) {
  target_ = constrain(target_, 0, 255);
  const bool changed = target_ != rampTarget || up != rampUpMs || down != rampDownMs;
  rampTarget = target_; commandedPwm = target_; rampUpMs = up; rampDownMs = down;
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
  o.safeDirPwm = SAFE_DIRECTION_CHANGE_PWM;
  o.positionKnown = navSnapshot().state == uint8_t(navi_power::MatcherState::Tracking);
  o.forward = motorDirection != 0; o.enrolled = autoEnrolled;
  return o;
}

// ------------------------------------------------------------- Hall task --
// 5 kHz native reads (5 per 1 ms tick). Two things happen here, and neither
// is gated on PWM, navigation state, or any baseline (David's instruction,
// 2026-10-24: do not withhold observations):
//   (a) the tick median (median of the 5 reads) is sent to rvmTask, where
//       the matched-filter recognizer correlates it;
//   (b) the 70x2 onset detector runs as a DIAGNOSTIC cross-check only,
//       against a trailing median level. Its onsets are counted and logged
//       to Serial; they never reach the matcher and never veto anything.
// Native/legacy NSR1 records for the Pi are unchanged.
static void hallTask(void*) {
  static navi_power::CandidateDetector candidate;  // diagnostic only
  static navi_power::DiagnosticLevel diagLevel;    // diagnostic level source
  static uint32_t serial = 0;
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    navi_sync::HallSample wire{};
    const uint64_t firstUs = esp_timer_get_time();
    const int8_t routeDir = travelDirection();
    const uint8_t dirByte = routeDir > 0 ? 1 : routeDir < 0 ? 2 : 0;
    const auto ctx = recorderContext();
    int16_t reads[5];
    for (unsigned i = 0; i < 5; ++i) {
      ++serial;
      const uint64_t us = esp_timer_get_time();
      const int raw = analogRead(HALL_PIN);
      reads[i] = int16_t(raw);
      wire.raw[i] = uint16_t(raw);
      navi_sync::NativeHallItem item{};
      item.serial = serial; item.tUs = us; item.raw = int16_t(raw); item.pwm = uint8_t(actualPwm); item.direction = dirByte;
      recorder->addNativeHall(item, ctx);
      // 70x2 diagnostic: unconditional, against the trailing median level.
      diagLevel.add(int16_t(raw));
      if (diagLevel.haveLevel()) {
        navi_power::CandidateOnset onset;
        if (candidate.onSample(uint16_t(raw), diagLevel.level(), serial, us,
                               uint8_t(actualPwm), onset)) {
          ++diagOnsets;
          Serial.printf("[DIAG70X2] onset pol=%+d openUs=%llu pwm=%u level=%.1f\n",
                        int(onset.pol), (unsigned long long)onset.openUs,
                        unsigned(onset.pwmAtOpen), double(onset.levelAtOpen));
        }
      }
    }
    // The tick median is the recognizer's input. Its timestamp is the first
    // read of the tick; the 5 reads span ~230 us, negligible against the
    // 359 ms recognition latency. The median is also recorded in the NSR
    // Hall record (it was previously left as INT16_MIN).
    const int16_t tickMedian = medianOf5(reads);
    wire.median = tickMedian;
    wire.pwmActual = uint8_t(actualPwm); wire.pwmCommanded = uint8_t(commandedPwm);
    wire.flags = motorDirection ? navi_sync::HALL_F_DIR_FWD : 0;
    if (actualPwm == 0) wire.flags |= navi_sync::HALL_F_HOLD;
    recorder->addHall(firstUs, uint32_t(firstUs / 1000), wire, ctx);
    const TickMedian m{tickMedian, firstUs};
    if (!medianQ || xQueueSend(medianQ, &m, 0) != pdTRUE) ++medianQueueDrops;
    vTaskDelayUntil(&wake, 1);
  }
}

// -------------------------------------------------------------- RVM task --
static void publishSnapshot() {
  NavSnapshot s;
  s.state = uint8_t(matcher.state()); s.last = matcher.lastMm(); s.next = matcher.nextMm(); s.dir = matcher.dir();
  s.confirmed = matcher.confirmed(); s.nonqualifying = matcher.discrepancies(); s.abandoned = 0;
  s.duplicateGuards = matcher.duplicateGuards();
  s.encounterActive = false;
  portENTER_CRITICAL(&navMux);
  navShared = s;
  portEXIT_CRITICAL(&navMux);
}
static void postEvent(const NavEvent& ev) { if (!navEvQ || xQueueSend(navEvQ, &ev, 0) != pdTRUE) ++navEventDrops; }

// Judge one recognized event against the current target (Option B: confirm
// on agreement, record on contradiction). The observation is already in the
// matcher's record; this only packages it for the loop thread.
static void judgeEvent(const NaviRecognizedEvent& e) {
  const uint8_t expectedBefore = matcher.nextMm();
  const auto out = matcher.onEvent(e);
  const auto& o = matcher.latest();
  NavEvent ev{};
  ev.expected = expectedBefore; ev.last = matcher.lastMm();
  ev.next = matcher.nextMm(); ev.dir = matcher.dir();
  ev.e.seq = o.seq; ev.e.passageUs = o.passageUs; ev.e.recognizedUs = o.recognizedUs;
  ev.e.r = o.r; ev.e.amplitude = o.amplitude; ev.e.pol = o.polarity;
  ev.e.expectedMm = o.expectedMm; ev.e.expectedPol = o.expectedPolarity;
  ev.e.outcome = uint8_t(o.outcome);
  ev.e.pwmAtRecognition = uint8_t(actualPwm);
  switch (out) {
    case navi_power::MatcherOutcome::Confirmed:
      ev.kind = EV_CONFIRMED; ev.mm = matcher.lastMm(); break;
    case navi_power::MatcherOutcome::Discrepancy:
      ev.kind = EV_NONQUALIFYING; ev.mm = 0; break;
    case navi_power::MatcherOutcome::DuplicateGuard:
      ev.kind = EV_DUPLICATE_GUARD; ev.mm = 0; break;
    default:  // NoClaim: no target to judge against; still reported
      ev.kind = EV_OBSERVATION; ev.mm = 0; break;
  }
  postEvent(ev);
}
static void applyControl() {
  NavCtl c;
  while (ctlQ && xQueueReceive(ctlQ, &c, 0) == pdTRUE) {
    // The control takes effect now (esp_timer_get_time, the same clock
    // as the event passage times). The matcher records this time so an
    // event whose passage predates it is never judged against the new
    // navigation state (see the ordering rule in NaviWaveformMatcher.h).
    const uint64_t nowUs = esp_timer_get_time();
    if (c.kind == CTL_DECLARE) {
      matcher.declare(c.last, c.next, c.dir, nowUs);
      NavEvent ev{}; ev.kind = EV_DECLARED; ev.last = matcher.lastMm(); ev.next = matcher.nextMm(); ev.dir = matcher.dir();
      publishSnapshot(); postEvent(ev);
    } else {
      matcher.suspend(nowUs);
      NavEvent ev{}; ev.kind = EV_SUSPENDED; ev.last = matcher.lastMm(); ev.next = matcher.nextMm(); ev.dir = matcher.dir();
      publishSnapshot(); postEvent(ev);
    }
  }
}
static void rvmTask(void*) {
  // Medians lost to a median-queue overflow, as counted by hallTask.
  // The count is the loss signal (see noteDataLoss); it is checked
  // before feeding each median, so the first post-loss median is fed
  // into a fresh core.
  uint32_t lastMedianDrops = 0;
  for (;;) {
    // Drain the tick medians (block briefly so declarations and the snapshot
    // still get serviced when the Hall stream pauses).
    TickMedian m;
    bool got = medianQ && xQueueReceive(medianQ, &m, pdMS_TO_TICKS(50)) == pdTRUE;
    if (got) {
      auto feedMedian = [&](const TickMedian& mm) {
        const uint32_t drops = medianQueueDrops.load();
        if (drops != lastMedianDrops) {
          recognizer.noteDataLoss();
          lastMedianDrops = drops;
        }
        recognizer.pushTickMedian(mm.median, mm.tUs);
      };
      feedMedian(m);
      while (medianQ && xQueueReceive(medianQ, &m, 0) == pdTRUE)
        feedMedian(m);
    }
    // Judge events already declared BEFORE applying any pending control,
    // so an observation is evaluated against the navigation state in
    // effect when it was observed, not against a state established after
    // it. (The matcher's control-timestamp guard covers the recognizer's
    // ~359 ms latency window, where an event is declared after the
    // control that follows its passage.)
    while (recognizer.hasEvent()) judgeEvent(recognizer.nextEvent());
    applyControl();
    publishSnapshot();
    rvmStackFree.store(uxTaskGetStackHighWaterMark(nullptr));
  }
}

// ---------------------------------------------------------- nav reporting --
static const char* stateName(uint8_t s) {
  return s == uint8_t(navi_power::MatcherState::Tracking) ? "TRACKING" : "UNSET";
}
static char lastNavEvent[24] = "NONE";
static void publishNavState() {
  const NavSnapshot s = navSnapshot();
  // Payload format is kept stable for the Pi-side logger (docs/CLAUDE.md):
  // the timing_* fields are 0/false (no timing gate in this build) and the
  // blocks_* fields are 0 (no 20 ms block pipeline); duplicate_guards is
  // new. `nonqualifying` counts recorded polarity discrepancies.
  char payload[640];
  snprintf(payload, sizeof(payload),
    "{\"state\":\"%s\",\"mm\":%u,\"target\":%u,\"dir\":%d,\"session_dir\":\"%s\","
    "\"position_reliable\":%u,\"event\":\"%s\","
    "\"confirmed\":%lu,\"nonqualifying\":%lu,\"abandoned\":%lu,\"duplicate_guards\":%lu,\"encounter_active\":%u,"
    "\"blocks_emitted\":%lu,\"blocks_dropped\":%lu,"
    "\"timing_have_anchor\":%u,\"timing_have_profile\":%u,\"timing_calibration_ready\":%u,\"timing_calibration_covered_mm\":%u,\"timing_calibration_required_mm\":%u,\"timing_result\":%u,\"timing_predicted_ms\":%lu,\"timing_lower_ms\":%lu,\"timing_upper_ms\":%lu,\"timing_measured_ms\":%lu,\"timing_tolerance_ms\":%lu,\"timing_agrees\":%u,"
    "\"authority\":\"NAVI_POWER_OF_NORMAL_MANUAL_ONLY\"}",
    stateName(s.state), s.last, s.next, s.dir, sessionDir > 0 ? "CW" : sessionDir < 0 ? "CCW" : "UNSET",
    s.state == uint8_t(navi_power::MatcherState::Tracking), lastNavEvent,
    (unsigned long)s.confirmed, (unsigned long)s.nonqualifying, (unsigned long)s.abandoned, (unsigned long)s.duplicateGuards, s.encounterActive,
    0UL, 0UL,
    0, 0, 0, 0u, 0u, 0u, 0UL, 0UL, 0UL, 0UL, 0UL, 0u);
  pub("state/nav", payload, true);
  pub("state/nav_ready", s.state == uint8_t(navi_power::MatcherState::Tracking) ? "1" : "0", true);
}
static void serviceNavEvents() {
  NavEvent ev;
  for (unsigned k = 0; k < 4 && navEvQ && xQueueReceive(navEvQ, &ev, 0) == pdTRUE; ++k) {
    if (ev.kind == EV_DECLARED) {
      strlcpy(lastNavEvent, "DECLARED", sizeof(lastNavEvent));
      stationDriver.reset();
      Serial.printf("[NAVI] DECLARED last=MM%03u next=MM%03u dir=%d\n", ev.last, ev.next, ev.dir);
    } else if (ev.kind == EV_SUSPENDED) {
      strlcpy(lastNavEvent, "SUSPENDED", sizeof(lastNavEvent));
    } else {
      const char* result = ev.kind == EV_CONFIRMED ? "CONFIRMED" : ev.kind == EV_DUPLICATE_GUARD ? "DUPLICATE_GUARD" : ev.kind == EV_OBSERVATION ? "OBSERVATION" : ev.kind == EV_ABANDONED ? "ABANDONED" : "NONQUALIFYING";
      strlcpy(lastNavEvent, result, sizeof(lastNavEvent));
      char mm[8] = "null";
      if (ev.kind == EV_CONFIRMED) snprintf(mm, sizeof(mm), "%u", ev.mm);
      const auto& e = ev.e;
      const char* expPol = e.outcome == uint8_t(navi_power::MatcherOutcome::NoClaim) ? "NONE" : (e.expectedPol > 0 ? "N" : "S");
      // The row keeps its existing fields (width_blocks/peak_adc stay 0, as
      // they already were; meets_min_width stays 1, vestigial) and gains the
      // recognizer's event fields, so the exact point of divergence is
      // diagnosable. opening_us is the passage (template-peak) time;
      // opening_pwm is the PWM at recognition time.
      char row[560];
      snprintf(row, sizeof(row),
        "{\"boot\":\"%016llX\",\"result\":\"%s\",\"mm\":%s,\"expected_mm\":%u,"
        "\"expected_polarity\":\"%s\",\"polarity\":\"%s\",\"dir\":%d,\"width_blocks\":%u,"
        "\"peak_adc\":%.1f,\"meets_min_width\":%u,"
        "\"opening_us\":%llu,\"opening_pwm\":%u,"
        "\"seq\":%lu,\"passage_us\":%llu,\"recognized_us\":%llu,\"score_r\":%.3f,\"amplitude_adc\":%.1f,\"outcome\":\"%s\","
        "\"authority\":\"WAVEFORM_RECOGNITION\"}",
        (unsigned long long)bootId, result, mm, ev.expected,
        expPol, e.pol > 0 ? "N" : "S", ev.dir,
        0u, 0.0, 1u,
        (unsigned long long)e.passageUs, unsigned(e.pwmAtRecognition),
        (unsigned long)e.seq, (unsigned long long)e.passageUs, (unsigned long long)e.recognizedUs,
        double(e.r), double(e.amplitude), result);
      pub("diag/navi_encounter", row);
      Serial.printf("[NAVI] %s\n", row);
      if (ev.kind == EV_CONFIRMED) pub("mm/marker", row);
      // A discrepancy or a duplicate-guard refusal is logged, not warned:
      // a recorded observation that disagrees with the map is evidence, not
      // a fault. Nothing in this build warns or stops for it.
    }
    publishNavState();
  }
}
static void navControl(uint8_t kind, uint8_t last = 0, uint8_t next = 0, int8_t dir = 0) {
  NavCtl c{kind, last, next, dir};
  if (!ctlQ || xQueueSend(ctlQ, &c, 0) != pdTRUE) { ++ctlDrops; warn("NAVIGATION CONTROL QUEUE FULL: declaration not applied"); }
}
static void withdrawAuto(const char* why);
static void declare(uint8_t last, uint8_t next, int8_t dir, const char* interval) {
  withdrawAuto("AUTO WITHDRAWN: new declaration");
  recordDeclaration(navi_sync::InputKind::Declaration, last, dir);
  navControl(CTL_DECLARE, last, next, dir);
  clearWarning();
  char value[8]; snprintf(value, sizeof(value), "%u", last);
  pub("state/start_mm", value, true);
  pub("state/start_interval", interval, true);
}
static void suspendIfDeclared(const char* why) {
  const NavSnapshot s = navSnapshot();
  if (s.state == uint8_t(navi_power::MatcherState::Unset)) return;
  recordDeclaration(navi_sync::InputKind::Reversal, uint32_t(travelDirection() > 0 ? 1 : 2), travelDirection());
  navControl(CTL_SUSPEND);
  warn(why);
}

// ---------------------------------------------------- AUTO and the bubble --
static void publishAuto() { pub("state/auto", autoEnrolled ? "1" : "0", true); pub("state/auto_running", autoRunning ? "1" : "0", true); }
static void withdrawAuto(const char* why) {
  const bool was = autoEnrolled || autoRunning;
  autoRunning = autoEnrolled = false;
  requestPwm(0, 0, AUTO_STEP_DOWN_MS);
  publishAuto();
  if (was && why) warn(why);
}
static void onPeer(const esp_now_recv_info_t* info, const uint8_t* bytes, int length) {
  if (!info || !bytes || length != int(sizeof(rvm_ops::PeerPacket))) { ++peerRxRejected; return; }
  rvm_ops::PeerPacket p; memcpy(&p, bytes, sizeof(p));
  if (p.magic != 0xB7 || p.version != 3 || p.locoId == uint32_t(LOCO_ID)) { ++peerRxRejected; return; }
  portENTER_CRITICAL(&peerMux);
  peerRx = p; peerRxMs = millis(); peerSeen = true; ++peerRxCount;
  portEXIT_CRITICAL(&peerMux);
}
static rvm_ops::PeerView peerView() {
  rvm_ops::PeerView v;
  portENTER_CRITICAL(&peerMux);
  v.everSeen = peerSeen; v.p = peerRx; const uint32_t at = peerRxMs;
  portEXIT_CRITICAL(&peerMux);
  v.ageMs = v.everSeen ? millis() - at : 0;
  return v;
}
static void servicePeerTx() {
  static uint32_t last = 0;
  if (!espNowReady || millis() - last < PEER_TX_MS) return;
  last = millis();
  const NavSnapshot s = navSnapshot();
  rvm_ops::PeerPacket p;
  p.locoId = uint32_t(LOCO_ID); p.seq = ++peerTxSeq;
  p.reliable = s.state == uint8_t(navi_power::MatcherState::Tracking); p.dir = s.dir; p.lastMm = s.last;
  p.station = lastDrive.station; p.execution = uint8_t(lastDrive.cmd.execution);
  p.autoRunning = autoRunning; p.bubble = uint8_t(lastDrive.bubble); p.role = uint8_t(lastDrive.role); p.pwm = uint8_t(actualPwm < 0 ? 0 : actualPwm > 255 ? 255 : actualPwm);
  if (esp_now_send(BROADCAST_MAC, reinterpret_cast<const uint8_t*>(&p), sizeof(p)) != ESP_OK) ++peerTxFail;
}
static void publishStation(bool force) {
  static uint32_t last = 0; static uint8_t lastExec = 0xFF, lastStation = 0xFE, lastKind = 0xFF, lastBubble = 0xFF;
  const auto& d = lastDrive;
  static uint8_t lastLink = 0xFF, lastRole = 0xFF;
  const bool changed = uint8_t(d.cmd.execution) != lastExec || d.station != lastStation || uint8_t(d.kind) != lastKind ||
                       uint8_t(d.bubble) != lastBubble || uint8_t(d.link) != lastLink || uint8_t(d.role) != lastRole;
  lastLink = uint8_t(d.link); lastRole = uint8_t(d.role);
  if (!force && !changed && millis() - last < 1000) return;
  last = millis(); lastExec = uint8_t(d.cmd.execution); lastStation = d.station; lastKind = uint8_t(d.kind); lastBubble = uint8_t(d.bubble);
  const rvm_ops::PeerView v = peerView();
  const NavSnapshot s = navSnapshot();
  char payload[700];
  snprintf(payload, sizeof(payload),
    "{\"auto\":%u,\"running\":%u,\"mm\":%u,\"dir\":%d,\"station\":\"%s\",\"target_kind\":\"%s\",\"target_mm\":%u,"
    "\"section\":\"%s\",\"remaining_mm\":%ld,\"execution\":\"%s\",\"pwm_cmd\":%u,\"pwm\":%d,\"bubble\":\"%s\",\"occupancy\":\"%s\","
    "\"peer\":{\"seen\":%u,\"age_ms\":%lu,\"loco\":%lu,\"reliable\":%u,\"dir\":%d,\"mm\":%u,\"station\":%u,\"execution\":\"%s\",\"auto\":%u},"
    "\"paired\":%u,\"link\":\"%s\",\"role\":\"%s\",\"confirmed_role\":\"%s\",\"peer_role\":\"%s\",\"platform_held\":%u,"
    "\"peer_rx\":%lu,\"peer_rx_rejected\":%lu,\"peer_tx_fail\":%lu,\"espnow\":%u,\"hold_mm\":%u}",
    unsigned(autoEnrolled), unsigned(autoRunning), s.last, s.dir,
    d.station < rvm_ops::STATION_COUNT ? rvm_ops::STATIONS[d.station].name : "NONE",
    rvm_ops::targetKindName(d.kind), d.targetMm, rvm_ops::sectionName(d.geo.section), long(d.geo.remainingMm / rvm_ops::kStopMm),
    rvm_ops::executionName(d.cmd.execution), unsigned(d.cmd.pwm), int(actualPwm),
    d.bubble == rvm_ops::BubbleState::Holding ? "HOLDING" : d.bubble == rvm_ops::BubbleState::Approaching ? "APPROACHING" : "NONE",
    rvm_ops::occupancyName(d.occupancy),
    unsigned(v.everSeen), (unsigned long)v.ageMs, (unsigned long)v.p.locoId, unsigned(v.p.reliable), int(v.p.dir), v.p.lastMm,
    unsigned(v.p.station), rvm_ops::executionName(rvm_ops::StopExecution(v.p.execution & 3)), unsigned(v.p.autoRunning),
    unsigned(d.paired), rvm_ops::linkName(d.link), rvm_ops::roleName(d.role), rvm_ops::roleName(d.confirmedRole),
    rvm_ops::roleName(rvm_ops::Role(v.p.role > 2 ? 0 : v.p.role)), unsigned(d.platformHeld),
    (unsigned long)peerRxCount, (unsigned long)peerRxRejected, (unsigned long)peerTxFail, unsigned(espNowReady), unsigned(rvm_ops::BUBBLE_HOLD_MM));
  pub("state/station", payload, true);
}
// One AUTO decision per loop pass. Navigation is read from the snapshot only.
// There is no arrival-boundary stop any more (removed 2026-10-24, David's
// instruction: "missing a magnet is not our problem; correctly recognizing
// it is"). AUTO_GO_PERMITTED stays false regardless, so this path is not
// reachable in this build either way.
static void serviceAuto() {
  const NavSnapshot s = navSnapshot();
  const bool tracking = s.state == uint8_t(navi_power::MatcherState::Tracking);
  if (autoRunning && !tracking)
    withdrawAuto("AUTO WITHDRAWN: navigation not tracking — controlled stop.");
  if (autoRunning && (estopped || estopAsserted || lowVoltage)) { autoRunning = false; publishAuto(); }
  rvm_ops::DriveInput in;
  in.reliable = tracking; in.pos = s.last; in.dir = s.dir;
  in.cruise = tracking ? navi_one::cruisePwmAt(s.last, s.dir, NAVI_BASE_CRUISE_PWM) : NAVI_BASE_CRUISE_PWM;
  in.appliedPwm = uint8_t(actualPwm); in.commandedPwm = uint8_t(rampTarget); in.nowMs = millis();
  in.enabled = autoRunning; in.peer = peerView();
  lastDrive = stationDriver.step(in);
  if (autoRunning && !estopped && !estopAsserted && !lowVoltage) {
    const int value = constrain(int(lastDrive.cmd.pwm), 0, int(NAVI_MAX_AUTO_PWM));
    if (rampTarget != value) {
      const uint16_t up = lastDrive.cmd.execution == rvm_ops::StopExecution::Released ? STOP_RESTART_STEP_UP_MS : AUTO_STEP_UP_MS;
      const uint16_t down = (lastDrive.cmd.execution == rvm_ops::StopExecution::Braking && !lastDrive.urgent) ? STOP_FINAL_STEP_DOWN_MS : AUTO_STEP_DOWN_MS;
      requestPwm(value, up, down);
    }
  }
  publishStation(false);
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
  const bool dispatcher = strstr(command.topic, "/dispatcher/") != nullptr;
  if (dispatcher && strstr(command.topic, "/go/")) leaf = "go";
  else if (dispatcher && strstr(command.topic, "/stop/")) leaf = "stop";
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
  } else if (!strcmp(leaf, "auto")) {
    bool want;
    if (!parseBool(command.payload, want)) { warn("AUTO REFUSED: expected 0 or 1"); return; }
    if (!want) { withdrawAuto(nullptr); return; }
    // HARD GATE: not an MQTT-settable option. Removing the arrival-boundary
    // stop (2026-10-24) did not lift this — nothing has been approved for
    // AUTO operation yet, and this instruction did not authorize it either.
    if (!navi_power::AUTO_GO_PERMITTED) {
      warn("AUTO REFUSED: not approved for this build — manual operation only");
      return;
    }
    if (Refusal reason = admitAuto(o)) { warn(reason); return; }
    autoEnrolled = true; publishAuto();
  } else if (!strcmp(leaf, "dispatcher_release")) {
    autoRunning = autoEnrolled = false;
    requestPwm(0, 0, AUTO_STEP_DOWN_MS);
    publishAuto();
    warn("RELEASED by dispatcher");
  } else if (!strcmp(leaf, "go")) {
    if (!navi_power::AUTO_GO_PERMITTED) { warn("GO REFUSED: not approved for this build — manual operation only"); return; }
    if (Refusal reason = admitGo(o)) { warn(reason); return; }
    autoRunning = true; clearWarning(); publishAuto();
  } else if (!strcmp(leaf, "throttle")) {
    if (Refusal reason = admitThrottle(o)) { warn(reason); return; }
    int value;
    if (!parseInt(command.payload, value)) { warn("THROTTLE REFUSED: invalid value"); return; }
    requestPwm(value, MANUAL_STEP_UP_MS, 0);
  } else if (!strcmp(leaf, "stop")) {
    if (autoRunning) { autoRunning = false; publishAuto(); }
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
    withdrawAuto("AUTO WITHDRAWN: direction changed");
    motorDirection = forward ? 1 : 0;
    pub("state/direction", motorDirection ? "2" : "0", true);
    if (travelDirection() != before)
      suspendIfDeclared("DIRECTION CHANGED: navigation suspended. Declare the position again.");
  } else if (!strcmp(leaf, "session_direction")) {
    int8_t direction;
    if (!parseSessionDir(command.payload, direction)) { warn("SESSION_DIRECTION: expected CW or CCW"); return; }
    if (Refusal reason = admitDeclaration(o)) { warn(reason); return; }
    const int8_t before = travelDirection();
    withdrawAuto("AUTO WITHDRAWN: session direction changed");
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
  // Payload format is kept stable for the Pi-side logger (docs/CLAUDE.md):
  // the retired block/onset queue counters and blocks_* fields are kept
  // and report 0 (those queues and the 20 ms block pipeline no longer
  // exist); the new median_q_drop, diag_onsets and duplicate_guards
  // fields are added alongside them, not in their place.
  char payload[700];
  snprintf(payload, sizeof(payload),
    "{\"build\":\"%s\",\"class\":\"%s\",\"pwm\":%d,\"commanded\":%d,\"estop\":%u,\"lowvolt\":%u,"
    "\"nav\":\"%s\",\"confirmed\":%lu,\"nonqualifying\":%lu,\"abandoned\":%lu,\"duplicate_guards\":%lu,"
    "\"block_q_drop\":%lu,\"onset_q_drop\":%lu,\"median_q_drop\":%lu,\"diag_onsets\":%lu,\"nav_event_drop\":%lu,\"ctl_drop\":%lu,"
    "\"blocks_emitted\":%lu,\"blocks_dropped\":%lu,\"auto_go_permitted\":%u,\"rvm_stack_free\":%lu,"
    "\"nsr_hall_drop\":%lu,\"nsr_native_drop\":%lu,\"nsr_udp_fail\":%lu,\"pub_drop\":%lu,\"cmd_drop\":%lu,"
    "\"heap_free\":%u,\"heap_min\":%u}",
    SKETCH_NAME, BUILD_CLASS, int(actualPwm), int(commandedPwm), unsigned(estopped || estopAsserted), unsigned(lowVoltage),
    stateName(s.state), (unsigned long)s.confirmed, (unsigned long)s.nonqualifying, (unsigned long)s.abandoned, (unsigned long)s.duplicateGuards,
    0UL, 0UL, (unsigned long)medianQueueDrops.load(), (unsigned long)diagOnsets.load(), (unsigned long)navEventDrops.load(), (unsigned long)ctlDrops.load(),
    0UL, 0UL, unsigned(navi_power::AUTO_GO_PERMITTED), (unsigned long)rvmStackFree.load(),
    (unsigned long)recorder->hallDrops(), (unsigned long)recorder->nativeDrops(),
    (unsigned long)naviSyncUdpFailures, (unsigned long)pubDrops, (unsigned long)cmdDrops,
    unsigned(ESP.getFreeHeap()), unsigned(ESP.getMinFreeHeap()));
  pub("state/loopstat", payload);
  publishNavState();
  char value[16];
  snprintf(value, sizeof(value), "%d", int(actualPwm)); pub("state/throttle", value, true);
  pub("state/direction", motorDirection ? "2" : "0", true);
  pub("state/auto", autoEnrolled ? "1" : "0", true);
  pub("state/auto_running", autoRunning ? "1" : "0", true);
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
    static uint32_t nextEspNow = 0;
    if (wifiConnected && !espNowReady && now >= nextEspNow) {
      nextEspNow = now + 15000;
      esp_err_t r = esp_now_init();
      if (r == ESP_OK) r = esp_now_register_recv_cb(onPeer);
      if (r == ESP_OK) {
        esp_now_peer_info_t peer{}; memcpy(peer.peer_addr, BROADCAST_MAC, 6); peer.channel = 0; peer.ifidx = WIFI_IF_STA; peer.encrypt = false;
        if (!esp_now_is_peer_exist(BROADCAST_MAC)) r = esp_now_add_peer(&peer);
      }
      espNowReady = r == ESP_OK;
      if (!espNowReady) esp_now_deinit();
      Serial.printf("[PEER] ESP-NOW %s (err %d)\n", espNowReady ? "READY" : "FAILED", int(r));
    }
    if (wifiConnected && !mqtt.connected() && now >= nextConnect) {
      wifiClient.setConnectionTimeout(3000);
      nextConnect = now + 2000;
      char id[48], online[72];
      snprintf(id, sizeof(id), "POWNM_%s", LOCO_NAME);
      snprintf(online, sizeof(online), "ngr/loco/%s/online", LOCO_NAME);
      if (mqtt.connect(id, online, 0, true, "0")) {
        mqtt.publish(online, "1", true);
        const char* commands[] = {"estop", "throttle", "direction", "brake", "stop", "auto", "go",
                                  "session_direction", "start_mm", "start_interval", "dispatcher_release"};
        for (const char* c : commands) {
          char topic[72]; snprintf(topic, sizeof(topic), "ngr/loco/%s/cmd/%s", LOCO_NAME, c); mqtt.subscribe(topic);
        }
        mqtt.subscribe("ngr/dispatcher/cmd/estop");
        { char topic[72];
          snprintf(topic, sizeof(topic), "ngr/dispatcher/cmd/go/%s", LOCO_NAME); mqtt.subscribe(topic);
          snprintf(topic, sizeof(topic), "ngr/dispatcher/cmd/stop/%s", LOCO_NAME); mqtt.subscribe(topic); }
        char boot[400];
        snprintf(boot, sizeof(boot),
          "{\"sketch\":\"%s\",\"class\":\"%s\",\"boot_id\":\"%016llX\",\"field_accepted\":0,"
          "\"navigation\":\"NAVI_POWER_OF_NORMAL\",\"recognizer\":\"NAVI_WAVEFORM_MATCHED_FILTER_SINGLE240\","
          "\"auto_go_permitted\":%u,\"auto\":\"LOCKED\",\"bubble_hold_mm\":%u}",
          SKETCH_NAME, BUILD_CLASS, (unsigned long long)bootId, unsigned(navi_power::AUTO_GO_PERMITTED), unsigned(rvm_ops::BUBBLE_HOLD_MM));
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
  bootId = (uint64_t(esp_random()) << 32) | esp_random();
  stationDriver.seed(esp_random());
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
  medianQ = xQueueCreate(256, sizeof(TickMedian));   // 256 ms of tick medians
  ctlQ = xQueueCreate(8, sizeof(NavCtl));
  navEvQ = xQueueCreate(16, sizeof(NavEvent));
  if (!pubQ || !cmdQ || !medianQ || !ctlQ || !navEvQ) haltStartup("queues", true);
  if (xTaskCreatePinnedToCore(hallTask, "hall", 4096, nullptr, 3, nullptr, 0) != pdPASS) haltStartup("hall task", true);
  if (xTaskCreatePinnedToCore(rvmTask, "rvm", 8192, nullptr, 1, nullptr, 0) != pdPASS) haltStartup("rvm task", true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  mqtt.setServer(MQTT_BROKER, 1883);
  mqtt.setCallback(onMqtt);
  if (!mqtt.setBufferSize(1024)) haltStartup("mqtt buffer", true);
  if (xTaskCreatePinnedToCore(networkTask, "net", 8192, nullptr, 1, nullptr, 1) != pdPASS) haltStartup("network task", true);
  if (!inaReady) warn("INA219 NOT FOUND — no battery protection this session");
  Serial.printf("[BOOT] %s %s loco=%s auto_go_permitted=%u — EXPERIMENTAL, MANUAL ONLY, not field accepted\n",
                SKETCH_NAME, BUILD_CLASS, LOCO_NAME, unsigned(navi_power::AUTO_GO_PERMITTED));
}

void loop() {
  serviceRamp();
  CmdMsg command;
  while (cmdQ && xQueueReceive(cmdQ, &command, 0) == pdTRUE) { handleCommand(command); serviceRamp(); }
  serviceNavEvents();
  serviceAuto();
  serviceRamp();
  servicePeerTx();
  serviceBattery();
  serviceStatus();
  delay(1);
}
