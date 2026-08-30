/*
 * ============================================================================
 * NAVI_ONE 0.1  —  Ninobur Garden Railway navigation, built from the ground up
 * ============================================================================
 * Development. NOT FIELD ACCEPTED.
 *
 * One target. One advance. One strike.
 *
 * Built 2026-08-29 at the operator's direction: "Make the simplest and most
 * reliable model possible. The constraints are it must correctly identify
 * magnets and reject non magnet signals. An error stops the locomotive. One
 * strike rule."
 *
 * ----------------------------------------------------------------------------
 * THE SHAPE OF IT
 * ----------------------------------------------------------------------------
 *   HallCapture       GPIO 33 at 1 kHz -> a Passage. Judges nothing.
 *   MagnetRecognizer  "Is this a magnet, and is it new?"  POSITION-FREE.
 *   Navigator         "Is it the one the map says comes next?"  Owns navMm.
 *   RouteMap          the surveyed truth. A declaration, per decision 0056.
 *
 * The recognizer never learns where the locomotive is, so a corrupted position
 * cannot corrupt a shape judgement. The navigator never sees a waveform. That
 * split is CODEX's, from NAVI_FRESH, and it is the best idea in any of the
 * three predecessors.
 *
 * ----------------------------------------------------------------------------
 * WHAT IS DELIBERATELY ABSENT
 * ----------------------------------------------------------------------------
 * No offsets. No scoring committee. No adoption. No evidence ring. No
 * quarantine. No phantom rejection. No NO_QUORUM. No suffix rescue. No
 * velocity model. No dead reckoning. No motion gate and no PWM anywhere in the
 * accept path. And no "silent magnet" -- the term and the concept are banned,
 * because it was the alibi that let a navigator skip over track it had not
 * travelled.
 *
 * IR observes on GPIO 34 and publishes. It has no vote, no verdict, and no
 * path to navMm. It has failed silently in both directions -- undercounting up
 * to 39%, overcounting 3-8x -- and an instrument that can be wrong in silence
 * may witness but not adjudicate.
 *
 * ----------------------------------------------------------------------------
 * TRANSPORT
 * ----------------------------------------------------------------------------
 * The Hall task is pinned and never blocks: every publish only enqueues, and a
 * separate network task owns the radio. On 2026-07-29 a main-loop stall in an
 * earlier lineage overflowed the event queue and destroyed 67 marker events.
 * ============================================================================
 */
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

#include "LocoConfig.h"
#include "credentials.h"
#include "RouteMap.h"
#include "MagnetRecognizer.h"
#include "Navigator.h"
#include "HallCapture.h"

using namespace navi_one;

#define SKETCH_NAME "NAVI_ONE_0_1"

// Types used in function signatures must appear before the Arduino
// prototype generator's insertion point, which is just after the includes.
struct PubMsg { char topic[72]; char payload[420]; bool retain; };
struct CmdMsg { char topic[72]; char payload[64]; };

static const char*   MQTT_BROKER = "192.168.68.142";
static const uint16_t MQTT_PORT  = 1883;

static constexpr uint8_t HALL_PIN = 33;      // ADC1_CH5
static constexpr uint8_t IR_PIN   = 34;      // ADC1_CH6, input-only, no pull
static constexpr uint8_t I2C_SDA  = 21, I2C_SCL = 22;

static constexpr uint8_t  AUTO_CRUISE_PWM = 90;
static constexpr uint16_t RAMP_UP_MS   = 5600;   // passenger-gentle
static constexpr uint16_t RAMP_DOWN_MS = 2800;
// Manual throttle is paced PER STEP, as QUORUM did: the feel of the controls
// must not change because the navigator did. 0 -> 90 takes ~13.5 s up.
static constexpr uint16_t MANUAL_STEP_UP_MS   = 150;
static constexpr uint16_t MANUAL_STEP_DOWN_MS = 200;
static constexpr float    LOW_VOLTAGE_V = 14.4f;

