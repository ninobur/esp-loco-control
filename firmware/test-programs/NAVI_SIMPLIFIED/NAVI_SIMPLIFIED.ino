/*
 * NAVI_SIMPLIFIED — development build, not field accepted.
 * Magnet openings, physical safeguards and transparent NAV judgment are
 * separate. Do not flash until the physical Vmax and integration gates pass.
 * A baseline is attempted after the event closes: 80 ms guard, 200 samples,
 * 32-count spread, preceding event gap <= 3 s, PWM > 30 throughout.
 * Rejected attempts carry the previous baseline. IR publishes only.
 * NOT FIELD ACCEPTED. Flash and test under operator supervision.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

#include "LocoConfig.h"
#include "../../QUORUM/credentials.h"
#include "RouteMap.h"
#include "SimpleNavigator.h"
#include "Ops.h"
#include "Stations.h"
#include "SimpleHall.h"
#include "Reachability.h"

#ifndef NAVI_BASELINE_ADAPT_PWM
#error "This locomotive's profile has no NAVI_BASELINE_ADAPT_PWM. Measure the tractive floor from its own PWM/speed fit; do not copy another locomotive's."
#endif

using namespace navi_one;

// Published on state/bootid. It is the ONLY thing that tells telemetry which
// build is running, so it advances with every behavioural change.
//
// Corrective field-test build. Decisions 0080/0081: morphology is diagnostic
// only, passages are never paused or stitched, and the rebound guard is 500 ms.
#define SKETCH_NAME    "NAVI_SIMPLIFIED_DEV_20260918"
#define BUILD_CLASS    "DEVELOPMENT_NOT_FOR_FIELD"
#define BUILD_SUBTITLE "Opening / reachability / judgment navigator"
#define FIELD_ACCEPTED 0

// Types used in function signatures must appear before the Arduino
// prototype generator's insertion point, which is just after the includes.
// One judged passage crossing to the loop thread. Hoisted with the other
// types: the Arduino prototype generator inserts declarations just after the
// includes, so anything used in a function signature must be defined above it.
struct Judged {
  uint32_t epoch, serial, openedAtMs, priorGapMs;
  int16_t raw, baseline;
  uint8_t polarity, pwm;
  bool restartUncertain, zeroDuringCandidate;
};
struct HallDecisionMsg { HallDecision d; uint32_t t, serial; int16_t raw; };
struct PubMsg { char topic[72]; char payload[704]; uint16_t len; bool retain; };
struct CmdMsg { char topic[72]; char payload[64]; };

static const char*   MQTT_BROKER = "192.168.68.142";
static const uint16_t MQTT_PORT  = 1883;

static constexpr uint8_t HALL_PIN = 33;      // ADC1_CH5
static constexpr uint8_t IR_PIN   = 34;      // ADC1_CH6, input-only, no pull
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
// LAYER 1/2 — acquisition and recognition. Both live on the Hall task.
// ---------------------------------------------------------------------------
static SimpleHall hall;
static Navigator navigator;

// STATIONS (decision 0068). Pure machine in Stations.h; this is the only place
// it touches the locomotive. It is serviced on every loop pass, not only on an
// advance, because the dwell clock and the completion of the zero ramp are both
// events with no marker behind them.
static StationMachine stationMachine;

// One judged passage, crossing to the loop thread. The waveform does NOT cross
// there: the recognizer already ran, on the task that captured it. (It is
// separately retained in waveformWindow, above, for the rare case AUTO gets
// withdrawn and the operator wants to see it.)
static QueueHandle_t judgedQ = nullptr;
static QueueHandle_t hallDecisionQ = nullptr;
static volatile uint32_t hallEventDrops = 0, hallDecisionDrops = 0;
static uint32_t hallDecisionPubSkips = 0;
static uint64_t bootId = 0;
static volatile uint32_t nextEventSerial = 0;
static uint32_t stationarySuppressed = 0, physicalSuppressed = 0;
static uint32_t suppressionPubDropped = 0;
static uint32_t lastAcceptedOpeningMs = 0;
static bool haveAcceptedOpening = false;
#if !defined(NAVI_PHYSICAL_VMAX_MM_S) || !defined(NAVI_MAX_OPERATING_PWM)
#error "NAVI_SIMPLIFIED requires a per-locomotive physical speed bound and PWM ceiling"
#endif
// Average speed over the last confirmed interval: surveyed distance divided by
// measured time. A measurement, not a model -- nothing in the accept path uses
// it, and no PWM value appears in it. Display only.
static uint32_t lastAdvanceMs = 0; static uint32_t estMmPerS = 0;

// The Hall task owns detection. navEpoch rejects queued events from an old
// declaration or direction; the reset request is consumed on the Hall task.
static volatile bool recognizerResetRequest = false;
static volatile uint32_t navEpoch = 0;
static uint32_t staleJudged = 0;

static void carryResetRequest(){
  if (!navigator.takeResetRequest()) return;
  navEpoch++;
  recognizerResetRequest = true;
  // A declaration or a direction change moves the locomotive's idea of where it
  // is. A station armed against the old frame would be measuring its approach
  // from a position that no longer exists, so the machine stands down with it.
  stationMachine.reset();
}

// ---------------------------------------------------------------------------
// IR OBSERVER — GPIO 34. Publishes. Never votes. (decisions 0055, 0057)
// ---------------------------------------------------------------------------
// ADC1 IS SHARED WITH THE HALL SENSOR, AND THAT IS NOT FREE.
// Measured 2026-08-29 on the bench: ~1.1 floor rejects per second on the Hall
// line, INVARIANT to the motor (1.03/s running, 1.12/s stopped) and to the
// table (metal vs stone, no difference). A rate that ignores everything
// external is internal, and the cause is here: pin 34 was sampled every 1 ms
// alongside pin 33, with nothing connected to it. GPIO34 is input-only with no
// internal pull, so it floats, and the converter's sample-and-hold does not
// settle when swung between two very different channel voltages -- so the
// FLOATING pin drags the reading of the sensor the locomotive navigates by.
//
// Three changes, all of which should have been there from the start:
//   * do not sample pin 34 at all until IR proves it is fitted;
//   * when fitted, sample at 100 Hz, not 1 kHz. Spokes arrive ~30 ms apart at
//     cruise, so 1 kHz was ten times more than the measurement needs;
//   * discard the first read after a channel switch, the standard remedy.
#define IR_SAMPLE_EVERY   10      // ticks -> 100 Hz
#define IR_PRESENT_SPAN   60      // raw p2p over the probe before we believe it
#define IR_PROBE_MS     4000UL    // probe window at boot
#define IR_ENV_N 2048
#define IR_MIN_SPAN 120
#define IR_RISE_DEBOUNCE_MS 15UL
static const float IR_MM_PER_PULSE = 9.652f;     // decision 0022
static uint8_t  irWin[IR_ENV_N]; static uint16_t irHist[256];
static int irWi=0, irWfill=0, irMin=0, irMax=0;
static bool irPrimed=false, irInPulse=false;
static unsigned long irRiseMs=0, irEnvMs=0;
static volatile uint32_t irPulses=0, irRises=0, irChatter=0, irSat=0;
static volatile int32_t  irSpan=0, irRaw=0, irRawMin=4095, irRawMax=0;

static void irSample(unsigned long now);
// DECLARED in the locomotive's profile, not inferred. The boot probe survives
// only as a REPORT: when IR is declared fitted it says what the pin looked
// like, and disagreement is printed rather than acted on. When it is declared
// absent the pin is never read at all -- not even to probe it, which also
// keeps the 4 s probe from sitting on ADC1 while the Hall baseline primes.
static bool irFitted = (IR_FITTED != 0);
static bool irProbing = (IR_FITTED != 0);
static int  irProbeMin=4095, irProbeMax=0;
static unsigned long irProbeStart=0;

// Called every tick. Decides whether pin 34 is worth touching at all, and how
// often. Until IR proves it is fitted, the pin is read only during the boot
// probe and then left alone entirely.
// ONE HALL SAMPLE IS THE MEDIAN OF FIVE READS. The 2026-08-28 survey build
// (QUORUM 1.13X) averaged ADC_SAMPLES reads per sample and its 187 crossings
// carried not one reading more than 20 counts off the median-filtered copy.
// NAVI_ONE read the pin once, and on 2026-09-02 forty of seventy-four
// undecimated passages carried one -- half of them DROPOUTS toward the idle
// level in the middle of a 120-count flank, which no field can produce. A
// burst of them refused MM157 at 0.1506 and stopped the train (decision
// 0071's record).
//
// The operator's bench tests -- electronics on, motor off; on blocks at 90
// PWM -- showed no transients, which fits: a read that returns a stale or
// mid-rail value is invisible while the true signal IS the idle level, and
// shows only on a flank. "It is all the magnets" in the sense that the
// magnets are where the signal is not idle.
//
// A MEDIAN, not an average: an average dilutes a wild read by 1/N and leaves
// it in; a median of five rejects up to two outright and passes the others
// untouched. Five one-shot reads cost about 100 us of the 1 ms tick.
static int16_t hallRead(){
  int r[5];
  for (int i = 0; i < 5; ++i) r[i] = analogRead(HALL_PIN);
  for (int i = 1; i < 5; ++i) { int v = r[i], j = i - 1; while (j >= 0 && r[j] > v) { r[j+1] = r[j]; --j; } r[j+1] = v; }
  return (int16_t)r[2];
}

static void irService(unsigned long now, uint32_t tick){
  if (!irFitted) return;                       // declared absent: never read
  if (tick % IR_SAMPLE_EVERY) return;          // 100 Hz, probe included
  if (irProbing) {
    if (!irProbeStart) irProbeStart = now;
    int r = analogRead(IR_PIN);
    (void)analogRead(HALL_PIN);                // and settle back, as below
    if (r < irProbeMin) irProbeMin = r;
    if (r > irProbeMax) irProbeMax = r;
    if (now - irProbeStart >= IR_PROBE_MS) {
      irProbing = false;
      const int span = irProbeMax - irProbeMin;
      Serial.printf("[IR] declared FITTED; probe saw %d..%d span=%d%s\n",
                    irProbeMin, irProbeMax, span,
                    span < IR_PRESENT_SPAN ? "  (quiet — stationary, or check the wiring)" : "");
    }
    return;
  }
  (void)analogRead(IR_PIN);                    // settle: discard after the switch
  irSample(now);
  // AND SETTLE BACK. 0.1 discarded the first read after switching TO pin 34
  // and nothing discarded the first read after switching back, so every tenth
  // Hall sample -- the sensor the locomotive navigates by -- was taken
  // immediately after the mux left the IR channel, with exactly the
  // sample-and-hold behaviour this file's own comment describes.
  (void)analogRead(HALL_PIN);
}

static void irSample(unsigned long now){
  int raw = analogRead(IR_PIN);
  irRaw = raw;
  if (raw < irRawMin) irRawMin = raw;
  if (raw > irRawMax) irRawMax = raw;
  if (raw >= 4000) irSat++;
  uint8_t b = (uint8_t)(raw >> 4);
  if (irWfill == IR_ENV_N) irHist[irWin[irWi]]--;
  irWin[irWi] = b; irHist[b]++;
  irWi = (irWi + 1) % IR_ENV_N; if (irWfill < IR_ENV_N) irWfill++;
  if (now - irEnvMs >= 250 && irWfill >= 64) {
    irEnvMs = now;
    int needLo = (irWfill * 5) / 100, needHi = (irWfill * 95) / 100;
    int cum = 0, lo = -1, hi = -1;
    for (int i = 0; i < 256; i++) { cum += irHist[i];
      if (lo < 0 && cum > needLo) lo = i;
      if (hi < 0 && cum >= needHi) { hi = i; break; } }
    if (lo >= 0 && hi >= 0) { irMin = lo * 16; irMax = hi * 16 + 15; }
    irPrimed = irWfill >= 512;
  }
  int span = irMax - irMin; irSpan = span;
  if (!(irPrimed && span >= IR_MIN_SPAN)) { irInPulse = false; return; }
  int thrHi = irMin + (span * 2) / 3, thrLo = irMin + span / 3;
  if (!irInPulse && raw > thrHi) {
    if (irRiseMs && (now - irRiseMs) < IR_RISE_DEBOUNCE_MS) { irChatter++; }
    else { irInPulse = true; irRiseMs = now; irRises++; }
  } else if (irInPulse && raw < thrLo) { irInPulse = false; irPulses++; }
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
                       StopCause stopCause = StopCause::Controlled){
  if (target > NAVI_MAX_OPERATING_PWM) {
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
       T_STATION,T_ACQ_DIAG,T_DISCREPANCY,T_SUPPRESSION,T_CNT };
// Size follows the enum sentinel so adding a topic cannot silently create an
// out-of-bounds row (X16 audit B1).
static char T[T_CNT][72];

static void topic(int i,const char* suffix){ snprintf(T[i],72,"ngr/loco/%s/%s",LOCO_NAME,suffix); }
static void buildTopics(){
  topic(T_ONLINE,"online");            topic(T_NAV,"state/nav");
  topic(T_MARKER,"mm/marker");         topic(T_ALERT,"alert");
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
  topic(T_ACQ_DIAG,"diag/hall_decision");
  topic(T_DISCREPANCY,"nav/discrepancy");
  topic(T_SUPPRESSION,"nav/suppression");
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

// ---------------------------------------------------------------------------
// RULE 5 — one strike. An IDENTITY failure means the map and the world
// disagree; the navigator's job at that moment is to stop saying where it is.
// In MANUAL the operator has authority and the throttle is left alone.
//
// THE STRIKE LATCHES (operator's ruling, 2026-08-30). Navigator::judge() has
// already put the navigator in Struck, so positionKnown() is false and every
// later passage rules NoPosition. This function withdraws the rest:
// enrolment goes, state/auto goes to 0, nav_ready goes to 0, and GO is refused
// until a new declaration. Review of 0.1 found the strike was advisory -- it
// printed "position is not known", kept publishing a position, kept judging
// (so a matching polarity during the coast-down ADVANCED on the position the
// strike had just discredited), and let cmd/go restart AUTO on it. A stop the
// program itself does not believe is worse than no stop at all.
// ---------------------------------------------------------------------------
static void withdraw(const char* text){
  warnStick(text);
  estMmPerS = 0; pub(T_SPEED,"0",true);
  autoRunning = false;
  if (autoEnrolled) { autoEnrolled = false; pub(T_ST_AUTO,"0",true); }
  requestPwm(0,0,AUTO_STEP_DOWN_MS,StopCause::Safety);
  pub(T_ST_NAVREADY,"0",true);
  lastAdvanceMs = 0;              // the next speed estimate must not span this
}

static void publishNav(const char* event,const Judged* j,Ruling r){
  const NavStatus& s = navigator.status();
  char b[500];
  if (j) {
    const int n=snprintf(b,sizeof(b),
      "{\"event\":\"%s\",\"boot_id\":\"%016llX\",\"event_serial\":%lu,"
      "\"nav_state\":\"%s\",\"mm\":%u,\"tgt\":%u,\"dir\":\"%s\","
      "\"ruling\":\"%s\",\"obs\":\"%c\",\"expected\":\"%c\","
      "\"opened_ms\":%lu,\"gap_ms\":%lu,\"raw_open\":%d,"
      "\"base_open\":%d,\"pwm_open\":%u,\"unresolved_count\":%u,"
      "\"seq_len\":%u,\"seq_matches\":%u}",
      event,(unsigned long long)bootId,(unsigned long)j->serial,
      navStateName(s.state),s.navMm,s.target,
      s.navDir>0?"CW":(s.navDir<0?"CCW":"UNSET"),rulingName(r),
      poleChar(j->polarity),poleChar(polarityAt(s.target)),
      (unsigned long)j->openedAtMs,(unsigned long)j->priorGapMs,
      (int)j->raw,(int)j->baseline,(unsigned)j->pwm,
      (unsigned)s.unresolvedCount,(unsigned)s.sequenceLength,(unsigned)s.sequenceMatches);
    if (n<0 || n>=(int)sizeof(b)) { ++pubDropped; return; }
    pub(T_MARKER,b,false);
  } else {
    const int n=snprintf(b,sizeof(b),
      "{\"event\":\"%s\",\"nav_state\":\"%s\",\"mm\":%u,"
      "\"tgt\":%u,\"dir\":\"%s\",\"adv\":%lu,\"ref\":%lu}",
      event,navStateName(s.state),s.navMm,s.target,
      s.navDir>0?"CW":(s.navDir<0?"CCW":"UNSET"),
      (unsigned long)s.advances,(unsigned long)s.refusals);
    if (n<0 || n>=(int)sizeof(b)) { ++pubDropped; return; }
  }
  pub(T_NAV,b,true);
  Serial.printf("[NAV] %s\n",b);
}

static void publishSuppression(const Judged& j,const char* reason,uint32_t dt,uint16_t spacing){
  char b[320];
  int n=snprintf(b,sizeof(b),
    "{\"boot_id\":\"%016llX\",\"event_serial\":%lu,\"opened_ms\":%lu,"
    "\"polarity\":\"%c\",\"pwm\":%u,\"reason\":\"%s\","
    "\"dt_ms\":%lu,\"spacing_mm\":%u,\"supp_zero\":%lu,\"supp_physical\":%lu}",
    (unsigned long long)bootId,(unsigned long)j.serial,(unsigned long)j.openedAtMs,
    poleChar(j.polarity),(unsigned)j.pwm,reason,(unsigned long)dt,(unsigned)spacing,
    (unsigned long)stationarySuppressed,(unsigned long)physicalSuppressed);
  if (n<0 || n>=(int)sizeof(b) || !pub(T_SUPPRESSION,b)) ++suppressionPubDropped;
}

static void publishDiscrepancy(const Judged& j,const NavStatus& before,Ruling ruling,
                               bool directionConflict,uint32_t dt,uint16_t spacing){
  const NavStatus& after=navigator.status();
  char b[700];
  const char* evaluation=after.sequenceMatches==1 ? "UNIQUE_TEN_MARKER_WORD" :
                         after.sequenceLength<10 ? "WORD_INCOMPLETE" : "NO_UNIQUE_WORD";
  const char* action=ruling==Ruling::UnresolvedLimitStop ? "SAFETY_RAMP_ZERO" :
                     ruling==Ruling::Reestablished ? "POSITION_REESTABLISHED" : "SKIP_STATION_CONTINUE";
  int n=snprintf(b,sizeof(b),
    "{\"boot_id\":\"%016llX\",\"event_serial\":%lu,\"opened_ms\":%lu,"
    "\"pre_mm\":%u,\"pre_state\":\"%s\",\"target\":%u,\"expected\":\"%c\","
    "\"observed\":\"%c\",\"dt_ms\":%lu,\"spacing_mm\":%u,\"pwm\":%u,"
    "\"pwm_target\":%d,\"recent_mm_s\":%lu,\"station_phase\":%u,"
    "\"session_dir\":%d,\"motor_fwd\":%u,\"nav_dir\":%d,"
    "\"restart_uncertain\":%u,\"direction_conflict\":%u,"
    "\"alternatives\":[\"reread\",\"polarity_error\",\"missed_marker\",\"wrong_position_or_direction\"],"
    "\"evaluation\":\"%s\",\"matches\":%u,\"ruling\":\"%s\","
    "\"post_mm\":%u,\"post_state\":\"%s\",\"unresolved_count\":%u,\"action\":\"%s\"}",
    (unsigned long long)bootId,(unsigned long)j.serial,(unsigned long)j.openedAtMs,
    (unsigned)before.navMm,navStateName(before.state),(unsigned)before.target,
    poleChar(polarityAt(before.target)),poleChar(j.polarity),(unsigned long)dt,
    (unsigned)spacing,(unsigned)j.pwm,rampTarget,(unsigned long)estMmPerS,
    (unsigned)stationMachine.phase(),(int)sessionDir,(unsigned)motorDirection,
    (int)before.navDir,(unsigned)j.restartUncertain,
    (unsigned)directionConflict,evaluation,(unsigned)after.sequenceMatches,
    rulingName(ruling),(unsigned)after.navMm,navStateName(after.state),
    (unsigned)after.unresolvedCount,action);
  if (n<0 || n>=(int)sizeof(b)) { ++pubDropped; return; }
  pub(T_DISCREPANCY,b);
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
    navigator.setDirection(d);        // steps navMm back along the OLD heading
      carryResetRequest();
    publishNav("DIRECTION",nullptr,Ruling::NoPosition);
  }
}

static void declarePosition(uint8_t mm,int8_t dir,const char* interval){
  navigator.declare(mm,dir);
  haveAcceptedOpening=false;
  carryResetRequest();
  warnSticky = false; pub(T_WARN,"",true);   // the declaration answers the strike
  lastAdvanceMs = 0; estMmPerS = 0;          // no speed estimate spans a declaration
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
static void hallTask(void*){
  TickType_t wake = xTaskGetTickCount();
  uint32_t tick = 0;
  uint32_t myEpoch = 0;
  uint32_t lastSerial = 0;
  bool hadOpen = false, stoppedSinceOpen = false;
  for (;;) {
    const uint32_t now = millis();
    if (recognizerResetRequest) {
      hall.resetFrame();
      myEpoch = navEpoch;
      recognizerResetRequest = false;
    }
    const int16_t raw = hallRead();
    const uint8_t pwm = (uint8_t)actualPwm;
    // Quiet Hall samples cannot prove spatial clearance from the old magnet.
    if (hadOpen && pwm == 0) stoppedSinceOpen = true;
    const int16_t before = hall.baseline();
    const HallDecision d = hall.sample(now, raw, pwm);
    if (d.event == HallEvent::Open) {
      if (nextEventSerial == UINT32_MAX) {
        estopAsserted = true; // never reuse an event identity in this boot
        continue;
      }
      lastSerial = ++nextEventSerial;
      Judged j{myEpoch,lastSerial,d.openedMs,d.priorGapMs,raw,before,d.polarity,pwm,
               hadOpen && stoppedSinceOpen,d.zeroDuringCandidate};
      hadOpen = true; stoppedSinceOpen = pwm == 0;
      if (judgedQ && xQueueSend(judgedQ,&j,0) != pdTRUE) ++hallEventDrops;
    }
    if (d.event == HallEvent::Open || d.event == HallEvent::Close ||
        d.event == HallEvent::BaselineAccepted ||
        d.event == HallEvent::BaselineRejected) {
      HallDecisionMsg m{d,now,lastSerial,raw};
      if (hallDecisionQ && xQueueSend(hallDecisionQ,&m,0) != pdTRUE)
        ++hallDecisionDrops;
    }
    irService(now,tick++);
    vTaskDelayUntil(&wake,1);
  }
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
      char id[48]; snprintf(id,sizeof(id),"NAVI_ONE_%s",LOCO_NAME);
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
                            "cmd/dispatcher_release","cmd/brake"};
        for (auto c: cmds){ snprintf(sub,72,"ngr/loco/%s/%s",LOCO_NAME,c); mqtt.subscribe(sub); }
        snprintf(sub,72,"ngr/dispatcher/cmd/go/%s",LOCO_NAME);   mqtt.subscribe(sub);
        snprintf(sub,72,"ngr/dispatcher/cmd/stop/%s",LOCO_NAME); mqtt.subscribe(sub);
        mqtt.subscribe("ngr/dispatcher/cmd/estop");
        Serial.println("[NET] MQTT connected");
      }
    }
    mqtt.loop();
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
    } else if (Refusal r = admitAuto(o)) {
      refuse(r);
    } else {
      autoEnrolled = true;
    }
    char v[4]; snprintf(v,sizeof(v),"%u",autoEnrolled?1:0); pub(T_ST_AUTO,v,true);

  } else if (!strcmp(leaf,"go") || (dispatcher && strstr(c.topic,"/go/"))) {
    if (Refusal r = admitGo(o)) { refuse(r); return; }
    autoRunning=true;
    requestPwm(AUTO_CRUISE_PWM,AUTO_STEP_UP_MS,AUTO_STEP_DOWN_MS);
    warnClear();

  } else if (!strcmp(leaf,"stop") || (dispatcher && strstr(c.topic,"/stop/"))) {
    autoRunning=false; requestPwm(0,0,AUTO_STEP_DOWN_MS);

  } else if (!strcmp(leaf,"throttle")) {
    if (Refusal r = admitThrottle(o)) { refuse(r); return; }
    int n;
    if (!parseInt(c.payload,n)) { warn("THROTTLE REFUSED: not a number"); return; }
    if (n > NAVI_MAX_OPERATING_PWM) warn("THROTTLE CAPPED at PWM 120 for NAVI reachability");
    requestPwm(constrain(n,0,NAVI_MAX_OPERATING_PWM), MANUAL_STEP_UP_MS, 0);

  } else if (!strcmp(leaf,"direction")) {
    bool fwd;
    if (!parseMotorDir(c.payload,fwd)) { warn("DIRECTION REFUSED: expected 0 (REV) or 2 (FWD)"); return; }
    if (fwd == (motorDirection!=0)) return;                 // already there
    if (Refusal r = admitMotorDirection(o)) { refuse(r); return; }
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

static void serviceIr(){
  static uint32_t last=0, lastP=0; if(millis()-last<1000) return; last=millis();
  uint32_t p=irPulses, d=p-lastP; lastP=p;
  char b[300];
  snprintf(b,sizeof(b),
    "{\"pulses\":%lu,\"d\":%lu,\"rises\":%lu,\"chatter\":%lu,\"mm\":%ld,"
    "\"raw\":%ld,\"raw_min\":%ld,\"raw_max\":%ld,\"span\":%ld,\"sat\":%lu,\"votes\":0}",
    (unsigned long)p,(unsigned long)d,(unsigned long)irRises,(unsigned long)irChatter,
    (long)(p*IR_MM_PER_PULSE),(long)irRaw,(long)irRawMin,(long)irRawMax,
    (long)irSpan,(unsigned long)irSat);
  pub(T_IR,b,false);
  irRawMin=4095; irRawMax=0;
}

static void serviceStatus(){
  static uint32_t last=0; if(millis()-last<1000) return; last=millis();
  const NavStatus& s=navigator.status();
  char b[700];
  int w = snprintf(b,sizeof(b),
    // The console reads position from "dead_reckoned_mm" and renders it only
    // while "nav" is one of TRACKING / NORMAL / EVALUATING (USABLE_NAV). Both
    // names are the existing contract and are not ours to change.
    //
    // NAVI_ONE dead-reckons NOTHING: every advance is magnet-confirmed. The
    // name is wrong for this navigator and is kept only so the dashboard keeps
    // working.
    //
    // ONE EXCEPTION, AND IT IS VISIBLE. Immediately after a declaration -- and
    // after a direction change, which steps navMm back one so the next advance
    // lands on the marker about to be met again -- this field carries a marker
    // the locomotive has not confirmed. That is what a declaration IS: the
    // operator's word, not the track's. It is why "trust" reads DECLARED
    // rather than PROVEN until ten magnets have gone by, and why nav_state
    // reads DECLARED rather than NORMAL. The claim is published alongside its
    // own provenance, every second.
    "{\"level\":\"%s\",\"reason\":\"STATUS\",\"loco\":\"%s\",\"uptime_ms\":%lu,"
    "\"state\":\"%s\",\"nav\":\"%s\",\"mm\":%u,\"dead_reckoned_mm\":%u,"
    "\"last_confirmed_landmark\":\"%s\",\"tgt\":%u,\"dir\":\"%s\","
    "\"session_dir\":\"%s\",\"trust\":\"%s\",\"powered\":%u,\"est_mm_s\":%lu,"
    "\"pwm\":%d,\"auto\":%u,\"running\":%u,\"estop\":%u,\"lowvolt\":%u,"
    // MOTION, for the console's motion bar. It derives MOVING / STOPPED /
    // UNKNOWN from this field of the 1 Hz alert, which Otto's firmware sends
    // and this one never did -- so Toby's bar read "UNKNOWN -- STALE" all
    // day, dwelling or not, and on 2026-09-02 the operator read it as
    // position at the one moment it mattered. Throttle above zero is the
    // claim being made, and nothing else is: the Hall sensor's own view of
    // movement lives in the nav events.
    "\"moving\":%u,"
    // NAVI_ONE has no candidates, no viable set and no miss streak. They are
    // published as their empty values so the console's panels render rather
    // than blanking, and so their absence is visible rather than implied.
    "\"candidate_mm\":-1,\"viable\":[],\"miss_streak\":0,"
    "\"agree\":%lu,\"disagree\":%lu,\"notmag\":%lu,"
    "\"baseline\":%ld,\"shadow_baseline\":%ld,\"shadow_delta\":%ld,\"floor_rej\":%lu,"
    // Retained protocol field from the superseded 0070 discard experiment.
    // Ordinary acquisition discards no opening based on morphology.
    "\"discards\":%lu,"
    "\"nav_state\":\"%s\",\"seq_at\":%u,\"ina\":%u,"
    "\"pub_drop\":%lu,\"cmd_drop\":%lu,\"stale\":%lu,"
    "\"ir_fitted\":%u,\"ir_probe_span\":%d}",
    navigator.positionKnown()?"CLEAR":"UNSET", LOCO_NAME,(unsigned long)millis(),
    navigator.positionKnown()?"NORMAL":"UNSET",
    navigator.positionKnown()?"NORMAL":"UNSET", s.navMm, s.navMm,
    landmarkAt(s.navMm), s.target,
    s.navDir>0?"CW":(s.navDir<0?"CCW":"UNSET"),
    sessionDir>0?"CW":(sessionDir<0?"CCW":"UNSET"),
    trustName(s.trust), (actualPwm>0)?1u:0u, (unsigned long)estMmPerS,
    actualPwm, autoEnrolled?1:0, autoRunning?1:0, estopped?1:0, lowVoltage?1:0,
    (actualPwm>0)?1u:0u,
    (unsigned long)s.advances,(unsigned long)s.refusals,0UL,
    (long)hall.baseline(),(long)hall.baseline(),
    0L,0UL,
    0UL,
    navStateName(s.state), s.sequenceLength, inaReady?1u:0u,
    (unsigned long)pubDropped,(unsigned long)cmdDropped,(unsigned long)staleJudged,
    irFitted?1u:0u, irProbing ? -1 : (irProbeMax-irProbeMin));
  // NEVER ENQUEUE TRUNCATED JSON. snprintf truncates silently, the Pi's
  // json.loads() then throws, and the consumer discards the WHOLE alert --
  // so one field too many makes every field disappear. That is exactly what
  // happened on 2026-08-29: b[400] cut the payload at 399 bytes and took KPH,
  // PWM-actual and the agree/disagree counters with it. QUORUM had this guard
  // and I did not carry it over.
  if (w >= (int)sizeof(b) || w < 0) {
    char m[160];
    snprintf(m,sizeof(m),
      "{\"level\":\"WARN\",\"reason\":\"ALERT_OVERSIZE\",\"loco\":\"%s\",\"bytes\":%d}",
      LOCO_NAME, w);
    pub(T_ALERT,m,false);
    Serial.printf("[ALERT] OVERSIZE %d bytes\n", w);
  } else {
    pub(T_ALERT,b,false);
  }
  char v[8];
  snprintf(v,sizeof(v),"%d",actualPwm); pub(T_ST_THR,v,true);
  snprintf(v,sizeof(v),"%u",motorDirection?2:0); pub(T_ST_DIR,v,true);
  snprintf(v,sizeof(v),"%u",estopped?1:0); pub(T_ST_ESTOP,v,true);
  snprintf(v,sizeof(v),"%u",navigator.positionKnown()?1:0); pub(T_ST_NAVREADY,v,true);
  char counters[320];
  snprintf(counters,sizeof(counters),
    "{\"boot_id\":\"%016llX\",\"events\":%lu,\"zero_supp\":%lu,"
    "\"physical_supp\":%lu,\"supp_pub_drop\":%lu,\"hall_q_drop\":%lu,"
    "\"diag_q_drop\":%lu,\"diag_pub_skip\":%lu,\"pub_drop\":%lu,"
    "\"unresolved_count\":%u}",
    (unsigned long long)bootId,(unsigned long)nextEventSerial,
    (unsigned long)stationarySuppressed,(unsigned long)physicalSuppressed,
    (unsigned long)suppressionPubDropped,(unsigned long)hallEventDrops,
    (unsigned long)hallDecisionDrops,(unsigned long)hallDecisionPubSkips,
    (unsigned long)pubDropped,(unsigned)s.unresolvedCount);
  pub(T_STAT,counters,false);
}

void setup(){
  Serial.begin(115200); delay(300);
  bootId = ((uint64_t)esp_random() << 32) | esp_random();
  analogReadResolution(12);
  pinMode(HALL_PIN,INPUT); pinMode(IR_PIN,INPUT);
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
  judgedQ=xQueueCreate(16,sizeof(Judged));
  hallDecisionQ=xQueueCreate(64,sizeof(HallDecisionMsg));
  pubQ  =xQueueCreate(48,sizeof(PubMsg));    // holds ~5 s while the broker is away
  cmdQ  =xQueueCreate(16,sizeof(CmdMsg));
  Serial.printf("[BOOT] %s \"%s\" — %s\n",SKETCH_NAME,BUILD_SUBTITLE,LOCO_NAME);
  Serial.printf("[BOOT] NAVI_SIMPLIFIED DEVELOPMENT — not field accepted.\n");
  Serial.printf("[CAL] 2 s baseline — keep clear of magnets\n");
  if (!judgedQ || !hallDecisionQ || !pubQ || !cmdQ) {
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
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID,WIFI_PASS);
  mqtt.setServer(MQTT_BROKER,MQTT_PORT); mqtt.setCallback(onMqtt); mqtt.setBufferSize(900);
  if (xTaskCreatePinnedToCore(networkTask,"net",8192,nullptr,1,nullptr,1) != pdPASS)
    Serial.println("[BOOT] WARNING: network task would not start — running blind");
  char b[512];
  const int bootLen = snprintf(b,sizeof(b),
    "{\"sketch\":\"%s\",\"subtitle\":\"%s\",\"build_class\":\"%s\",\"field_accepted\":%d,"
    "\"loco\":\"%s\",\"entry\":%d,\"exit\":%d,\"floor_ms\":%d,"
    "\"amp_floor\":%.2f,\"guard_ms\":%lu,\"seq_n\":%d,"
    "\"offsets\":0,\"quorum\":0,\"velocity_model\":0,\"post_stop_successor\":0,"
    "\"baseline_mode\":\"interval_median_200\",\"shadow_median_n\":0,"
    "\"baseline_adapt_pwm\":%d,\"departure_diag_hz\":0,\"ir_votes\":0,"
    "\"boot_id\":\"%016llX\",\"pwm_max\":%u,\"vmax_mm_s\":%lu}",
    SKETCH_NAME,BUILD_SUBTITLE,BUILD_CLASS,(int)FIELD_ACCEPTED,
    LOCO_NAME,70,70,2,0.0,
    80UL,0,30,(unsigned long long)bootId,(unsigned)NAVI_MAX_OPERATING_PWM,
    (unsigned long)NAVI_PHYSICAL_VMAX_MM_S);
  if (bootLen < 0 || bootLen >= (int)sizeof(b)) {
    Serial.printf("[BOOT] FATAL: boot record oversize (%d bytes) — halting\n",bootLen);
    writePwm(0); for(;;) delay(1000);
  }
  pub(T_BOOT,b,true);
  if (!inaReady) warn("INA219 NOT FOUND — no battery protection this session");
  Serial.println("[BOOT] ready. session_direction, then start_mm, then auto, then GO.");
}

// The station machine's single call site. AUTO only: MANUAL keeps operator
// authority, and a struck locomotive has autoRunning false so this cannot put
// power back into one that has stopped.
//
// The cruise it is handed is the SECTION cruise, so an approach off the Grillers
// climb or the Patio curve ramps down from the speed actually being run rather
// than from the flat base (decisions 0066, 0067).
static void stationService(uint32_t now){
  const NavStatus& s = navigator.status();
  if (!autoRunning || !navigator.positionKnown()) {
    if (stationMachine.phase() != StPhase::Idle) stationMachine.reset();
    return;
  }
  const uint8_t cruise = cruisePwmAt(s.navMm, s.navDir, AUTO_CRUISE_PWM);
  StationOrder o = stationMachine.tick(s.navMm, s.navDir,
                                       (uint8_t)actualPwm, cruise, now);

  if (o.setThrottle) requestPwm((int)o.pwm, AUTO_STEP_UP_MS, o.stepMs);
  if (o.event) {
    char b[192];
    snprintf(b,sizeof(b),
      "{\"event\":\"%s\",\"phase\":\"%s\",\"station\":\"%s\",\"off\":%d,"
      "\"mm\":%u,\"dir\":\"%s\",\"pwm\":%u,\"step_ms\":%u}",
      o.event, stPhaseName(stationMachine.phase()), o.station, (int)o.offset,
      (unsigned)s.navMm, s.navDir>0?"CW":"CCW", (unsigned)o.pwm, (unsigned)o.stepMs);
    pub(T_STATION,b);
  }
}

void loop(){
  CmdMsg c; while (cmdQ && xQueueReceive(cmdQ,&c,0)==pdTRUE) handleCommand(c);
  stationService(millis());

  Judged j;
  while (judgedQ && xQueueReceive(judgedQ,&j,0)==pdTRUE) {
    // Captured under a declaration that no longer stands. It is evidence about
    // a frame that has ended and it may not advance this one.
    if (j.epoch != navEpoch) { staleJudged++; publishSuppression(j,"STALE_FRAME",0,0); continue; }
    const NavStatus before=navigator.status();
    const uint32_t dt=haveAcceptedOpening ? j.openedAtMs-lastAcceptedOpeningMs : 0;
    const uint16_t spacing=navigator.positionKnown() ? spanMm(before.navMm,before.navDir) : 0;
    if (j.zeroDuringCandidate || j.pwm==0) {
      ++stationarySuppressed;
      publishSuppression(j,"PWM_ZERO_CANDIDATE",dt,spacing);
      continue;
    }
    // Physical impossibility is tested against mapped spacing and the single
    // configured upper bound. PWM and recent measured speed never adjust it.
    if (haveAcceptedOpening && navigator.positionKnown() &&
        physicallyUnreachable(dt,spacing,NAVI_PHYSICAL_VMAX_MM_S)) {
      ++physicalSuppressed;
      publishSuppression(j,"PHYSICALLY_UNREACHABLE",dt,spacing);
      continue;
    }
    const bool directionConflict=before.navDir != travelDir() || travelDir()==0;
    Passage p; p.openedAtMs=j.openedAtMs; p.polarity=j.polarity;
    p.directionConflict=directionConflict; p.restartUncertain=j.restartUncertain;
    Ruling r = navigator.judge(p);
    if (r==Ruling::Ambiguous || r==Ruling::Reestablished ||
        r==Ruling::UnresolvedLimitStop) {
      publishDiscrepancy(j,before,r,directionConflict,dt,spacing);
    }
    switch (r) {
      case Ruling::Advanced: {
        lastAcceptedOpeningMs=j.openedAtMs; haveAcceptedOpening=true;
        // surveyed distance just covered / measured time. Display only.
        const NavStatus& ns = navigator.status();
        uint8_t from = routeMod((int32_t)ns.navMm - ns.navDir);
        if (lastAdvanceMs) {
          uint32_t dt = j.openedAtMs - lastAdvanceMs;
          if (dt > 0 && dt < 30000)
            estMmPerS = (uint32_t)((uint32_t)spanMm(from, ns.navDir) * 1000UL / dt);
        }
        lastAdvanceMs = j.openedAtMs;
        char sv[12]; snprintf(sv,sizeof(sv),"%lu",(unsigned long)estMmPerS);
        pub(T_SPEED,sv,true);

        // SECTION CRUISE (decisions 0066, 0067). The throttle follows POSITION
        // on the Grillers climb CW and the curve into Patio CCW, nowhere else.
        // Open-loop grade compensation -- see the retirement note in RouteMap.h
        // before carrying any of this into a speed-based controller. Requested only
        // when the section target actually changes, so the pacing clock is not
        // restarted every marker mid-ramp.
        //
        // AUTO only. MANUAL keeps operator authority, and a struck locomotive
        // has already had requestPwm(0) from withdraw() -- autoRunning is false
        // by then, so this cannot restore power to a locomotive that stopped.
        //
        // Backing off uses GRADE_STEP_MS (one PWM count at a time). Every other
        // downward path -- withdraw, e-stop, low voltage -- sets its own rate
        // explicitly at its own call site, so none of them inherits this one.
        // A station in charge owns the throttle: a section boundary crossed
        // during a stop or a dwell must not wind the locomotive back up.
        if (autoRunning && stationMachine.phase() == StPhase::Idle) {
          const uint8_t want = cruisePwmAt(ns.navMm, ns.navDir, AUTO_CRUISE_PWM);
          if ((int)want != rampTarget)
            requestPwm((int)want, AUTO_STEP_UP_MS,
                       (int)want < rampTarget ? GRADE_STEP_MS : AUTO_STEP_DOWN_MS);
        }
        publishNav("AGREE",&j,r);
        break; }
      case Ruling::Ambiguous:
        if (before.state != NavState::Unresolved) {
          // Uncertainty skips the station, including an already-started ramp.
          stationMachine.reset();
          if (autoRunning) requestPwm(cruisePwmAt(before.navMm,before.navDir,AUTO_CRUISE_PWM),
                                      AUTO_STEP_UP_MS,AUTO_STEP_DOWN_MS);
          pub(T_ST_NAVREADY,"0",true);
        }
        publishNav("AMBIGUOUS",&j,r); break;
      case Ruling::Reestablished:
        lastAcceptedOpeningMs=j.openedAtMs; haveAcceptedOpening=true;
        lastAdvanceMs=0; estMmPerS=0;
        publishNav("REESTABLISHED",&j,r);
        pub(T_ST_NAVREADY,"1",true); break;
      case Ruling::UnresolvedLimitStop:
        publishNav("UNRESOLVED_LIMIT_STOP",&j,r);
        withdraw("NAVI unresolved after 10 further magnets. Declare position."); break;
      default:                 publishNav("NO_POSITION",&j,r); break;
    }
  }
  HallDecisionMsg hd;
  while (hallDecisionQ && xQueueReceive(hallDecisionQ,&hd,0)==pdTRUE) {
    const char* event = hd.d.event == HallEvent::Open ? "OPEN" :
                        hd.d.event == HallEvent::Close ? "CLOSE" :
                        hd.d.event == HallEvent::BaselineAccepted ? "BASELINE_ACCEPT" :
                        "BASELINE_REJECT";
    const char* why = hd.d.reject == BaselineReject::SlowCadence ? "SLOW_CADENCE" :
                      hd.d.reject == BaselineReject::LowPwm ? "LOW_PWM" :
                      hd.d.reject == BaselineReject::Spread ? "SPREAD" : "NONE";
    char b[360];
    snprintf(b,sizeof(b),
      "{\"event\":\"%s\",\"reason\":\"%s\",\"boot_id\":\"%016llX\","
      "\"event_serial\":%lu,\"t\":%lu,"
      "\"opened_ms\":%lu,\"closed_ms\":%lu,\"prior_gap_ms\":%lu,"
      "\"raw\":%d,\"baseline\":%d,\"candidate\":%d,\"peak\":%d,"
      "\"spread\":%u,\"polarity\":%u,\"pwm\":%u,"
      "\"accepted\":%lu,\"rejected\":%lu,\"event_drop\":%lu,\"diag_drop\":%lu,\"diag_pub_skip\":%lu}",
      event,why,(unsigned long long)bootId,(unsigned long)hd.serial,
      (unsigned long)hd.t,(unsigned long)hd.d.openedMs,
      (unsigned long)hd.d.closedMs,(unsigned long)hd.d.priorGapMs,
      (int)hd.raw,(int)hd.d.baseline,(int)hd.d.candidate,(int)hd.d.peak,
      (unsigned)hd.d.spread,(unsigned)hd.d.polarity,(unsigned)hd.d.pwm,
      (unsigned long)hall.accepted(),(unsigned long)hall.rejected(),
      (unsigned long)hallEventDrops,(unsigned long)hallDecisionDrops,
      (unsigned long)hallDecisionPubSkips);
    if (pubQ && uxQueueSpacesAvailable(pubQ) > 8) pub(T_ACQ_DIAG,b,false);
    else ++hallDecisionPubSkips;
  }
  static uint32_t handledEventDrops=0;
  if (hallEventDrops!=handledEventDrops && navigator.positionKnown()) {
    navigator.haltForLoss();
    withdraw("HALL EVENT QUEUE OVERFLOW. Position is not known. Declare it.");
    handledEventDrops = hallEventDrops;
  }
  serviceRamp(); serviceIna(); serviceIr(); serviceStatus();
  delay(2);
}
