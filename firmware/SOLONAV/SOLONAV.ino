/*
 * ============================================================================
 * SOLONAV_2_21  —  Ninobur Garden Railway single-locomotive navigation
 * ============================================================================
 *
 * A locomotive that knows where it has been, where it is, and what is
 * possible next.
 *
 * ---------------------------------------------------------------------------
 * WHY THIS IS A REWRITE AND NOT ANOTHER REVISION
 * ---------------------------------------------------------------------------
 * The previous lineage asked "where am I?" from scratch at every marker: take
 * the last twelve polarity readings, look them up, announce a position with
 * total confidence. History was the junior partner. When the pattern matcher
 * disagreed with the odometer, the matcher won — which is how a locomotive at
 * MM133 came to believe, at certainty 1.000, that it was at MM105 travelling
 * the other way.
 *
 * This inverts it. Position comes from history and the map. A magnet reading
 * is a vote, not a verdict. One bad read costs a confidence point; it cannot
 * teleport the train.
 *
 * The second change is that the DETECTOR NO LONGER MAKES DECISIONS. It reports
 * what it saw, with quality attached, and never rejects anything on its own
 * authority. Rejection needs context — what marker is due, which way we are
 * going, how fast — and the detector has none of that. Every rejection rule it
 * was ever given eventually threw away something real.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE 2026-07-26 DATA ESTABLISHED
 * ---------------------------------------------------------------------------
 *   * The median baseline works. `delta` stayed within a few counts for a
 *     whole session and the tracker never once froze. Carried over unchanged.
 *   * The Hall task works. Task gaps of 1–4 ms while the main loop stalled for
 *     20 seconds. Carried over unchanged.
 *   * Marker reading is excellent at full signal: 20 of 20, then 18 of 19.
 *   * Position was still lost, and the recovery jumped 26 markers.
 *   * Every state machine that wedged did so because a state had no exit.
 *     Four separate instances in one week. Every state here has one.
 *
 * ---------------------------------------------------------------------------
 * SCOPE — SINGLE LOCOMOTIVE
 * ---------------------------------------------------------------------------
 * Multi-train awareness was prototyped here in 2.5 and has been REMOVED. It
 * does not belong in a sketch named SOLONAV. The peer table, occupancy
 * comparison and traffic-dependent lost policy are deferred until the CTO3
 * architecture requires them; the reasoning and the review findings that
 * produced them are preserved in docs/CTO3_DESIGN_NOTES.md.
 *
 * Solo on a closed loop, position uncertainty has no physical consequence.
 * The train cannot hit anything and cannot leave the track. Being lost costs
 * station stops and nothing else. Behaviour here is sized to that fact.
 *
 * ---------------------------------------------------------------------------
 * FOUR LAYERS, ONE CONTRACT EACH
 * ---------------------------------------------------------------------------
 *   1  SENSOR      raw ADC + median baseline. The baseline itself cannot
 *                  stick; an OPEN EVENT still can, if the signal never
 *                  settles inside the exit band. No guard is added for
 *                  that -- no log has yet shown it. Watch event_open_ms.
 *   2  DETECTOR    threshold crossing -> event {polarity, ms, peak, drift}.
 *                  Reports everything. Judges nothing.
 *   3  NAVIGATOR   odometer is truth. Map predicts. Reading votes.
 *                  LOST is a real state with a real exit.
 *   4  OPERATIONS  station profile and PWM ramp. Consumes position and
 *                  confidence; never touches the sensor.
 *
 * Deliberately absent: baseline freeze, tracker recovery state, stuck-event
 * guard, settling qualifier, dominance classifier, track window, stability
 * test, alternating service. Every one of those existed to prop up a detector
 * that had to be right on its own. Give it a navigator that can absorb being
 * wrong and they are all unnecessary.
 * ============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <pgmspace.h>
#include "LocoConfig.h"

#define SKETCH_NAME "SOLONAV_2_21"

// Broker lives here, not in LocoConfig.h — same as the previous lineage.
#define MQTT_BROKER "192.168.68.142"
#define MQTT_PORT   1883

// ===========================================================================
// TYPES — must precede every function.
// The Arduino IDE auto-generates function prototypes and inserts them near the
// top of the .ino, ABOVE anything declared further down. A type used in a
// signature but defined mid-file produces "does not name a type" even though
// the code reads correctly. Keep all structs and enums in this block.
// ===========================================================================
enum MapDirection : int8_t { MAP_UNSET=0, MAP_CW=1, MAP_CCW=-1 };

struct StationDefinition {
  const char* name;
  uint8_t centerMm;
  uint8_t zonePwm;    // held from M-5 through M: the station approach speed
  uint8_t finalPwm;   // M+1, the speed the stop is made from
  int8_t  stopOffset; // marker past centre at which the zero ramp begins
};

struct MarkerEvent {
  uint8_t       polarity;      // 1=N 0=S
  int           peak;
  uint16_t      durationMs;
  int16_t       baselineDrift; // counts the baseline moved during the event
  unsigned long detectedAtMs;  // captured at detection, not at processing
};

// Outbound and inbound MQTT messages cross the loop<->network task boundary as
// values on a queue, so no locomotive-state thread ever touches the radio and
// the network never touches locomotive state. Topic pointers in PubMsg are safe
// to store: the T_* topic strings are static char arrays that never move.
// payload[512] covers today's largest payload (navPublishState, 384) and the
// v3.0 QUORUM payload (512).
struct PubMsg { const char* topic; char payload[512]; bool retain; };
struct CmdMsg { char topic[64]; char payload[128]; };

enum NavState     : uint8_t { NAV_UNSET=0, NAV_TRACKING, NAV_LOST };
enum StationPhase : uint8_t { ST_IDLE=0, ST_APPROACH, ST_FINAL, ST_RAMP, ST_DWELL, ST_DEPART };

// ===========================================================================
// HARDWARE
// ===========================================================================
#define HALL_PIN            33
#define ADC_SAMPLES          8
#define MOTOR_DEAD_ZONE_PWM 20

static inline void pwmAttachCompat(){ ledcAttach(MOTOR_PWM_PIN,PWM_FREQUENCY,PWM_RESOLUTION); }
static inline void pwmWriteCompat(int v){ ledcWrite(MOTOR_PWM_PIN,constrain(v,0,255)); }

// ---------------------------------------------------------------------------
// DIRECTION — one source of truth.
//
// 2_1 kept motorDirection and navDir as independent variables, and Codex found
// three ways they could disagree: cmd/direction moved the motor without moving
// the navigator, session_direction flipped the pin under power, and the
// movement check raced the ramp. Two variables for one physical fact.
//
// They are not independent. sessionDir declares which MAP direction the motor
// travels when it is FORWARD; navDir is therefore DERIVED, never assigned.
// Reversing the motor reverses the navigator by construction.
// ---------------------------------------------------------------------------
static int    motorDirection = DIRECTION_FORWARD;
static int8_t sessionDir     = MAP_UNSET;
static int8_t navDir         = MAP_UNSET;   // derived; see applyDirection()

static inline int8_t oppositeDir(int8_t d){
  return d==MAP_CW?MAP_CCW:(d==MAP_CCW?MAP_CW:MAP_UNSET);
}

// ===========================================================================
// MAP  — 171 markers, verified unique at W=10, locked at W=12
// ===========================================================================
#define DNA_N 171

const uint8_t NGR_DNA1[DNA_N] PROGMEM = {
  1,1,0,0,0,1,1,0,0,0,0,1,1,1,1,0,0,1,0,0,
  0,1,0,1,1,1,0,0,1,1,1,1,1,0,1,1,0,0,0,0,
  0,0,1,0,1,0,0,0,1,1,0,1,0,0,0,0,1,0,0,1,
  0,1,0,1,0,1,1,0,0,1,0,1,0,1,1,0,1,1,1,0,
  1,1,1,1,0,0,0,1,1,0,1,1,0,0,1,0,1,1,0,0,
  1,0,0,1,0,0,0,1,1,1,1,1,1,1,0,1,0,0,1,1,
  1,0,0,0,1,0,1,1,0,1,0,1,1,0,0,1,1,0,0,0,
  0,0,1,0,1,1,1,1,0,1,0,1,1,1,0,1,0,1,0,0,
  1,0,1,0,0,0,0,1,1,1,0
};

static const uint16_t spacingMm[DNA_N] PROGMEM = {
  330,340,330,315,325,330,315,300,300,295,
  300,290,300,315,315,325,310,300,300,320,
  315,315,305,300,295,300,300,300,300,315,
  330,320,315,310,300,300,300,300,300,300,
  300,300,300,300,300,300,300,300,300,300,
  300,300,300,300,300,300,300,295,320,300,
  315,320,315,325,315,305,300,305,300,300,
  295,295,300,300,300,300,300,300,300,305,
  300,300,305,300,300,330,300,300,305,300,
  300,300,300,300,300,300,300,300,300,300,
  300,300,295,300,300,300,300,300,305,300,
  300,320,320,300,300,300,300,300,300,300,
  300,300,300,300,300,300,280,300,300,290,
  300,300,300,300,300,300,300,300,300,300,
  300,300,300,300,305,300,305,300,295,300,
  300,300,305,300,300,305,320,290,320,300,
  300,305,330,330,320,325,315,355,330,330,
  330
};

// Per-station speeds. One profile cannot serve a level approach and an uphill
// one: Grillers climbs, and on the 2026-07-27 lap it nearly stalled at the
// common approach speed. Its numbers are raised accordingly.
//
// zonePwm  is held for five markers before the centre, so the locomotive is
//          already settled at approach speed when it reaches the station --
//          not still decelerating through it.
// finalPwm is the speed the stop is made FROM. The zero ramp begins here, so
//          it should be the slowest speed the locomotive holds reliably on
//          that stretch.
// stopOffset is where passengers actually end up, set from observed stops on
// the 2026-07-27 run rather than from geometry. Grillers and Bamboo both
// overran the platform at +2 and were pulled back a marker.
//                          centre  zone  final  stop
static const StationDefinition STATIONS[] = {
  {"Patio",    15,            60,   45,     2},   // observed good
  {"Grillers", 63,            72,   58,     1},   // was +2, stopped past 65 -- too far up
  {"Arches",  107,            60,   45,     2},   // observed good, stops just past 109
  {"Bamboo",  157,            60,   45,     1}    // was +2, stopped past 161, wanted past 159
};
static const uint8_t STATION_COUNT = sizeof(STATIONS)/sizeof(STATIONS[0]);

static inline uint8_t routeMod(int32_t v){ v%=DNA_N; if(v<0) v+=DNA_N; return (uint8_t)v; }
static inline uint8_t dnaAt(uint8_t mm){ return pgm_read_byte(&NGR_DNA1[mm%DNA_N]); }
static inline char    polChar(uint8_t p){ return p?'N':'S'; }
static inline uint8_t nextMm(uint8_t mm,int8_t dir){ return routeMod((int32_t)mm+dir); }
static const char* dirName(int8_t d){ return d==MAP_CW?"CW":(d==MAP_CCW?"CCW":"UNSET"); }

static const char* landmarkAt(uint8_t mm){
  switch(mm%DNA_N){
    case 0:return "Southpoint"; case 15:return "Patio";      case 63:return "Grillers";
    case 72:return "Westpoint"; case 98:return "Northpoint"; case 107:return "Arches";
    case 140:return "Eastpoint";case 157:return "Bamboo";    default:return "";
  }
}

// Signed marker offset from mm to a station centre, along the travel direction.
// Negative = station is ahead.
static int16_t offsetToCentre(uint8_t mm,int8_t dir,uint8_t centre){
  int32_t d = (dir==MAP_CW) ? routeMod((int32_t)mm-centre) : routeMod((int32_t)centre-mm);
  return (d > DNA_N/2) ? (int16_t)(d-DNA_N) : (int16_t)d;
}

// ===========================================================================
// LAYER 1 — SENSOR
// ---------------------------------------------------------------------------
// Median of the last 64 seconds. The operating assumption is that TOTAL magnet
// occupancy across the whole window stays under half -- not merely that each
// individual magnet does, since several crossings in one window add up.
// At observed spacing and speed that holds with wide margin. Drift moves every sample, so
// it follows immediately. No gating, no freeze, no recovery state, no way to
// wedge. Proven over a full session on 2026-07-26: delta within a few counts,
// TRACKING throughout, including while sitting 90 counts inside a magnet.
// ===========================================================================
#define MEDIAN_WINDOW     128
#define MEDIAN_SAMPLE_MS  500UL
#define CALIBRATION_MS   2000UL

static volatile int baselineCounts = 0;   // written by task, read by loop
static int      northEnter=0, northExit=0, southEnter=0, southExit=0;
static int16_t  medRing[MEDIAN_WINDOW];
static uint8_t  medIndex   = 0;
static bool     medPrimed  = false;
static unsigned long medLastPushMs = 0;

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
  for(int i=1;i<MEDIAN_WINDOW;i++){                 // 128 elements every 500 ms
    int16_t k=t[i]; int j=i-1;                      // is nothing on a 240 MHz core
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
// LAYER 2 — DETECTOR
// ---------------------------------------------------------------------------
// Contract: a threshold crossing produces an event. Polarity is decided by the
// OPENING pole, so a merged N/S pair cannot be arbitrated into the wrong
// answer. Peak and duration ride along as quality. Baseline drift during the
// event rides along so the navigator can discount a reading taken against a
// moving reference.
//
// The only thing the detector refuses is an event shorter than the debounce
// floor, because that is electrically not a magnet. Everything else it reports
// and lets the navigator judge. Weak reads are TAGGED, not dropped — a weak
// reading that matches prediction is still good evidence.
// ===========================================================================
#define EVENT_EXIT_HOLD_MS   20UL
#define EVENT_FLOOR_MS       40UL
#define HALL_TASK_TICK_MS     1

static QueueHandle_t eventQueue = nullptr;
static volatile unsigned long queueDrops=0, floorRejects=0;
static volatile unsigned long taskMaxGapMs=0, taskLastRunMs=0;
static volatile int lastRaw=0;

static bool     evActive=false;
static uint8_t  evOpenPole=0;
static int      evPeakN=0, evPeakS=0, evStartBaseline=0;
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
      evStartBaseline=baselineCounts;
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
      e.polarity      = evOpenPole;
      e.peak          = evOpenPole?evPeakN:evPeakS;
      e.durationMs    = (uint16_t)min(dur,(unsigned long)65535);
      e.baselineDrift = (int16_t)(baselineCounts-evStartBaseline);
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
    vTaskDelay(HALL_TASK_TICK_MS);
  }
}

// ===========================================================================
// CROSS-LAYER STATE
// ---------------------------------------------------------------------------
// The navigator needs three things from the operations layer: whether AUTO is
// driving, whether a station currently owns the throttle, and the two speeds it
// may request. Declared here, above their use, rather than forward-declared
// piecemeal further down.
//
// This is the only coupling between layers 3 and 4, and it runs one way: the
// navigator may ask operations for a speed; operations never reaches back into
// navigation state.
// ===========================================================================
static const uint8_t  CRUISE_PWM       = 100;   // raised so the LOST drop to 60 is obvious
static const uint8_t  STATION_ZONE_PWM = 60;
static const uint16_t NORMAL_STEP_MS   = 150;

static bool         autoEnrolled=false, autoRunning=false;
// `estopped` is written from BOTH threads: the loop-thread command handler and,
// as of v2.20, directly from the network task's MQTT callback so that engaging
// E-stop never waits on the command queue (see onMqttEnqueue). Single volatile
// bool, one writer path at a time, read every loop() pass by servicePwmRamp --
// safe on ESP32 without a lock.
static volatile bool estopped=false;
static StationPhase stPhase=ST_IDLE;

static void requestPwm(int target,uint16_t stepMs);
static int  cruiseForPosition();   // section cruise speed; defined in LAYER 4

// ===========================================================================
// LAYER 3 — NAVIGATOR
// ---------------------------------------------------------------------------
// Dead reckoning with periodic fixes. This is the part that was missing.
//
//   The odometer ALWAYS advances on a marker event. Position comes from
//   history plus the map. The reading is compared against what the map says
//   should be there, and the result adjusts CONFIDENCE — it does not replace
//   position.
//
//   A single wrong polarity costs one confidence point. It cannot move the
//   train. Only sustained contradiction does, and then the answer is LOST, not
//   a different position.
//
//   Re-acquisition from LOST requires a pattern match that (a) is unique,
//   (b) lies in the declared direction, and (c) SURVIVES THE NEXT MARKER —
//   the candidate must correctly predict one more reading before it is
//   believed. A lucky match on corrupted data almost never predicts.
// ===========================================================================
#define CONFIDENCE_MAX      10
#define CONFIDENCE_FLOOR     0
// A DECLARED position is operator-supplied ground truth -- the most
// authoritative input the system has. A REACQUIRED one is something the
// locomotive inferred from twelve readings and could have got lucky on. They
// deserve different trust and had been sharing one constant.
//
// On the 2026-07-27 run this cost a station: position was declared at MM050,
// three long events followed immediately, and two clean disagreements took
// confidence from 4 to 0. The locomotive went LOST at MM054 and buffered 34
// markers straight through Grillers, which therefore never armed. Once
// running it sits at 8-10 and shrugs off the same errors.
//
// At 10 it takes five clean disagreements to doubt the operator, which is a
// genuine run of bad reads rather than two unlucky ones at launch.
#define CONFIDENCE_DECLARED    10
#define CONFIDENCE_REACQUIRED   4
#define CONFIDENCE_GOOD      6   // reporting threshold only -- gates nothing

// ---------------------------------------------------------------------------
// LOST BUDGET
//
// A lost locomotive that keeps running is a collision mechanism, not an
// inconvenience: under CTO the follower's spacing is computed from positions
// that are no longer true, and the failure mode is a rear-end strike. But it
// cannot simply stop either, because reacquisition needs markers and markers
// need motion.
//
// So LOST is a BOUNDED SEARCH. Drop to creep immediately, and if the map has
// not come back within the budget, halt and hold. Speed is earned by evidence;
// with no evidence the budget is what buys the chance to earn some.
// ---------------------------------------------------------------------------
#define STATUS_BROADCAST_MS  1000UL

// A lost locomotive may have missed markers as well as misread them, so it can
// be AHEAD of its own dead reckoning, not only behind it. This is the forward
// margin added when publishing the occupancy bound.
#define LOST_FRONT_MARGIN_MARKERS 5
#define ALERT_REPEAT_MS        1000UL

#define DNA_W               12   // 171 unique windows verified at W=10
#define WEAK_PEAK_COUNTS    (HALL_MIN_PEAK_DELTA)
#define DRIFT_SUSPECT_COUNTS 15

static NavState navState      = NAV_UNSET;
static uint8_t  navMm         = 0;
static int8_t   navConfidence = 0;
static uint32_t navMarkers=0, navAgree=0, navDisagree=0, navLostCount=0;

static uint8_t  dnaBuf[DNA_W];
static uint8_t  dnaBufLen=0;
static bool     pendingValid=false;
static uint8_t  pendingMm=0;
// A candidate must predict REACQ_CONFIRMS readings in a row before it is
// believed. One binary prediction passes half the time on garbage; two cuts
// surviving false matches roughly fourfold, at the cost of one extra marker.
// Reset on any failed prediction so a candidate never carries credit across a
// miss.
#define REACQ_CONFIRMS 2
static uint8_t  pendingConfirms=0;

static unsigned long lastMarkerMs=0;
static int16_t       lastOdomDisagreement=0;   // published on REACQUIRED
static int16_t       lastReacqOffset=0;        // accepted candidate's offset from navMm; +ve = odometer behind
static unsigned long lostSinceMs=0;            // LOST budget
static uint16_t      lostMarkers=0;

// The last marker whose reading actually AGREED with the map. Everything after
// it is inference. This -- not the dead-reckoned navMm -- is the position a
// following locomotive can trust, and the anchor of the uncertainty envelope.
static uint8_t       lastConfirmedMm=0;
static unsigned long lastConfirmedMs=0;
static uint16_t      markersSinceConfirmed=0;
static bool          haveConfirmed=false;
static uint16_t      lastSegmentDt=0;

static void navPublishState(const char* ev,const MarkerEvent* e);

// Track distance spanned by n markers travelled from mm in direction dir.
static uint32_t spanMm(uint8_t mm,int8_t dir,uint16_t n){
  uint32_t t=0; uint8_t cur=mm;
  for(uint16_t i=0;i<n && i<DNA_N;i++){
    uint8_t nxt=nextMm(cur,dir);
    t += (dir==MAP_CW) ? pgm_read_word(&spacingMm[cur]) : pgm_read_word(&spacingMm[nxt]);
    cur=nxt;
  }
  return t;
}

// Retained alert. A follower or dispatcher connecting late still sees it.
static void publishAlert(const char* level,const char* reason);

// Declares POSITION only. Direction has exactly one assignment point,
// applyDirection(), and this is not it.
static void navDeclare(uint8_t mm){
  navMm=mm; navState=NAV_TRACKING;
  lostMarkers=0;
  lastConfirmedMm=mm; lastConfirmedMs=millis();
  markersSinceConfirmed=0; haveConfirmed=true;
  navConfidence=CONFIDENCE_DECLARED;
  dnaBufLen=0; pendingValid=false;
  navPublishState("DECLARED",nullptr);
}

static void navEnterLost(const char* why){
  navState=NAV_LOST; navConfidence=CONFIDENCE_FLOOR;
  dnaBufLen=0; pendingValid=false; pendingConfirms=0; navLostCount++;
  lostSinceMs=millis(); lostMarkers=0;
  // Slow to station speed as a VISIBLE SIGNAL, not as a safety measure. Solo
  // on a closed loop there is nothing to protect against; a lost train can run
  // all day and the only cost is battery. Sixty on the open main is
  // unmistakable from across the garden. It does NOT stop -- reacquisition
  // needs markers, and markers need motion.
  //
  // AUTO only. In manual the operator has the throttle and the navigator does
  // not take it. Navigation observes always; navigation acts only in AUTO.
  if(autoRunning) requestPwm(STATION_ZONE_PWM,NORMAL_STEP_MS);
  Serial.printf("[NAV] LOST (%s) -- slowing to station speed\n",why);
  navPublishState("LOST",nullptr);
  publishAlert("LOST",why);
}

static void dnaPush(uint8_t pol){
  if(dnaBufLen<DNA_W){ dnaBuf[dnaBufLen++]=pol; return; }
  for(uint8_t i=1;i<DNA_W;i++) dnaBuf[i-1]=dnaBuf[i];
  dnaBuf[DNA_W-1]=pol;
}

// Reacquisition search is CONSTRAINED to a window around the odometer. The
// odometer keeps counting while LOST, so navMm is already the expected answer;
// searching the whole ring turned a 12-bit pattern into a 1-in-171 lottery.
// Measured: false-match probability drops from 4.17% to 0.27% at a +-5 window.
// DNA_W stays 12 -- shortening the pattern would give most of that gain back.
#define REACQ_WINDOW_MARKERS 5

// Unique window match, searched only near navMm and only in the declared
// direction. Returns the mm of the LAST marker in the window, or 255 if not
// unique within the window.
static uint8_t dnaMatch(int8_t dir){
  if(dnaBufLen<DNA_W || dir==MAP_UNSET) return 255;
  uint8_t found=255, count=0;
  for(int8_t off=-REACQ_WINDOW_MARKERS; off<=REACQ_WINDOW_MARKERS; off++){
    uint8_t end   = routeMod((int32_t)navMm + off);
    uint8_t start = routeMod((int32_t)end - (int32_t)dir*(DNA_W-1));
    bool ok=true; uint8_t mm=start;
    for(uint8_t i=0;i<DNA_W;i++){
      if(dnaAt(mm)!=dnaBuf[i]){ ok=false; break; }
      mm=nextMm(mm,dir);
    }
    if(ok){ if(++count>1) return 255; found=end; }
  }
  return count==1 ? found : 255;
}

static void navOnMarker(const MarkerEvent& e){
  navMarkers++;
  // Timed from DETECTION, not from when loop() got round to draining the
  // queue. A stalled loop draining several events at once used to report
  // near-zero segment times. Diagnostic only -- no PWM authority. (Codex)
  lastSegmentDt = lastMarkerMs ? (uint16_t)min(e.detectedAtMs-lastMarkerMs,(unsigned long)65535) : 0;
  lastMarkerMs = e.detectedAtMs;

  // Quality flags. A weak or drifting reading still counts, but it is a
  // weaker vote — it can confirm but it cannot, by itself, condemn.
  const bool weak     = (e.peak < (int)WEAK_PEAK_COUNTS + 15);
  const bool drifting = (abs((int)e.baselineDrift) > DRIFT_SUSPECT_COUNTS);
  const bool soft     = weak || drifting;

  if(navState==NAV_LOST){
    // The odometer advances here too. Codex was right that freezing it while
    // LOST contradicted the architecture -- and keeping it running is not
    // merely tidier, it gives reacquisition something to be checked against.
    navMm = nextMm(navMm,navDir);

    if(markersSinceConfirmed<65535) markersSinceConfirmed++;
    lostMarkers++;
    dnaPush(e.polarity);

    if(pendingValid){
      // A candidate is on probation: it must predict this reading, and it must
      // do so REACQ_CONFIRMS times running before it is believed. navMm and
      // pendingMm advance in lockstep, so the odometer-vs-DNA offset established
      // when the window matched is constant across the confirmations.
      uint8_t predicted=nextMm(pendingMm,navDir);
      if(dnaAt(predicted)==e.polarity){
        pendingMm=predicted;
        if(++pendingConfirms>=REACQ_CONFIRMS){
          // off: the accepted candidate's offset from the dead-reckoned navMm,
          // positive when the odometer is running BEHIND -- the only direction
          // the measured data shows it errs. Computed before navMm is moved.
          int16_t off = offsetToCentre(pendingMm,navDir,navMm);
          lastReacqOffset      = off;
          lastOdomDisagreement = offsetToCentre(navMm,navDir,pendingMm); // odom vs DNA
          navMm=pendingMm; navState=NAV_TRACKING;
          navConfidence=CONFIDENCE_REACQUIRED;
          pendingValid=false; pendingConfirms=0;
          lostMarkers=0;
          lastConfirmedMm=navMm; lastConfirmedMs=millis();
          markersSinceConfirmed=0; haveConfirmed=true;
          // Back to cruise, unless a station approach already owns the throttle.
          if(autoRunning && stPhase==ST_IDLE) requestPwm(cruiseForPosition(),NORMAL_STEP_MS);
          publishAlert("CLEAR","REACQUIRED");
          Serial.printf("[NAV] REACQUIRED mm=%u dir=%s off=%d\n",
                        navMm,dirName(navDir),(int)off);
          navPublishState("REACQUIRED",&e);
        }else{
          // Correct so far, but not yet REACQ_CONFIRMS times. Still on probation.
          navPublishState("CANDIDATE",&e);
        }
      }else{
        pendingValid=false; pendingConfirms=0;    // candidate failed its test
        // Codex: don't throw away a fresh window just because the old
        // candidate failed. Promote it immediately instead of losing a marker.
        uint8_t again=dnaMatch(navDir);
        if(again!=255){ pendingMm=again; pendingValid=true; pendingConfirms=0; navPublishState("CANDIDATE",&e); }
        else            navPublishState("CANDIDATE_REJECTED",&e);
      }
      return;
    }

    uint8_t cand=dnaMatch(navDir);
    if(cand!=255){
      pendingMm=cand; pendingValid=true; pendingConfirms=0;  // must survive REACQ_CONFIRMS markers
      navPublishState("CANDIDATE",&e);
    }else{
      navPublishState("BUFFERING",&e);
    }
    return;
  }

  if(navState!=NAV_TRACKING) return;

  // --- dead reckoning: the odometer always advances ---
  navMm = nextMm(navMm,navDir);
  uint8_t expected = dnaAt(navMm);
  dnaPush(e.polarity);

  const int8_t confBefore = navConfidence;
  if(e.polarity==expected){
    navAgree++;
    if(navConfidence<CONFIDENCE_MAX) navConfidence++;
    lastConfirmedMm=navMm; lastConfirmedMs=millis();
    markersSinceConfirmed=0; haveConfirmed=true;
    navPublishState("AGREE",&e);
  }else{
    navDisagree++;
    if(markersSinceConfirmed<65535) markersSinceConfirmed++;
    // A soft reading that disagrees costs one point; a clean reading that
    // disagrees costs two. Either way the train stays where the map says.
    navConfidence -= soft ? 1 : 2;
    navPublishState("DISAGREE",&e);
    if(navConfidence<=CONFIDENCE_FLOOR){ navEnterLost("sustained contradiction"); return; }
  }

  // Confidence no longer gates anything -- these crossings are reported purely
  // as evidence about read quality. A stop made while low is worth correlating
  // with where it actually landed.
  if(confBefore>=CONFIDENCE_GOOD && navConfidence<CONFIDENCE_GOOD)
    navPublishState("CONFIDENCE_LOW",&e);
  else if(confBefore<CONFIDENCE_GOOD && navConfidence>=CONFIDENCE_GOOD)
    navPublishState("CONFIDENCE_RECOVERED",&e);
}

// ===========================================================================
// LAYER 4 — OPERATIONS
// ---------------------------------------------------------------------------
// Prescribed PWM by location. Nothing here calculates speed from marker
// timing; Hall dt is diagnostic only and has no motor authority.
//
// EVERY STATE HAS AN EXIT. The 2026-07-26 run wedged because a station armed
// for Patio, the position jumped past the centre, and the machine sat in
// STOP_SERVICE forever — silently skipping every station for the rest of the
// session. Two changes prevent it:
//   * arming triggers on a RANGE, not on exact equality with one marker;
//   * overshooting the centre publishes MISSED and returns to IDLE.
// ===========================================================================
// ---------------------------------------------------------------------------
// RAMPS ARE DURATIONS, NOT STEP RATES
//
// 2_10 specified ramps in milliseconds per PWM count, so how long a ramp took
// depended on where it started. The zero ramp at 300 ms/step from PWM 61 took
// EIGHTEEN SECONDS -- fourteen marker intervals at the measured 1.26 s spacing.
// The locomotive could not stop at any of the four stations; the overshoot
// escape fired at +6 every time, with PWM still at 47-69.
//
// Measured on the 2026-07-27 lap: marker interval 1.26 s median, 243 mm/s.
// Ramps are now given a DURATION and the step rate is derived, so a ramp takes
// the same wall time from any starting speed.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// SECTION CRUISE SPEED
//
// Some stretches must be driven at a different speed than the open main. This
// is a property of the LAYOUT, not of any station, so it lives in its own
// table: a section carries its OWN cruise speed, which REPLACES CRUISE_PWM
// while the locomotive is inside it.
//
// This is a whole target, not an additive boost. The previous boost mechanism
// had two faults Codex flagged: it wrote commandedPwm outside requestPwm(),
// creating a second PWM authority, and because it changed mid-ramp it broke the
// guarantee that a ramp is a fixed duration. A section speed is just another
// target requested through the normal path, so neither can recur —
// requestPwm()/requestPwmOver() stay the only writers of commandedPwm, and a
// ramp always runs to completion.
//
// MM065-MM080 CW is the climb out of Grillers. Toby stalled there on
// 2026-07-27 with three coaches at the common cruise of 100. Clockwise only:
// counter-clockwise the same stretch is a descent, where more speed would be
// exactly wrong.
//
// AUTO only. In manual the operator has the throttle and nothing modifies it;
// every request path that consults cruiseForPosition() runs under AUTO.
// ---------------------------------------------------------------------------
struct GradeSegment {
  uint8_t fromMm, toMm;   // inclusive, in the direction given
  int8_t  dir;            // MAP_CW or MAP_CCW
  uint8_t cruisePwm;      // cruise speed WITHIN this section
};
static const GradeSegment GRADES[] = {
  { 65, 80, MAP_CW, 120 }   // climb out of Grillers: stalled at 100 with three coaches
};
static const uint8_t GRADE_COUNT = sizeof(GRADES)/sizeof(GRADES[0]);

static const uint16_t APPROACH_RAMP_MS = 700;   // ~half a marker: settled before the next
static const uint16_t FINAL_RAMP_MS    = 700;
static const uint16_t STOP_RAMP_MS     = 2800;  // gentler: 1500 read as a switch opening
static const uint16_t DEPART_RAMP_MS   = 2800;  // acceleration wants longer than deceleration
static const int8_t   APPROACH_START      = -10;
static const int8_t   ZONE_START          = -5;
// (superseded by per-station stopOffset in STATIONS[])
static const int8_t   OVERSHOOT_ABANDON   = 5;   // markers past centre -> MISSED
static const uint32_t DWELL_MS            = 15000UL;
// If M+1 is crossed and M+2 does not arrive within this, the zero ramp starts
// anyway. M+2 stays the PRIMARY trigger, so accurate stops stay accurate; this
// only covers the case where the loco runs out of tractive effort short of it,
// which is what left it standing past Grillers on 2026-07-26. Expected M+1 to
// M+2 interval at final approach is 1-2 s, so 5 s will not fire on a merely
// slow crossing.
static const uint32_t FINAL_M1_TIMEOUT_MS = 5000UL;

// Backstop for ST_APPROACH only. ST_FINAL now has the fallback above, ST_RAMP
// always completes because the ramp is monotonic, and ST_DEPART is exempt --
// a loco accelerating from a stop on clear track has nothing to be rescued
// from, and a timeout there protected against nothing.
static const uint32_t STATION_MAX_PHASE_MS= 120000UL;

// Approach targets are DERIVED from cruise and station speed, not written out.
// Hard-coded values silently stop matching the moment CRUISE_PWM changes; with
// the derivation the ramp always begins at cruise and lands exactly on station
// speed at the zone boundary, whatever those two numbers are.
//
//   step = (CRUISE - ZONE) / (ZONE_START - APPROACH_START)
//   at 100/60 over offsets -10..-6:  92, 84, 76, 68, 60
static int approachTargetForOffset(int16_t o,uint8_t zonePwm){
  if(o <  APPROACH_START) return CRUISE_PWM;
  if(o >= ZONE_START)     return zonePwm;
  const int steps = (int)ZONE_START - (int)APPROACH_START;      // 5
  const int idx   = (int)(o - APPROACH_START) + 1;              // 1..5
  return (int)CRUISE_PWM - ((int)CRUISE_PWM - (int)zonePwm)*idx/steps;
}

// M and M+1 only. The 2026-07-26 run showed PWM 25 is below the tractive
// floor on at least one approach — the loco reached 25 a marker early and
// stopped short of its own zero-ramp trigger, wedging the station machine.
// The zero ramp now begins from 42 at M+2 instead of stepping through 25.
// (superseded by per-station zonePwm / finalPwm in STATIONS[])

static int8_t        stIndex=-1;
static unsigned long stPhaseEnteredMs=0;
static unsigned long stDwellStartedMs=0;
static unsigned long stMPlus1AtMs=0;      // when M+1 was crossed, 0 if not yet
static unsigned long stDepartBeganMs=0;   // for the slow-departure notice only
static bool          stDepartWarned=false;

// PWM authority: targets normally enter via requestPwm(). The deliberate
// exceptions are servicePwmRamp() (the actuator), setup(), and E-stop.
static int      commandedPwm=0, actualPwm=0;
static uint16_t pwmStepMs=NORMAL_STEP_MS;
static unsigned long lastPwmStepMs=0;


static void stationPublish(const char* ev,int16_t off,const char* note);

// True if the motor is turning OR about to be. Codex found that testing
// actualPwm alone raced the ramp: during acceleration commandedPwm can already
// be high while actualPwm is still under the deadzone, so a direction command
// was accepted a moment before real power arrived in the new direction.
// Any energisation at all, not merely enough to turn the wheels. Codex noted
// the deadzone version did not deliver what the comment claimed; a stopped
// loco has both values at zero anyway, so the stricter test costs nothing and
// makes "no direction-pin write while PWM is applied" literally true.
static bool motorIsMoving(){
  return actualPwm>0 || commandedPwm>0;
}

// The single place direction is applied. Recomputes navDir from sessionDir and
// motorDirection, discards direction-dependent reacquisition evidence if it
// changed, and writes the pin.
static void applyDirection(){
  // NEUTRAL is an interlock, not a third travel direction. It prevents motion
  // (see the clamp in servicePwmRamp) but leaves the navigator's direction
  // alone -- a stationary loco has not changed which way it faces on the map,
  // and unsetting navDir here would reintroduce the frozen-odometer hole.
  if(motorDirection!=DIRECTION_NEUTRAL){
    int8_t derived = (sessionDir==MAP_UNSET) ? MAP_UNSET
                   : (motorDirection==DIRECTION_FORWARD ? sessionDir : oppositeDir(sessionDir));
    if(derived!=navDir){
      navDir=derived;
      dnaBufLen=0; pendingValid=false;    // buffer is direction-dependent
    }
    digitalWrite(MOTOR_DIR_PIN,(motorDirection==DIRECTION_FORWARD)?HIGH:LOW);
  }
}

// The cruise speed for the current position: a section's own cruise while the
// locomotive is inside one, otherwise the open-main CRUISE_PWM. The membership
// arithmetic is the same modular test the old grade boost used, which Codex
// verified correct in both directions.
static int cruiseForPosition(){
  if(navState==NAV_TRACKING && navDir!=MAP_UNSET){
    for(uint8_t i=0;i<GRADE_COUNT;i++){
      const GradeSegment& g=GRADES[i];
      if(g.dir!=navDir) continue;
      // Distance in markers from the segment start, travelling in g.dir.
      int32_t into = (g.dir==MAP_CW) ? (int32_t)routeMod((int32_t)navMm-g.fromMm)
                                     : (int32_t)routeMod((int32_t)g.fromMm-navMm);
      int32_t span = (g.dir==MAP_CW) ? (int32_t)routeMod((int32_t)g.toMm-g.fromMm)
                                     : (int32_t)routeMod((int32_t)g.fromMm-g.toMm);
      if(into<0 || into>span) continue;               // not inside this segment
      return (int)g.cruisePwm;
    }
  }
  return CRUISE_PWM;
}

static void requestPwm(int target,uint16_t stepMs){
  commandedPwm=constrain(target,0,255);
  pwmStepMs=stepMs;
}

// Reach the target in roughly durationMs, whatever the current PWM. Derives
// the step rate from the distance still to travel.
static void requestPwmOver(int target,uint16_t durationMs){
  int t=constrain(target,0,255);
  int delta=abs(t-actualPwm);
  commandedPwm=t;
  pwmStepMs = (delta>0) ? (uint16_t)max(5UL,(unsigned long)durationMs/(unsigned long)delta) : 50;
}

static void servicePwmRamp(){
  // NEUTRAL interlock: no throttle command can produce movement. This is what
  // made an accidental throttle nudge harmless in the previous system, and
  // what makes clearing an E-stop require a deliberate direction selection.
  if(motorDirection==DIRECTION_NEUTRAL){ commandedPwm=0; actualPwm=0; pwmWriteCompat(0); return; }
  // Under E-stop the ramp is HELD AT ZERO, not merely masked at the hardware.
  // Masking allowed actualPwm to climb internally while stopped, so clearing
  // E-stop exposed an already-elevated PWM on the next write. (Codex)
  if(estopped){ commandedPwm=0; actualPwm=0; pwmWriteCompat(0); return; }
  unsigned long now=millis();
  if(now-lastPwmStepMs < pwmStepMs) return;
  lastPwmStepMs=now;
  if(actualPwm<commandedPwm) actualPwm++;
  else if(actualPwm>commandedPwm) actualPwm--;
  else return;
  pwmWriteCompat(actualPwm);
}

static void stationSetPhase(StationPhase p){ stPhase=p; stPhaseEnteredMs=millis(); }

static void stationReset(const char* note){
  stPhase=ST_IDLE; stIndex=-1; stPhaseEnteredMs=millis();
  stationPublish("RESET",999,note);
}

static void serviceStations(){
  // The absolute escape runs FIRST, before any state gate. Codex found it sat
  // below the NAV_TRACKING return, so losing position mid-approach disabled
  // the only autonomous exit and left the phase wedged until an external
  // command arrived. "Every state has an exit" has to mean unconditionally.
  // ST_FINAL has its own physical fallback, ST_DWELL has its own timer, and
  // ST_DEPART is deliberately exempt. This guards ST_APPROACH and ST_RAMP.
  if((stPhase==ST_APPROACH || stPhase==ST_RAMP) &&
     millis()-stPhaseEnteredMs > STATION_MAX_PHASE_MS){
    stationReset("PHASE_TIMEOUT");
    // Only return to cruise if the navigator actually knows where it is.
    // Codex: a timeout firing while LOST would otherwise promote a
    // navigation failure straight back to full speed. Speed is earned by
    // evidence; while lost, hold the reduced approach speed.
    if(autoRunning && navState==NAV_TRACKING) requestPwm(cruiseForPosition(),NORMAL_STEP_MS);
    return;
  }

  // The M+1 fallback runs regardless of navigation state. It is a fact about
  // the motor, not about the map, and a loco that goes LOST during final
  // approach still needs to stop.
  if(stPhase==ST_FINAL && stMPlus1AtMs &&
     millis()-stMPlus1AtMs >= FINAL_M1_TIMEOUT_MS){
    stationSetPhase(ST_RAMP);
    requestPwmOver(0,STOP_RAMP_MS);
    stationPublish("ZERO_RAMP",1,"TRIGGER_M1_TIMEOUT_DID_NOT_REACH_M2");
    return;
  }

  if(!autoRunning || navState!=NAV_TRACKING || navDir==MAP_UNSET) return;

  if(stPhase==ST_IDLE){
    // Range arming. A skipped marker or a position correction can no longer
    // cause a station to be silently missed.
    //
    // There is deliberately NO confidence threshold here. Refusing to arm on a
    // shaky fix meant the locomotive drove past the station rather than
    // stopping in a slightly wrong place -- suppressing the behaviour the
    // sketch exists to produce, and hiding the sensor problem behind it.
    // The asymmetry runs the other way: a stop made on poor evidence is
    // visible and informative, a station silently skipped is neither. It also
    // gated a decision on navConfidence as though it were a probability, when
    // it is a tally of recent agreements. Confidence is now REPORTED with the
    // arming event instead of vetoing it.
    for(uint8_t i=0;i<STATION_COUNT;i++){
      int16_t o=offsetToCentre(navMm,navDir,STATIONS[i].centerMm);
      if(o<=APPROACH_START && o>APPROACH_START-3){
        stIndex=(int8_t)i; stationSetPhase(ST_APPROACH);
        requestPwmOver(approachTargetForOffset(o,STATIONS[i].zonePwm),APPROACH_RAMP_MS);
        stationPublish("ARMED",o,(navConfidence>=CONFIDENCE_GOOD)?"RANGE_ARM_CONF_HIGH":"RANGE_ARM_CONF_LOW");
        return;
      }
    }
    // No station armed: keep cruise following the section map through the
    // normal path. requestPwmOver() remains the sole writer of commandedPwm.
    int want = cruiseForPosition();
    if(commandedPwm != want) requestPwmOver(want, APPROACH_RAMP_MS);
    return;
  }

  const int16_t o=offsetToCentre(navMm,navDir,STATIONS[stIndex].centerMm);

  // Overshoot escape, checked in every non-idle phase before anything else.
  if(o > OVERSHOOT_ABANDON && stPhase!=ST_DWELL && stPhase!=ST_DEPART){
    stationPublish("MISSED",o,"OVERSHOT_CENTRE_RETURNING_TO_IDLE");
    stationReset("MISSED");
    requestPwm(cruiseForPosition(),NORMAL_STEP_MS);
    return;
  }

  switch(stPhase){
    case ST_APPROACH:
      if(o>=APPROACH_START && o<ZONE_START){
        requestPwmOver(approachTargetForOffset(o,STATIONS[stIndex].zonePwm),APPROACH_RAMP_MS);
        stationPublish("APPROACH",o,"DERIVED_CRUISE_TO_ZONE");
      }else if(o>=ZONE_START && o<0){
        requestPwmOver(STATIONS[stIndex].zonePwm,APPROACH_RAMP_MS);
        stationPublish("ZONE_HOLD",o,"HOLD_60");
      }else if(o>=0){
        stationSetPhase(ST_FINAL);
        stMPlus1AtMs=0;
        // Hold approach speed THROUGH the centre. The stop is made after the
        // sensor has passed the station, not while arriving at it.
        requestPwmOver(STATIONS[stIndex].zonePwm,FINAL_RAMP_MS);
        stationPublish("FINAL_APPROACH",o,"LOCATION_TRIGGERED_TARGETS");
      }
      break;

    case ST_FINAL: {
      // Braces required: stopAt is declared here, and without a scope block
      // the later case labels jump past its initialisation, which C++ forbids.
      //
      // Location-triggered, one target per marker. Not a timed interpolation
      // to a single distant target — that is what left the loco at PWM 25 a
      // marker early and out of tractive effort.
      const int8_t stopAt = STATIONS[stIndex].stopOffset;
      if(o>=0 && o<stopAt){
        // M   : still at zone speed, passing the station
        // M+1 : ease to finalPwm -- the speed the stop is made from
        requestPwmOver(o==0?STATIONS[stIndex].zonePwm:STATIONS[stIndex].finalPwm,
                       FINAL_RAMP_MS);
        if(o==1 && stMPlus1AtMs==0) stMPlus1AtMs=millis();  // start the fallback clock
        stationPublish("FINAL_TARGET",o,o==0?"AT_CENTRE_ZONE_SPEED":"M_PLUS_1_FINAL_SPEED");
      }else if(o>=stopAt){
        stationSetPhase(ST_RAMP);
        requestPwmOver(0,STOP_RAMP_MS);
        // Which trigger fired is a fact worth logging: repeated M1_TIMEOUT at
        // one station means that approach profile is too aggressive there.
        stationPublish("ZERO_RAMP",o,"TRIGGER_M2_REACHED");
      }
      break;
    }

    case ST_RAMP:
      if(actualPwm<=0){
        stationSetPhase(ST_DWELL);
        stDwellStartedMs=millis();
        stationPublish("DWELL_BEGIN",o,"FIXED_DWELL");
      }
      break;

    case ST_DWELL:
      if(millis()-stDwellStartedMs>=DWELL_MS){
        stationSetPhase(ST_DEPART);
        stDepartBeganMs=millis(); stDepartWarned=false;
        // Straight to cruise. Station speed exists to arrive accurately; once
        // stopped there is nothing left to be careful about. 2_13 departed at
        // the GLOBAL station speed of 60 -- not even the station's own -- and
        // held it for three markers, which on the Grillers climb with cars
        // attached was not enough to pull the grade. The locomotive crawled,
        // magnet events stretched to four seconds, occupancy of the median
        // window reached 90%, the baseline was corrupted and navigation was
        // lost. A throttle number caused a navigation failure.
        requestPwmOver(cruiseForPosition(),DEPART_RAMP_MS);
        stationPublish("DWELL_COMPLETE",o,"DEPART_TO_CRUISE");
      }
      break;

    case ST_DEPART:
      // No timeout here. A loco accelerating from a stop on clear track has
      // nothing to be rescued from, and a phase reset would only hide a real
      // mechanical fault. If departure is slow, TELL THE OPERATOR and keep
      // trying -- notification, not intervention.
      if(!stDepartWarned && millis()-stDepartBeganMs > 15000UL){
        stDepartWarned=true;
        stationPublish("DEPARTURE_SLOW",o,"NOT_CLEARED_ZONE_IN_15S_CHECK_LOCO");
      }
      if(o>=STATIONS[stIndex].stopOffset+3){
        // Already at cruise; this only releases the station machine so the
        // next one can arm.
        stationPublish("DEPARTURE_COMPLETE",o,"CLEARED_ZONE");
        stationReset("DEPARTED");
      }
      break;

    default: break;
  }
}

// ===========================================================================
// MQTT
// ===========================================================================
static WiFiClient   espClient;
static PubSubClient mqtt(espClient);

// The two doors between loop() and the network task. pub() enqueues onto
// pubQueue (drained and published by networkTask); the MQTT callback enqueues
// onto cmdQueue (drained by serviceCommands() on the loop thread, which owns all
// locomotive state). pubDrops is cumulative; pubQueueHw is the windowed max
// occupancy, reset on each loopstat publish.
static QueueHandle_t pubQueue=nullptr, cmdQueue=nullptr;
static uint32_t      pubDrops=0;
static uint16_t      pubQueueHw=0;
// CHANGE 1 (v2.21) — markers get their OWN publish queue. On 2026-07-30 the
// single 32-slot pubQueue, evicting its OLDEST entry when full, dropped 22,774
// telemetry messages and tore visible holes in the marker stream (MM154->MM145
// in one step). Status is re-sent within a second and its stale value is worth
// evicting; a marker event happens once and cannot be re-derived. They must not
// share a queue. markerPubDrops is cumulative; markerPubHw is the windowed max
// occupancy, both reported in loopstat and reset like their pubQueue twins.
static QueueHandle_t markerPubQueue=nullptr;
static uint32_t      markerPubDrops=0;
static uint16_t      markerPubHw=0;
// Change 4 (v2.20). cmdDrops: inbound commands lost to a full cmdQueue. Written
// on the network task (onMqttEnqueue), read on the loop thread (publishStat) --
// volatile. A dropped command must never be silent; v2.19 ignored the send
// result entirely. pubWindowCount: publish calls in the current loopstat window,
// bumped in pub() and reset each publishStat, reported as pub_per_s. If Change 3
// works this drops from ~12/s to ~1/s parked.
static volatile uint32_t cmdDrops=0;
static volatile uint32_t pubWindowCount=0;

static char T_ONLINE[64],T_NAV[64],T_MARKER[64],T_STATION[64],T_STAT[64],T_BOOT[64],T_ALERT[64];

// ---------------------------------------------------------------------------
// DASHBOARD STATE TOPICS
//
// The Flask console does not merely send commands, it waits for the firmware
// to acknowledge them on these topics before unlocking its controls. 2_8
// dropped them, so the session-direction badge stayed red on "SET SESSION
// DIRECTION TO ENABLE THROTTLE" and the dashboard correctly refused to drive a
// locomotive that had not confirmed it was ready. The firmware was at fault,
// not the dashboard.
//
// Names and payload shapes match SOLONAV_1_x exactly. Fields this sketch no
// longer has (brake, start interval, must-hold-eligible) are published as
// inert constants so the console's parsing does not break.
// ---------------------------------------------------------------------------
static char T_ST_THROTTLE[64],T_ST_DIRECTION[64],T_ST_BRAKE[64],T_ST_ESTOP[64],
            T_ST_AUTO[64],T_ST_SESSDIR[64],T_ST_STARTINT[64],T_ST_STARTMM[64],
            T_ST_MHE[64],T_ST_NAVREADY[64],T_ST_WARNING[64];
static char T_CMD_AUTO[64],T_CMD_GO[64],T_CMD_STOP[64],T_CMD_DIR[64],T_CMD_SESSDIR[64];
static char T_CMD_STARTMM[64],T_CMD_ESTOP[64],T_CMD_THROTTLE[64],T_CMD_STARTINT[64],
            T_CMD_RELEASE[64],T_CMD_FORCELOST[64];

static void buildTopics(){
  const char* id=LOCO_NAME;
  snprintf(T_ONLINE ,64,"ngr/loco/%s/online"        ,id);
  snprintf(T_NAV    ,64,"ngr/loco/%s/state/nav"     ,id);
  snprintf(T_MARKER ,64,"ngr/loco/%s/mm/marker"     ,id);
  snprintf(T_STATION,64,"ngr/loco/%s/state/station" ,id);
  snprintf(T_STAT   ,64,"ngr/loco/%s/state/loopstat",id);
  snprintf(T_BOOT   ,64,"ngr/loco/%s/state/bootid"  ,id);
  snprintf(T_ALERT  ,64,"ngr/loco/%s/alert"         ,id);
  snprintf(T_ST_THROTTLE ,64,"ngr/loco/%s/state/throttle"         ,id);
  snprintf(T_ST_DIRECTION,64,"ngr/loco/%s/state/direction"        ,id);
  snprintf(T_ST_BRAKE    ,64,"ngr/loco/%s/state/brake"            ,id);
  snprintf(T_ST_ESTOP    ,64,"ngr/loco/%s/state/estop"            ,id);
  snprintf(T_ST_AUTO     ,64,"ngr/loco/%s/state/auto"             ,id);
  snprintf(T_ST_SESSDIR  ,64,"ngr/loco/%s/state/session_direction",id);
  snprintf(T_ST_STARTINT ,64,"ngr/loco/%s/state/start_interval"   ,id);
  snprintf(T_ST_STARTMM  ,64,"ngr/loco/%s/state/start_mm"         ,id);
  snprintf(T_ST_MHE      ,64,"ngr/loco/%s/state/must_hold_eligible",id);
  snprintf(T_ST_NAVREADY ,64,"ngr/loco/%s/state/nav_ready"        ,id);
  snprintf(T_ST_WARNING  ,64,"ngr/loco/%s/state/warning"          ,id);
  snprintf(T_CMD_AUTO    ,64,"ngr/loco/%s/cmd/auto"             ,id);
  snprintf(T_CMD_DIR     ,64,"ngr/loco/%s/cmd/direction"        ,id);
  snprintf(T_CMD_SESSDIR ,64,"ngr/loco/%s/cmd/session_direction",id);
  snprintf(T_CMD_STARTMM ,64,"ngr/loco/%s/cmd/start_mm"         ,id);
  snprintf(T_CMD_STARTINT,64,"ngr/loco/%s/cmd/start_interval"   ,id);
  snprintf(T_CMD_RELEASE ,64,"ngr/loco/%s/cmd/dispatcher_release",id);
  snprintf(T_CMD_FORCELOST,64,"ngr/loco/%s/cmd/force_lost"       ,id);
  snprintf(T_CMD_ESTOP   ,64,"ngr/loco/%s/cmd/estop"            ,id);
  snprintf(T_CMD_THROTTLE,64,"ngr/loco/%s/cmd/throttle"         ,id);
  snprintf(T_CMD_GO      ,64,"ngr/dispatcher/cmd/go/%s"         ,id);
  snprintf(T_CMD_STOP    ,64,"ngr/dispatcher/cmd/stop/%s"       ,id);
}

// pub() ENQUEUES and returns immediately -- it must not reference mqtt at all,
// so no publish site on the loop thread can block on the network. networkTask is
// the only place a message actually leaves the radio. When the link degrades the
// queue fills and we drop the OLDEST message: stale telemetry is worth less than
// fresh, and losing some log is the correct failure -- losing navigation never
// was.
static void pub(const char* t,const char* m,bool retain=false){
  if(!pubQueue) return;
  pubWindowCount++;                        // Change 4: counted for pub_per_s
  PubMsg msg; msg.topic=t; msg.retain=retain;
  strlcpy(msg.payload,m,sizeof(msg.payload));
  if(xQueueSend(pubQueue,&msg,0)!=pdTRUE){
    PubMsg discard;                          // queue full: drop the OLDEST
    xQueueReceive(pubQueue,&discard,0);
    xQueueSend(pubQueue,&msg,0);
    pubDrops++;
  }
  UBaseType_t w=uxQueueMessagesWaiting(pubQueue);
  if(w>pubQueueHw) pubQueueHw=(uint16_t)w;
}

// The marker-only door. Same enqueue-and-return contract as pub(), but onto
// markerPubQueue and NEVER retained -- mm/marker is an event stream.
//
// DROP-NEWEST, not drop-oldest, and the asymmetry with pub() is deliberate. A
// contiguous run of markers with a gap only at the END is reconstructable from
// dead reckoning; a run with holes scattered through it is not. So if this queue
// ever fills we refuse the newcomer rather than evict a delivered-in-order
// backlog, and markerPubDrops records exactly how many are missing.
static void pubMarker(const char* t,const char* m){
  if(!markerPubQueue) return;
  pubWindowCount++;                        // counts toward pub_per_s like pub()
  PubMsg msg; msg.topic=t; msg.retain=false;
  strlcpy(msg.payload,m,sizeof(msg.payload));
  if(xQueueSend(markerPubQueue,&msg,0)!=pdTRUE) markerPubDrops++;   // drop NEWEST
  UBaseType_t w=uxQueueMessagesWaiting(markerPubQueue);
  if(w>markerPubHw) markerPubHw=(uint16_t)w;
}

static void navPublishState(const char* ev,const MarkerEvent* e){
  char b[384];
  if(e){
    // "off" is the last reacquisition offset -- meaningful on REACQUIRED, where
    // it sizes the search window from operational data. It carries its last
    // value on other events; consumers filter on event=="REACQUIRED".
    snprintf(b,sizeof(b),
      "{\"event\":\"%s\",\"state\":\"%s\",\"mm\":%u,\"landmark\":\"%s\",\"dir\":\"%s\","
      "\"confidence\":%d,\"obs\":\"%c\",\"expected\":\"%c\",\"peak\":%d,\"ms\":%u,"
      "\"drift\":%d,\"dt\":%u,\"agree\":%lu,\"disagree\":%lu,\"lost\":%lu,"
      "\"odom_disagreement\":%d,\"off\":%d,\"motor_dir\":\"%s\"}",
      ev, navState==NAV_TRACKING?"TRACKING":(navState==NAV_LOST?"LOST":"UNSET"),
      navMm, landmarkAt(navMm), dirName(navDir), navConfidence,
      polChar(e->polarity), polChar(dnaAt(navMm)), e->peak, e->durationMs,
      e->baselineDrift, lastSegmentDt, navAgree, navDisagree, navLostCount,
      (int)lastOdomDisagreement, (int)lastReacqOffset,
      motorDirection==DIRECTION_FORWARD?"FWD":"REV");
  }else{
    // The short payload is what DIRECTION and SESSION_DIRECTION publish, so it
    // has to carry the direction fields — those are the events a consumer most
    // wants them on. (Codex)
    snprintf(b,sizeof(b),
      "{\"event\":\"%s\",\"state\":\"%s\",\"mm\":%u,\"dir\":\"%s\","
      "\"motor_dir\":\"%s\",\"session_dir\":\"%s\",\"confidence\":%d}",
      ev, navState==NAV_TRACKING?"TRACKING":(navState==NAV_LOST?"LOST":"UNSET"),
      navMm, dirName(navDir),
      motorDirection==DIRECTION_FORWARD?"FWD":"REV", dirName(sessionDir),
      navConfidence);
  }
  pub(T_NAV,b);
  Serial.printf("[NAV] %s\n",b);
}

// The console shows state/warning to the operator. Without it a refusal is
// published, logged, and invisible -- which on 2026-07-27 meant a backwards
// start interval was rejected, GO was then refused for having no position, and
// from the garden it looked like the locomotive had simply stopped responding.
// Anything ending in _REFUSED, plus MISSED and DEPARTURE_SLOW, goes here.
static unsigned long warningSetMs=0;
static void publishWarning(const char* text){
  pub(T_ST_WARNING,text);
  warningSetMs = text[0] ? millis() : 0;
}
// Clear it after a while so a stale warning does not sit on the dashboard.
static void serviceWarningExpiry(){
  if(warningSetMs && millis()-warningSetMs > 20000UL){ warningSetMs=0; pub(T_ST_WARNING,""); }
}

static void stationPublish(const char* ev,int16_t off,const char* note){
  // Publish on TRANSITION only. serviceStations() runs every loop pass, so
  // 2_10 republished the same line continuously -- APPROACH at offset -10 went
  // out 79 times, 2381 station messages in one lap. Flooding the broker and
  // burying the transitions that matter.
  static const char* lastEv=nullptr;
  static int16_t     lastOff=32767;
  if(ev==lastEv && off==lastOff) return;
  lastEv=ev; lastOff=off;

  char b[256];
  const char* nm = (stIndex>=0 && stIndex<(int8_t)STATION_COUNT) ? STATIONS[stIndex].name : "NONE";
  snprintf(b,sizeof(b),
    "{\"event\":\"%s\",\"phase\":%u,\"station\":\"%s\",\"offset\":%d,"
    "\"commanded_pwm\":%d,\"actual_pwm\":%d,\"note\":\"%s\"}",
    ev,(unsigned)stPhase,nm,off,commandedPwm,actualPwm,note);
  pub(T_STATION,b);
  Serial.printf("[STN] %s\n",b);

  // Surface anything the operator needs to act on.
  if(strstr(ev,"REFUSED") || !strcmp(ev,"MISSED") || !strcmp(ev,"DEPARTURE_SLOW")){
    char w[160]; snprintf(w,sizeof(w),"%s: %s",ev,note);
    publishWarning(w);
  }
}

// ---------------------------------------------------------------------------
// ALERT — retained, so a follower or the dispatcher sees it even on late
// connect. This is the message that has to be actionable by another train:
// not "something is wrong" but where this locomotive was last certain, how far
// it may have travelled since, and whether it is still moving.
// ---------------------------------------------------------------------------
static unsigned long lastAlertMs=0;

static void publishAlert(const char* level,const char* reason){
  lastAlertMs=millis();
  char b[640];

  // OCCUPANCY BOUND — the actionable part for a following locomotive.
  //   rear  : the rearmost point this loco can possibly occupy. It cannot be
  //           behind the last marker it actually confirmed.
  //   front : dead reckoning plus a margin, because a loco that is misreading
  //           markers may also be MISSING them and running ahead of its own
  //           odometer.
  // A follower should hold clear of `rear`, not of `dead_reckoned_mm`.
  uint8_t  rear  = haveConfirmed ? lastConfirmedMm : navMm;
  uint8_t  front = (navDir==MAP_UNSET) ? navMm
                 : routeMod((int32_t)navMm + (int32_t)navDir*LOST_FRONT_MARGIN_MARKERS);
  uint32_t envelope = (navDir==MAP_UNSET) ? 0
                    : spanMm(rear,navDir,(uint16_t)(markersSinceConfirmed+LOST_FRONT_MARGIN_MARKERS));

  // Rough ground speed from the last segment, for a follower estimating how
  // fast the envelope is growing. Diagnostic quality only.
  uint32_t mmPerSec = 0;
  if(lastSegmentDt>0 && navDir!=MAP_UNSET)
    mmPerSec = (uint32_t)spanMm(routeMod((int32_t)navMm-navDir),navDir,1)*1000UL/lastSegmentDt;

  snprintf(b,sizeof(b),
    "{\"level\":\"%s\",\"reason\":\"%s\",\"loco\":\"%s\",\"uptime_ms\":%lu,"
    "\"nav\":\"%s\",\"moving\":%d,\"pwm\":%d,\"est_mm_s\":%lu,"
    "\"dir\":\"%s\",\"session_dir\":\"%s\",\"motor_dir\":\"%s\","
    "\"rear_bound_mm\":%u,\"front_bound_mm\":%u,\"envelope_mm\":%lu,"
    "\"last_confirmed_mm\":%d,\"last_confirmed_landmark\":\"%s\",\"age_ms\":%lu,"
    "\"markers_since_confirmed\":%u,\"dead_reckoned_mm\":%u,\"confidence\":%d,"
    "\"candidate_mm\":%d,\"lost_markers\":%u,\"lost_ms\":%lu,"
    "\"agree\":%lu,\"disagree\":%lu,\"lost_count\":%lu,\"auto\":%d}",
    level,reason,LOCO_NAME,(unsigned long)millis(),
    navState==NAV_TRACKING?"TRACKING":(navState==NAV_LOST?"LOST":"UNSET"),
    (actualPwm>0)?1:0,actualPwm,(unsigned long)mmPerSec,
    dirName(navDir),dirName(sessionDir),
    motorDirection==DIRECTION_FORWARD?"FWD":(motorDirection==DIRECTION_REVERSE?"REV":"NEU"),
    rear,front,(unsigned long)envelope,
    haveConfirmed?(int)lastConfirmedMm:-1,
    haveConfirmed?landmarkAt(lastConfirmedMm):"",
    (unsigned long)(haveConfirmed?(millis()-lastConfirmedMs):0UL),
    (unsigned)markersSinceConfirmed,navMm,navConfidence,
    pendingValid?(int)pendingMm:-1,
    (unsigned)lostMarkers,
    (unsigned long)(navState==NAV_LOST?(millis()-lostSinceMs):0UL),
    navAgree,navDisagree,navLostCount,autoRunning?1:0);

  pub(T_ALERT,b,true);          // retained: a late subscriber still learns of it
  Serial.printf("[ALERT] %s\n",b);
}

// Every locomotive broadcasts its occupancy bound once a second, ALWAYS --
// not only when in trouble. A peer cannot decide whether it is safe to keep
// moving unless the others are continuously saying where they are.
static void serviceStatusBroadcast(){
  if(millis()-lastAlertMs < STATUS_BROADCAST_MS) return;
  const char* level = (navState==NAV_LOST) ? "LOST"
                    : (navState==NAV_TRACKING ? "CLEAR" : "UNSET");
  publishAlert(level,"STATUS");
}

static uint8_t startIntervalA=0, startIntervalB=0;
static bool     haveStartInterval=false;

// The current start_interval string ("AAA-BBB" or "000-000"), written by both
// the on-change publisher and the connect-time republish.
static void formatStartInterval(char* out,size_t n){
  if(haveStartInterval) snprintf(out,n,"%03u-%03u",startIntervalA,startIntervalB);
  else                  snprintf(out,n,"000-000");
}

// ---------------------------------------------------------------------------
// CHANGE 3 (v2.20) — DASHBOARD STATE IS PUBLISHED ON CHANGE, RETAINED.
//
// v2.19's publishSimpleStates() emitted ten messages every second regardless of
// whether anything moved. That is what saturated pubQueue on 2026-07-30 and left
// the E-stop unread in a socket buffer. Now each topic is published only when its
// value changes, with the retain flag set, so a subscriber -- including a
// dashboard refreshed mid-run -- gets the current value from the broker on
// connect. The per-second broadcast was standing in for exactly that.
//
// The 1000 ms gate below stays, as a CEILING on values that change fast:
// start_mm ticks every marker and throttle every ramp step, and neither needs
// sub-second resolution on a status topic.
//
// RETAINED-STATE HAZARD: these retained values OUTLIVE the locomotive -- the
// broker serves the last one to any late subscriber even after the loco is off.
// The retained `online` flag, driven to "0" by the MQTT last will, is what makes
// them interpretable: any consumer MUST treat all of this state as stale when
// online is 0.
// ---------------------------------------------------------------------------

// Publish an int state topic retained, but only when it differs from last time.
// `*last` starts at -1, which none of these states ever takes (all are >=0), so
// the first call after boot publishes once.
static void pubStateIntChanged(const char* t,int v,int* last){
  if(v==*last) return;
  *last=v; char b[12]; snprintf(b,sizeof(b),"%d",v); pub(t,b,true);
}
// Same, for a string state topic.
static void pubStateStrChanged(const char* t,const char* v,char* last,size_t lastSz){
  if(!strcmp(v,last)) return;
  strlcpy(last,v,lastSz); pub(t,v,true);
}

// Last-published values for the eight changing states. Ints seeded to -1 (an
// impossible value) so each publishes once at boot; strings seeded empty.
static int  lastThrottle=-1,lastDirection=-1,lastEstop=-1,
            lastAuto=-1,lastStartMm=-1,lastNavReady=-1;
static char lastSessDir[8]="", lastStartInt[12]="";

static unsigned long lastSimpleMs=0;
static void publishSimpleStates(){
  unsigned long now=millis();
  if(now-lastSimpleMs < 1000UL) return;   // ceiling on the fast-changing values
  lastSimpleMs=now;
  pubStateIntChanged(T_ST_THROTTLE ,commandedPwm,      &lastThrottle);
  pubStateIntChanged(T_ST_DIRECTION,motorDirection,    &lastDirection);
  pubStateIntChanged(T_ST_ESTOP    ,estopped?1:0,      &lastEstop);
  pubStateIntChanged(T_ST_AUTO     ,autoEnrolled?1:0,  &lastAuto);
  pubStateStrChanged(T_ST_SESSDIR  ,dirName(sessionDir),lastSessDir,sizeof(lastSessDir));
  { char si[12]; formatStartInterval(si,sizeof(si));
    pubStateStrChanged(T_ST_STARTINT,si,lastStartInt,sizeof(lastStartInt)); }
  pubStateIntChanged(T_ST_STARTMM  ,navMm,             &lastStartMm);
  // The console unlocks the throttle on this: a declared direction and a
  // position it can name.
  pubStateIntChanged(T_ST_NAVREADY ,(sessionDir!=MAP_UNSET && navState==NAV_TRACKING)?1:0,&lastNavReady);
}

// CHANGE 3c (v2.20) — republish ALL ten state topics, retained, on every
// successful MQTT connect (called from attemptReconnect right after online=1).
// Ten writes per reconnect instead of ten per second, and it reseeds a broker
// that restarted without persisting its retained store. The two inert channels
// (brake, must_hold_eligible) live ONLY here now -- one retained zero at connect
// rather than the 86,400 a day v2.19 sent for hardware that does not exist.
static void publishAllStatesRetained(){
  char b[12];
  snprintf(b,sizeof(b),"%d",commandedPwm);     pub(T_ST_THROTTLE ,b,true);
  snprintf(b,sizeof(b),"%d",motorDirection);   pub(T_ST_DIRECTION,b,true);
  pub(T_ST_BRAKE,"0",true);                     // no brake channel in SOLONAV
  snprintf(b,sizeof(b),"%d",estopped?1:0);     pub(T_ST_ESTOP    ,b,true);
  snprintf(b,sizeof(b),"%d",autoEnrolled?1:0); pub(T_ST_AUTO     ,b,true);
  pub(T_ST_SESSDIR,dirName(sessionDir),true);
  { char si[12]; formatStartInterval(si,sizeof(si)); pub(T_ST_STARTINT,si,true); }
  snprintf(b,sizeof(b),"%d",navMm);            pub(T_ST_STARTMM  ,b,true);
  pub(T_ST_MHE,"0",true);                       // no CTO hold eligibility yet
  snprintf(b,sizeof(b),"%d",(sessionDir!=MAP_UNSET && navState==NAV_TRACKING)?1:0);
  pub(T_ST_NAVREADY,b,true);
}

static unsigned long loopMaxGapMs=0, lastStatMs=0;

// Change 4 — turn "did connect stall" and "did we come close to overflowing"
// into numbers. mqttConnectMs is the duration of the LAST mqtt.connect() call;
// mqttAttempts is the running count of connect attempts since boot; both
// persist across loopstat windows. queueHighWater is the max queue occupancy
// seen at drainMarkers() entry within the current loopstat window, reset on
// publish like loop_max_gap_ms -- so a normal lap reads low single digits and a
// stall spikes it.
static volatile unsigned long mqttConnectMs=0;
static uint32_t               mqttAttempts=0;
static uint16_t               queueHighWater=0;

static void publishStat(){
  unsigned long now=millis();
  if(now-lastStatMs<1000UL) return;
  lastStatMs=now;
  // Change 4 (v2.19) — pub_drops is cumulative; pub_queue_hw is the windowed
  // max occupancy of the outbound queue, reset below like loop_max_gap_ms. The
  // whole test of this version is that loop_max_gap_ms stays tiny even with the
  // broker down; pub_drops/pub_queue_hw show the network backing up behind the
  // queue while the loop keeps running.
  // Change 4 (v2.20). cmd_drops: cumulative inbound commands lost to a full
  // cmdQueue -- must never be silent. pub_per_s: publish calls in the window just
  // ended, read before the reset below (the T_STAT publish that follows counts
  // toward the next window). Expected ~12/s before Change 3, ~1/s parked after.
  uint32_t pubPerS=pubWindowCount;
  char b[512];
  snprintf(b,sizeof(b),
    "{\"loop_max_gap_ms\":%lu,\"hall_task_max_gap_ms\":%lu,\"hall_task_age_ms\":%lu,"
    "\"baseline\":%d,\"raw\":%d,\"delta\":%d,\"queue_drops\":%lu,\"floor_rejects\":%lu,"
    "\"queue_high_water\":%u,\"mqtt_connect_ms\":%lu,\"mqtt_attempts\":%lu,"
    "\"pub_drops\":%lu,\"pub_queue_hw\":%u,\"cmd_drops\":%lu,\"pub_per_s\":%lu,"
    "\"marker_pub_drops\":%lu,\"marker_pub_hw\":%u,"
    "\"nav\":\"%s\",\"mm\":%u,\"conf\":%d,\"pwm\":%d,"
    "\"lost_markers\":%u,\"lost_ms\":%lu,\"motor_dir\":\"%s\"}",
    loopMaxGapMs,(unsigned long)taskMaxGapMs,(unsigned long)(now-taskLastRunMs),
    baselineCounts,(int)lastRaw,(int)lastRaw-baselineCounts,
    queueDrops,floorRejects,
    (unsigned)queueHighWater,(unsigned long)mqttConnectMs,(unsigned long)mqttAttempts,
    (unsigned long)pubDrops,(unsigned)pubQueueHw,(unsigned long)cmdDrops,(unsigned long)pubPerS,
    (unsigned long)markerPubDrops,(unsigned)markerPubHw,
    navState==NAV_TRACKING?"TRACKING":(navState==NAV_LOST?"LOST":"UNSET"),
    navMm,navConfidence,actualPwm,
    (unsigned)lostMarkers,
    (unsigned long)(navState==NAV_LOST?(now-lostSinceMs):0UL),
    motorDirection==DIRECTION_FORWARD?"FWD":(motorDirection==DIRECTION_REVERSE?"REV":"NEU"));
  pub(T_STAT,b);
  loopMaxGapMs=0; taskMaxGapMs=0; queueHighWater=0; pubQueueHw=0; markerPubHw=0; pubWindowCount=0;
}

static void publishBootId(){
  char b[320];
  snprintf(b,sizeof(b),
    "{\"sketch\":\"%s\",\"loco\":\"%s\",\"deadband\":%d,\"entry_margin\":%d,"
    "\"min_peak\":%d,\"floor_ms\":%lu,\"baseline\":\"median_%d_at_%lums\","
    "\"dna_w\":%d,\"conf_max\":%d}",
    SKETCH_NAME,LOCO_NAME,(int)HALL_DEADBAND_COUNTS,(int)HALL_ENTRY_MARGIN_COUNTS,
    (int)HALL_MIN_PEAK_DELTA,EVENT_FLOOR_MS,MEDIAN_WINDOW,MEDIAN_SAMPLE_MS,
    DNA_W,CONFIDENCE_MAX);
  pub(T_BOOT,b,true);          // retained: kills the stale-identity ghost
}

// ===========================================================================
// COMMANDS
// ===========================================================================
// Runs on the LOOP thread (via serviceCommands), so every handler below still
// touches navMm, commandedPwm, sessionDir and the station machine on the thread
// that owns them -- exactly as when onMqtt ran inside loop(). The only change is
// where it is called from; the handler bodies are unchanged. This is why no
// mutex is needed once MQTT moves to its own task.
static void handleCommand(const char* topic,const char* msg){
  if(!strcmp(topic,T_CMD_SESSDIR)){
    // Same movement guard as cmd/direction: 2_1 flipped the pin here with no
    // check at all, so a manually reversing loco could be thrown over under
    // power. (Codex)
    if(motorIsMoving()){ stationPublish("SESSION_DIR_REFUSED",0,"WAIT_FOR_STOP"); return; }
    // Anything unparseable used to become MAP_UNSET, which left the navigator
    // TRACKING with no direction: nextMm(mm,MAP_UNSET) returns mm, so the
    // odometer silently stopped advancing and judged every further reading
    // against the same marker. Refuse instead of accepting a broken state.
    int8_t req = (!strcasecmp(msg,"CW"))?MAP_CW:((!strcasecmp(msg,"CCW"))?MAP_CCW:MAP_UNSET);
    if(req==MAP_UNSET){ stationPublish("SESSION_DIR_REFUSED",0,"INVALID_MUST_BE_CW_OR_CCW"); return; }
    sessionDir = req;
    motorDirection=DIRECTION_FORWARD;
    applyDirection();                      // recomputes navDir, writes the pin
    stationReset("SESSION_DIRECTION_SET");
    navPublishState("SESSION_DIRECTION",nullptr);
  }
  else if(!strcmp(topic,T_CMD_STARTINT)){
    // "AAA-BBB" — the two magnets the locomotive is standing between. This is
    // a GEOMETRIC interval, always ascending (085-086), because that is what
    // the console's slider produces and what the operator can see on the
    // ground. It is NOT a travel-order pair.
    //
    // 2_14 required travel order, so it demanded 086-085 when running CCW and
    // rejected every interval the console is capable of sending. The start
    // interval was unusable in one direction, which is why CCW sessions could
    // not be launched at all.
    //
    // Which end the locomotive is leaving depends on which way it faces:
    //     CW  : next marker is B, so position is declared at A
    //     CCW : next marker is A, so position is declared at B
    int a=-1,b=-1;
    if(sscanf(msg,"%d-%d",&a,&b)!=2){
      stationPublish("START_INTERVAL_REFUSED",0,"FORMAT_MUST_BE_AAA-BBB"); return;
    }
    if(a<0||a>=DNA_N||b<0||b>=DNA_N){
      stationPublish("START_INTERVAL_REFUSED",0,"MARKER_OUT_OF_RANGE"); return;
    }
    if(navDir==MAP_UNSET){
      stationPublish("START_INTERVAL_REFUSED",0,"SET_SESSION_DIRECTION_FIRST"); return;
    }
    // Adjacency is checked geometrically, in either order, so the console's
    // ascending pair is accepted whichever way the session runs.
    uint8_t behind;
    if(nextMm((uint8_t)a,MAP_CW)==(uint8_t)b)       behind = (navDir==MAP_CW)?(uint8_t)a:(uint8_t)b;
    else if(nextMm((uint8_t)b,MAP_CW)==(uint8_t)a)  behind = (navDir==MAP_CW)?(uint8_t)b:(uint8_t)a;
    else { stationPublish("START_INTERVAL_REFUSED",0,"MARKERS_NOT_ADJACENT"); return; }

    startIntervalA=(uint8_t)a; startIntervalB=(uint8_t)b; haveStartInterval=true;
    navDeclare(behind);
    stationPublish("START_INTERVAL_SET",0,
                   (navDir==MAP_CW)?"CW_DECLARED_AT_LOWER":"CCW_DECLARED_AT_UPPER");
  }
  else if(!strcmp(topic,T_CMD_STARTMM)){
    int mm=atoi(msg);
    if(mm>=0 && mm<DNA_N && navDir!=MAP_UNSET) navDeclare((uint8_t)mm);
  }
  else if(!strcmp(topic,T_CMD_AUTO)){
    autoEnrolled=(atoi(msg)!=0);
    if(!autoEnrolled){ autoRunning=false; requestPwm(0,NORMAL_STEP_MS); }
    stationReset("AUTO_CHANGED");
  }
  else if(!strcmp(topic,T_CMD_GO)){
    // E-stop must be cleared explicitly. GO is not an implicit reset.
    if(estopped){ stationPublish("GO_REFUSED",0,"ESTOP_ACTIVE"); return; }
    // GO forces direction to FORWARD, so it must not run while the loco is
    // already rolling manually in reverse. Same guard as cmd/direction and
    // session_direction; this was the one path still missing it.
    if(motorIsMoving()){ stationPublish("GO_REFUSED",0,"WAIT_FOR_STOP"); return; }
    if(motorDirection==DIRECTION_NEUTRAL){ stationPublish("GO_REFUSED",0,"NEUTRAL_SELECT_DIRECTION"); return; }
    // Every refusal says why. This one used to fall through silently: a lost
    // loco could be sent GO all night and neither move nor explain itself.
    if(!autoEnrolled){ stationPublish("GO_REFUSED",0,"NOT_ENROLLED_IN_AUTO"); return; }
    if(navState==NAV_LOST){ stationPublish("GO_REFUSED",0,"POSITION_LOST_DECLARE_START_MM"); return; }
    if(navState!=NAV_TRACKING){ stationPublish("GO_REFUSED",0,"NO_POSITION_DECLARE_START_MM"); return; }
    if(navDir==MAP_UNSET){ stationPublish("GO_REFUSED",0,"NO_SESSION_DIRECTION"); return; }
    if(autoRunning){ stationPublish("GO_REFUSED",0,"ALREADY_RUNNING"); return; }
    {
      autoRunning=true;
      motorDirection=DIRECTION_FORWARD; applyDirection();
      requestPwm(cruiseForPosition(),NORMAL_STEP_MS);
      stationPublish("GO",0,"LAUNCH");
    }
  }
  else if(!strcmp(topic,T_CMD_STOP)){
    autoRunning=false; requestPwm(0,NORMAL_STEP_MS);
    stationReset("DISPATCHER_STOP");
  }
  else if(!strcmp(topic,T_CMD_DIR)){
    // Physical motor direction. Refused while moving and while AUTO is driving.
    if(autoRunning){ stationPublish("DIR_REFUSED",0,"AUTO_IN_CONTROL"); return; }
    if(motorIsMoving()){ stationPublish("DIR_REFUSED",0,"WAIT_FOR_STOP"); return; }
    int v=constrain(atoi(msg),0,2);
    // NEUTRAL is a real selection now, not a no-op. It engages the interlock;
    // it is not a stop command and is refused while moving, like any other
    // direction change.
    motorDirection=v; applyDirection();            // navDir follows the motor
    navPublishState("DIRECTION",nullptr);
  }
  else if(!strcmp(topic,T_CMD_RELEASE)){
    // END CTO — hand the locomotive back to the operator. Drops AUTO
    // enrolment as well as AUTO running, so the console's manual controls
    // ungrey and a stray GO cannot restart it.
    autoRunning=false; autoEnrolled=false;
    requestPwm(0,NORMAL_STEP_MS);
    stationReset("DISPATCHER_RELEASE");
    publishWarning("RELEASED: dispatcher control ended");
    navPublishState("DISPATCHER_RELEASE",nullptr);
  }
  else if(!strcmp(topic,T_CMD_ESTOP)){
    estopped=(atoi(msg)!=0);
    if(!estopped){
      // Clearing E-stop drops into NEUTRAL. Motion resumes only after the
      // operator deliberately selects a direction. (Restores prior behaviour.)
      motorDirection=DIRECTION_NEUTRAL; applyDirection();
      navPublishState("ESTOP_CLEARED_NEUTRAL",nullptr);
    }
    if(estopped){
      autoRunning=false; commandedPwm=0; actualPwm=0; pwmWriteCompat(0);
      motorDirection=DIRECTION_NEUTRAL; applyDirection();
      // The loco has stopped somewhere unplanned; a half-finished station
      // approach must not resume against it after E-stop is cleared. (Codex)
      stationReset("ESTOP");
    }
  }
  else if(!strcmp(topic,T_CMD_THROTTLE)){
    if(!autoRunning && !estopped) requestPwm(atoi(msg),NORMAL_STEP_MS);   // manual only
  }
  else if(!strcmp(topic,T_CMD_FORCELOST)){
    // TEST FIXTURE. Reacquisition essentially never fires at the measured error
    // rate, so LOST must be inducible or M1_TEST_SPEC cannot be run. Any payload
    // forces it. Not a guard and not on any operational path.
    navEnterLost("TEST");
  }
}

static void subscribeAll(){
  mqtt.subscribe(T_CMD_AUTO);     mqtt.subscribe(T_CMD_GO);
  mqtt.subscribe(T_CMD_STOP);     mqtt.subscribe(T_CMD_DIR);
  mqtt.subscribe(T_CMD_SESSDIR);  mqtt.subscribe(T_CMD_STARTMM);
  mqtt.subscribe(T_CMD_ESTOP);    mqtt.subscribe(T_CMD_THROTTLE);
  mqtt.subscribe(T_CMD_STARTINT);
  mqtt.subscribe(T_CMD_RELEASE);
  mqtt.subscribe(T_CMD_FORCELOST);
}

// The MQTT callback runs on the NETWORK task. It must not touch locomotive
// state, so it only copies the command onto cmdQueue and returns; loop()'s
// serviceCommands() runs the actual handler on the thread that owns the state.
// Dropped if full -- commands are rare, and a backed-up command queue would mean
// loop() itself is wedged, which is exactly what this change prevents.
static void onMqttEnqueue(char* topic,byte* payload,unsigned int len){
  CmdMsg c;
  strlcpy(c.topic,topic,sizeof(c.topic));
  unsigned n=min(len,(unsigned)(sizeof(c.payload)-1));
  memcpy(c.payload,payload,n); c.payload[n]=0;

  // CHANGE 1 (v2.20) — ENGAGING E-stop must not depend on the command queue.
  // servicePwmRamp() clamps the motor to zero every loop() pass while `estopped`
  // is set, so raising the flag HERE -- on the network task, before anything is
  // enqueued -- stops the locomotive within ~35 ms even if cmdQueue is full and
  // the queued handler below is lost. This is the single most important change in
  // this version: on 2026-07-30 the E-stop sat unread and Otto could not be
  // stopped. Only ENGAGING bypasses; CLEARING stays queued (below) so resuming
  // motion is deliberate and runs the full handler.
  if(!strcmp(c.topic,T_CMD_ESTOP) && atoi(c.payload)!=0) estopped=true;

  // Fall through and ALSO enqueue, so the full handler still runs (NEUTRAL,
  // autoRunning=false, stationReset, the alert). If this send is dropped the
  // locomotive has still stopped. CHANGE 4: count the loss -- v2.19 ignored the
  // send result, so a dropped command was silent.
  if(cmdQueue && xQueueSend(cmdQueue,&c,0)!=pdTRUE) cmdDrops++;
}

// Drained on the LOOP thread, symmetric with drainMarkers(). Runs the existing
// command handlers unchanged, so all locomotive state stays owned by loop().
static void serviceCommands(){
  CmdMsg c;
  while(cmdQueue && xQueueReceive(cmdQueue,&c,0)==pdTRUE)
    handleCommand(c.topic,c.payload);
}

// Reconnect, throttled to one attempt per 5 s. Behaviour is unchanged from the
// v2.18 serviceNetwork(): time the connect, publish online/boot/alert, and
// resubscribe. It runs ONLY on networkTask, so mqtt.connect() blocking here can
// no longer stall loop(). (The online/boot/alert publishes go through pub(),
// i.e. onto pubQueue, and are flushed by networkTask on its next pass.)
static unsigned long nextMqttTryMs=0;
static void attemptReconnect(){
  unsigned long now=millis();
  if(now<nextMqttTryMs) return;
  nextMqttTryMs=now+5000UL;
  unsigned long t0=millis();
  mqttAttempts++;
  bool ok=mqtt.connect(LOCO_NAME,T_ONLINE,0,true,"0");
  mqttConnectMs=millis()-t0;
  if(ok){
    pub(T_ONLINE,"1",true);
    publishAllStatesRetained();        // Change 3c: reseed all ten state topics
    publishBootId();
    publishAlert(navState==NAV_LOST?"LOST":"CLEAR","MQTT_CONNECT");
    subscribeAll();                    // mqtt.subscribe; reached only from here
    Serial.println("[NET] MQTT connected");
  }
}

// The network task owns the radio exclusively: it is the ONLY place mqtt.loop()
// and mqtt.publish() run, and it calls attemptReconnect() (mqtt.connect/
// subscribe). Pinned to core 0 alongside the WiFi stack at priority 1 -- below
// hallTask's 2, so magnet sampling always wins. 8192 stack: PubSubClient plus
// WiFi needs more than hallTask's 4096.
static void networkTask(void*){
  for(;;){
    if(WiFi.status()==WL_CONNECTED){
      if(!mqtt.connected()){
        attemptReconnect();            // may block; that is now harmless
      }else{
        // CHANGE 2 (v2.20) — inbound FIRST, every pass, and the outbound drain is
        // BOUNDED. v2.19 read inbound once, then drained up to 32 blocking
        // publishes through a degraded socket before reading again; on 2026-07-30
        // that left cmd/estop unread while the queue stayed pinned full. At most 4
        // publishes per pass with a 5 ms tick gives ~200 inbound polls/s that
        // outbound congestion can no longer starve.
        //
        // Do NOT raise the 4 to clear a backlog faster: a persistent backlog means
        // the link cannot carry the traffic, and the fix for that is publish-on-
        // change (Change 3), not a bigger gulp that re-creates the starvation.
        mqtt.loop();
        PubMsg m;
        // CHANGE 2 (v2.21) — markers drain FIRST and UNCAPPED. They arrive at
        // ~1/s while this loop runs at ~200/s, so the queue is normally empty;
        // the only time it holds several is right after a stall, and then every
        // one of them should go out at once rather than trickle 4 per pass behind
        // status. Uncapped is safe precisely because the arrival rate is so far
        // below the drain rate. Markers may starve status; status must never
        // starve markers -- that inversion is the whole point of the split.
        while(xQueueReceive(markerPubQueue,&m,0)==pdTRUE)
          mqtt.publish(m.topic,m.payload,false);   // mm/marker is never retained
        // Status keeps its bounded drain (Change 2, v2.20): at most 4 per pass so
        // a congested outbound queue cannot starve inbound mqtt.loop().
        uint8_t n=0;
        while(n<4 && xQueueReceive(pubQueue,&m,0)==pdTRUE){
          mqtt.publish(m.topic,m.payload,m.retain);
          n++;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// ===========================================================================
static void drainMarkers(){
  MarkerEvent e;
  // Sample occupancy BEFORE draining: this is how deep the queue got while the
  // previous loop pass was busy (e.g. blocked in serviceNetwork). Windowed max,
  // reset by publishStat.
  if(eventQueue){
    UBaseType_t waiting=uxQueueMessagesWaiting(eventQueue);
    if(waiting>queueHighWater) queueHighWater=(uint16_t)waiting;
  }
  while(eventQueue && xQueueReceive(eventQueue,&e,0)==pdTRUE){
    navOnMarker(e);
    char b[224];
    snprintf(b,sizeof(b),
      "{\"mm\":%u,\"landmark\":\"%s\",\"obs\":\"%c\",\"peak\":%d,\"ms\":%u,"
      "\"drift\":%d,\"dt\":%u,\"conf\":%d}",
      navMm,landmarkAt(navMm),polChar(e.polarity),e.peak,e.durationMs,
      e.baselineDrift,lastSegmentDt,navConfidence);
    pubMarker(T_MARKER,b);   // dedicated queue; status can no longer evict it
  }
}

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

  analogReadResolution(12);
  pinMode(HALL_PIN,INPUT);
  pinMode(MOTOR_DIR_PIN,OUTPUT);
  applyDirection();          // never leave the pin at its power-up level
  pwmAttachCompat(); pwmWriteCompat(0);

  buildTopics();
  calibrate();

  // 256 slots is ~5 min of headroom at cruise (~1.1 s/marker), up from the 32
  // (~35 s) that overflowed and destroyed 67 marker events on 2026-07-29. This
  // does not fix a stall; it converts a data-destroying failure into a merely
  // delayed one -- survivable, because detectedAtMs is stamped at detection.
  eventQueue=xQueueCreate(256,sizeof(MarkerEvent));
  if(!eventQueue){ Serial.println("[FATAL] queue alloc failed"); while(1) delay(1000); }
  if(xTaskCreatePinnedToCore(hallTask,"hallTask",4096,nullptr,2,nullptr,0)!=pdPASS){
    Serial.println("[FATAL] hall task creation failed"); while(1) delay(1000);
  }

  // The MQTT queues must exist before pub()/pubMarker() or the callback can be
  // reached. markerPubQueue (v2.21): 64 slots at 512 B is ~33 KB -- a minute of
  // markers at cruise, affordable at 15% RAM. pubQueue stays 32: it filling
  // under a degraded link is correct behaviour, not the problem.
  pubQueue=xQueueCreate(32,sizeof(PubMsg));         // ~17 KB
  markerPubQueue=xQueueCreate(64,sizeof(PubMsg));   // ~33 KB, markers only
  cmdQueue=xQueueCreate(16,sizeof(CmdMsg));
  if(!pubQueue||!markerPubQueue||!cmdQueue){ Serial.println("[FATAL] mqtt queue alloc failed"); while(1) delay(1000); }

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID,WIFI_PASS);
  mqtt.setServer(MQTT_BROKER,MQTT_PORT);
  mqtt.setSocketTimeout(2);
  // Bound the underlying TCP connect. On the installed ESP32 core (3.3.11)
  // WiFiClient is a typedef of NetworkClient, whose connect() timeout is set by
  // setConnectionTimeout(MILLISECONDS) -- NOT by setTimeout(), which on this
  // core is the inherited Stream read timeout and has no effect on connect().
  // The core default is already 3000 ms, so this is belt-and-suspenders and
  // makes the 3 s bound explicit and version-independent. See the v2.18 notes:
  // the 33 s stalls were almost certainly measured on an older 2.x core, where
  // the connect default was ~30 s and setTimeout()'s units were seconds.
  espClient.setConnectionTimeout(3000);   // ms; yields the ~3 s the spec asked for
  mqtt.setCallback(onMqttEnqueue);        // callback only enqueues; loop() runs handlers
  mqtt.setBufferSize(2048);

  // MQTT now lives entirely on its own task. loop() never calls a mqtt.* function
  // again, so a degraded link can no longer stall navigation.
  if(xTaskCreatePinnedToCore(networkTask,"net",8192,nullptr,1,nullptr,0)!=pdPASS){
    Serial.println("[FATAL] network task creation failed"); while(1) delay(1000);
  }

  Serial.println("[BOOT] ready. Set session_direction, then start_mm, then auto, then GO.");
}

static unsigned long loopPrevMs=0;
void loop(){
  unsigned long now=millis();
  unsigned long gap=now-loopPrevMs;
  if(gap>loopMaxGapMs) loopMaxGapMs=gap;
  loopPrevMs=now;

  // Not one of these can block on the network: MQTT runs on networkTask, and
  // every publish site only enqueues. serviceCommands() drains inbound commands
  // first (symmetric with the outbound queue) so a command and the markers it
  // affects are processed in the same pass, on this one thread.
  serviceCommands();
  drainMarkers();
  serviceStatusBroadcast();
  serviceWarningExpiry();
  publishSimpleStates();
  serviceStations();
  servicePwmRamp();
  publishStat();
}