// ---------------------------------------------------------------------------
// LAYER 1/2 — acquisition and recognition. Both live on the Hall task.
// ---------------------------------------------------------------------------
static CaptureConfig captureCfg = {
  /*entryMargin*/ (int16_t)(HALL_DEADBAND_COUNTS + HALL_ENTRY_MARGIN_COUNTS),  // 38
  /*exitMargin */ (int16_t)HALL_DEADBAND_COUNTS,                               // 25
  /*exitHoldMs */ 8, /*floorMs*/ 40, /*baselineMs*/ 25, /*primeMs*/ 2000
};
static HallCapture<512> capture(captureCfg);
static RecognizerConfig recCfg;              // 0.34 / 0.13 / 200 ms, all measured
static MagnetRecognizer recognizer(recCfg);
static Navigator        navigator(recognizer);

// One judged passage, crossing to the loop thread. The waveform does NOT cross:
// the recognizer already ran, on the task that captured it.
struct Judged {
  uint32_t openedAtMs, closedAtMs;
  uint16_t peak; uint8_t polarity;
  uint8_t  outcome; uint8_t isMagnet;
  float    ratio, residual; uint8_t shapeTested; uint32_t gapMs; uint16_t gain;
};
static QueueHandle_t judgedQ = nullptr;
static volatile bool recognizerResetRequest = false;

// ---------------------------------------------------------------------------
// IR OBSERVER — GPIO 34. Publishes. Never votes. (decisions 0055, 0057)
// ---------------------------------------------------------------------------
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
static int8_t sessionDir = 0;
static uint32_t rampStartMs = 0; static int rampFrom = 0, rampTo = 0; static uint16_t rampMs = 0;
static Adafruit_INA219 ina219; static bool inaReady=false;
static float busV=0, busA=0, busW=0;

static void writePwm(int v){
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(MOTOR_PWM_PIN, v);
#else
  ledcWrite(PWM_CHANNEL, v);
#endif
}
static void requestPwm(int target, uint16_t ms){
  rampFrom = actualPwm; rampTo = target; rampMs = ms; rampStartMs = millis();
  commandedPwm = target;
}
static void serviceRamp(){
  if (estopped || lowVoltage) { actualPwm = 0; writePwm(0); return; }
  uint32_t el = millis() - rampStartMs;
  int v = (rampMs == 0 || el >= rampMs) ? rampTo
        : rampFrom + (int)((int32_t)(rampTo - rampFrom) * (int32_t)el / (int32_t)rampMs);
  actualPwm = v;
  digitalWrite(MOTOR_DIR_PIN, motorDirection ? HIGH : LOW);
  writePwm(v);
}

// ---------------------------------------------------------------------------
// TRANSPORT — publishes only enqueue. The network task owns the radio.
// ---------------------------------------------------------------------------
static QueueHandle_t pubQ = nullptr, cmdQ = nullptr;
static WiFiClient wifiClient; static PubSubClient mqtt(wifiClient);
static char T[24][72];
enum { T_ONLINE=0,T_NAV,T_MARKER,T_ALERT,T_IR,T_STAT,T_BOOT,T_WARN,
       T_ST_AUTO,T_ST_ESTOP,T_ST_THR,T_ST_DIR,T_ST_SESSDIR,T_ST_STARTMM,
       T_ST_NAVREADY,T_ST_LOWV,T_ST_STARTINT,T_V,T_A,T_W,T_CNT };

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
  topic(T_V,"telem/voltage");
  topic(T_A,"telem/current");          topic(T_W,"telem/power");
}
static void pub(int t,const char* payload,bool retain=false){
  if(!pubQ) return;
  PubMsg m; strlcpy(m.topic,T[t],sizeof(m.topic));
  strlcpy(m.payload,payload,sizeof(m.payload)); m.retain=retain;
  xQueueSend(pubQ,&m,0);
}
static void warn(const char* text){ pub(T_WARN,text,true); Serial.printf("[WARN] %s\n",text); }

