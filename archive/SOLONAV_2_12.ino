/*
 * ============================================================================
 * SOLONAV_2_12  —  Ninobur Garden Railway single-locomotive navigation
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

#define SKETCH_NAME "SOLONAV_2_12"

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
};

struct MarkerEvent {
  uint8_t       polarity;      // 1=N 0=S
  int           peak;
  uint16_t      durationMs;
  int16_t       baselineDrift; // counts the baseline moved during the event
  unsigned long detectedAtMs;  // captured at detection, not at processing
};

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
//                          centre  zone  final
static const StationDefinition STATIONS[] = {
  {"Patio",    15,            60,   45},
  {"Grillers", 63,            72,   58},   // uphill: nearly stalled at 60/45
  {"Arches",  107,            60,   45},
  {"Bamboo",  157,            60,   45}
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

static bool         autoEnrolled=false, autoRunning=false, estopped=false;
static StationPhase stPhase=ST_IDLE;

static void requestPwm(int target,uint16_t stepMs);

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
#define CONFIDENCE_START     4
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

static unsigned long lastMarkerMs=0;
static int16_t       lastOdomDisagreement=0;   // published on REACQUIRED
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
  navConfidence=CONFIDENCE_START;
  dnaBufLen=0; pendingValid=false;
  navPublishState("DECLARED",nullptr);
}

static void navEnterLost(const char* why){
  navState=NAV_LOST; navConfidence=CONFIDENCE_FLOOR;
  dnaBufLen=0; pendingValid=false; navLostCount++;
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

// Unique window match, searched only in the declared direction.
// Returns the mm of the LAST marker in the window, or 255 if not unique.
static uint8_t dnaMatch(int8_t dir){
  if(dnaBufLen<DNA_W || dir==MAP_UNSET) return 255;
  uint8_t found=255; uint8_t count=0;
  for(uint8_t start=0;start<DNA_N;start++){
    bool ok=true; uint8_t mm=start;
    for(uint8_t i=0;i<DNA_W;i++){
      if(dnaAt(mm)!=dnaBuf[i]){ ok=false; break; }
      mm=nextMm(mm,dir);
    }
    if(ok){ if(++count>1) return 255; found=routeMod((int32_t)start+(int32_t)dir*(DNA_W-1)); }
  }
  return count==1?found:255;
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
      // A candidate is on probation: it must predict this reading.
      uint8_t predicted=nextMm(pendingMm,navDir);
      if(dnaAt(predicted)==e.polarity){
        lastOdomDisagreement = offsetToCentre(navMm,navDir,predicted); // odom vs DNA
        int16_t disagree = lastOdomDisagreement;
        navMm=predicted; navState=NAV_TRACKING;
        navConfidence=CONFIDENCE_START; pendingValid=false;
        lostMarkers=0;
        lastConfirmedMm=navMm; lastConfirmedMs=millis();
        markersSinceConfirmed=0; haveConfirmed=true;
        // Back to cruise, unless a station approach already owns the throttle.
        if(autoRunning && stPhase==ST_IDLE) requestPwm(CRUISE_PWM,NORMAL_STEP_MS);
        publishAlert("CLEAR","REACQUIRED");
        Serial.printf("[NAV] REACQUIRED mm=%u dir=%s odom_disagreement=%d\n",
                      navMm,dirName(navDir),(int)disagree);
        navPublishState("REACQUIRED",&e);
      }else{
        pendingValid=false;                       // candidate failed its test
        // Codex: don't throw away a fresh window just because the old
        // candidate failed. Promote it immediately instead of losing a marker.
        uint8_t again=dnaMatch(navDir);
        if(again!=255){ pendingMm=again; pendingValid=true; navPublishState("CANDIDATE",&e); }
        else            navPublishState("CANDIDATE_REJECTED",&e);
      }
      return;
    }

    uint8_t cand=dnaMatch(navDir);
    if(cand!=255){
      pendingMm=cand; pendingValid=true;          // must survive one more marker
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
static const uint16_t APPROACH_RAMP_MS = 700;   // ~half a marker: settled before the next
static const uint16_t FINAL_RAMP_MS    = 700;
static const uint16_t STOP_RAMP_MS     = 1500;  // ~180 mm of travel at approach speed
static const int8_t   APPROACH_START      = -10;
static const int8_t   ZONE_START          = -5;
static const int8_t   ZERO_RAMP_OFFSET    = 2;
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
    if(autoRunning && navState==NAV_TRACKING) requestPwm(CRUISE_PWM,NORMAL_STEP_MS);
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
    return;
  }

  const int16_t o=offsetToCentre(navMm,navDir,STATIONS[stIndex].centerMm);

  // Overshoot escape, checked in every non-idle phase before anything else.
  if(o > OVERSHOOT_ABANDON && stPhase!=ST_DWELL && stPhase!=ST_DEPART){
    stationPublish("MISSED",o,"OVERSHOT_CENTRE_RETURNING_TO_IDLE");
    stationReset("MISSED");
    requestPwm(CRUISE_PWM,NORMAL_STEP_MS);
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

    case ST_FINAL:
      // Location-triggered, one target per marker. Not a timed interpolation
      // to a single distant target — that is what left the loco at PWM 25 a
      // marker early and out of tractive effort.
      if(o>=0 && o<ZERO_RAMP_OFFSET){
        // M   : still at zone speed, passing the station
        // M+1 : ease to finalPwm -- the speed the stop is made from
        requestPwmOver(o==0?STATIONS[stIndex].zonePwm:STATIONS[stIndex].finalPwm,
                       FINAL_RAMP_MS);
        if(o==1 && stMPlus1AtMs==0) stMPlus1AtMs=millis();  // start the fallback clock
        stationPublish("FINAL_TARGET",o,o==0?"AT_CENTRE_ZONE_SPEED":"M_PLUS_1_FINAL_SPEED");
      }else if(o>=ZERO_RAMP_OFFSET){
        stationSetPhase(ST_RAMP);
        requestPwmOver(0,STOP_RAMP_MS);
        // Which trigger fired is a fact worth logging: repeated M1_TIMEOUT at
        // one station means that approach profile is too aggressive there.
        stationPublish("ZERO_RAMP",o,"TRIGGER_M2_REACHED");
      }
      break;

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
        requestPwm(STATION_ZONE_PWM,NORMAL_STEP_MS);
        stationPublish("DWELL_COMPLETE",o,"DEPART_TO_60");
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
      if(o>=ZERO_RAMP_OFFSET+3){
        requestPwm(CRUISE_PWM,NORMAL_STEP_MS);
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
            T_ST_MHE[64],T_ST_NAVREADY[64];
static char T_CMD_AUTO[64],T_CMD_GO[64],T_CMD_STOP[64],T_CMD_DIR[64],T_CMD_SESSDIR[64];
static char T_CMD_STARTMM[64],T_CMD_ESTOP[64],T_CMD_THROTTLE[64],T_CMD_STARTINT[64];

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
  snprintf(T_CMD_AUTO    ,64,"ngr/loco/%s/cmd/auto"             ,id);
  snprintf(T_CMD_DIR     ,64,"ngr/loco/%s/cmd/direction"        ,id);
  snprintf(T_CMD_SESSDIR ,64,"ngr/loco/%s/cmd/session_direction",id);
  snprintf(T_CMD_STARTMM ,64,"ngr/loco/%s/cmd/start_mm"         ,id);
  snprintf(T_CMD_STARTINT,64,"ngr/loco/%s/cmd/start_interval"   ,id);
  snprintf(T_CMD_ESTOP   ,64,"ngr/loco/%s/cmd/estop"            ,id);
  snprintf(T_CMD_THROTTLE,64,"ngr/loco/%s/cmd/throttle"         ,id);
  snprintf(T_CMD_GO      ,64,"ngr/dispatcher/cmd/go/%s"         ,id);
  snprintf(T_CMD_STOP    ,64,"ngr/dispatcher/cmd/stop/%s"       ,id);
}

static void pub(const char* t,const char* m,bool retain=false){
  if(mqtt.connected()) mqtt.publish(t,m,retain);
}

static void navPublishState(const char* ev,const MarkerEvent* e){
  char b[320];
  if(e){
    snprintf(b,sizeof(b),
      "{\"event\":\"%s\",\"state\":\"%s\",\"mm\":%u,\"landmark\":\"%s\",\"dir\":\"%s\","
      "\"confidence\":%d,\"obs\":\"%c\",\"expected\":\"%c\",\"peak\":%d,\"ms\":%u,"
      "\"drift\":%d,\"dt\":%u,\"agree\":%lu,\"disagree\":%lu,\"lost\":%lu,"
      "\"odom_disagreement\":%d,\"motor_dir\":\"%s\"}",
      ev, navState==NAV_TRACKING?"TRACKING":(navState==NAV_LOST?"LOST":"UNSET"),
      navMm, landmarkAt(navMm), dirName(navDir), navConfidence,
      polChar(e->polarity), polChar(dnaAt(navMm)), e->peak, e->durationMs,
      e->baselineDrift, lastSegmentDt, navAgree, navDisagree, navLostCount,
      (int)lastOdomDisagreement, motorDirection==DIRECTION_FORWARD?"FWD":"REV");
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

static void pubInt(const char* t,int v){ char b[12]; snprintf(b,sizeof(b),"%d",v); pub(t,b); }

static uint8_t startIntervalA=0, startIntervalB=0;
static bool     haveStartInterval=false;

static unsigned long lastSimpleMs=0;
static void publishSimpleStates(){
  unsigned long now=millis();
  if(now-lastSimpleMs < 1000UL) return;
  lastSimpleMs=now;
  pubInt(T_ST_THROTTLE ,commandedPwm);
  pubInt(T_ST_DIRECTION,motorDirection);
  pubInt(T_ST_BRAKE    ,0);                       // no brake channel in SOLONAV
  pubInt(T_ST_ESTOP    ,estopped?1:0);
  pubInt(T_ST_AUTO     ,autoEnrolled?1:0);
  pub  (T_ST_SESSDIR   ,dirName(sessionDir));     // "CW" / "CCW" / "UNSET"
  {
    char si[12];
    if(haveStartInterval) snprintf(si,sizeof(si),"%03u-%03u",startIntervalA,startIntervalB);
    else                  snprintf(si,sizeof(si),"000-000");
    pub(T_ST_STARTINT,si);
  }
  pubInt(T_ST_STARTMM  ,navMm);
  pubInt(T_ST_MHE      ,0);                       // no CTO hold eligibility yet
  // The console unlocks the throttle on this: a declared direction and a
  // position it can name.
  pubInt(T_ST_NAVREADY ,(sessionDir!=MAP_UNSET && navState==NAV_TRACKING)?1:0);
}

static unsigned long loopMaxGapMs=0, lastStatMs=0;
static void publishStat(){
  unsigned long now=millis();
  if(now-lastStatMs<1000UL) return;
  lastStatMs=now;
  char b[320];
  snprintf(b,sizeof(b),
    "{\"loop_max_gap_ms\":%lu,\"hall_task_max_gap_ms\":%lu,\"hall_task_age_ms\":%lu,"
    "\"baseline\":%d,\"raw\":%d,\"delta\":%d,\"queue_drops\":%lu,\"floor_rejects\":%lu,"
    "\"nav\":\"%s\",\"mm\":%u,\"conf\":%d,\"pwm\":%d,"
    "\"lost_markers\":%u,\"lost_ms\":%lu,\"motor_dir\":\"%s\"}",
    loopMaxGapMs,(unsigned long)taskMaxGapMs,(unsigned long)(now-taskLastRunMs),
    baselineCounts,(int)lastRaw,(int)lastRaw-baselineCounts,
    queueDrops,floorRejects,
    navState==NAV_TRACKING?"TRACKING":(navState==NAV_LOST?"LOST":"UNSET"),
    navMm,navConfidence,actualPwm,
    (unsigned)lostMarkers,
    (unsigned long)(navState==NAV_LOST?(now-lostSinceMs):0UL),
    motorDirection==DIRECTION_FORWARD?"FWD":(motorDirection==DIRECTION_REVERSE?"REV":"NEU"));
  pub(T_STAT,b);
  loopMaxGapMs=0; taskMaxGapMs=0;
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
static void onMqtt(char* topic,byte* payload,unsigned int len){
  char msg[64]; unsigned n=min(len,(unsigned)63);
  memcpy(msg,payload,n); msg[n]=0;

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
    // "AAA-BBB" — the two magnets the loco is standing between, given in
    // TRAVEL ORDER: A is behind, B is the next one it will cross. Declaring
    // position as A means the first marker event advances the odometer to B,
    // which is what actually happens on the railway.
    //
    // Naming the interval is easier for the operator than naming a single
    // marker: you can see which two magnets you are between, but not which
    // number the one behind you carries.
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
    if(nextMm((uint8_t)a,navDir)!=(uint8_t)b){
      // B must actually follow A in the direction of travel, or the operator
      // has the pair backwards and the loco would count the wrong way.
      stationPublish("START_INTERVAL_REFUSED",0,"NOT_ADJACENT_IN_TRAVEL_DIRECTION"); return;
    }
    startIntervalA=(uint8_t)a; startIntervalB=(uint8_t)b; haveStartInterval=true;
    navDeclare((uint8_t)a);
    stationPublish("START_INTERVAL_SET",0,"POSITION_DECLARED_AT_A");
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
      requestPwm(CRUISE_PWM,NORMAL_STEP_MS);
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
}

static void subscribeAll(){
  mqtt.subscribe(T_CMD_AUTO);     mqtt.subscribe(T_CMD_GO);
  mqtt.subscribe(T_CMD_STOP);     mqtt.subscribe(T_CMD_DIR);
  mqtt.subscribe(T_CMD_SESSDIR);  mqtt.subscribe(T_CMD_STARTMM);
  mqtt.subscribe(T_CMD_ESTOP);    mqtt.subscribe(T_CMD_THROTTLE);
  mqtt.subscribe(T_CMD_STARTINT);
}

// Non-blocking. No while() waits anywhere in the network path — a stalled
// reconnect used to blind the detector for tens of seconds.
static unsigned long nextMqttTryMs=0;
static void serviceNetwork(){
  if(WiFi.status()!=WL_CONNECTED) return;
  if(mqtt.connected()){ mqtt.loop(); return; }
  unsigned long now=millis();
  if(now<nextMqttTryMs) return;
  nextMqttTryMs=now+5000UL;
  if(mqtt.connect(LOCO_NAME,T_ONLINE,0,true,"0")){
    pub(T_ONLINE,"1",true);
    publishBootId();
    publishAlert(navState==NAV_LOST?"LOST":"CLEAR","MQTT_CONNECT");
    subscribeAll();
    Serial.println("[NET] MQTT connected");
  }
}

// ===========================================================================
static void drainMarkers(){
  MarkerEvent e;
  while(eventQueue && xQueueReceive(eventQueue,&e,0)==pdTRUE){
    navOnMarker(e);
    char b[224];
    snprintf(b,sizeof(b),
      "{\"mm\":%u,\"landmark\":\"%s\",\"obs\":\"%c\",\"peak\":%d,\"ms\":%u,"
      "\"drift\":%d,\"dt\":%u,\"conf\":%d}",
      navMm,landmarkAt(navMm),polChar(e.polarity),e.peak,e.durationMs,
      e.baselineDrift,lastSegmentDt,navConfidence);
    pub(T_MARKER,b);
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

  eventQueue=xQueueCreate(32,sizeof(MarkerEvent));
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
  mqtt.setBufferSize(2048);

  Serial.println("[BOOT] ready. Set session_direction, then start_mm, then auto, then GO.");
}

static unsigned long loopPrevMs=0;
void loop(){
  unsigned long now=millis();
  unsigned long gap=now-loopPrevMs;
  if(gap>loopMaxGapMs) loopMaxGapMs=gap;
  loopPrevMs=now;

  serviceNetwork();
  drainMarkers();
  serviceStatusBroadcast();
  publishSimpleStates();
  serviceStations();
  servicePwmRamp();
  publishStat();
}
