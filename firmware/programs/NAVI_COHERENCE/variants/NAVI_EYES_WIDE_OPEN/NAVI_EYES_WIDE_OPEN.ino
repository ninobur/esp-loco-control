/*
 * NAVI_EYES_WIDE_OPEN - Toby. 2026-09-28. Built from NAVI_COHERENCE 0.6
 * POSITION_STATIONS_R1_20Q3 + NSR1 (3d6a1d3). The end of X22R in this sketch.
 *   * X22/X22R is removed from the Hall -> NAVI path (decision 0102). The Hall
 *     task only acquires: one median-of-five ADC reading per ms (0073) into a
 *     sample ring. NAVI reads every native sample on the loop thread.
 *   * MM opening: |raw - NAVI's Hall reference| >= 70 counts on two
 *     consecutive same-sign samples; the second sample is the opening and
 *     fixes its polarity. Every opening reaches NAVI's judge().
 *   * NAVI's Hall reference is spatial (0107, operator specification
 *     2026-09-28): after each accepted opening with an IR point, 0-100 mm of
 *     IR route travel is clearance, 100-200 mm is collected, and at 200 mm the
 *     median becomes the reference for the next MM. Stationary spans add
 *     nothing. A stationary boot reference is taken first (0107).
 *   * Motive PWM 0 (0106): openings are retained with no navigation authority;
 *     IR pulses measured at PWM 0 are not route displacement.
 *   * With valid MM-referenced IR distance, a wrong-polarity opening does not
 *     become the expected MM; it is held as contradictory evidence.
 * NOT FLASHED. NOT FIELD TESTED. See CHANGES_EYES_WIDE_OPEN.md.
 *
 * Inherited history:
 * NAVI_COHERENCE 0.6 PROXIMAL_R1 - Toby. 2026-09-23 recovery revision.
 * IR_SPEED_R3: NAVI interprets settled stops for the display; raw IR unchanged.
 * POSITION_STATIONS_R1: current position governs station targets, including
 * pause/resume inside the approach or zone. Independent review/field test pending.
 * Replaces global matching with physical filtering and proximal recovery.
 * Previous CAL0_FIX1 revision added observation-only IR health indicators.
 * The earlier observation-only health change preserved the 0.5 control baseline.
 * 0.5 AUTO_ENABLED - Toby. Operator rulings 2026-09-22:
 *   AUTO enabled; IR eligibility window +/-15% (was 10%); a 10-magnet observed
 *   sequence overrules a bad declaration ("Operator declaration is the truth, but
 *   a 10 magnet sequence is also the truth").
 *   Also: direction set before a declaration no longer reports TRACKING at MM0.
 * 0.4: telemetry-format safety correction.
 * 0.2: point-landmark semantics; +/-10% per-interval IR eligibility; mapped DNA history;
 * no ungated Hall advance; reversal preserves position/history and resets unsigned IR frame.
 * NAVI knows where Toby was, what comes next on the track, and how far he has moved.
 * Hall normally CONFIRMS the expected landmark; it does not repeatedly relocalize the train.
 * Too-early Hall excursions are non-landmarks. Wrong polarity is a discrepancy, not a crisis.
 * Missed landmarks are skipped when measured travel places Toby at a later mapped magnet.
 * AUTO is enabled in this build. It is a field test, not an accepted build.
 * Based on NAVI_SIMPLIFIED's console/motor glue at ae4b7e6; not its navigator.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <Preferences.h>

// NSR1 is a send-only observation tap.  The critical section protects only
// completed recorder records; it is never held across Wi-Fi or MQTT work.
static portMUX_TYPE naviSyncMux = portMUX_INITIALIZER_UNLOCKED;
#define NAVI_SYNC_ENTER_CRITICAL() portENTER_CRITICAL(&naviSyncMux)
#define NAVI_SYNC_EXIT_CRITICAL()  portEXIT_CRITICAL(&naviSyncMux)

// Operator authorized AUTO for this build, 2026-09-22.
#define NGR_ENABLE_EXPERIMENTAL_AUTO 1
#include "LocoConfig.h"
#include "../../../QUORUM/credentials.h"
#include "RouteMap.h"
#include "Navigator.h"
#include "Ops.h"
#include "Stations.h"
#include "NaviHall.h"
#include "RecoveryControl.h"
#include "IrHealthMonitor.h"
#include "IrSpeedTelemetry.h"
#include "NaviSyncRecorder.h"

using namespace navi_one;

// Published on state/bootid. It is the ONLY thing that tells telemetry which
// build is running, so it advances with every behavioural change.
// No upstream Hall detector exists. The only timing rule with navigation
// authority is NAVI's 650 ms Hall-only fallback (Navigator.h), used only
// without valid MM-referenced IR distance.
#define SKETCH_NAME    "NAVI_EYES_WIDE_OPEN"
#define BUILD_CLASS    "AUTO_ENABLED_FIELD_TEST"
#define BUILD_SUBTITLE "X22R removed; NAVI-owned 70x2 openings and 100-200 mm spatial Hall reference; NSR1 recorder"
#define FIELD_ACCEPTED 0

// Types used in function signatures must appear before the Arduino
// prototype generator's insertion point, which is just after the includes.
// One Hall opening as NAVI judges it (navi_hall::Opening plus bookkeeping).
struct JudgedOpening { navi_hall::Opening o; uint32_t serial, priorGapMs, referenceAgeMs; };
struct IrRx { uint8_t mac[6]; uint64_t receivedUs; uint8_t bytes[110]; };
struct PubMsg { char topic[72]; char payload[1200]; uint16_t len; bool retain; };
struct CmdMsg { char topic[72]; char payload[64]; };

struct HallReading {
  uint16_t raw[5];
  int16_t median;
};

static const char*   MQTT_BROKER = "192.168.68.142";
static const uint16_t MQTT_PORT  = 1883;

static constexpr uint8_t HALL_PIN = 33;      // ADC1_CH5
static constexpr uint8_t I2C_SDA  = 21, I2C_SCL = 22;

// Per-locomotive and per-railway values come from the profile. The repo's rule
// is "per-locomotive values belong in the config headers, not in the sketch",
// and 0.1 broke it in three places at once: cruise PWM, the whole battery
// policy, and the recognizer's measured thresholds. LocoConfig.h now refuses
// to build a profile that has not had the recognizer survey done on it.
static constexpr uint8_t  AUTO_CRUISE_PWM = NAVI_AUTO_CRUISE_PWM;
// Manual throttle is paced PER STEP, as QUORUM did: the feel of the controls
// must not change because the navigator did. 0 -> 90 takes ~13.5 s up.
static constexpr uint16_t MANUAL_STEP_UP_MS   = 150;
// THE BRAKE (decision 0011, and the behaviour the operator actually wants --
// LocoDriver_v2_1_share.ino:318-340, which predates every sketch in this repo).
// It is not a force applied to the motor. It is the DECELERATION STEP RATE:
//     brake   0 -> 400 ms per step   (slow coast-down)
//     brake 255 ->  15 ms per step   (hard brake)
// Acceleration is unaffected. That is why it works on a PWM-only locomotive,
// which is the question 0011 said had to be answered first -- it was answered
// years ago in the original driver.
static constexpr uint16_t BRAKE_STEP_COAST_MS = 400;
static constexpr uint16_t BRAKE_STEP_HARD_MS  = 15;
static uint8_t brakeValue = 0;
static inline uint16_t brakeStepMs(){
  return (uint16_t)(BRAKE_STEP_COAST_MS -
    ((uint32_t)(BRAKE_STEP_COAST_MS - BRAKE_STEP_HARD_MS) * brakeValue) / 255U);
}
// Battery policy lives in the locomotive's profile, not here. 0.1 hard-coded
// LOW_VOLTAGE_V = 14.4 in the sketch while Toby's profile carried a complete,
// older, field-derived policy that was left as dead code -- two policies, one
// of them invisible. The profile's is the one that has been on the railway:
//     SHUTDOWN_VOLTAGE            13.25  trip
//     RECOVERY_VOLTAGE            14.0   clear (a real band, not one threshold)
//     DISCONNECTED_VOLTAGE_THRESHOLD 12.5  below this there is no pack at all
//     VOLTAGE_COUNTER_LIMIT       5      consecutive readings before tripping
//
// 0.1's single 14.4 threshold did two wrong things on 2026-08-29: it warned
// LOW VOLTAGE when the operator had simply disconnected the battery, and trip
// and recovery shared one number, so a pack sagging near it would chatter in
// and out every 5 s poll.

// ---------------------------------------------------------------------------
// LAYER 1 — acquisition (Hall task, core 0): ADC reading into the ring.
// LAYER 2 — NAVI (loop, core 1): openings, spatial reference, judgment.
// ---------------------------------------------------------------------------
static navi_hall::HallSampleRing<1024> hallRing;   // ~1 s of native samples
static navi_hall::OpeningObserver openingObserver;  // NAVI-owned 70x2 observation
static navi_hall::SpatialReference hallReference;   // NAVI-owned Hall reference
static uint32_t hallCursor=0; static bool hallCursorReady=false;
static uint32_t hallSampleLoss=0;                    // samples overwritten before NAVI read them
static uint64_t frameStartUs=0;                      // openings before this belong to an old frame
static uint32_t lastOpeningMs=0;
static bool irPwmZeroSeen=true;                      // motive PWM 0 seen since the last IR point
static ngr_nav::MovementSource movement;
static ngr_nav::IrHealthMonitor irHealth;
static ngr_nav::IrSpeedTelemetry irSpeed;
static ngr_nav::IrSpeedQualification irSpeedQualification;
static ngr_nav::NaviSpeedInterpretation naviSpeedInterpretation;
static bool irCarCoupled=false; // Operator-confirmed, deliberately not persisted.
static navi_sync::Recorder naviSyncRecorder;
static uint32_t naviSyncSession=0;
static WiFiUDP naviSyncUdp;
static IPAddress naviSyncDest;
static bool naviSyncDestValid=false;
static uint32_t naviSyncUdpFailures=0, naviSyncDatagrams=0;
static constexpr const char* NAVI_SYNC_HOST = "192.168.68.142";
static constexpr uint16_t NAVI_SYNC_PORT = 47620;
static volatile uint8_t naviSyncNavMm=navi_sync::MM_NA;
static volatile int8_t naviSyncNavDir=0;
static volatile uint8_t naviSyncStationPhase=0;
static volatile uint8_t naviSyncContextFlags=0;
static QueueHandle_t irQ=nullptr;
static volatile uint32_t irQueueDrops=0;
static uint32_t handledIrDrops=0;
static uint8_t lastSeenMac[6]{};
static Preferences pairing;
static uint32_t seenFrames=0;
static bool radioReady=false;
// Diagnostic context only (NSR1 flags): applied and commanded PWM both 0.
static volatile bool observationHold=true;
static bool hallReady=false;                         // NAVI's boot Hall reference exists
static Navigator navigator;

// STATIONS (decision 0068). Pure machine in Stations.h; this is the only place
// it touches the locomotive. It is serviced on every loop pass, not only on an
// advance, because the dwell clock and the completion of the zero ramp are both
// events with no marker behind them.
static StationMachine stationMachine;

static uint64_t bootId = 0;
static uint32_t nextEventSerial = 0;                  // every opening NAVI receives


#if !defined(NAVI_MAX_OPERATING_PWM)
#error "NAVI_COHERENCE requires a per-locomotive PWM ceiling"
#endif
// Average speed over the last confirmed interval: surveyed distance divided by
// measured time. A measurement, not a model -- nothing in the accept path uses
// it, and no PWM value appears in it. Display only.
static uint32_t lastAdvanceMs = 0; static uint32_t estMmPerS = 0;
// 20Q3: Hall-derived speed is operator/diagnostic telemetry with no NAV or
// control authority. The last actual measurement is kept (never replaced by
// an invented 0) until a new legitimate Hall-derived measurement; its age is
// published so a stale value cannot pass as current. 0 = never measured.
static uint32_t estMeasuredAtMs = 0;

// An opening whose samples predate a declaration or direction change belongs
// to the old frame and is counted, not judged (as 20Q3's navEpoch did).
static uint32_t staleJudged = 0;

static void carryResetRequest(){
  if (!navigator.takeResetRequest()) return;
  frameStartUs=esp_timer_get_time();
  hallReference.frameChanged(); // route from the accepted opening is broken; reference retained
  movement.newFrame();
  irHealth.invalidateReference(); // NAV frame changes do not end instrument odometry.
  // A declaration or a direction change moves the locomotive's idea of where it
  // is. A station armed against the old frame would be measuring its approach
  // from a position that no longer exists, so the machine stands down with it.
  stationMachine.reset();
}

static HallReading hallRead(){
  HallReading out{}; int sorted[5];
  for (int i = 0; i < 5; ++i) {
    out.raw[i] = (uint16_t)analogRead(HALL_PIN);
    sorted[i] = out.raw[i];
  }
  for (int i = 1; i < 5; ++i) {
    int v = sorted[i], j = i - 1;
    while (j >= 0 && sorted[j] > v) { sorted[j+1] = sorted[j]; --j; }
    sorted[j+1] = v;
  }
  out.median = (int16_t)sorted[2];
  return out;
}


// ---------------------------------------------------------------------------
// LAYER 4 — operations. Motor, ramps, safety.
// ---------------------------------------------------------------------------
static volatile int commandedPwm = 0, actualPwm = 0;
static uint8_t motorDirection = 1;              // 1 = FWD, 0 = REV
static bool autoEnrolled=false, autoRunning=false, estopped=false, lowVoltage=false;
// An e-stop that is merely queued can be dropped -- cmdQ is 16 deep and
// xQueueSend was called with zero timeout. This one cannot be: onMqtt raises
// it the instant the packet is parsed, and serviceRamp() reads it directly, so
// the motor stops whether or not the command ever reaches loop().
static volatile bool estopAsserted = false;
static int8_t sessionDir = 0;

static navi_sync::Context naviSyncContext(){
  navi_sync::Context c;
  c.navMm=naviSyncNavMm; c.navDir=naviSyncNavDir;
  c.stationPhase=naviSyncStationPhase; c.flags=naviSyncContextFlags;
  return c;
}

static void naviSyncRefreshContext(){
  const NavStatus& s=navigator.status();
  uint8_t flags=0;
  if(navigator.positionKnown()) flags|=navi_sync::CTX_NAV_KNOWN;
  if(autoEnrolled) flags|=navi_sync::CTX_AUTO_ENROLLED;
  if(autoRunning) flags|=navi_sync::CTX_AUTO_RUNNING;
  if(estopped || estopAsserted) flags|=navi_sync::CTX_ESTOP;
  if(lowVoltage) flags|=navi_sync::CTX_LOW_VOLT;
  if(observationHold) flags|=navi_sync::CTX_HOLD;
  naviSyncNavMm=navigator.positionKnown()?s.navMm:navi_sync::MM_NA;
  naviSyncNavDir=navigator.positionKnown()?s.navDir:0;
  naviSyncStationPhase=(uint8_t)stationMachine.phase();
  naviSyncContextFlags=flags;
}
// Per-STEP ramping, as LocoDriver_v2_1 did. The first cut computed a total
// duration when the throttle command arrived and then interpolated, so moving
// the brake DURING a deceleration changed nothing -- which is the only way a
// brake is ever used. serviceRamp() now asks brakeStepMs() at every step.
//
// AUTO keeps FIXED step rates and is deliberately NOT brake-governed: with the
// brake at rest (400 ms/step) an automatic stop from PWM 90 would take 36
// seconds, and the one-strike stop must not be slow. E-stop still bypasses
// ramping entirely.
static constexpr uint16_t AUTO_STEP_UP_MS   = 62;   // ~5.6 s to cruise 90
static constexpr uint16_t AUTO_STEP_DOWN_MS = 31;   // ~2.8 s to stop from 90
static int rampTarget = 0;
static uint16_t stepUpMs = MANUAL_STEP_UP_MS, stepDownMs = 0;  // 0 = use the brake
static unsigned long lastStepMs = 0;
static Adafruit_INA219 ina219; static bool inaReady=false;
static float busV=0, busA=0, busW=0;

static void writePwm(int v){
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(MOTOR_PWM_PIN, v);
#else
  ledcWrite(PWM_CHANNEL, v);
#endif
}
// down=0 means "governed by the brake slider" (manual). A non-zero down is a
// fixed rate the operator cannot slow down (auto).
static void requestPwm(int target, uint16_t up, uint16_t down,
                       StopCause stopCause = StopCause::Controlled,
                       bool manualRequest = false){
  // The experimental profile constrains AUTO, not the operator's throttle.
  target = constrain(target,0,255);
  if (!manualRequest && target > NAVI_MAX_OPERATING_PWM) {
    Serial.printf("[PWM] requested %d capped at %u for reachability contract\n",
                  target,(unsigned)NAVI_MAX_OPERATING_PWM);
    target = NAVI_MAX_OPERATING_PWM;
  }
  rampTarget = target; commandedPwm = target;
  stepUpMs = up; stepDownMs = down;
}
static void serviceRamp(){
  digitalWrite(MOTOR_DIR_PIN, motorDirection ? HIGH : LOW);
  // E-STOP IS INSTANT. LOW VOLTAGE IS NOT -- it ramps, steeply.
  //
  // 0.1 gave low voltage the e-stop's instant cut, in MANUAL as well, so one
  // glitched INA reading hard-stopped a hand-driven locomotive. The operator
  // ruled on 2026-08-29 that the fast stop is a MESSAGE: "One strike/low
  // battery. Steep ramp is informative. It signals that something is wrong."
  // A steep ramp, not a dead short. serviceIna() requests AUTO_STEP_DOWN_MS.
  if (estopped || estopAsserted) {
      actualPwm = 0; rampTarget = 0; writePwm(0); return;
  }
  if (lowVoltage && rampTarget != 0) {
    rampTarget = 0; stepDownMs = AUTO_STEP_DOWN_MS;
  }
  if (actualPwm == rampTarget) return;
  const bool down = rampTarget < actualPwm;
  const uint16_t need = down ? (stepDownMs ? stepDownMs : brakeStepMs())
                             : (stepUpMs ? stepUpMs : 1);
  unsigned long now = millis();
  if (now - lastStepMs < need) return;
  lastStepMs = now;
  actualPwm += down ? -1 : +1;
  writePwm(actualPwm);
}

// ---------------------------------------------------------------------------
// TRANSPORT — publishes only enqueue. The network task owns the radio.
// ---------------------------------------------------------------------------
static QueueHandle_t pubQ = nullptr, cmdQ = nullptr;
static WiFiClient wifiClient; static PubSubClient mqtt(wifiClient);
// NOTHING IS LOST IN SILENCE.
//
// 0.1's network task dequeued every message and then published it only if the
// broker happened to be connected -- so a broker blink threw away marker
// events, DISAGREE verdicts and the WRONG MAGNET warning itself, with no
// record that anything had gone. In a project whose founding transport story
// is "67 marker events destroyed", that was the same loss, deliberate.
//
// Now the queue HOLDS while the broker is away, and every message that is
// genuinely lost is counted and published in the status line.
static uint32_t pubDropped = 0, cmdDropped = 0;
enum { T_ONLINE=0,T_NAV,T_MARKER,T_ALERT,T_IR,T_STAT,T_BOOT,T_WARN,
       T_ST_AUTO,T_ST_ESTOP,T_ST_THR,T_ST_DIR,T_ST_SESSDIR,T_ST_STARTMM,
       T_ST_NAVREADY,T_ST_LOWV,T_ST_STARTINT,T_BRAKE,T_V,T_A,T_W,T_SPEED,
       T_STATION,T_HALL_REF,T_DISCREPANCY,T_IR_LINK,T_IR_COMPARE,T_HYPOTHESIS,T_IR_HEALTH,T_RECOVERY,T_SANS_MM,T_CNT };
// Size follows the enum sentinel so adding a topic cannot silently create an
// out-of-bounds row (X16 audit B1).
static char T[T_CNT][72];

static void topic(int i,const char* suffix){ snprintf(T[i],72,"ngr/loco/%s/%s",LOCO_NAME,suffix); }
static void buildTopics(){
  topic(T_ONLINE,"online");            topic(T_NAV,"state/nav");
  topic(T_MARKER,"mm/marker");         topic(T_ALERT,"alert");
  topic(T_IR_HEALTH,"diag/ir_health");
  topic(T_RECOVERY,"diag/recovery");
  topic(T_SANS_MM,"nav/sans_mm");         // 20Q3: POSITION_ADVANCED_SANS_MM detail
  topic(T_IR,"telem/ir");              topic(T_STAT,"state/loopstat");
  topic(T_BOOT,"state/bootid");        topic(T_WARN,"state/warning");
  topic(T_ST_AUTO,"state/auto");       topic(T_ST_ESTOP,"state/estop");
  topic(T_ST_THR,"state/throttle");    topic(T_ST_DIR,"state/direction");
  topic(T_ST_SESSDIR,"state/session_direction");
  topic(T_ST_STARTMM,"state/start_mm");topic(T_ST_NAVREADY,"state/nav_ready");
  topic(T_ST_LOWV,"state/lowvolt");    topic(T_ST_STARTINT,"state/start_interval");
  topic(T_BRAKE,"state/brake");
  topic(T_V,"telem/voltage");
  topic(T_A,"telem/current");          topic(T_W,"telem/power");
  topic(T_SPEED,"telem/speed");
  // Not retained: a one-shot diagnostic burst, meaningful only right after
  // the strike/contradiction that triggered it, per decisions on waveform
  // capture (2026-08-31). A retained copy would misdescribe every later
  // boot as if it had just been struck.
  topic(T_STATION,"state/station");
  // EYES_WIDE_OPEN: X22R's diag/hall_decision, nav/hall_window and the never
  // published nav/suppression are gone with X22R. NAVI's own reference record:
  topic(T_HALL_REF,"nav/hall_reference");
  topic(T_DISCREPANCY,"nav/discrepancy");
  topic(T_IR_LINK,"diag/ir_link");topic(T_IR_COMPARE,"nav/ir_compare");topic(T_HYPOTHESIS,"nav/hypotheses");
}
static bool pub(int t,const char* payload,bool retain=false){
  if(!pubQ) return false;
  PubMsg m; strlcpy(m.topic,T[t],sizeof(m.topic));
  if (strlen(payload)>=sizeof(m.payload)) { ++pubDropped; return false; }
  strlcpy(m.payload,payload,sizeof(m.payload)); m.len=0; m.retain=retain;
  if (xQueueSend(pubQ,&m,0) != pdTRUE) { ++pubDropped; return false; }
  return true;
}
// A warning the operator has been told to go and read must survive the next
// thing that happens. 0.1 published warn("") on GO and on clearing e-stop,
// which overwrote the retained WRONG MAGNET text -- the one field the operator
// had just been pointed at. A sticky warning is cleared only by declaring
// position, which is the act that answers it.
static bool warnSticky = false;
static void warn(const char* text){ pub(T_WARN,text,true); Serial.printf("[WARN] %s\n",text); }
static void warnStick(const char* text){ warn(text); warnSticky = true; }
static void warnClear(){ if (warnSticky) return; pub(T_WARN,"",true); }

// AUTO withdrawal is a controlled stop, never an automatic restart.
static void withdraw(const char* text){
  warnStick(text);
  autoRunning = false;
  if (autoEnrolled) { autoEnrolled = false; pub(T_ST_AUTO,"0",true); }
  requestPwm(0,0,AUTO_STEP_DOWN_MS,StopCause::Safety);
  pub(T_ST_NAVREADY,"0",true);
  lastAdvanceMs = 0;              // the next speed estimate must not span this
}

static void publishNav(const char* event,const JudgedOpening* j,Ruling r){
  const NavStatus& s = navigator.status();
  char b[700];
  if (j) {
    const int n=snprintf(b,sizeof(b),
      "{\"event\":\"%s\",\"boot_id\":\"%016llX\",\"event_serial\":%lu,"
      "\"state\":\"%s\",\"nav\":\"%s\",\"nav_state\":\"%s\",\"mm\":%u,\"tgt\":%u,\"dir\":\"%s\","
      "\"ruling\":\"%s\",\"evidence\":\"%s\",\"obs\":\"%c\",\"expected\":\"%c\","
      "\"opened_ms\":%lu,\"gap_ms\":%lu,\"raw_open\":%d,"
      "\"base_open\":%d,\"pwm_open\":%u,\"unresolved_count\":%u,"
      "\"seq_len\":%u,\"seq_matches\":%u,\"trust\":\"%s\",\"corrections\":%lu}",
      event,(unsigned long long)bootId,(unsigned long)j->serial,
      consoleNavName(s.state),consoleNavName(s.state),navStateName(s.state),s.navMm,s.target,
      s.navDir>0?"CW":(s.navDir<0?"CCW":"UNSET"),rulingName(r),evidenceClassName(s.evidence),
      poleChar(j->o.polarity),poleChar(polarityAt(s.target)),
      (unsigned long)j->o.tMs,(unsigned long)j->priorGapMs,
      (int)j->o.raw,(int)j->o.reference,(unsigned)j->o.pwm,
      (unsigned)s.unresolvedCount,(unsigned)s.sequenceLength,(unsigned)s.sequenceMatches,
      trustName(s.trust),(unsigned long)s.corrections);
    if (n<0 || n>=(int)sizeof(b)) { ++pubDropped; return; }
    pub(T_MARKER,b,false);
  } else {
    const int n=snprintf(b,sizeof(b),
      "{\"event\":\"%s\",\"state\":\"%s\",\"nav\":\"%s\",\"nav_state\":\"%s\",\"mm\":%u,"
      "\"tgt\":%u,\"dir\":\"%s\",\"adv\":%lu,\"ref\":%lu}",
      event,consoleNavName(s.state),consoleNavName(s.state),navStateName(s.state),s.navMm,s.target,
      s.navDir>0?"CW":(s.navDir<0?"CCW":"UNSET"),
      (unsigned long)s.advances,(unsigned long)s.refusals);
    if (n<0 || n>=(int)sizeof(b)) { ++pubDropped; return; }
  }
  pub(T_NAV,b,true);
  Serial.printf("[NAV] %s\n",b);
}

// A physically feasible proximal alternative uniquely improved the history.
// Recovery is bounded by ProximalRecovery, not a whole-route pattern search.
// Existing station logic follows the working model and recomputes its offset
// on the next tick; its overshoot/timeout withdrawal remains unchanged.
static void applySequenceCorrection(const SequenceCorrection& f){
  char w[140];
  snprintf(w,sizeof(w),"POSITION CORRECTED by proximal evidence: MM%03u -> MM%03u (offset %+d, %u/%u agreed before)",
           f.fromMm,f.toMm,(int)f.offset,(unsigned)f.matchesBefore,(unsigned)navigator.status().sequenceLength);
  warnStick(w);
  // No correction-only station trigger. stationService evaluates the current MM.
  // Keep the legacy armed_after diagnostic field; no separate arming occurs here.
  char b[240];
  snprintf(b,sizeof(b),"{\"event\":\"SEQUENCE_CORRECTED\",\"from_mm\":%u,\"to_mm\":%u,\"offset\":%d,"
           "\"matches_before\":%u,\"auto_running\":%u,\"station_phase\":\"%s\",\"armed_after\":%u}",
           f.fromMm,f.toMm,(int)f.offset,(unsigned)f.matchesBefore,autoRunning?1:0,
           stPhaseName(stationMachine.phase()),0u);
  pub(T_NAV,b,true);
  Serial.printf("[NAV] %s\n",b);
  lastAdvanceMs=0;                 // no speed estimate spans a correction
}

static void publishDecision(const JudgedOpening& j,const NavStatus& before,Ruling result,
                            const ngr_nav::MotionPoint& point){
  char b[900];
  int n=snprintf(b,sizeof(b),
    "{\"event_serial\":%lu,\"captured_us\":%llu,\"opened_ms\":%lu,"
    "\"opening\":\"%c\",\"window\":null,\"window_valid\":null,\"raw\":%d,"
    "\"baseline\":%d,\"base_age_ms\":%lu,\"peak_signed\":null,\"pre_mm\":%u,"
    "\"departure\":%d,\"pwm\":%u,\"expected\":\"%c\",\"judgment_reason\":\"%s\","
    "\"ruling\":\"%s\",\"evidence\":\"%s\",\"state\":\"%s\",\"ambiguity_count\":%u,"
    "\"ir_reason\":\"%s\",\"ir_interval\":\"%s\",\"distance_assessable\":%u,\"ir_boot\":\"%016llX\",\"ir_seq\":%lu,"
    "\"ir_alignment_us\":%lld,\"distance_bounds\":\"UNVALIDATED\","
    "\"action\":\"%s\"}",
    (unsigned long)j.serial,(unsigned long long)j.o.tUs,(unsigned long)j.o.tMs,
    poleChar(j.o.polarity),(int)j.o.raw,
    (int)j.o.reference,(unsigned long)j.referenceAgeMs,(unsigned)before.navMm,
    (int)j.o.departure,(unsigned)j.o.pwm,poleChar(polarityAt(before.target)),
    judgmentReasonName(navigator.status().reason),
    rulingName(result),evidenceClassName(navigator.status().evidence),navStateName(navigator.status().state),navigator.status().unresolvedCount,
    ngr_nav::issueName(point.issue),distanceBasisName(navigator.status().distance),
    navigator.status().distanceAssessable?1:0,(unsigned long long)point.wire.bootId,
    (unsigned long)point.wire.sequence,(long long)point.alignmentUs,
    navigator.unresolved()?"LOCATION_UNRESOLVED":navigator.positionKnown()?"EXPECT_CONFIRM_ADVANCE":"POSITION_WITHDRAWN");
  if(n>0 && n<(int)sizeof(b))pub(T_DISCREPANCY,b);
  else ++pubDropped;
  const auto& h=navigator.hypotheses();
  for(uint8_t i=0;i<h.count();++i){
    const auto& x=h.hypothesis(i);
    snprintf(b,sizeof(b),
      "{\"event_serial\":%lu,\"branch\":%u,\"branches\":%u,\"mm\":%u,"
      "\"anchor_ms\":%lu,\"faults\":%u,\"causes\":%u,\"clean\":%u,"
      "\"distance_consistent\":%u,\"ir_interval\":\"%s\",\"authority\":\"ONE_COHERENT_POSITION\"}",
      (unsigned long)j.serial,i,h.count(),x.mm,(unsigned long)x.lastRealMs,x.faults,x.causes,x.clean,x.distanceAgrees?1:0,distanceBasisName(x.distance));
    pub(T_HYPOTHESIS,b);
  }
}

static void compareMovement(const JudgedOpening& j,const ngr_nav::IrOdometryPoint& point){
  // 20Q3: distance is the same-epoch travel from NAVI's last MM/IR
  // synchronization -- the quantity the +/-15% window uses. No legacy
  // TRACKING-at-both-endpoints interval and no 500 ms guard remain here.
  const auto& h=navigator.hypotheses();
  if(!h.count())return;
  const int8_t dir=h.direction();
  double travel=0;const bool have=navigator.distanceAt(point,travel);
  const unsigned anchor=navigator.haveReference()?navigator.referenceMm():h.hypothesis(0).mm;
  const uint32_t elapsed=j.o.tMs-h.hypothesis(0).lastRealMs;
  unsigned mm=anchor;unsigned distance=0;
  for(unsigned step=0;step<=10;++step){
    const bool tooSoon=NGR_PHYSICAL_VMAX_MM_S &&
      uint64_t(elapsed)*NGR_PHYSICAL_VMAX_MM_S<uint64_t(distance)*1000;
    char pulses[32]="null",nominal[40]="null",residual[40]="null";
    if(have){
      snprintf(pulses,sizeof(pulses),"%llu",(unsigned long long)(travel*1000.0/point.pitchUm+0.5));
      snprintf(nominal,sizeof(nominal),"%.3f",travel);
      snprintf(residual,sizeof(residual),"%.3f",travel-distance);
    }
    char b[650];
    snprintf(b,sizeof(b),
      "{\"event_serial\":%lu,\"branch\":0,\"anchor_mm\":%u,\"candidate_mm\":%u,"
      "\"steps\":%u,\"mapped_mm\":%u,\"ir_pulses\":%s,\"ir_nominal_mm\":%s,"
      "\"residual_mm\":%s,\"ir_quality\":\"%s\",\"ir_hard_verdict\":\"CANNOT_ASSESS\","
      "\"timing_ms\":%lu,\"physical_timing\":\"%s\","
      "\"bounds\":\"UNVALIDATED\",\"opening_matches\":%u}",
      (unsigned long)j.serial,anchor,mm,step,distance,pulses,nominal,residual,
      have?"MM_REFERENCED_EPOCH":navigator.haveReference()?"EPOCH_BREAK_OR_NO_IR_POINT":"NO_MM_REFERENCE",
      (unsigned long)elapsed,
      NGR_PHYSICAL_VMAX_MM_S?(tooSoon?"EXCLUDED":"POSSIBLE"):"UNCONFIGURED",
      j.o.polarity==polarityAt(mm)?1:0);
    pub(T_IR_COMPARE,b);
    distance+=spanMm(mm,dir);mm=nextMarker(mm,dir);
  }
}

// TRAVEL DIRECTION IS NOT SESSION DIRECTION.
// sessionDir is which way round the route the locomotive runs WHEN DRIVEN
// FORWARD. Reverse the motor and it travels the other way, so navDir must be
// derived from both. NAVI_ONE 0.1 used sessionDir alone, and on 2026-08-29 the
// operator started AUTO with the motor in REVERSE: it drove backwards while
// the navigator advanced as though going forward, and position was wrong from
// the first magnet.
static int8_t travelDir(){
  if (sessionDir == 0) return 0;
  return motorDirection ? sessionDir : (int8_t)(-sessionDir);
}
static void applyTravelDirection(){
  int8_t d = travelDir();
  if (d != 0 && d != navigator.status().navDir) {
    navigator.setDirection(d,millis()); // preserve interval/history; new unsigned-IR frame
    carryResetRequest();
    publishNav("DIRECTION",nullptr,Ruling::NoPosition);
  }
}

static void declarePosition(uint8_t mm,int8_t dir,const char* interval){
  navigator.declare(mm,dir,millis());
  carryResetRequest();
  warnSticky = false; pub(T_WARN,"",true);   // the declaration answers the strike
  lastAdvanceMs = 0;                         // no speed estimate spans a declaration
  pub(T_SPEED,"0",true);
  char v[16]; snprintf(v,sizeof(v),"%u",mm); pub(T_ST_STARTMM,v,true);
  // The echo exists so the console's badge can read CONFIRMED. It is
  // LABELLING, NOT A GATE, and it must never become one.
  //
  // An earlier console DID gate AUTO on this echo, and on 2026-08-13 it stopped
  // Toby being enlisted at all -- a locomotive that never echoed could not be
  // put into AUTO however many times its position was declared. The operator's
  // ruling deleted it: "I don't need to wait for confirmation. Besides, it
  // prevented me from enlisting Toby in auto mode." The console now asks
  // whether the OPERATOR supplied orientation and location. Withholding
  // cmd/auto is sequencing, not authority.
  if (interval && *interval) pub(T_ST_STARTINT,interval,true);
  else { char iv[12];
         snprintf(iv,sizeof(iv),"%03u-%03u", dir>0?mm:(unsigned)nextMarker(mm,-1),
                                             dir>0?(unsigned)nextMarker(mm,+1):mm);
         pub(T_ST_STARTINT,iv,true); }
  pub(T_ST_NAVREADY,"1",true);
  publishNav("DECLARED",nullptr,Ruling::NoPosition);
}

// ---------------------------------------------------------------------------
// TASKS
// ---------------------------------------------------------------------------
// ACQUISITION ONLY. One reading per millisecond: the median of five ADC
// conversions taken within ~100 us (decision 0073: rejects a single wrong
// conversion inside one reading; it spans no second sample). The reading, its
// esp_timer timestamp and the applied PWM go into hallRing for NAVI and, as
// before, into the NSR1 recorder. No reference, threshold, window, gate or
// interpretation exists on this task.
static void hallTask(void*){
  TickType_t wake=xTaskGetTickCount();
  for(;;){
    const uint64_t tUs=esp_timer_get_time();
    const uint32_t now=(uint32_t)(tUs/1000);
    const HallReading reading=hallRead();
    navi_hall::HallSample sample;
    sample.tUs=tUs;sample.raw=reading.median;sample.pwm=(uint8_t)actualPwm;
    hallRing.push(sample);
    navi_sync::HallSample evidence{};
    memcpy(evidence.raw,reading.raw,sizeof(evidence.raw));
    evidence.median=reading.median;
    evidence.pwmActual=(uint8_t)actualPwm;
    evidence.pwmCommanded=(uint8_t)commandedPwm;
    evidence.flags=(motorDirection ? navi_sync::HALL_F_DIR_FWD : 0) |
                    (estopped || estopAsserted ? navi_sync::HALL_F_ESTOP : 0) |
                    (autoRunning ? navi_sync::HALL_F_AUTO : 0) |
                    ((naviSyncContextFlags & navi_sync::CTX_NAV_KNOWN) ? navi_sync::HALL_F_NAV_KNOWN : 0) |
                    (observationHold ? navi_sync::HALL_F_HOLD : 0) |
                    (lowVoltage ? navi_sync::HALL_F_LOW_VOLT : 0);
    naviSyncRecorder.addHall(tUs,now,evidence,naviSyncContext());
    vTaskDelayUntil(&wake,1);
  }
}

static void onIr(const esp_now_recv_info_t* info,const uint8_t* bytes,int length){
  if(!info || !bytes || length!=110 || bytes[0]!=0x52 || bytes[1]!=0x49 || bytes[3]!=5)return;
  IrRx packet;memcpy(packet.mac,info->src_addr,6);packet.receivedUs=esp_timer_get_time();
  memcpy(packet.bytes,bytes,110);
  if(irQ && xQueueSend(irQ,&packet,0)!=pdTRUE)irQueueDrops=irQueueDrops+1;
}
static void publishHallReference(const char* event);
// Every newly accepted IR odometry point, in order: NAVI's route-displacement
// account (pulses at motive PWM 0 excluded, decision 0106), then the spatial
// Hall-reference collection, which walks the native samples in that span.
static void onIrPoint(){
  static ngr_nav::IrOdometryPoint last{};
  const auto p=irHealth.odometry().point();
  if(!p.owner || (p.owner==last.owner && p.epoch==last.epoch && p.capturedUs==last.capturedUs))return;
  last=p;
  navigator.observeIr(p,irPwmZeroSeen);
  irPwmZeroSeen=(actualPwm==0);
  const auto ev=hallReference.irPoint(p,uint64_t(int64_t(p.capturedUs)+irHealth.clockOffsetUs()),
                                     navigator.route(),hallRing,millis());
  if(ev==navi_hall::RefEvent::Closed)publishHallReference("REFERENCE_SET");
  else if(ev==navi_hall::RefEvent::CollectionOpened)publishHallReference("COLLECTION_OPENED");
  else if(ev==navi_hall::RefEvent::Ended)publishHallReference("COLLECTION_ENDED");
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
    if(ngr_nav::movementCrc(packet.bytes,108)==uint16_t(packet.bytes[108]|uint16_t(packet.bytes[109])<<8)){
      memcpy(lastSeenMac,packet.mac,6);++seenFrames;
    }
    const ngr_nav::RxResult movementResult =
      movement.receive(packet.mac,packet.bytes,110,packet.receivedUs);
    irHealth.receive(packet.mac,packet.bytes,110,packet.receivedUs);
    onIrPoint();
    if (movementResult == ngr_nav::RxResult::Accepted ||
        movementResult == ngr_nav::RxResult::Reset) {
      // This is the actual accepted ~100 ms IR stream, not serviceIr()'s 1 Hz
      // dashboard/status publication. The snapshot remains byte-for-byte
      // available inside wire; recorder output cannot influence either
      // movement or health admission.
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

static void onMqtt(char* t,byte* payload,unsigned int len){
  CmdMsg c; strlcpy(c.topic,t,sizeof(c.topic));
  unsigned n = len < sizeof(c.payload)-1 ? len : sizeof(c.payload)-1;
  memcpy(c.payload,payload,n); c.payload[n]=0;
  // The one command that must never be dropped is acted on before it is
  // queued. Ops.h rule 2: anything unreadable on this topic means STOP.
  const char* leaf = strrchr(c.topic,'/'); leaf = leaf ? leaf+1 : c.topic;
  if (!strcmp(leaf,"estop")) { bool want=true; parseEstop(c.payload,want); if (want) estopAsserted = true; }
  if (cmdQ && xQueueSend(cmdQ,&c,0) != pdTRUE) ++cmdDropped;
}

static bool naviSyncSendBytes(const void* data,size_t length){
  if(!naviSyncUdp.beginPacket(naviSyncDest,NAVI_SYNC_PORT) ||
     naviSyncUdp.write(reinterpret_cast<const uint8_t*>(data),length) != length ||
     !naviSyncUdp.endPacket()) {
    ++naviSyncUdpFailures;
    return false;
  }
  ++naviSyncDatagrams;
  return true;
}
static bool naviSyncSend(const navi_sync::HallWire& wire){
  return naviSyncSendBytes(&wire,sizeof(wire));
}
static bool naviSyncSend(const navi_sync::IrWire& wire){
  return naviSyncSendBytes(&wire,sizeof(wire));
}
static bool naviSyncSend(const navi_sync::StatusWire& wire){
  return naviSyncSendBytes(&wire,sizeof(wire));
}

static void naviSyncNetworkDrain(){
  if(!naviSyncDestValid || WiFi.status()!=WL_CONNECTED) return;

  // Hall is the high-rate stream; IR is interleaved deliberately so a busy
  // Hall backlog cannot hide the accepted ~100 ms IR observations.
  for(uint8_t i=0;i<12;++i){
    bool did=false;
    if((i&1)==0){
      navi_sync::HallWire hallWire;
      if(naviSyncRecorder.popHall(hallWire)){naviSyncSend(hallWire);did=true;}
      if(!did){navi_sync::IrWire irWire;if(naviSyncRecorder.popIr(irWire)){naviSyncSend(irWire);did=true;}}
    } else {
      navi_sync::IrWire irWire;
      if(naviSyncRecorder.popIr(irWire)){naviSyncSend(irWire);did=true;}
      if(!did){navi_sync::HallWire hallWire;if(naviSyncRecorder.popHall(hallWire)){naviSyncSend(hallWire);did=true;}}
    }
    if(!did) break;
  }

  static uint32_t nextStatus=0;
  const uint32_t nowMs=millis();
  if((int32_t)(nowMs-nextStatus)>=0){
    nextStatus=nowMs+1000;
    navi_sync::Status status{};
    status.tMs=nowMs; status.tUs=esp_timer_get_time();
    status.hallSamples=naviSyncRecorder.hallSamples();
    status.hallRingDrops=naviSyncRecorder.hallDrops();
    status.irAccepted=naviSyncRecorder.irAccepted();
    status.irRingDrops=naviSyncRecorder.irDrops();
    status.irInputQueueDrops=irQueueDrops;
    status.udpFailures=naviSyncUdpFailures;
    status.datagramsSent=naviSyncDatagrams;
    status.maxHallGapUs=naviSyncRecorder.maxHallGapUs();
    status.hallHighWater=naviSyncRecorder.hallHighWater();
    status.irHighWater=naviSyncRecorder.irHighWater();
    status.wifiConnected=WiFi.status()==WL_CONNECTED?1:0;
    status.mqttConnected=mqtt.connected()?1:0;
    const navi_sync::StatusWire wire=naviSyncRecorder.makeStatus(
      nowMs,status.tUs,naviSyncContext(),status);
    naviSyncSend(wire);
  }
}

static void networkTask(void*){
  char sub[72];
  uint32_t nextConnectMs = 0, wifiSinceMs = 0;
  for(;;){
    const uint32_t now = millis();
    // The core's auto-reconnect is undocumented for 3.3.11 and was never
    // verified here. Re-associate explicitly after 15 s down.
    if (WiFi.status()!=WL_CONNECTED) {
      if (!wifiSinceMs) wifiSinceMs = now;
      else if (now - wifiSinceMs > 15000) {
        WiFi.disconnect(); WiFi.begin(WIFI_SSID,WIFI_PASS); wifiSinceMs = now;
        Serial.println("[NET] WiFi re-associating");
      }
    } else wifiSinceMs = 0;

    if (WiFi.status()==WL_CONNECTED && !mqtt.connected() && now >= nextConnectMs) {
      // setTimeout() is NOT the connect timeout on this core -- it is the
      // stream read timeout, in seconds. setConnectionTimeout() is the one
      // that stops a dead broker blocking this task for the full TCP wait,
      // during which the publish queue fills and the loss becomes real.
      wifiClient.setConnectionTimeout(3000);
      nextConnectMs = now + 2000;              // and do not hammer it
      char id[48]; snprintf(id,sizeof(id),"NAVI_COHERENCE_%s",LOCO_NAME);
      if (mqtt.connect(id,T[T_ONLINE],0,true,"0")) {
        mqtt.publish(T[T_ONLINE],"1",true);
        // NO INERT RETAINED ZERO HERE. Decision 0011 rejects it by name: the
        // old firmware published state/brake "0" so the console's parsing
        // would not break -- "compatibility maintained with a deleted
        // capability, the exact silent drift this decision log exists to
        // prevent", and "a labelled lie is still a lie". The console defaults
        // its own tile to 0; this locomotive will not assert a brake state it
        // does not have.
        const char* cmds[]={"cmd/auto","cmd/estop","cmd/throttle","cmd/direction",
                            "cmd/session_direction","cmd/start_mm","cmd/start_interval",
                            "cmd/dispatcher_release","cmd/brake","cmd/ir_pair","cmd/ir_coupled"};
        for (auto c: cmds){ snprintf(sub,72,"ngr/loco/%s/%s",LOCO_NAME,c); mqtt.subscribe(sub); }
        snprintf(sub,72,"ngr/dispatcher/cmd/go/%s",LOCO_NAME);   mqtt.subscribe(sub);
        snprintf(sub,72,"ngr/dispatcher/cmd/stop/%s",LOCO_NAME); mqtt.subscribe(sub);
        mqtt.subscribe("ngr/dispatcher/cmd/estop");
        Serial.println("[NET] MQTT connected");
      }
    }
    mqtt.loop();
    naviSyncNetworkDrain();
    // ONLY DEQUEUE WHAT CAN ACTUALLY BE SENT. While the broker is away the
    // queue holds, so a blink of a few seconds costs nothing at all; a longer
    // outage overflows at the enqueue end, where pub() counts every loss.
    if (mqtt.connected()) {
      PubMsg m;
      while (pubQ && xQueueReceive(pubQ,&m,0)==pdTRUE) {
        // Explicit length always -- text messages carry len=0 (use
        // strlen), the waveform dump carries its true byte count, which may
        // include zero bytes that mqtt.publish(topic,cstring,retain) would
        // have truncated at.
        size_t n = m.len ? m.len : strlen(m.payload);
        if (!mqtt.publish(m.topic,(const uint8_t*)m.payload,n,m.retain)) ++pubDropped;
      }
    }
    vTaskDelay(10);
  }
}

// ---------------------------------------------------------------------------
// A snapshot of everything the policy is allowed to know. Ops.h decides; this
// function is the only place that reads the live flags.
static Ops opsNow(){
  Ops o;
  o.positionKnown = navigator.positionKnown();
  o.enrolled      = autoEnrolled;   o.running      = autoRunning;
  o.estopped      = estopped;       o.lowVoltage   = lowVoltage;
  o.forward       = motorDirection; o.actualPwm    = actualPwm;
  o.commandedPwm  = commandedPwm;   o.sessionDir   = sessionDir;
  o.safeDirPwm    = SAFE_DIRECTION_CHANGE_PWM;
  return o;
}

// Every refusal SAYS SO. 0.1 dropped throttle commands on the floor while
// enlisted with no message at all, so after a strike the operator could
// neither restart AUTO nor drive by hand and nothing on the console explained
// which. "Refusing to run is my least favorite form of messages from a
// locomotive" -- then the least it can do is name the reason.
static void refuse(Refusal r){ warn(r); }

static void handleCommand(const CmdMsg& c){
  const char* leaf = strrchr(c.topic,'/'); leaf = leaf ? leaf+1 : c.topic;
  const bool dispatcher = strstr(c.topic,"/dispatcher/") != nullptr;
  const Ops o = opsNow();

  if(!strcmp(leaf,"ir_coupled")) {
    if(strcmp(c.payload,"0") && strcmp(c.payload,"1")){warn("IR COUPLED: expected 0 or 1");return;}
    irCarCoupled=!strcmp(c.payload,"1");
    return; // Display qualification only; no motor or navigation changes.
  }

  if(!strcmp(leaf,"ir_pair")){
    if(Refusal r=admitDeclaration(o)){refuse(r);return;}
    unsigned a[6];char extra;
    if(sscanf(c.payload,"%2x:%2x:%2x:%2x:%2x:%2x%c",&a[0],&a[1],&a[2],&a[3],&a[4],&a[5],&extra)!=6){warn("IR PAIR: expected six-byte MAC");return;}
    uint8_t mac[6];for(unsigned i=0;i<6;++i)mac[i]=a[i];
    if(!ngr_nav::validMac(mac)){warn("IR PAIR: invalid unicast MAC");return;}
    movement.pair(mac);pairing.putBytes("ir_mac",mac,6);
    irHealth.pair(mac,esp_timer_get_time());
    irCarCoupled=false;
    warn("IR source paired; new movement frame, no position change");return;
  }

  if (!strcmp(leaf,"estop")) {
    // RULE 2 in Ops.h: on an emergency topic, anything unreadable STOPS.
    bool want = true;
    const bool understood = parseEstop(c.payload, want);
    estopped = want;
    estopAsserted = want;
    if (estopped) {
      autoRunning = false; requestPwm(0,0,1,StopCause::Safety);
      warn(understood ? "ESTOP" : "ESTOP: unreadable payload, assumed STOP");
    } else {
      warnClear();
    }
    char v[4]; snprintf(v,sizeof(v),"%u",estopped?1:0); pub(T_ST_ESTOP,v,true);

  } else if (!strcmp(leaf,"session_direction")) {
    int8_t d;
    if (!parseSessionDir(c.payload,d)) { warn("SESSION_DIRECTION REFUSED: expected CW or CCW"); return; }
    if (Refusal r = admitDeclaration(o)) { refuse(r); return; }
    sessionDir = d; pub(T_ST_SESSDIR,d>0?"CW":"CCW",true);
    applyTravelDirection();

  } else if (!strcmp(leaf,"start_interval")) {
    // "AAA-BBB" -- the two magnets the locomotive stands between, GEOMETRIC and
    // always ascending, because that is what the console's slider produces and
    // what the operator can see on the ground. It is NOT a travel-order pair.
    // Which end it is leaving depends on which way it faces: running CW the
    // next marker is B, so position is A; running CCW the next is A, so
    // position is B.
    if (Refusal r = admitStartMarker(o)) { refuse(r); return; }
    int a=-1,b=-1;
    if (!parseInterval(c.payload,a,b)) { warn("START_INTERVAL REFUSED: expected AAA-BBB"); return; }
    if (a<0||a>=ROUTE_N||b<0||b>=ROUTE_N){ warn("START_INTERVAL REFUSED: out of range"); return; }
    if (nextMarker((uint8_t)a,+1)!=(uint8_t)b){ warn("START_INTERVAL REFUSED: markers not adjacent"); return; }
    declarePosition(travelDir()>0?(uint8_t)a:(uint8_t)b, travelDir(), c.payload);

  } else if (!strcmp(leaf,"start_mm")) {
    if (Refusal r = admitStartMarker(o)) { refuse(r); return; }
    int n;
    if (!parseInt(c.payload,n))          { warn("START_MM REFUSED: not a number"); return; }
    if (n<0 || n>=ROUTE_N)               { warn("START_MM REFUSED: out of range"); return; }
    declarePosition((uint8_t)n,travelDir(),nullptr);

  } else if (!strcmp(leaf,"brake")) {
    int n;
    if (!parseInt(c.payload,n)) { warn("BRAKE REFUSED: not a number"); return; }
    brakeValue = (uint8_t)constrain(n,0,255);
    // A LIVE state/brake, which is what 0011 asked for -- "a live state/brake
    // replacing the retained inert 0". Never an inert zero again.
    char v[8]; snprintf(v,sizeof(v),"%u",brakeValue); pub(T_BRAKE,v,true);

  } else if (!strcmp(leaf,"dispatcher_release")) {
    // The dispatcher console's RELEASE. P9 keeps release on that console, and
    // it must actually release: withdraw enrolment, stop, and say so. Same
    // effect as cmd/auto 0. NAVI_ONE 0.1 did not subscribe to it at all, so
    // the button responded on screen and the locomotive stayed enlisted.
    if (autoEnrolled || autoRunning) {
      autoEnrolled=false; autoRunning=false; requestPwm(0,0,AUTO_STEP_DOWN_MS);
      pub(T_ST_AUTO,"0",true);
      warn("RELEASED by dispatcher");
      publishNav("DISPATCHER_RELEASE",nullptr,Ruling::NoPosition);
    }

  } else if (!strcmp(leaf,"auto")) {
    bool want;
    if (!parseBool(c.payload,want)) { warn("AUTO REFUSED: expected 0 or 1"); return; }
    if (!want) {
      autoEnrolled=false; autoRunning=false; requestPwm(0,0,AUTO_STEP_DOWN_MS);
    } else if (!NGR_ENABLE_EXPERIMENTAL_AUTO || !hallReady) {
      warn("AUTO disabled pending supervised NAVI_COHERENCE station acceptance");
    } else if (Refusal r = admitAuto(o)) {
      refuse(r);
    } else {
      autoEnrolled = true;
    }
    char v[4]; snprintf(v,sizeof(v),"%u",autoEnrolled?1:0); pub(T_ST_AUTO,v,true);

  } else if (!strcmp(leaf,"go") || (dispatcher && strstr(c.topic,"/go/"))) {
    if (!NGR_ENABLE_EXPERIMENTAL_AUTO || !hallReady) {warn("GO disabled pending supervised NAVI_COHERENCE acceptance");return;}
    if (Refusal r = admitGo(o)) { refuse(r); return; }
    autoRunning=true;
    // stationService selects the current position's target before serviceRamp.
    warnClear();

  } else if (!strcmp(leaf,"stop") || (dispatcher && strstr(c.topic,"/stop/"))) {
    autoRunning=false; requestPwm(0,0,AUTO_STEP_DOWN_MS);

  } else if (!strcmp(leaf,"throttle")) {
    if (Refusal r = admitThrottle(o)) { refuse(r); return; }
    int n;
    if (!parseInt(c.payload,n)) { warn("THROTTLE REFUSED: not a number"); return; }
    requestPwm(n, MANUAL_STEP_UP_MS, 0, StopCause::Controlled, true);

  } else if (!strcmp(leaf,"direction")) {
    bool fwd;
    if (!parseMotorDir(c.payload,fwd)) { warn("DIRECTION REFUSED: expected 0 (REV) or 2 (FWD)"); return; }
    if (fwd == (motorDirection!=0)) return;                 // already there
    if (Refusal r = admitMotorDirection(o)) { refuse(r); return; }
    if(actualPwm || commandedPwm){warn("DIRECTION: stop fully, then reverse and redeclare");return;}
    motorDirection = fwd ? 1 : 0;
    applyTravelDirection();          // reversing the motor reverses travel
  }
}

static uint8_t lowVoltCount = 0;
static void serviceIna(){
  static uint32_t last=0;
  if (!inaReady) return;
  if (millis()-last<5000) return; last=millis();
  busV=ina219.getBusVoltage_V(); busA=ina219.getCurrent_mA()/1000.0f; busW=ina219.getPower_mW()/1000.0f;

  const bool noPack = busV < DISCONNECTED_VOLTAGE_THRESHOLD;
  if (noPack) {
    lowVoltCount = 0;                      // bench power is not a flat battery
  } else if (busV < SHUTDOWN_VOLTAGE) {
    if (lowVoltCount < VOLTAGE_COUNTER_LIMIT) ++lowVoltCount;
  } else {
    lowVoltCount = 0;
  }

  if (!lowVoltage && lowVoltCount >= VOLTAGE_COUNTER_LIMIT) {
    lowVoltage = true;
    autoRunning = false;
    requestPwm(0,0,AUTO_STEP_DOWN_MS,StopCause::Safety); // steep, informative, NOT instant
    char w[96]; snprintf(w,sizeof(w),"LOW VOLTAGE %.2f V — stopping",(double)busV);
    warnStick(w);
  } else if (lowVoltage && busV >= RECOVERY_VOLTAGE) {
    lowVoltage = false; lowVoltCount = 0;
    Serial.printf("[BATT] recovered at %.2f V\n",(double)busV);
  }
  pub(T_ST_LOWV, lowVoltage?"1":"0", true);

  char v[16];
  snprintf(v,sizeof(v),"%.2f",busV); pub(T_V,v,true);
  snprintf(v,sizeof(v),"%.2f",busA); pub(T_A,v,true);
  snprintf(v,sizeof(v),"%.2f",busW); pub(T_W,v,true);
}

static void serviceIrHealth(){
  static uint32_t last=0,revision=UINT32_MAX;
  const uint32_t now=millis();
  if(now-last<1000 && (revision==irHealth.revision() || now-last<200))return;
  last=now;
  char payload[960];
  const int n=irHealth.format(payload,sizeof(payload),esp_timer_get_time(),bootId);
  revision=irHealth.revision();
  if(n>0 && n<(int)sizeof(payload))pub(T_IR_HEALTH,payload);
  else ++pubDropped;
  Serial.printf("[IR HEALTH] %s / %s epoch=%llu active=%u MMref=%s (MEASUREMENT SOURCE)\n",
    ngr_nav::irHealthName(irHealth.health().fault),
    ngr_nav::irReadinessName(irHealth.health().readiness),
    (unsigned long long)irHealth.odometry().epochId(),irHealth.odometry().epochActive()?1u:0u,
    irHealth.reference().validFor(irHealth.odometry())?"VALID":"UNAVAILABLE");
}

static void serviceIr(){
  static uint32_t last=0;if(millis()-last<1000)return;last=millis();
  static uint32_t previousAdvances=0;
  const uint32_t advances=navigator.status().advances;
  const uint64_t now=esp_timer_get_time();
  const auto& w=movement.latest();
  const uint64_t age=movement.have() && now>=movement.lastArrival()?
    (now-movement.lastArrival())/1000:UINT64_MAX;
  const auto speed=irSpeedQualification.assess(
    irSpeed.sample(irHealth,now,movement.paired(),radioReady),now,irCarCoupled,
    actualPwm>0,advances!=previousAdvances);
  const auto displaySpeed=naviSpeedInterpretation.assess(speed,now,irCarCoupled,
    commandedPwm==0,actualPwm>0,advances!=previousAdvances,
    movement.paired() && radioReady && age<=1000,w);
  previousAdvances=advances;
  char speedFields[560];
  const int speedSize=ngr_nav::formatIrSpeed(speedFields,sizeof(speedFields),speed,irCarCoupled,&displaySpeed);
  if(speedSize<=0 || speedSize>=(int)sizeof(speedFields)){++pubDropped;return;}
  uint8_t channel=0;wifi_second_chan_t second;esp_wifi_get_channel(&channel,&second);
  char b[1200];
  const int n=snprintf(b,sizeof(b),
    "{\"paired\":%u,\"channel\":%u,\"channel_ok\":%u,\"radio_ready\":%u,"
    "\"seen_mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\","
    "\"seen\":%lu,\"accepted\":%lu,\"rejected\":%lu,\"duplicate\":%lu,\"reboot\":%lu,"
    "\"queue_drop\":%lu,\"fresh\":%u,\"age_ms\":%llu,\"reason\":%u,"
    "\"boot\":\"%016llX\",\"seq\":%lu,\"pulses\":%llu,\"span\":%u,"
    "\"distance_bounds\":\"UNVALIDATED\",\"authority\":\"NOMINAL_HYPOTHESIS_EVIDENCE\",%s}",
    movement.paired()?1:0,channel,channel==11?1:0,radioReady?1:0,
    lastSeenMac[0],lastSeenMac[1],lastSeenMac[2],lastSeenMac[3],lastSeenMac[4],lastSeenMac[5],
    (unsigned long)seenFrames,(unsigned long)movement.accepted,(unsigned long)movement.rejected,
    (unsigned long)movement.duplicates,(unsigned long)movement.reboots,(unsigned long)irQueueDrops,
    age<=1000?1:0,(unsigned long long)age,w.opticalReason,(unsigned long long)w.bootId,
    (unsigned long)w.sequence,(unsigned long long)w.completedPulses,w.span,speedFields);
  if(n>0 && n<(int)sizeof(b)){pub(T_IR_LINK,b);pub(T_IR,b);}
  else ++pubDropped;
}

static void serviceStatus(){
  static uint32_t last=0;static ngr_nav::MotionPoint prior;
  if(millis()-last<1000)return;last=millis();
  const NavStatus& status=navigator.status();
  ngr_nav::MotionPoint current;
  current.wire=movement.latest();current.frame=movement.frame();
  current.issue=movement.have() && esp_timer_get_time()-movement.lastArrival()<=1000000 ?
                 ngr_nav::MotionIssue::None:ngr_nav::MotionIssue::Stale;
  const auto motion=ngr_nav::between(prior,current);prior=current;
  const char* moving=motion.usable() && motion.pulses?"1":"null";
  char estAge[16]="null"; // Hall speed age: null = no Hall-derived measurement yet
  if(estMeasuredAtMs)snprintf(estAge,sizeof(estAge),"%lu",(unsigned long)(last-estMeasuredAtMs));
  char payload[960];
  int n=snprintf(payload,sizeof(payload),
    "{\"level\":\"%s\",\"reason\":\"STATUS\",\"loco\":\"%s\",\"uptime_ms\":%lu,"
    "\"state\":\"%s\",\"nav\":\"%s\",\"nav_state\":\"%s\",\"mm\":%u,"
    "\"dead_reckoned_mm\":%u,\"tgt\":%u,\"dir\":\"%s\",\"session_dir\":\"%s\",\"trust\":\"%s\","
    "\"powered\":%u,\"moving\":%s,\"motion_source\":\"IR\",\"pwm\":%d,"
    "\"auto\":%u,\"running\":%u,\"estop\":%u,\"lowvolt\":%u,\"ina\":%u,"
    "\"est_mm_s\":%lu,\"est_age_ms\":%s,\"baseline\":%d,\"base_age_ms\":%lu,"
    "\"distance_confirmed\":%u,\"distance_assessable\":%u,\"ir_interval\":\"%s\",\"ir_waits\":%lu,\"ir_bounds\":\"UNVALIDATED\",\"ir_fitted\":%u,"
    "\"agree\":%lu,\"disagree\":%lu,\"pub_drop\":%lu,\"cmd_drop\":%lu}",
    navigator.positionKnown()?"CLEAR":navigator.evaluating()?"EVALUATING":"UNSET",LOCO_NAME,(unsigned long)last,
    consoleNavName(status.state),consoleNavName(status.state),
    navStateName(status.state),status.navMm,status.navMm,status.target,
    status.navDir>0?"CW":status.navDir<0?"CCW":"UNSET",
    sessionDir>0?"CW":sessionDir<0?"CCW":"UNSET",trustName(status.trust),
    actualPwm>0?1:0,moving,actualPwm,autoEnrolled?1:0,autoRunning?1:0,
    estopped?1:0,lowVoltage?1:0,inaReady?1:0,(unsigned long)estMmPerS,estAge,
    (int)hallReference.value(),(unsigned long)(hallReady?last-hallReference.status().setAtMs:0),status.distanceConfirmed?1:0,
    status.distanceAssessable?1:0,distanceBasisName(status.distance),(unsigned long)status.irWaits,
    movement.paired()?1:0,(unsigned long)status.advances,(unsigned long)status.refusals,
    (unsigned long)pubDropped,(unsigned long)cmdDropped);
  if(n>0 && n<(int)sizeof(payload))pub(T_ALERT,payload);
  else {++pubDropped;pub(T_ALERT,"{\"level\":\"WARN\",\"reason\":\"ALERT_OVERSIZE\"}");}
  char value[16];
  snprintf(value,sizeof(value),"%d",actualPwm);pub(T_ST_THR,value,true);
  pub(T_ST_DIR,motorDirection?"2":"0",true);
  pub(T_ST_ESTOP,estopped?"1":"0",true);
  pub(T_ST_NAVREADY,consoleHasReference(status.state)?"1":"0",true);
  snprintf(payload,sizeof(payload),
    "{\"hall_events\":%lu,\"hall_drop\":%lu,\"route_excluded_pulses\":%llu,\"ir_drop\":%lu,"
    "\"stale_frame\":%lu,\"pub_drop\":%lu,\"cmd_drop\":%lu}",
    (unsigned long)nextEventSerial,(unsigned long)hallSampleLoss,(unsigned long long)navigator.route().excludedTotal(),
    (unsigned long)irQueueDrops,(unsigned long)staleJudged,(unsigned long)pubDropped,(unsigned long)cmdDropped);
  pub(T_STAT,payload);
}

void setup(){
  Serial.begin(115200); delay(300);
  bootId = ((uint64_t)esp_random() << 32) | esp_random();
  naviSyncSession=(uint32_t)esp_random();
  if(!naviSyncSession)naviSyncSession=1;
  naviSyncRecorder.begin((uint32_t)LOCO_ID,naviSyncSession,bootId);
  naviSyncDestValid=naviSyncDest.fromString(NAVI_SYNC_HOST);
  if(naviSyncDestValid)
    Serial.printf("[REC] NSR1 native Hall + accepted IR -> %s:%u session=%08lX (observation only)\n",
                  NAVI_SYNC_HOST,(unsigned)NAVI_SYNC_PORT,(unsigned long)naviSyncSession);
  else
    Serial.printf("[REC] NSR1 host is not a dotted quad: %s (not recording)\n",NAVI_SYNC_HOST);
  analogReadResolution(12);
  pinMode(HALL_PIN,INPUT);
  pinMode(MOTOR_DIR_PIN,OUTPUT); pinMode(MOTOR_PWM_PIN,OUTPUT);
  digitalWrite(MOTOR_DIR_PIN,HIGH);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(MOTOR_PWM_PIN,PWM_FREQUENCY,PWM_RESOLUTION);
#else
  ledcSetup(PWM_CHANNEL,PWM_FREQUENCY,PWM_RESOLUTION);
  ledcAttachPin(MOTOR_PWM_PIN,PWM_CHANNEL);
#endif
  writePwm(0);
  Wire.begin(I2C_SDA,I2C_SCL); inaReady=ina219.begin();
  // A loose I2C wire used to convert "protected" into "unprotected" in
  // silence: 0.1 set inaReady=false, serviceIna() returned forever, and there
  // was no warning, no status field and not even a serial line. Battery
  // protection absent for a whole session, invisibly.
  if (!inaReady) Serial.println("[BATT] INA219 NOT FOUND — NO BATTERY PROTECTION THIS SESSION");
  buildTopics();
  irQ=xQueueCreate(24,sizeof(IrRx));
  pairing.begin("ngr-nav",false);
  uint8_t savedMac[6]{};if(pairing.getBytesLength("ir_mac")==6)pairing.getBytes("ir_mac",savedMac,6);
  movement.pair(savedMac);
  irHealth.pair(savedMac,esp_timer_get_time());
  pubQ  =xQueueCreate(48,sizeof(PubMsg));    // holds ~5 s while the broker is away
  cmdQ  =xQueueCreate(16,sizeof(CmdMsg));
  naviSyncRefreshContext();
  Serial.printf("[BOOT] %s \"%s\" — %s\n",SKETCH_NAME,BUILD_SUBTITLE,LOCO_NAME);
  Serial.printf("[BOOT] %s %s — PROXIMAL recovery active; not field accepted.\n",SKETCH_NAME,BUILD_CLASS);
  Serial.printf("[CAL] 2 s NAVI boot Hall reference — keep clear of magnets\n");
  if (!pubQ || !cmdQ || !irQ) {
    Serial.println("[BOOT] FATAL: queue allocation failed — halting");
    writePwm(0); for(;;) delay(1000);
  }
  if (xTaskCreatePinnedToCore(hallTask,"hall",4096,nullptr,3,nullptr,0) != pdPASS) {
    // QUORUM checked this and halted loudly. 0.1 ignored the return, so a
    // failed Hall task would boot a locomotive that navigates by nothing and
    // says nothing about it.
    Serial.println("[BOOT] FATAL: Hall task would not start — halting");
    writePwm(0); for(;;) delay(1000);
  }
  WiFi.mode(WIFI_STA);WiFi.setSleep(false);
  radioReady=esp_now_init()==ESP_OK;
  if(radioReady)radioReady=esp_now_register_recv_cb(onIr)==ESP_OK;
  WiFi.begin(WIFI_SSID,WIFI_PASS);
  mqtt.setServer(MQTT_BROKER,MQTT_PORT); mqtt.setCallback(onMqtt); mqtt.setBufferSize(1408);
  if (xTaskCreatePinnedToCore(networkTask,"net",8192,nullptr,1,nullptr,1) != pdPASS)
    Serial.println("[BOOT] WARNING: network task would not start — running blind");
  char b[700];
  const int bootLen=snprintf(b,sizeof(b),
    "{\"sketch\":\"%s\",\"loco\":\"%s\",\"boot_id\":\"%016llX\","
    "\"field_accepted\":0,\"auto_enabled\":%u,\"hall_entry\":%d,\"hall_opening_samples\":%u,"
    "\"hall_only_guard_ms\":%u,\"baseline\":\"NAVI_SPATIAL_100_200MM_MEDIAN\","
    "\"ref_clearance_mm\":%u,\"ref_collect_end_mm\":%u,\"hall_detector\":\"NONE_UPSTREAM\","
    "\"ir_source\":\"ESPNOW_TYPE5\",\"ir_bounds\":\"UNVALIDATED\","
    "\"ir_authority\":\"PHYSICAL_PROGRESS_GATE\","
    "\"normal_model\":\"EXPECT_CONFIRM_ADVANCE\",\"recovery_word\":10,\"traffic_coordination\":0,"
    "\"ir_health_mode\":\"RECOVERY_MEASUREMENT\",\"ir_window_pct\":15,\"sequence_authority\":\"PROXIMAL_UNIQUE_BETTER\"}",
    SKETCH_NAME,LOCO_NAME,(unsigned long long)bootId,
    (unsigned)NGR_ENABLE_EXPERIMENTAL_AUTO,(int)navi_hall::OPENING_DEPARTURE_COUNTS,
    (unsigned)navi_hall::OPENING_SAMPLES,(unsigned)HALL_ONLY_GUARD_MS,
    (unsigned)(navi_hall::REF_CLEARANCE_END_UM/1000),(unsigned)(navi_hall::REF_COLLECT_END_UM/1000));
  if (bootLen < 0 || bootLen >= (int)sizeof(b)) {
    Serial.printf("[BOOT] FATAL: boot record oversize (%d bytes) — halting\n",bootLen);
    writePwm(0); for(;;) delay(1000);
  }
  pub(T_BOOT,b,true);
  if (!inaReady) warn("INA219 NOT FOUND — no battery protection this session");
  Serial.println("[BOOT] NAVI_COHERENCE: declare interval/direction; NAVI expects the next mapped magnet. AUTO ENABLED (field test).");
}

// The station machine's single call site. AUTO only: MANUAL keeps operator
// authority, and unresolved navigation withdraws position-dependent AUTO so this cannot
// drive station logic from a stale MM.
//
// The cruise it is handed is the SECTION cruise, so an approach off the Grillers
// climb or the Patio curve ramps down from the speed actually being run rather
// than from the flat base (decisions 0066, 0067).
static void stationService(uint32_t now){
  stationMachine.setRunning(autoRunning,now);
  if(!autoRunning)return;
  if(!navigator.positionKnown()){
    withdraw("NAV location unresolved: position-dependent AUTO withdrawn");return;
  }
  // Station logic consumes only a coherent current MM.
  auto decision=ngr_nav::stationConsensus(stationMachine,navigator.hypotheses(),
                     actualPwm,AUTO_CRUISE_PWM,now);
  if(!decision.agrees){withdraw("Station action unresolved or approach failed: controlled stop");return;}
  stationMachine=decision.next;
  const auto& order=decision.order;
  if(order.setThrottle)requestPwm(order.pwm,
      order.pwm>actualPwm?order.stepMs:AUTO_STEP_UP_MS,
      order.pwm<actualPwm?order.stepMs:AUTO_STEP_DOWN_MS);
  if(!order.setThrottle && stationMachine.phase()==StPhase::Idle && navigator.positionKnown()){
    const auto& status=navigator.status();
    const uint8_t cruise=cruisePwmAt(status.navMm,status.navDir,AUTO_CRUISE_PWM);
    if(cruise!=rampTarget)requestPwm(cruise,AUTO_STEP_UP_MS,
                                  cruise<rampTarget?GRADE_STEP_MS:AUTO_STEP_DOWN_MS);
  }
  if(order.event){
    char b[220];snprintf(b,sizeof(b),
      "{\"event\":\"%s\",\"station\":\"%s\",\"phase\":\"%s\",\"off\":%d,\"pwm\":%u,\"consensus\":1}",
      order.event,order.station,stPhaseName(stationMachine.phase()),order.offset,order.pwm);
    pub(T_STATION,b);
  }
}

// 20Q3: POSITION_ADVANCED_SANS_MM. NAVI judged that the expected MM's
// complete +/-15% window was physically traversed with no accepted Hall
// landmark. navMm is NAVI's position judgment, not the last Hall-observed MM.
static void publishSansMm(){
  SansMmAdvance a;
  while(navigator.takeSansAdvance(a)){
    char b[320];
    snprintf(b,sizeof(b),
      "{\"ruling\":\"POSITION_ADVANCED_SANS_MM\",\"mm\":%u,\"dir\":\"%s\",\"reference_mm\":%u,"
      "\"travel_mm\":%.1f,\"window_high_mm\":%.1f,\"consecutive\":%u,\"auto_limit\":%u,"
      "\"hall_evidence\":\"%s\",\"polarity\":\"%c\",\"drops\":%lu}",
      a.mm,a.dir>0?"CW":"CCW",a.referenceMm,a.travelMm,a.windowHighMm,a.consecutive,
      (unsigned)SANS_MM_AUTO_LIMIT,a.contradictory?"HELD_WRONG_POLARITY":"NONE",
      a.contradictory?poleChar(a.polarity):'?',(unsigned long)navigator.sansAdvanceDrops());
    pub(T_SANS_MM,b);
    publishNav(rulingName(Ruling::PositionAdvancedSansMm),nullptr,Ruling::PositionAdvancedSansMm);
    lastAdvanceMs=0; // Hall-interval speed never spans a position with no Hall landmark
  }
}

// Continuous exact-window evaluation against the current same-epoch IR point.
// Called after serviceHall() has judged every opening in the native samples
// read this pass. Those samples run up to "now", later than the latest IR
// point's capture, so a traversal can never pre-empt an opening before it.
static void serviceTraversal(){
  navigator.traverse(irHealth.odometry().point());
  publishSansMm();
  // Provisional AUTO landmark-authority limit: normal End AUTO Operations.
  // Not LOST: navMm, route history and healthy IR are all retained.
  if(autoRunning && navigator.autoLandmarkLimitReached())
    withdraw("AUTO ENDED: 10 consecutive MMs traversed without a Hall landmark. Manual available.");
}

static void publishHallReference(const char* event){
  const auto& r=hallReference.status();
  char b[420];
  const int n=snprintf(b,sizeof(b),
    "{\"event\":\"%s\",\"phase\":\"%s\",\"reference\":%d,\"previous\":%d,"
    "\"anchor_event_serial\":%lu,\"route_mm\":%.1f,\"samples\":%lu,\"stationary_skipped\":%lu,"
    "\"median_of\":%lu,\"closes\":%lu,\"ends\":%lu,\"last_end\":\"%s\",\"saturated\":%u,"
    "\"clearance_mm\":%u,\"collect_end_mm\":%u,\"basis\":\"IR_ROUTE_DISTANCE\",\"authority\":\"NAVI\"}",
    event,navi_hall::refPhaseName(r.phase),(int)r.value,(int)r.previous,(unsigned long)r.anchorSerial,
    double(r.routeUm)/1000.0,(unsigned long)r.collected,(unsigned long)r.stationarySkipped,
    (unsigned long)r.lastCount,(unsigned long)r.closes,(unsigned long)r.ends,navi_hall::refEndName(r.lastEnd),
    r.saturated?1:0,(unsigned)(navi_hall::REF_CLEARANCE_END_UM/1000),(unsigned)(navi_hall::REF_COLLECT_END_UM/1000));
  if(n>0 && n<(int)sizeof(b))pub(T_HALL_REF,b);else ++pubDropped;
  Serial.printf("[HALL REF] %s\n",b);
}

// NAVI judges one opening. This is 20Q3's per-opening loop body, fed by NAVI's
// own opening observation instead of X22R's queue.
static void judgeOpening(const navi_hall::Opening& o){
  JudgedOpening j{};j.o=o;
  nextEventSerial=nextEventSerial+1;j.serial=nextEventSerial;
  j.priorGapMs=lastOpeningMs?o.tMs-lastOpeningMs:0;lastOpeningMs=o.tMs;
  j.referenceAgeMs=o.tMs-hallReference.status().setAtMs;
  if(o.tUs<frameStartUs){++staleJudged;return;}
  const auto point=movement.at(o.tUs,esp_timer_get_time());
  const auto before=navigator.status();
  NavObservation p;p.openedAtMs=o.tMs;p.polarity=o.polarity;
  p.motivePwmZero=o.pwm==0;
  p.directionConflict=before.navDir!=travelDir() || !travelDir();
  p.odometry=irHealth.at(o.tUs,esp_timer_get_time());
  compareMovement(j,p.odometry);
  const uint32_t syncsBefore=navigator.synchronizations();
  const Ruling result=navigator.judge(p);
  publishSansMm(); // expected MMs traversed before this opening, if any
  publishDecision(j,before,result,point);publishNav(rulingName(result),&j,result);
  const bool accepted=result==Ruling::Advanced || result==Ruling::AdvancedWithDiscrepancy;
  if(accepted) {
    char recoveryJson[512];
    const int n=navigator.recovery().format(recoveryJson,sizeof(recoveryJson),j.serial);
    if(n>0 && n<int(sizeof(recoveryJson)))pub(T_RECOVERY,recoveryJson);else ++pubDropped;
    // NAVI's spatial Hall reference restarts from this accepted opening, at
    // the IR point NAVI synchronized to it (none: no collection this interval).
    const bool synced=navigator.synchronizations()!=syncsBefore;
    const auto& anchor=navigator.referencePoint();
    const auto ev=hallReference.acceptedOpening(j.serial,synced?anchor:ngr_nav::IrOdometryPoint{},
        navigator.referenceExcluded(),uint64_t(int64_t(anchor.capturedUs)+irHealth.clockOffsetUs()),hallRing);
    publishHallReference(ev==navi_hall::RefEvent::Began?"CLEARANCE_BEGAN":"NO_COLLECTION");
  }
  SequenceCorrection fix;
  if(navigator.takeCorrection(fix))applySequenceCorrection(fix);
  if(accepted)
    irHealth.acceptedMm(navigator.status().navMm,o.tMs,o.tUs,j.serial,esp_timer_get_time());
  else if(!navigator.positionKnown())irHealth.invalidateReference();
  if(navigator.status().state==NavState::Uncertain && autoRunning)
    withdraw("NAV location unresolved: controlled stop; Manual may continue");
  if(accepted && before.navDir && lastAdvanceMs){
    uint32_t elapsed=o.tMs-lastAdvanceMs;
    if(elapsed && elapsed<30000){estMmPerS=spanMm(before.navMm,before.navDir)*1000UL/elapsed;estMeasuredAtMs=o.tMs;}
  }
  if(accepted)lastAdvanceMs=o.tMs;
  char speed[16];snprintf(speed,sizeof(speed),"%lu",(unsigned long)estMmPerS);pub(T_SPEED,speed,true);
}

// NAVI reads every native Hall sample. Until the boot reference exists the
// samples build it; afterwards each is measured against NAVI's reference.
// A sample overwritten before NAVI read it is Hall evidence lost: NAVI stops
// claiming a position, exactly as 20Q3 did for a dropped detection.
static void serviceHall(){
  const uint32_t head=hallRing.head();
  if(!hallCursorReady){hallCursor=head;hallCursorReady=true;} // boot backlog is not evidence
  bool lost=false;
  navi_hall::HallSample x;
  while(hallCursor!=head){
    if(!hallRing.at(hallCursor,x)){lost=true;hallCursor=head;break;}
    ++hallCursor;
    if(!hallReference.ready()){
      if(hallReference.bootSample(x.raw,uint32_t(x.tUs/1000))==navi_hall::RefEvent::BootReady){
        hallReady=true;publishHallReference("BOOT_REFERENCE_SET");
      }
      continue;
    }
    navi_hall::Opening o;
    if(openingObserver.sample(x,hallReference.value(),o))judgeOpening(o);
  }
  if(lost){
    ++hallSampleLoss;
    navigator.haltForLoss();
    irHealth.invalidateReference();
    if(autoRunning)withdraw("HALL SAMPLES LOST BEFORE NAVI READ THEM. Declare position.");
    else warnStick("HALL SAMPLES LOST BEFORE NAVI READ THEM. Declare position.");
  }
}

void loop(){
  CmdMsg c;while(cmdQ && xQueueReceive(cmdQ,&c,0)==pdTRUE)handleCommand(c);
  if(actualPwm==0)irPwmZeroSeen=true;
  serviceMovement();
  if(actualPwm==0)irPwmZeroSeen=true;
  observationHold=actualPwm==0 && commandedPwm==0;
  serviceHall();
  serviceTraversal();
  stationService(millis());
  naviSyncRefreshContext();
  serviceRamp(); serviceIna(); serviceIr(); serviceStatus(); serviceIrHealth();
  if(actualPwm==0)irPwmZeroSeen=true;
  delay(2);
}