// ---------------------------------------------------------------------------
// RULE 5 — one strike. An IDENTITY failure means the map and the world
// disagree; the navigator's job at that moment is to stop saying where it is.
// In MANUAL the operator has authority and the throttle is left alone.
// ---------------------------------------------------------------------------
static void oneStrike(const Judged& j){
  const NavStatus& s = navigator.status();
  char w[200];
  snprintf(w,sizeof(w),
    "WRONG MAGNET at MM%03u: expected %c at MM%03u, read %c. Position is not "
    "known. Declare it.", s.navMm, poleChar(polarityAt(s.target)), s.target,
    poleChar(j.polarity));
  warn(w);
  if (autoRunning) { autoRunning = false; requestPwm(0, RAMP_DOWN_MS); }
}

static void publishNav(const char* event,const Judged* j,Ruling r){
  const NavStatus& s = navigator.status();
  char b[420];
  if (j) {
    snprintf(b,sizeof(b),
      "{\"event\":\"%s\",\"nav_state\":\"%s\",\"mm\":%u,\"tgt\":%u,"
      "\"landmark\":\"%s\",\"dir\":\"%s\",\"ruling\":\"%s\",\"why\":\"%s\","
      "\"obs\":\"%c\",\"expected\":\"%c\",\"peak\":%u,\"ratio\":%.3f,"
      "\"resid\":%.4f,\"shape\":%u,\"gap_ms\":%lu,\"gain\":%u,"
      "\"trust\":\"%s\",\"seq_at\":%u,\"adv\":%lu,\"ref\":%lu,\"notmag\":%lu}",
      event, s.state==NavState::Unset?"UNSET":"DECLARED", s.navMm, s.target,
      landmarkAt(r==Ruling::Advanced ? s.navMm : s.target),
      s.navDir>0?"CW":(s.navDir<0?"CCW":"UNSET"),
      rulingName(r), outcomeName((Outcome)j->outcome),
      poleChar(j->polarity),
      poleChar(polarityAt(r==Ruling::Advanced ? s.navMm : s.target)),
      j->peak, (double)j->ratio, (double)j->residual, j->shapeTested,
      (unsigned long)j->gapMs, j->gain,
      trustName(s.trust), s.seqAt,
      (unsigned long)s.advances,(unsigned long)s.refusals,(unsigned long)s.notMagnets);
    pub(T_MARKER,b,false);
  } else {
    snprintf(b,sizeof(b),
      "{\"event\":\"%s\",\"nav_state\":\"%s\",\"mm\":%u,\"tgt\":%u,\"dir\":\"%s\","
      "\"trust\":\"%s\",\"adv\":%lu,\"ref\":%lu}",
      event, s.state==NavState::Unset?"UNSET":"DECLARED", s.navMm, s.target,
      s.navDir>0?"CW":(s.navDir<0?"CCW":"UNSET"), trustName(s.trust),
      (unsigned long)s.advances,(unsigned long)s.refusals);
  }
  pub(T_NAV,b,true);
  Serial.printf("[NAV] %s\n",b);
}

static void declarePosition(uint8_t mm,int8_t dir,const char* interval){
  navigator.declare(mm,dir);
  recognizerResetRequest = true;
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
  for(;;){
    unsigned long now = millis();
    if (recognizerResetRequest) { recognizer.reset(); recognizerResetRequest = false; }
    if (capture.sample(now,(int16_t)analogRead(HALL_PIN))) {
      const Passage& p = capture.passage();
      Verdict v = recognizer.examine(p);
      Judged j{ p.openedAtMs,p.closedAtMs,p.peakCounts,p.polarity,
                (uint8_t)v.outcome,(uint8_t)v.isMagnet,
                v.amplitudeRatio,v.residual,(uint8_t)v.shapeTested,v.gapMs,v.gain };
      if (judgedQ) xQueueSend(judgedQ,&j,0);
    }
    irSample(now);
    vTaskDelayUntil(&wake,1);
  }
}

