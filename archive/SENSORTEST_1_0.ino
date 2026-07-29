/*
 * ============================================================================
 * SENSORTEST_1_0  —  "Locomotive, tell me what you see."
 * ============================================================================
 *
 * A measuring instrument, not a navigator.
 *
 * PURPOSE
 *   Establish the per-marker error rate of the Hall detector, now that the
 *   median baseline has replaced the gated IIR tracker. Every downstream
 *   design question depends on this one number and nobody has measured it:
 *
 *     error rate    LOST within 200 markers    markers to reacquire
 *          5%                 0%                       18
 *         10%                 2%                       27
 *         20%                35%                       70
 *         27%                82%                      162
 *
 *   At 5% the SOLONAV architecture works untouched. At 20% it cannot work at
 *   all, whatever is done to the lost-handling. The whole design rests on a
 *   quantity measured once, on twenty markers, on one station approach.
 *
 * WHY THIS SKETCH CONTAINS NO MAP
 *   The locomotive must not score itself. Every previous measurement compared
 *   each reading against an `expected` value derived from the odometer -- so a
 *   single MISSED marker desynchronised the odometer and made every subsequent
 *   reading score as wrong. One error became a hundred. That is why the 27%
 *   figure was only ever an upper bound.
 *
 *   Three failures are indistinguishable from inside the locomotive and want
 *   completely different fixes:
 *
 *     wrong polarity   read S where the magnet is N   -> threshold / classify
 *     missed marker    no event where a magnet exists -> sensitivity / speed
 *     spurious marker  event where no magnet exists   -> noise / debounce
 *
 *   Only offline alignment against the known sequence can separate them. So
 *   this sketch reports the raw observation and nothing else. The operator
 *   knows the map; the comparison happens in post.
 *
 * WHAT IS DELIBERATELY ABSENT
 *   No DNA map. No odometer. No `expected`. No confidence. No navigation, no
 *   stations, no governor, no automatic PWM of any kind. The throttle answers
 *   only to the operator. Nothing in this sketch can stop the train.
 *
 * CARRIED OVER UNCHANGED (both field-proven 2026-07-26/27)
 *   * median baseline  -- delta within a few counts for a whole session,
 *                         never froze, unaffected while sitting 90 counts
 *                         inside a magnet
 *   * Hall task        -- 1-4 ms task gaps while the main loop stalled 20 s
 *
 * SEQUENCE NUMBERS
 *   Every event carries `seq`, incrementing without gaps. If the log shows
 *   seq 41 followed by seq 43, MQTT dropped a message -- the sensor did not
 *   miss a marker. Without this the aligner would score network loss as
 *   sensor error, which is the same class of mistake this sketch exists to
 *   avoid.
 *
 * USE
 *   Manual operation only. Drive the loco over the section under test at a
 *   steady speed, several passes, both directions. Note the starting marker
 *   and direction for your own records -- the sketch neither knows nor needs
 *   them. Then:
 *
 *     mosquitto_sub -h <broker> -t 'ngr/marker/+/event' -v > run.log
 *     python3 align_markers.py run.log --dir CW
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "LocoConfig.h"

#define SKETCH_NAME  "SENSORTEST_1_0"
#define MQTT_BROKER  "192.168.68.142"
#define MQTT_PORT    1883

#define HALL_PIN      33
#define ADC_SAMPLES    8

// ===========================================================================
// TYPES — above every function; the Arduino IDE hoists prototypes.
// ===========================================================================
struct MarkerEvent {
  uint32_t      seq;
  uint8_t       polarity;      // 1 = N, 0 = S
  int           peak;          // counts from baseline, of the opening pole
  int           oppositePeak;  // the other pole's excursion, for merged-pair study
  uint16_t      durationMs;
  int16_t       baselineDrift; // baseline movement during the event
  int           baselineAt;
  int           rawAtTrigger;
  unsigned long detectedAtMs;  // captured at detection, not at publication
};

// ===========================================================================
// SENSOR — median baseline
// ===========================================================================
#define MEDIAN_WINDOW     128
#define MEDIAN_SAMPLE_MS  500UL
#define CALIBRATION_MS   2000UL

static volatile int baselineCounts = 0;      // written by task, read by loop
static int      northEnter=0, northExit=0, southEnter=0, southExit=0;
static int16_t  medRing[MEDIAN_WINDOW];
static uint8_t  medIndex=0;
static bool     medPrimed=false;
static unsigned long medLastPushMs=0;

static void recomputeThresholds(){
  northEnter = baselineCounts + HALL_DEADBAND_COUNTS + HALL_ENTRY_MARGIN_COUNTS;
  northExit  = baselineCounts + HALL_DEADBAND_COUNTS;
  southEnter = baselineCounts - HALL_DEADBAND_COUNTS - HALL_ENTRY_MARGIN_COUNTS;
  southExit  = baselineCounts - HALL_DEADBAND_COUNTS;
}

static int readAveragedADC(){
  uint32_t s=0; for(uint8_t i=0;i<ADC_SAMPLES;i++) s+=analogRead(HALL_PIN);
  return (int)(s/ADC_SAMPLES);
}

static int medianOfRing(){
  int16_t t[MEDIAN_WINDOW];
  memcpy(t,medRing,sizeof(t));
  for(int i=1;i<MEDIAN_WINDOW;i++){
    int16_t k=t[i]; int j=i-1;
    while(j>=0 && t[j]>k){ t[j+1]=t[j]; j--; }
    t[j+1]=k;
  }
  return (int)t[MEDIAN_WINDOW/2];
}

static void primeMedian(int seed){
  for(int i=0;i<MEDIAN_WINDOW;i++) medRing[i]=(int16_t)seed;
  medIndex=0; medPrimed=true; medLastPushMs=millis();
}

static void updateBaseline(int raw,unsigned long now){
  if(!medPrimed) primeMedian(raw);
  if(now-medLastPushMs < MEDIAN_SAMPLE_MS) return;
  medLastPushMs=now;
  medRing[medIndex]=(int16_t)raw;
  medIndex=(uint8_t)((medIndex+1)%MEDIAN_WINDOW);
  int m=medianOfRing();
  if(m!=baselineCounts){ baselineCounts=m; recomputeThresholds(); }
}

// ===========================================================================
// DETECTOR — reports, judges nothing
// ---------------------------------------------------------------------------
// Polarity comes from the OPENING pole, so a merged N/S pair cannot be
// arbitrated into the wrong answer. The opposite pole's peak is reported
// alongside, so merged pairs are identifiable in post rather than being
// silently resolved here.
//
// The only rejection is the debounce floor, because an event shorter than that
// is electrically not a magnet. Weak events are REPORTED, not dropped -- a
// weak reading that turns out to be correct is exactly the evidence needed to
// decide whether HALL_MIN_PEAK_DELTA is set too high.
// ===========================================================================
#define EVENT_EXIT_HOLD_MS  20UL
#define EVENT_FLOOR_MS      40UL

static QueueHandle_t eventQueue=nullptr;
static volatile unsigned long queueDrops=0, floorRejects=0;
static volatile unsigned long taskMaxGapMs=0, taskLastRunMs=0;
static volatile int lastRaw=0;
static volatile uint32_t seqCounter=0;

static bool     evActive=false;
static uint8_t  evOpenPole=0;
static int      evPeakN=0, evPeakS=0, evStartBaseline=0, evTriggerRaw=0;
static unsigned long evStartMs=0, evReturnMs=0;

static void detectorSample(){
  int raw=readAveragedADC();
  unsigned long now=millis();
  lastRaw=raw;

  updateBaseline(raw,now);          // every sample, in or out of an event

  int n=max(0,raw-baselineCounts), s=max(0,baselineCounts-raw);

  if(!evActive){
    if(raw>=northEnter || raw<=southEnter){
      evActive=true; evStartMs=now; evReturnMs=0;
      evStartBaseline=baselineCounts; evTriggerRaw=raw;
      evPeakN=n; evPeakS=s;
      evOpenPole=(raw>=northEnter)?1:0;
    }
    return;
  }

  evPeakN=max(evPeakN,n); evPeakS=max(evPeakS,s);

  if(raw<=northExit && raw>=southExit){
    if(evReturnMs==0) evReturnMs=now;
    if(now-evReturnMs>=EVENT_EXIT_HOLD_MS){
      unsigned long dur=now-evStartMs;
      evActive=false;
      if(dur<EVENT_FLOOR_MS){ floorRejects++; return; }
      MarkerEvent e;
      e.seq           = ++seqCounter;
      e.polarity      = evOpenPole;
      e.peak          = evOpenPole?evPeakN:evPeakS;
      e.oppositePeak  = evOpenPole?evPeakS:evPeakN;
      e.durationMs    = (uint16_t)min(dur,(unsigned long)65535);
      e.baselineDrift = (int16_t)(baselineCounts-evStartBaseline);
      e.baselineAt    = baselineCounts;
      e.rawAtTrigger  = evTriggerRaw;
      e.detectedAtMs  = evStartMs;
      if(eventQueue && xQueueSend(eventQueue,&e,0)!=pdTRUE) queueDrops++;
    }
  } else {
    evReturnMs=0;
  }
}

static void hallTask(void*){
  unsigned long prev=millis();
  for(;;){
    unsigned long now=millis();
    unsigned long gap=now-prev;
    if(gap>taskMaxGapMs) taskMaxGapMs=gap;
    prev=now; taskLastRunMs=now;
    detectorSample();
    vTaskDelay(1);
  }
}

// ===========================================================================
// MOTOR — operator authority only. Nothing here is automatic.
// ===========================================================================
static inline void pwmAttachCompat(){ ledcAttach(MOTOR_PWM_PIN,PWM_FREQUENCY,PWM_RESOLUTION); }
static inline void pwmWriteCompat(int v){ ledcWrite(MOTOR_PWM_PIN,constrain(v,0,255)); }

static int  motorDirection=DIRECTION_FORWARD;
static int  commandedPwm=0, actualPwm=0;
static bool estopped=false;
static unsigned long lastPwmStepMs=0;
#define PWM_STEP_MS 150UL

static inline void applyDirectionPin(){
  digitalWrite(MOTOR_DIR_PIN,(motorDirection==DIRECTION_FORWARD)?HIGH:LOW);
}
static bool motorIsMoving(){ return actualPwm>0 || commandedPwm>0; }

static void servicePwmRamp(){
  if(estopped || motorDirection==DIRECTION_NEUTRAL){
    commandedPwm=0; actualPwm=0; pwmWriteCompat(0); return;
  }
  unsigned long now=millis();
  if(now-lastPwmStepMs<PWM_STEP_MS) return;
  lastPwmStepMs=now;
  if(actualPwm<commandedPwm) actualPwm++;
  else if(actualPwm>commandedPwm) actualPwm--;
  else return;
  pwmWriteCompat(actualPwm);
}

// ===========================================================================
// MQTT
// ===========================================================================
static WiFiClient   espClient;
static PubSubClient mqtt(espClient);
static char T_ONLINE[64],T_EVENT[64],T_STAT[64],T_BOOT[64];
static char T_CMD_THROTTLE[64],T_CMD_DIR[64],T_CMD_ESTOP[64],T_CMD_MARK[64];

static void buildTopics(){
  const char* id=LOCO_NAME;
  snprintf(T_ONLINE,64,"ngr/marker/%s/online",id);
  snprintf(T_EVENT ,64,"ngr/marker/%s/event" ,id);
  snprintf(T_STAT  ,64,"ngr/marker/%s/stat"  ,id);
  snprintf(T_BOOT  ,64,"ngr/marker/%s/boot"  ,id);
  snprintf(T_CMD_THROTTLE,64,"ngr/marker/%s/cmd/throttle" ,id);
  snprintf(T_CMD_DIR     ,64,"ngr/marker/%s/cmd/direction",id);
  snprintf(T_CMD_ESTOP   ,64,"ngr/marker/%s/cmd/estop"    ,id);
  snprintf(T_CMD_MARK    ,64,"ngr/marker/%s/cmd/mark"     ,id);
}

static void pub(const char* t,const char* m,bool r=false){
  if(mqtt.connected()) mqtt.publish(t,m,r);
}

static void drainEvents(){
  MarkerEvent e;
  while(eventQueue && xQueueReceive(eventQueue,&e,0)==pdTRUE){
    char b[288];
    snprintf(b,sizeof(b),
      "{\"seq\":%lu,\"t\":%lu,\"pol\":\"%c\",\"peak\":%d,\"opp\":%d,\"ms\":%u,"
      "\"drift\":%d,\"baseline\":%d,\"trigger\":%d,\"pwm\":%d,\"dir\":\"%s\"}",
      (unsigned long)e.seq,e.detectedAtMs,e.polarity?'N':'S',
      e.peak,e.oppositePeak,e.durationMs,e.baselineDrift,
      e.baselineAt,e.rawAtTrigger,actualPwm,
      motorDirection==DIRECTION_FORWARD?"FWD":
        (motorDirection==DIRECTION_REVERSE?"REV":"NEU"));
    pub(T_EVENT,b);
    Serial.printf("[MARK] %s\n",b);
  }
}

static unsigned long lastStatMs=0, loopMaxGapMs=0;
static void publishStat(){
  unsigned long now=millis();
  if(now-lastStatMs<1000UL) return;
  lastStatMs=now;
  char b[288];
  snprintf(b,sizeof(b),
    "{\"t\":%lu,\"raw\":%d,\"baseline\":%d,\"delta\":%d,\"seq\":%lu,"
    "\"floor_rejects\":%lu,\"queue_drops\":%lu,\"loop_max_gap_ms\":%lu,"
    "\"task_max_gap_ms\":%lu,\"task_age_ms\":%lu,\"pwm\":%d,\"open\":%d}",
    now,(int)lastRaw,baselineCounts,(int)lastRaw-baselineCounts,
    (unsigned long)seqCounter,floorRejects,queueDrops,
    loopMaxGapMs,(unsigned long)taskMaxGapMs,(unsigned long)(now-taskLastRunMs),
    actualPwm,evActive?1:0);
  pub(T_STAT,b);
  loopMaxGapMs=0; taskMaxGapMs=0;
}

static void publishBoot(){
  char b[288];
  snprintf(b,sizeof(b),
    "{\"sketch\":\"%s\",\"loco\":\"%s\",\"deadband\":%d,\"entry_margin\":%d,"
    "\"min_peak\":%d,\"floor_ms\":%lu,\"exit_hold_ms\":%lu,"
    "\"baseline\":\"median_%d_at_%lums\"}",
    SKETCH_NAME,LOCO_NAME,(int)HALL_DEADBAND_COUNTS,(int)HALL_ENTRY_MARGIN_COUNTS,
    (int)HALL_MIN_PEAK_DELTA,EVENT_FLOOR_MS,EVENT_EXIT_HOLD_MS,
    MEDIAN_WINDOW,MEDIAN_SAMPLE_MS);
  pub(T_BOOT,b,true);
}

static void onMqtt(char* topic,byte* payload,unsigned int len){
  char msg[64]; unsigned n=min(len,(unsigned)63);
  memcpy(msg,payload,n); msg[n]=0;

  if(!strcmp(topic,T_CMD_THROTTLE)){
    if(!estopped) commandedPwm=constrain(atoi(msg),0,255);
  }
  else if(!strcmp(topic,T_CMD_DIR)){
    if(motorIsMoving()){ Serial.println("[CMD] direction refused: moving"); return; }
    motorDirection=constrain(atoi(msg),0,2);
    applyDirectionPin();
    Serial.printf("[CMD] direction=%d\n",motorDirection);
  }
  else if(!strcmp(topic,T_CMD_ESTOP)){
    estopped=(atoi(msg)!=0);
    if(estopped){ commandedPwm=0; actualPwm=0; pwmWriteCompat(0); }
  }
  else if(!strcmp(topic,T_CMD_MARK)){
    // Operator annotation. Drop a labelled fence into the log -- start of a
    // pass, the marker you are standing next to, anything worth aligning to.
    char b[128];
    snprintf(b,sizeof(b),"{\"seq\":%lu,\"t\":%lu,\"mark\":\"%s\"}",
             (unsigned long)seqCounter,millis(),msg);
    pub(T_EVENT,b);
    Serial.printf("[MARK] %s\n",b);
  }
}

static unsigned long nextMqttTryMs=0;
static void serviceNetwork(){
  if(WiFi.status()!=WL_CONNECTED) return;
  if(mqtt.connected()){ mqtt.loop(); return; }
  unsigned long now=millis();
  if(now<nextMqttTryMs) return;
  nextMqttTryMs=now+5000UL;
  if(mqtt.connect(LOCO_NAME,T_ONLINE,0,true,"0")){
    pub(T_ONLINE,"1",true);
    publishBoot();
    mqtt.subscribe(T_CMD_THROTTLE); mqtt.subscribe(T_CMD_DIR);
    mqtt.subscribe(T_CMD_ESTOP);    mqtt.subscribe(T_CMD_MARK);
    Serial.println("[NET] MQTT connected");
  }
}

// ===========================================================================
static void calibrate(){
  Serial.println("[CAL] 2 s baseline — keep clear of magnets");
  unsigned long t0=millis(); uint32_t sum=0,n=0;
  while(millis()-t0<CALIBRATION_MS){ sum+=readAveragedADC(); n++; delay(5); }
  baselineCounts = n?(int)(sum/n):readAveragedADC();
  primeMedian(baselineCounts);
  recomputeThresholds();
  Serial.printf("[CAL] baseline=%d  Nent=%d Nex=%d Sex=%d Sent=%d\n",
                baselineCounts,northEnter,northExit,southExit,southEnter);
}

void setup(){
  Serial.begin(115200); delay(300);
  Serial.printf("\n[BOOT] %s — %s\n",SKETCH_NAME,LOCO_NAME);
  Serial.println("[BOOT] observation only. No map, no navigation, no automatic throttle.");

  analogReadResolution(12);
  pinMode(HALL_PIN,INPUT);
  pinMode(MOTOR_DIR_PIN,OUTPUT);
  applyDirectionPin();
  pwmAttachCompat(); pwmWriteCompat(0);

  buildTopics();
  calibrate();

  eventQueue=xQueueCreate(64,sizeof(MarkerEvent));
  if(!eventQueue){ Serial.println("[FATAL] queue alloc failed"); while(1) delay(1000); }
  if(xTaskCreatePinnedToCore(hallTask,"hallTask",4096,nullptr,2,nullptr,0)!=pdPASS){
    Serial.println("[FATAL] hall task creation failed"); while(1) delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID,WIFI_PASS);
  mqtt.setServer(MQTT_BROKER,MQTT_PORT);
  mqtt.setSocketTimeout(2);
  mqtt.setCallback(onMqtt);
  mqtt.setBufferSize(1024);

  Serial.println("[BOOT] ready.");
}

static unsigned long loopPrevMs=0;
void loop(){
  unsigned long now=millis();
  unsigned long gap=now-loopPrevMs;
  if(gap>loopMaxGapMs) loopMaxGapMs=gap;
  loopPrevMs=now;

  serviceNetwork();
  drainEvents();
  servicePwmRamp();
  publishStat();
}
