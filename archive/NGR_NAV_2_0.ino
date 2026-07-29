/*
 * ============================================================================
 * NGR_NAV_2_0  —  Ninobur Garden Railway single-locomotive navigation
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
 * FOUR LAYERS, ONE CONTRACT EACH
 * ---------------------------------------------------------------------------
 *   1  SENSOR      raw ADC + median baseline. No state that can stick.
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

#include <WiFi.h>
#include <PubSubClient.h>
#include <pgmspace.h>
#include "LocoConfig.h"

#define SKETCH_NAME "NGR_NAV_2_0"

// ===========================================================================
// HARDWARE
// ===========================================================================
#define HALL_PIN            33
#define ADC_SAMPLES          8
#define MOTOR_DEAD_ZONE_PWM 20

static inline void pwmAttachCompat(){ ledcAttach(MOTOR_PWM_PIN,PWM_FREQUENCY,PWM_RESOLUTION); }
static inline void pwmWriteCompat(int v){ ledcWrite(MOTOR_PWM_PIN,constrain(v,0,255)); }

// ===========================================================================
// MAP  — 171 markers, verified unique at W=10, locked at W=12
// ===========================================================================
#define DNA_N 171

enum MapDirection : int8_t { MAP_UNSET=0, MAP_CW=1, MAP_CCW=-1 };

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

struct StationDefinition { const char* name; uint8_t centerMm; };
static const StationDefinition STATIONS[] = {
  {"Patio",    15},
  {"Grillers", 63},
  {"Arches",  107},
  {"Bamboo",  157}
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
// Median of the last 64 seconds. Magnets occupy well under half of that window
// even at station creep, so they cannot move it; drift moves every sample, so
// it follows immediately. No gating, no freeze, no recovery state, no way to
// wedge. Proven over a full session on 2026-07-26: delta within a few counts,
// TRACKING throughout, including while sitting 90 counts inside a magnet.
// ===========================================================================
#define MEDIAN_WINDOW     128
#define MEDIAN_SAMPLE_MS  500UL
#define CALIBRATION_MS   2000UL

static int      baselineCounts   = 0;
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

struct MarkerEvent {
  uint8_t       polarity;      // 1=N 0=S
  int           peak;
  uint16_t      durationMs;
  int16_t       baselineDrift; // counts the baseline moved during the event
  unsigned long detectedAtMs;  // captured at detection, not at processing
};

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
#define CONFIDENCE_GOOD      6   // above this, position is trusted for stations
#define DNA_W               12   // 171 unique windows verified at W=10
#define WEAK_PEAK_COUNTS    (HALL_MIN_PEAK_DELTA)
#define DRIFT_SUSPECT_COUNTS 15

enum NavState : uint8_t { NAV_UNSET=0, NAV_TRACKING, NAV_LOST };

static NavState navState      = NAV_UNSET;
static uint8_t  navMm         = 0;
static int8_t   navDir        = MAP_UNSET;
static int8_t   navConfidence = 0;
static uint32_t navMarkers=0, navAgree=0, navDisagree=0, navLostCount=0;

static uint8_t  dnaBuf[DNA_W];
static uint8_t  dnaBufLen=0;
static bool     pendingValid=false;
static uint8_t  pendingMm=0;

static unsigned long lastMarkerMs=0;
static uint16_t      lastSegmentDt=0;

static void navPublishState(const char* ev,const MarkerEvent* e);

static void navDeclare(uint8_t mm,int8_t dir){
  navMm=mm; navDir=dir; navState=NAV_TRACKING;
  navConfidence=CONFIDENCE_START;
  dnaBufLen=0; pendingValid=false;
  navPublishState("DECLARED",nullptr);
}

static void navEnterLost(const char* why){
  navState=NAV_LOST; navConfidence=CONFIDENCE_FLOOR;
  dnaBufLen=0; pendingValid=false; navLostCount++;
  Serial.printf("[NAV] LOST (%s)\n",why);
  navPublishState("LOST",nullptr);
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
  unsigned long now=millis();
  lastSegmentDt = lastMarkerMs ? (uint16_t)min(now-lastMarkerMs,(unsigned long)65535) : 0;
  lastMarkerMs = now;

  // Quality flags. A weak or drifting reading still counts, but it is a
  // weaker vote — it can confirm but it cannot, by itself, condemn.
  const bool weak     = (e.peak < (int)WEAK_PEAK_COUNTS + 15);
  const bool drifting = (abs((int)e.baselineDrift) > DRIFT_SUSPECT_COUNTS);
  const bool soft     = weak || drifting;

  if(navState==NAV_LOST){
    dnaPush(e.polarity);
    uint8_t cand=dnaMatch(navDir);
    if(pendingValid){
      // A candidate is on probation: it must predict this reading.
      uint8_t predicted=nextMm(pendingMm,navDir);
      if(dnaAt(predicted)==e.polarity){
        navMm=predicted; navState=NAV_TRACKING;
        navConfidence=CONFIDENCE_START; pendingValid=false;
        Serial.printf("[NAV] REACQUIRED mm=%u dir=%s\n",navMm,dirName(navDir));
        navPublishState("REACQUIRED",&e);
      }else{
        pendingValid=false;                       // candidate failed its test
        navPublishState("CANDIDATE_REJECTED",&e);
      }
    }else if(cand!=255){
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

  if(e.polarity==expected){
    navAgree++;
    if(navConfidence<CONFIDENCE_MAX) navConfidence++;
    navPublishState("AGREE",&e);
  }else{
    navDisagree++;
    // A soft reading that disagrees costs one point; a clean reading that
    // disagrees costs two. Either way the train stays where the map says.
    navConfidence -= soft ? 1 : 2;
    navPublishState("DISAGREE",&e);
    if(navConfidence<=CONFIDENCE_FLOOR){ navEnterLost("sustained contradiction"); }
  }
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
static const uint8_t  CRUISE_PWM          = 90;
static const uint8_t  STATION_ZONE_PWM    = 60;
static const uint16_t NORMAL_STEP_MS      = 150;
static const uint16_t FINAL_STOP_STEP_MS  = 300;
static const int8_t   APPROACH_START      = -10;
static const int8_t   ZONE_START          = -5;
static const int8_t   ZERO_RAMP_OFFSET    = 2;
static const int8_t   OVERSHOOT_ABANDON   = 5;   // markers past centre -> MISSED
static const uint32_t DWELL_MS            = 15000UL;
static const uint32_t STATION_MAX_PHASE_MS= 120000UL; // absolute escape

// M and M+1 only. The 2026-07-26 run showed PWM 25 is below the tractive
// floor on at least one approach — the loco reached 25 a marker early and
// stopped short of its own zero-ramp trigger, wedging the station machine.
// The zero ramp now begins from 42 at M+2 instead of stepping through 25.
static const uint8_t FINAL_APPROACH_PWM[2] = {60,42};

enum StationPhase : uint8_t { ST_IDLE=0, ST_APPROACH, ST_FINAL, ST_RAMP, ST_DWELL, ST_DEPART };

static StationPhase  stPhase=ST_IDLE;
static int8_t        stIndex=-1;
static unsigned long stPhaseEnteredMs=0;
static unsigned long stDwellStartedMs=0;

static int      commandedPwm=0, actualPwm=0;
static uint16_t pwmStepMs=NORMAL_STEP_MS;
static unsigned long lastPwmStepMs=0;

static bool autoEnrolled=false, autoRunning=false, estopped=false;
static int8_t sessionDir=MAP_UNSET;

static void stationPublish(const char* ev,int16_t off,const char* note);

static void requestPwm(int target,uint16_t stepMs){
  commandedPwm=constrain(target,0,255);
  pwmStepMs=stepMs;
}

static void servicePwmRamp(){
  unsigned long now=millis();
  if(now-lastPwmStepMs < pwmStepMs) return;
  lastPwmStepMs=now;
  if(actualPwm<commandedPwm) actualPwm++;
  else if(actualPwm>commandedPwm) actualPwm--;
  else return;
  pwmWriteCompat(estopped?0:actualPwm);
}

static void stationSetPhase(StationPhase p){ stPhase=p; stPhaseEnteredMs=millis(); }

static void stationReset(const char* note){
  stPhase=ST_IDLE; stIndex=-1; stPhaseEnteredMs=millis();
  stationPublish("RESET",999,note);
}

static void serviceStations(){
  if(!autoRunning || navState!=NAV_TRACKING || navDir==MAP_UNSET) return;

  // Absolute escape. If any phase outlasts this, something is wrong and the
  // machine returns to IDLE rather than disabling stations for the session.
  if(stPhase!=ST_IDLE && millis()-stPhaseEnteredMs > STATION_MAX_PHASE_MS){
    stationReset("PHASE_TIMEOUT");
    requestPwm(CRUISE_PWM,NORMAL_STEP_MS);
    return;
  }

  if(stPhase==ST_IDLE){
    // Range arming. A skipped marker or a position correction can no longer
    // cause a station to be silently missed.
    if(navConfidence < CONFIDENCE_GOOD) return;   // don't arm on a shaky fix
    for(uint8_t i=0;i<STATION_COUNT;i++){
      int16_t o=offsetToCentre(navMm,navDir,STATIONS[i].centerMm);
      if(o<=APPROACH_START && o>APPROACH_START-3){
        stIndex=(int8_t)i; stationSetPhase(ST_APPROACH);
        requestPwm(CRUISE_PWM-6,NORMAL_STEP_MS);
        stationPublish("ARMED",o,"RANGE_ARM");
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
        // o = -10..-6  ->  84,78,72,66,60
        requestPwm(STATION_ZONE_PWM+((ZONE_START-1)-o)*6,NORMAL_STEP_MS);
        stationPublish("APPROACH",o,"SUBTRACT_6_PER_MARKER");
      }else if(o>=ZONE_START && o<0){
        requestPwm(STATION_ZONE_PWM,NORMAL_STEP_MS);
        stationPublish("ZONE_HOLD",o,"HOLD_60");
      }else if(o>=0){
        stationSetPhase(ST_FINAL);
        requestPwm(FINAL_APPROACH_PWM[0],NORMAL_STEP_MS);
        stationPublish("FINAL_APPROACH",o,"LOCATION_TRIGGERED_TARGETS");
      }
      break;

    case ST_FINAL:
      // Location-triggered, one target per marker. Not a timed interpolation
      // to a single distant target — that is what left the loco at PWM 25 a
      // marker early and out of tractive effort.
      if(o>=0 && o<ZERO_RAMP_OFFSET){
        requestPwm(FINAL_APPROACH_PWM[o],NORMAL_STEP_MS);   // o = 0,1 -> 60,42
        stationPublish("FINAL_TARGET",o,"PER_MARKER_TARGET");
      }else if(o>=ZERO_RAMP_OFFSET){
        stationSetPhase(ST_RAMP);
        requestPwm(0,FINAL_STOP_STEP_MS);
        stationPublish("ZERO_RAMP",o,"300_MS_PER_PWM_TO_ZERO");
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
        requestPwm(STATION_ZONE_PWM,NORMAL_STEP_MS);
        stationPublish("DWELL_COMPLETE",o,"DEPART_TO_60");
      }
      break;

    case ST_DEPART:
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

static char T_ONLINE[64],T_NAV[64],T_MARKER[64],T_STATION[64],T_STAT[64],T_BOOT[64];
static char T_CMD_AUTO[64],T_CMD_GO[64],T_CMD_STOP[64],T_CMD_DIR[64],T_CMD_SESSDIR[64];
static char T_CMD_STARTMM[64],T_CMD_ESTOP[64],T_CMD_THROTTLE[64];

static void buildTopics(){
  const char* id=LOCO_NAME;
  snprintf(T_ONLINE ,64,"ngr/loco/%s/online"        ,id);
  snprintf(T_NAV    ,64,"ngr/loco/%s/state/nav"     ,id);
  snprintf(T_MARKER ,64,"ngr/loco/%s/mm/marker"     ,id);
  snprintf(T_STATION,64,"ngr/loco/%s/state/station" ,id);
  snprintf(T_STAT   ,64,"ngr/loco/%s/state/loopstat",id);
  snprintf(T_BOOT   ,64,"ngr/loco/%s/state/bootid"  ,id);
  snprintf(T_CMD_AUTO    ,64,"ngr/loco/%s/cmd/auto"             ,id);
  snprintf(T_CMD_DIR     ,64,"ngr/loco/%s/cmd/direction"        ,id);
  snprintf(T_CMD_SESSDIR ,64,"ngr/loco/%s/cmd/session_direction",id);
  snprintf(T_CMD_STARTMM ,64,"ngr/loco/%s/cmd/start_mm"         ,id);
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
      "\"drift\":%d,\"dt\":%u,\"agree\":%lu,\"disagree\":%lu,\"lost\":%lu}",
      ev, navState==NAV_TRACKING?"TRACKING":(navState==NAV_LOST?"LOST":"UNSET"),
      navMm, landmarkAt(navMm), dirName(navDir), navConfidence,
      polChar(e->polarity), polChar(dnaAt(navMm)), e->peak, e->durationMs,
      e->baselineDrift, lastSegmentDt, navAgree, navDisagree, navLostCount);
  }else{
    snprintf(b,sizeof(b),
      "{\"event\":\"%s\",\"state\":\"%s\",\"mm\":%u,\"dir\":\"%s\",\"confidence\":%d}",
      ev, navState==NAV_TRACKING?"TRACKING":(navState==NAV_LOST?"LOST":"UNSET"),
      navMm, dirName(navDir), navConfidence);
  }
  pub(T_NAV,b);
  Serial.printf("[NAV] %s\n",b);
}

static void stationPublish(const char* ev,int16_t off,const char* note){
  char b[256];
  const char* nm = (stIndex>=0 && stIndex<(int8_t)STATION_COUNT) ? STATIONS[stIndex].name : "NONE";
  snprintf(b,sizeof(b),
    "{\"event\":\"%s\",\"phase\":%u,\"station\":\"%s\",\"offset\":%d,"
    "\"commanded_pwm\":%d,\"actual_pwm\":%d,\"note\":\"%s\"}",
    ev,(unsigned)stPhase,nm,off,commandedPwm,actualPwm,note);
  pub(T_STATION,b);
  Serial.printf("[STN] %s\n",b);
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
    "\"nav\":\"%s\",\"mm\":%u,\"conf\":%d,\"pwm\":%d}",
    loopMaxGapMs,(unsigned long)taskMaxGapMs,(unsigned long)(now-taskLastRunMs),
    baselineCounts,(int)lastRaw,(int)lastRaw-baselineCounts,
    queueDrops,floorRejects,
    navState==NAV_TRACKING?"TRACKING":(navState==NAV_LOST?"LOST":"UNSET"),
    navMm,navConfidence,actualPwm);
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
    navDir = (!strcasecmp(msg,"CW"))?MAP_CW:((!strcasecmp(msg,"CCW"))?MAP_CCW:MAP_UNSET);
    sessionDir=navDir;
    stationReset("SESSION_DIRECTION_SET");
    navPublishState("SESSION_DIRECTION",nullptr);
  }
  else if(!strcmp(topic,T_CMD_STARTMM)){
    int mm=atoi(msg);
    if(mm>=0 && mm<DNA_N && navDir!=MAP_UNSET) navDeclare((uint8_t)mm,navDir);
  }
  else if(!strcmp(topic,T_CMD_AUTO)){
    autoEnrolled=(atoi(msg)!=0);
    if(!autoEnrolled){ autoRunning=false; requestPwm(0,NORMAL_STEP_MS); }
    stationReset("AUTO_CHANGED");
  }
  else if(!strcmp(topic,T_CMD_GO)){
    if(autoEnrolled && navState==NAV_TRACKING && navDir!=MAP_UNSET && !autoRunning){
      autoRunning=true; estopped=false;
      requestPwm(CRUISE_PWM,NORMAL_STEP_MS);
      stationPublish("GO",0,"LAUNCH");
    }
  }
  else if(!strcmp(topic,T_CMD_STOP)){
    autoRunning=false; requestPwm(0,NORMAL_STEP_MS);
    stationReset("DISPATCHER_STOP");
  }
  else if(!strcmp(topic,T_CMD_ESTOP)){
    estopped=(atoi(msg)!=0);
    if(estopped){ autoRunning=false; commandedPwm=0; actualPwm=0; pwmWriteCompat(0); }
  }
  else if(!strcmp(topic,T_CMD_THROTTLE)){
    if(!autoRunning) requestPwm(atoi(msg),NORMAL_STEP_MS);   // manual only
  }
}

static void subscribeAll(){
  mqtt.subscribe(T_CMD_AUTO);     mqtt.subscribe(T_CMD_GO);
  mqtt.subscribe(T_CMD_STOP);     mqtt.subscribe(T_CMD_DIR);
  mqtt.subscribe(T_CMD_SESSDIR);  mqtt.subscribe(T_CMD_STARTMM);
  mqtt.subscribe(T_CMD_ESTOP);    mqtt.subscribe(T_CMD_THROTTLE);
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
  mqtt.setServer(MQTT_BROKER,1883);
  mqtt.setSocketTimeout(2);
  mqtt.setCallback(onMqtt);
  mqtt.setBufferSize(1024);

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
  serviceStations();
  servicePwmRamp();
  publishStat();
}