static void onMqtt(char* t,byte* payload,unsigned int len){
  CmdMsg c; strlcpy(c.topic,t,sizeof(c.topic));
  unsigned n = len < sizeof(c.payload)-1 ? len : sizeof(c.payload)-1;
  memcpy(c.payload,payload,n); c.payload[n]=0;
  if (cmdQ) xQueueSend(cmdQ,&c,0);
}
static void networkTask(void*){
  char sub[72];
  for(;;){
    if (WiFi.status()==WL_CONNECTED && !mqtt.connected()) {
      char id[48]; snprintf(id,sizeof(id),"NAVI_ONE_%s",LOCO_NAME);
      if (mqtt.connect(id,T[T_ONLINE],0,true,"0")) {
        mqtt.publish(T[T_ONLINE],"1",true);
        const char* cmds[]={"cmd/auto","cmd/estop","cmd/throttle","cmd/direction",
                            "cmd/session_direction","cmd/start_mm","cmd/start_interval"};
        for (auto c: cmds){ snprintf(sub,72,"ngr/loco/%s/%s",LOCO_NAME,c); mqtt.subscribe(sub); }
        snprintf(sub,72,"ngr/dispatcher/cmd/go/%s",LOCO_NAME);   mqtt.subscribe(sub);
        snprintf(sub,72,"ngr/dispatcher/cmd/stop/%s",LOCO_NAME); mqtt.subscribe(sub);
        mqtt.subscribe("ngr/dispatcher/cmd/estop");
        Serial.println("[NET] MQTT connected");
      }
    }
    mqtt.loop();
    PubMsg m;
    while (pubQ && xQueueReceive(pubQ,&m,0)==pdTRUE)
      if (mqtt.connected()) mqtt.publish(m.topic,m.payload,m.retain);
    vTaskDelay(10);
  }
}

// ---------------------------------------------------------------------------
static void handleCommand(const CmdMsg& c){
  const char* leaf = strrchr(c.topic,'/'); leaf = leaf ? leaf+1 : c.topic;
  const bool dispatcher = strstr(c.topic,"/dispatcher/") != nullptr;
  int n = atoi(c.payload);
  if (!strcmp(leaf,"estop") || (dispatcher && !strcmp(leaf,"estop"))) {
    estopped = n != 0;
    if (estopped){ autoRunning=false; requestPwm(0,0); warn("ESTOP"); } else warn("");
  } else if (!strcmp(leaf,"session_direction")) {
    int8_t d = (!strcasecmp(c.payload,"CW")) ? +1 : (!strcasecmp(c.payload,"CCW") ? -1 : 0);
    if (d){ sessionDir=d; navigator.setDirection(d); recognizerResetRequest=true;
            pub(T_ST_SESSDIR,d>0?"CW":"CCW",true); publishNav("DIRECTION",nullptr,Ruling::NoPosition); }
  } else if (!strcmp(leaf,"start_interval")) {
    // "AAA-BBB" -- the two magnets the locomotive stands between, GEOMETRIC and
    // always ascending, because that is what the console's slider produces and
    // what the operator can see on the ground. It is NOT a travel-order pair.
    // Which end it is leaving depends on which way it faces: running CW the
    // next marker is B, so position is A; running CCW the next is A, so
    // position is B.
    if (sessionDir==0){ warn("START_INTERVAL REFUSED: set session_direction first"); return; }
    int a=-1,b=-1;
    if (sscanf(c.payload,"%d-%d",&a,&b)!=2){ warn("START_INTERVAL REFUSED: expected AAA-BBB"); return; }
    if (a<0||a>=ROUTE_N||b<0||b>=ROUTE_N){ warn("START_INTERVAL REFUSED: out of range"); return; }
    if (nextMarker((uint8_t)a,+1)!=(uint8_t)b){ warn("START_INTERVAL REFUSED: markers not adjacent"); return; }
    declarePosition(sessionDir>0?(uint8_t)a:(uint8_t)b, sessionDir, c.payload);
  } else if (!strcmp(leaf,"start_mm")) {
    if (sessionDir==0){ warn("START_MM REFUSED: set session_direction first"); return; }
    if (n<0 || n>=ROUTE_N){ warn("START_MM REFUSED: out of range"); return; }
    declarePosition((uint8_t)n,sessionDir,nullptr);
  } else if (!strcmp(leaf,"auto")) {
    if (n==0){ autoEnrolled=false; autoRunning=false; requestPwm(0,RAMP_DOWN_MS); }
    else if (!navigator.positionKnown()) warn("AUTO REFUSED: declare position first");
    else if (estopped||lowVoltage)       warn("AUTO REFUSED: safety interlock");
    else autoEnrolled=true;
    char v[4]; snprintf(v,sizeof(v),"%u",autoEnrolled?1:0); pub(T_ST_AUTO,v,true);
  } else if (!strcmp(leaf,"go") || (dispatcher && strstr(c.topic,"/go/"))) {
    if (!autoEnrolled)                    warn("GO REFUSED: not enrolled");
    else if (!navigator.positionKnown())  warn("GO REFUSED: no position");
    else if (estopped||lowVoltage)        warn("GO REFUSED: safety interlock");
    else { autoRunning=true; requestPwm(AUTO_CRUISE_PWM,RAMP_UP_MS); warn(""); }
  } else if (!strcmp(leaf,"stop") || (dispatcher && strstr(c.topic,"/stop/"))) {
    autoRunning=false; requestPwm(0,RAMP_DOWN_MS);
  } else if (!strcmp(leaf,"throttle")) {
    if (!autoEnrolled) {
      int t = constrain(n,0,255);
      int steps = t > actualPwm ? (t - actualPwm) : (actualPwm - t);
      uint16_t per = t > actualPwm ? MANUAL_STEP_UP_MS : MANUAL_STEP_DOWN_MS;
      uint32_t ms = (uint32_t)steps * per;
      requestPwm(t, (uint16_t)(ms > 65535 ? 65535 : ms));
    }
  } else if (!strcmp(leaf,"direction")) {
    if (!autoEnrolled && actualPwm<=SAFE_DIRECTION_CHANGE_PWM && n!=1) motorDirection=(n==2);
  }
}

static void serviceIna(){
  static uint32_t last=0; if(!inaReady || millis()-last<5000) return; last=millis();
  busV=ina219.getBusVoltage_V(); busA=ina219.getCurrent_mA()/1000.0f; busW=ina219.getPower_mW()/1000.0f;
  bool low = busV>0.1f && busV<LOW_VOLTAGE_V;
  if (low && !lowVoltage){ lowVoltage=true; autoRunning=false; requestPwm(0,RAMP_DOWN_MS); warn("LOW VOLTAGE"); }
  if (!low && busV>=LOW_VOLTAGE_V) lowVoltage=false;
  char v[16];
  snprintf(v,sizeof(v),"%.2f",busV); pub(T_V,v,true);
  snprintf(v,sizeof(v),"%.2f",busA); pub(T_A,v,true);
  snprintf(v,sizeof(v),"%.2f",busW); pub(T_W,v,true);
  snprintf(v,sizeof(v),"%u",lowVoltage?1:0); pub(T_ST_LOWV,v,true);
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
  char b[400];
  snprintf(b,sizeof(b),
    "{\"level\":\"%s\",\"reason\":\"STATUS\",\"loco\":\"%s\",\"uptime_ms\":%lu,"
    "\"nav\":\"%s\",\"mm\":%u,\"tgt\":%u,\"dir\":\"%s\",\"trust\":\"%s\","
    "\"moving\":%u,"
    "\"pwm\":%d,\"auto\":%u,\"running\":%u,\"estop\":%u,\"lowvolt\":%u,"
    "\"adv\":%lu,\"ref\":%lu,\"notmag\":%lu,\"baseline\":%ld,\"floor_rej\":%lu}",
    navigator.positionKnown()?"CLEAR":"UNSET", LOCO_NAME,(unsigned long)millis(),
    s.state==NavState::Unset?"UNSET":"DECLARED", s.navMm, s.target,
    s.navDir>0?"CW":(s.navDir<0?"CCW":"UNSET"), trustName(s.trust),
    (actualPwm>0)?1u:0u,
    actualPwm, autoEnrolled?1:0, autoRunning?1:0, estopped?1:0, lowVoltage?1:0,
    (unsigned long)s.advances,(unsigned long)s.refusals,(unsigned long)s.notMagnets,
    (long)capture.baseline(),(unsigned long)capture.floorRejects());
  pub(T_ALERT,b,false);
  char v[8];
  snprintf(v,sizeof(v),"%d",actualPwm); pub(T_ST_THR,v,true);
  snprintf(v,sizeof(v),"%u",motorDirection?2:0); pub(T_ST_DIR,v,true);
  snprintf(v,sizeof(v),"%u",estopped?1:0); pub(T_ST_ESTOP,v,true);
  snprintf(v,sizeof(v),"%u",navigator.positionKnown()?1:0); pub(T_ST_NAVREADY,v,true);
}

void setup(){
  Serial.begin(115200); delay(300);
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
  buildTopics();
  judgedQ=xQueueCreate(16,sizeof(Judged));
  pubQ  =xQueueCreate(48,sizeof(PubMsg));
  cmdQ  =xQueueCreate(16,sizeof(CmdMsg));
  Serial.printf("[BOOT] %s — %s\n",SKETCH_NAME,LOCO_NAME);
  Serial.printf("[CAL] 2 s baseline — keep clear of magnets\n");
  xTaskCreatePinnedToCore(hallTask,"hall",4096,nullptr,3,nullptr,0);
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID,WIFI_PASS);
  mqtt.setServer(MQTT_BROKER,MQTT_PORT); mqtt.setCallback(onMqtt); mqtt.setBufferSize(600);
  xTaskCreatePinnedToCore(networkTask,"net",8192,nullptr,1,nullptr,1);
  char b[300];
  snprintf(b,sizeof(b),
    "{\"sketch\":\"%s\",\"loco\":\"%s\",\"entry\":%d,\"exit\":%d,\"floor_ms\":%d,"
    "\"amp_floor\":%.2f,\"resid_ceil\":%.2f,\"guard_ms\":%lu,\"seq_n\":%d,"
    "\"offsets\":0,\"quorum\":0,\"velocity_model\":0,\"motion_gate\":0,\"ir_votes\":0}",
    SKETCH_NAME,LOCO_NAME,(int)captureCfg.entryMargin,(int)captureCfg.exitMargin,
    (int)captureCfg.floorMs,(double)recCfg.amplitudeFloor,(double)recCfg.residualCeiling,
    (unsigned long)recCfg.guardMs,(int)SEQ_N);
  pub(T_BOOT,b,true);
  Serial.println("[BOOT] ready. session_direction, then start_mm, then auto, then GO.");
}

void loop(){
  CmdMsg c; while (cmdQ && xQueueReceive(cmdQ,&c,0)==pdTRUE) handleCommand(c);

  Judged j;
  while (judgedQ && xQueueReceive(judgedQ,&j,0)==pdTRUE) {
    Passage p; p.openedAtMs=j.openedAtMs; p.closedAtMs=j.closedAtMs;
    p.peakCounts=j.peak; p.polarity=j.polarity;
    Verdict v; v.outcome=(Outcome)j.outcome; v.isMagnet=j.isMagnet;
    v.amplitudeRatio=j.ratio; v.residual=j.residual;
    v.shapeTested=j.shapeTested; v.gapMs=j.gapMs; v.gain=j.gain;
    Ruling r = navigator.judge(p,v);
    switch (r) {
      case Ruling::Advanced:   publishNav("ADVANCE",&j,r); break;
      case Ruling::WrongMagnet:publishNav("REFUSED",&j,r); oneStrike(j); break;
      case Ruling::NotAMagnet: publishNav("NOT_A_MAGNET",&j,r); break;
      default:                 publishNav("NO_POSITION",&j,r); break;
    }
  }
  serviceRamp(); serviceIna(); serviceIr(); serviceStatus();
  delay(2);
}
