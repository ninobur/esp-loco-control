// ============================================================================
// NGR_LL_PWM_DNA_AUTO_1_0.ino
//
// Single-locomotive Low Line AUTO validation firmware.
//
// Architecture:
//   * Hall/DNA and the 171-marker map determine location and direction only.
//   * spacingMm[] and Hall dt are retained for geometry and diagnostics only.
//   * Hall timing and calculated pKPH have NO motor-control authority.
//   * AUTO speed is prescribed entirely by PWM targets and time-based ramps.
//   * Cruise PWM: 90.
//   * Station approach: M-10..M-6, subtract 6 PWM per marker to PWM 60.
//   * Hold PWM 60 from M-5 through station midpoint M.
//   * At M begin a 150 ms/PWM ramp toward PWM 25.
//   * At M+2 set target PWM 0 with a 300 ms/PWM final coast-down ramp.
//   * Every station is a mandatory stop; fixed 15-second dwell.
//   * No CTO, peer interaction, bubble logic, station skipping, or adaptive
//     Hall-to-Hall speed governor is active in this series.
//
// CTO-ready separation is preserved: a later authorization layer can decide
// whether movement is permitted without changing navigation, station geometry,
// or the prescribed PWM profiles proven here.
// ============================================================================
#include "LocoConfig.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <pgmspace.h>
#include <string.h>
#include <math.h>

#define SKETCH_NAME "NGR_LL_PWM_DNA_AUTO_1_0"
#define DNA_N 171
#define DNA_W_LOCK 12
#define DNA_W_MAX 18
#define DNA_UNKNOWN 255
#define HALL_PIN 33
#define ADC_SAMPLES 8
#define CALIBRATION_TIME_MS 2000UL
#define DEBOUNCE_HOLD_MS 20UL
#define HALL_EVENT_EXIT_HOLD_MS 20UL
#define MOTOR_DEAD_ZONE_PWM 20
#define INA219_TELEM_INTERVAL_MS 5000UL
#define LOW_VOLTAGE_THRESHOLD_V 14.4f
#define MQTT_BROKER "192.168.68.142"
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID LOCO_NAME

#ifndef HALL_DEADBAND_COUNTS
#define HALL_DEADBAND_COUNTS 50
#endif
#ifndef HALL_ENTRY_MARGIN_COUNTS
  #ifdef HALL_TRIGGER_MARGIN_COUNTS
    #define HALL_ENTRY_MARGIN_COUNTS HALL_TRIGGER_MARGIN_COUNTS
  #else
    #define HALL_ENTRY_MARGIN_COUNTS 80
  #endif
#endif
#ifndef HALL_MIN_PEAK_DELTA
  #ifdef HALL_EVENT_MIN_PEAK_DELTA_COUNTS
    #define HALL_MIN_PEAK_DELTA HALL_EVENT_MIN_PEAK_DELTA_COUNTS
  #else
    #define HALL_MIN_PEAK_DELTA 80
  #endif
#endif
#ifndef HALL_DOMINANCE_PERCENT
#define HALL_DOMINANCE_PERCENT 120U
#endif

// Minimum accepted Hall event duration. Real magnet traversals produce
// hallms well above the exit-hold constant (observed 106-2103ms). Spurious
// threshold-flicker events cluster at 20-25ms (essentially HALL_EVENT_EXIT_HOLD_MS
// with zero field-traversal time). This gate rejects the flicker population by
// duration before any navigation consumption. Amplitude cannot discriminate:
// spurious peaks (38-54) overlap legitimate weak-magnet peaks (39-67).
// Per-loco override via LocoConfig; default suits fastest expected running.
#ifndef HALL_MIN_EVENT_MS
#define HALL_MIN_EVENT_MS 40UL
#endif

// Adaptive Hall baseline tracker. Decoder exit/re-arm still uses the narrow
// HALL_DEADBAND_COUNTS band. This broader tracking window is only for slow
// quiescent baseline drift and is intentionally far below real magnet peaks.
#ifndef HALL_BASELINE_TRACK_K
#define HALL_BASELINE_TRACK_K 128L
#endif
#ifndef HALL_BASELINE_TRACK_WINDOW_COUNTS
#define HALL_BASELINE_TRACK_WINDOW_COUNTS 140
#endif
#ifndef HALL_BASELINE_STABLE_DELTA_COUNTS
#define HALL_BASELINE_STABLE_DELTA_COUNTS 6
#endif
#ifndef HALL_BASELINE_STABLE_SAMPLES
#define HALL_BASELINE_STABLE_SAMPLES 4
#endif
#ifndef HALL_BASELINE_TELEM_INTERVAL_MS
#define HALL_BASELINE_TELEM_INTERVAL_MS 5000UL
#endif

enum HallState : uint8_t { HALL_NONE=0, HALL_NORTH=1, HALL_SOUTH=2 };
enum MapDirection : int8_t { MAP_UNSET=0, MAP_CW=1, MAP_CCW=-1 };
enum MotionTimingState : uint8_t { STOPPED_INFERRED=0, RESTARTING=1, SPEED_TRANSITION=2, MOVING=3 };
enum PositionState : uint8_t { POSITION_UNKNOWN=0, POSITION_DECLARED, POSITION_EXACT_LOCKED, POSITION_FAULT };
enum DirectionState : uint8_t { DIRECTION_UNKNOWN=0, DIRECTION_DECLARED, DIRECTION_CONFIRMED };

// Must precede Arduino-generated function prototypes.
enum StationTestPhase : uint8_t {
  STATION_TEST_IDLE=0,
  STATION_TEST_STOP_SERVICE,
  STATION_TEST_FINAL_RAMP,
  STATION_TEST_DWELL,
  STATION_TEST_DEPARTING,
  STATION_TEST_THROUGH_SERVICE
};
enum StationVisitKind : uint8_t { STATION_VISIT_NONE=0, STATION_VISIT_STOP, STATION_VISIT_THROUGH };

// Forward declaration required because Arduino generates function prototypes before later type definitions.
struct StationDefinition;

struct DnaCandidate { uint8_t mm; int8_t dir; };
struct DnaMatchResult { uint8_t count; DnaCandidate first; };

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

static inline uint8_t routeMod(int32_t v) { v %= DNA_N; if (v < 0) v += DNA_N; return (uint8_t)v; }
static inline uint8_t dnaAt(uint8_t mm) { return pgm_read_byte(&NGR_DNA1[mm % DNA_N]); }
static inline char dnaPolChar(uint8_t pol) { return pol ? 'N' : 'S'; }
static uint8_t stepMm(uint8_t mm, int8_t dir, uint8_t steps=1) { if (dir==MAP_CW) return routeMod((int32_t)mm+steps); if (dir==MAP_CCW) return routeMod((int32_t)mm-steps); return mm; }
static int8_t oppositeMapDir(int8_t d) { return d==MAP_CW?MAP_CCW:(d==MAP_CCW?MAP_CW:MAP_UNSET); }
static const char* mapDirName(int8_t d) { return d==MAP_CW?"CW":(d==MAP_CCW?"CCW":"UNSET"); }
static const char* posStateName(PositionState s) { switch(s){case POSITION_UNKNOWN:return "UNKNOWN";case POSITION_DECLARED:return "DECLARED";case POSITION_EXACT_LOCKED:return "EXACT_LOCKED";default:return "FAULT";} }
static const char* dirStateName(DirectionState s) { switch(s){case DIRECTION_UNKNOWN:return "UNKNOWN";case DIRECTION_DECLARED:return "DECLARED";default:return "CONFIRMED";} }
static const char* motionName(MotionTimingState s) { switch(s){case STOPPED_INFERRED:return "STOPPED_INFERRED";case RESTARTING:return "RESTARTING";case SPEED_TRANSITION:return "SPEED_TRANSITION";default:return "MOVING";} }
static const char* dnaLandmarkName(uint8_t mm) { switch(mm%DNA_N){case 0:return "Southpoint";case 15:return "Patio";case 63:return "Grillers";case 72:return "Westpoint";case 98:return "Northpoint";case 107:return "Arches";case 140:return "Eastpoint";case 157:return "Bamboo";default:return "";} }

static const uint16_t spacingMm[171] PROGMEM = {
  // MM000-MM009
  330, 340, 330, 315, 325, 330, 315, 300, 300, 295,
  // MM010-MM019
  300, 290, 300, 315, 315, 325, 310, 300, 300, 320,
  // MM020-MM029
  315, 315, 305, 300, 295, 300, 300, 300, 300, 315,
  // MM030-MM039
  330, 320, 315, 310, 300, 300, 300, 300, 300, 300,
  // MM040-MM049
  300, 300, 300, 300, 300, 300, 300, 300, 300, 300,
  // MM050-MM059
  300, 300, 300, 300, 300, 300, 300, 295, 320, 300,
  // MM060-MM069
  315, 320, 315, 325, 315, 305, 300, 305, 300, 300,
  // MM070-MM079
  295, 295, 300, 300, 300, 300, 300, 300, 300, 305,
  // MM080-MM089
  300, 300, 305, 300, 300, 330, 300, 300, 305, 300,
  // MM090-MM099
  300, 300, 300, 300, 300, 300, 300, 300, 300, 300,
  // MM100-MM109
  300, 300, 295, 300, 300, 300, 300, 300, 305, 300,
  // MM110-MM119
  300, 320, 320, 300, 300, 300, 300, 300, 300, 300,
  // MM120-MM129
  300, 300, 300, 300, 300, 300, 280, 300, 300, 290,
  // MM130-MM139
  300, 300, 300, 300, 300, 300, 300, 300, 300, 300,
  // MM140-MM149
  300, 300, 300, 300, 305, 300, 305, 300, 295, 300,
  // MM150-MM159
  300, 300, 305, 300, 300, 305, 320, 290, 320, 300,
  // MM160-MM169
  300, 305, 330, 330, 320, 325, 315, 355, 330, 330,
  // MM170 (MM170→MM000)
  330
};

static uint16_t segmentDistanceMm(uint8_t fromMM,uint8_t toMM,int8_t dir) {
  if(fromMM>=DNA_N||toMM>=DNA_N) return 0;
  if(dir==MAP_CW) { if(toMM!=routeMod((int32_t)fromMM+1)) return 0; return pgm_read_word(&spacingMm[fromMM]); }
  if(dir==MAP_CCW) { if(toMM!=routeMod((int32_t)fromMM-1)) return 0; return pgm_read_word(&spacingMm[toMM]); }
  return 0;
}
// Navigation-only helper reserved for station-distance decisions. It never selects PWM.
static uint16_t distanceAheadMm(uint8_t fromMM,uint8_t toMM,int8_t dir) {
  if(dir==MAP_UNSET) return 0; uint16_t total=0; uint8_t p=fromMM; uint16_t guard=0;
  while(p!=toMM && guard++<DNA_N){ uint8_t n=stepMm(p,dir); uint16_t s=segmentDistanceMm(p,n,dir); if(!s) return 0; total+=s; p=n; }
  return (p==toMM)?total:0;
}

// MQTT topics
static char TOPIC_ONLINE[48],TOPIC_CMD_THROTTLE[56],TOPIC_CMD_DIRECTION[56],TOPIC_CMD_BRAKE[56],TOPIC_CMD_ESTOP[56],TOPIC_CMD_AUTO[56],TOPIC_CMD_DISPATCHER_RELEASE[72],TOPIC_CMD_SESSION_DIRECTION[64],TOPIC_CMD_START_INTERVAL[64],TOPIC_CMD_START_MM[56],TOPIC_CMD_DNA_RESET[64],TOPIC_CMD_MHE[64];
static char TOPIC_STATE_THROTTLE[56],TOPIC_STATE_DIRECTION[56],TOPIC_STATE_BRAKE[56],TOPIC_STATE_ESTOP[56],TOPIC_STATE_AUTO[56],TOPIC_STATE_GOV[56],TOPIC_STATE_STATION[56],TOPIC_STATE_SESSION_DIRECTION[64],TOPIC_STATE_START_INTERVAL[64],TOPIC_STATE_START_MM[56],TOPIC_STATE_MHE[64],TOPIC_STATE_WARNING[56],TOPIC_STATE_DNA[56],TOPIC_STATE_HALL[56],TOPIC_DNA_EVENT[56],TOPIC_MM_MAG[56],TOPIC_MM_POS[56],TOPIC_MM_MOTION[56],TOPIC_MM_SPEED[56],TOPIC_TELEM_VOLTAGE[56],TOPIC_TELEM_CURRENT[56],TOPIC_TELEM_POWER[56],TOPIC_STATE_LOWVOLT[56],TOPIC_STATE_CTO[56],TOPIC_STATE_CTO_BOUNDARY[64],TOPIC_STATE_TRAFFIC[64],TOPIC_STATE_CTO_RADIO[64],TOPIC_STATE_CTO_LEGACY[56],TOPIC_STATE_BOOT[64];
static WiFiClient mqttWifiClient; static PubSubClient mqtt(mqttWifiClient);

// ---------------------------------------------------------------------------
// AUTO 4.0 Dispatcher protocol.  This is intentionally the same compact
// ESP-NOW packet layout and CMD_START/CMD_STOP vocabulary used by the proven
// GoldCore Dispatcher.  The Dispatcher grants movement permission only;
// speed remains a locomotive decision.
// ---------------------------------------------------------------------------
static const uint8_t ESPNOW_MAGIC=0x4E;
static const uint8_t ESPNOW_VERSION=14;
static const uint8_t ESPNOW_MSG_MANUAL=4;
static const uint8_t ESPNOW_MSG_ESTOP=3;
static const uint8_t ESPNOW_CMD_START=1;
static const uint8_t ESPNOW_CMD_STOP=2;
static const uint8_t ESPNOW_CMD_ESTOP=3;
static const uint32_t ESPNOW_BROADCAST_ID=0UL;
typedef struct __attribute__((packed)) {
  uint8_t magic; uint8_t version; uint8_t msgType; uint32_t senderId;
  uint32_t locoId; uint8_t blockId; uint8_t cmd; uint8_t flags;
  uint32_t sequenceNumber; uint32_t encounterId; uint16_t value;
  uint8_t roleId; uint8_t paramId;
} TrainPacket;

// ---------------------------------------------------------------------------
// CTO r11 universal traffic truth.  This packet carries only the sender's own
// operational facts.  No peer transmits a brake point, motor command, pair
// assignment, leader/follower identity, or station instruction.
// ---------------------------------------------------------------------------
static const uint8_t CTO2_MAGIC=0xC4;
static const uint8_t CTO2_VERSION=3;
static const uint32_t CTO2_STATUS_INTERVAL_MS=500UL;
static const uint32_t CTO2_PEER_STALE_MS=3000UL;
static const uint32_t CTO2_RADIO_HEALTH_INTERVAL_MS=20000UL;
static const uint8_t MAX_CTO_PEERS=8;

// Standard Phase-1 consist geometry.  Boundaries are marker-count geometry;
// physical segment distances remain reserved for measured-speed calculations.
static const uint8_t CTO2_STANDARD_FRONT_OFFSET_MM=5;
static const uint8_t CTO2_STANDARD_REAR_OFFSET_MM=5;
static const uint8_t TRAFFIC_TOUCH_HALL_GAP_MM=(uint8_t)(
  CTO2_STANDARD_FRONT_OFFSET_MM + CTO2_STANDARD_REAR_OFFSET_MM);
static const uint8_t TRAFFIC_RESTART_HALL_GAP_MM=12;
static const uint8_t TRAFFIC_DECEL_START_EDGE_GAP_MM=8;
static const int16_t TRAFFIC_DECEL_START_OFFSET_MM=-(int16_t)TRAFFIC_DECEL_START_EDGE_GAP_MM;

// MHE is dispatcher policy for station synchronization only.  It does not
// change universal proximity: every same-direction locomotive remains a
// physical occupancy peer whether MHE is enabled or not.
// Dispatcher-set attribute.  Default OFF prevents an unconfigured locomotive
// from creating a station synchronization obligation.
static bool mustHoldEligible=false;
static bool stationHoldingForRear=false;
static uint32_t stationHoldRearId=0;

// Persistent operational navigation truth.  Dispatcher declaration is a
// valid starting reference; odometry/Hall/DNA later refine it.  A single
// ambiguous or rejected observation cannot erase this state.
enum CtoTruthSource : uint8_t {
  CTO_TRUTH_NONE=0,
  CTO_TRUTH_DISPATCHER_DECLARED=1,
  CTO_TRUTH_MAGNET_COUNTED=2,
  CTO_TRUTH_DNA_REFINED=3
};
static const char* ctoTruthSourceName(uint8_t s){
  switch(s){
    case CTO_TRUTH_DISPATCHER_DECLARED:return "DISPATCHER_DECLARED";
    case CTO_TRUTH_MAGNET_COUNTED:return "MAGNET_COUNTED";
    case CTO_TRUTH_DNA_REFINED:return "DNA_REFINED";
    default:return "NONE";
  }
}
struct OperationalNavState {
  bool valid=false;
  uint8_t mm=DNA_UNKNOWN;
  int8_t mapDir=MAP_UNSET;
  uint8_t source=CTO_TRUTH_NONE;
  uint32_t updatedAtMs=0;
};
static OperationalNavState opNav;

// Traffic is not a role.  It is a temporary interruption to the local
// station mission.  TRAFFIC_WAIT is a stopped rear locomotive waiting for
// its current obstruction to open 12 Hall-to-Hall markers.
enum TrafficPhase : uint8_t {
  TRAFFIC_CLEAR=0,
  TRAFFIC_STOPPED_LEAD_APPROACH=1,
  TRAFFIC_FINAL_RAMP=2,
  TRAFFIC_WAIT_FOR_CLEARANCE=3
};
static TrafficPhase trafficPhase=TRAFFIC_CLEAR;
static bool trafficFinalRampActive=false;
static uint32_t trafficBlockerId=0;
static uint8_t trafficBlockerRearBoundaryMm=DNA_UNKNOWN;
static uint8_t trafficFinalHallMm=DNA_UNKNOWN;
static int16_t trafficLastOffsetMm=999;
static uint32_t trafficLastNearestAheadId=0;
static uint32_t trafficStoppedAtMs=0;
static uint32_t trafficStopBlockerSequence=0;
static uint8_t trafficStopBlockerHallMm=DNA_UNKNOWN;
static uint8_t trafficStopBlockerRearBoundaryMm=DNA_UNKNOWN;

// Packet field order is versioned.  All r11 locomotives must run this same
// version; r9/below packets are intentionally rejected rather than guessed.
typedef struct __attribute__((packed)) {
  uint8_t magic;
  uint8_t version;
  uint32_t senderId;
  uint32_t sequence;
  uint8_t hallMm;
  uint8_t frontBoundaryMm;
  uint8_t rearBoundaryMm;
  int8_t mapDir;
  uint8_t autoMode;
  uint8_t running;
  uint8_t motionState;
  uint8_t rampPwm;
  uint8_t speedValid;
  uint16_t lastMoveAgeDs;
  uint16_t speedX10;
  uint8_t frontOffset;
  uint8_t rearOffset;
  uint8_t truthSource;
  uint8_t stationPhase;
  uint8_t trafficPhase;
  uint8_t mustHoldEligible;
  uint32_t trafficStopForId;
  uint32_t senderRxAccepted;
  uint32_t senderTxAttempts;
  uint32_t senderTxImmediateErrors;
} CtoPeerPacket;

struct PeerEntry {
  bool seen=false;
  bool freshPreviously=false;
  uint32_t locoId=0;
  uint32_t sequence=0;
  uint32_t receivedAtMs=0;
  uint8_t mm=DNA_UNKNOWN;
  uint8_t frontBoundaryMm=DNA_UNKNOWN;
  uint8_t rearBoundaryMm=DNA_UNKNOWN;
  int8_t mapDir=MAP_UNSET;
  bool autoMode=false;
  bool running=false;
  uint8_t motionState=STOPPED_INFERRED;
  uint8_t rampPwm=0;
  bool speedValid=false;
  uint16_t lastMoveAgeDs=0;
  float speedPkph=0.0f;
  uint8_t frontOffset=0;
  uint8_t rearOffset=0;
  uint8_t truthSource=CTO_TRUTH_NONE;
  StationTestPhase stationPhase=STATION_TEST_IDLE;
  TrafficPhase trafficPhase=TRAFFIC_CLEAR;
  bool mustHoldEligible=false;
  uint32_t trafficStopForId=0;
  uint32_t peerRxAccepted=0;
  uint32_t peerTxAttempts=0;
  uint32_t peerTxImmediateErrors=0;
};
static PeerEntry peerRegistry[MAX_CTO_PEERS];
static uint32_t ctoTxSequence=0;
static uint32_t ctoLastStatusTxMs=0;
static bool ctoStatusDirty=true;
static uint32_t ctoTxAttempts=0;
static uint32_t ctoTxImmediateErrors=0;
static uint32_t ctoRxAccepted=0;
static uint32_t ctoRxBadMagicVersion=0;
static uint32_t ctoRxSelf=0;
static uint32_t ctoRxBadLength=0;
static uint32_t ctoLastRadioHealthMs=0;

// Motor command state. PWM remains the only motor actuator.
static bool eStopEngaged=false; static int brakeValue=0, direction=DIRECTION_NEUTRAL;
static int commandedPwm=0,rampTarget=0,rampCurrent=0; static unsigned long lastRampMs=0;
static uint16_t prescribedRampStepMs=NORMAL_PROFILE_STEP_MS;
static void requestPrescribedPwm(int target,uint16_t stepMs){
  commandedPwm=constrain(target,0,255);
  prescribedRampStepMs=stepMs;
  rampTarget=(eStopEngaged||direction==DIRECTION_NEUTRAL)?0:commandedPwm;
}

// Dispatcher authority is voluntarily enlisted locally. GO/STOP arrive by
// ESP-NOW. GO begins physical forward motion; the governor assumes speed
// control only after valid physical magnet-to-magnet feedback arrives.
static bool dispatcherAuto=false;
static bool running=false;

// Measured-speed governor. Magnet-to-magnet physical speed is the only
// feedback used to choose AUTO PWM.
enum SpeedGovMode : uint8_t { SPEED_GOV_IDLE, SPEED_GOV_PROVING_MOVEMENT, SPEED_GOV_STATION_DEPARTURE_PROVING, SPEED_GOV_STARTUP, SPEED_GOV_MAINTAIN };
static SpeedGovMode speedGovMode=SPEED_GOV_IDLE;
static float speedHistPkph[2]={0.0f,0.0f};
static uint8_t speedHistCount=0;
static uint32_t latestSegmentDtMs=0;
static bool speedRampActive=false;
static uint32_t speedRampStepMs=0;
static uint8_t speedRampStepsRemaining=0;
static const uint8_t LAUNCH_NO_MAGNET_PWM_LIMIT=100;
static const float STARTUP_HANDOFF_FRACTION=0.90f;
static const float MAINTAIN_HOLD_BAND_PKPH=0.50f;
static const float MAINTAIN_ONE_PWM_BAND_PKPH=1.50f;
static const float MAINTAIN_THREE_PWM_BAND_PKPH=3.00f;
static const float MAINTAIN_FOUR_PWM_BAND_PKPH=5.00f;
static const float TREND_VETO_PKPH=0.75f;

// Diagnostic conversion only. Calculated pKPH is published but never controls PWM.
static const float PKPH_MM_PER_SEC=5.37325f;

// Prescribed PWM operating profile.
static const uint8_t CRUISE_PWM=90;
static const uint8_t STATION_ZONE_PWM=60;
static const uint8_t FINAL_APPROACH_PWM=25;
static const uint16_t NORMAL_PROFILE_STEP_MS=150;
static const uint16_t FINAL_STOP_STEP_MS=300;
static const uint32_t STATION_DWELL_MS=15000UL;
static const int16_t APPROACH_START_OFFSET=-10;
static const int16_t STATION_ZONE_START_OFFSET=-5;
static const int16_t FINAL_APPROACH_START_OFFSET=0;
static const int16_t FINAL_ZERO_TRIGGER_OFFSET=2;

// ---------------------------------------------------------------------------
// Four-station Goldcore service: every station is a STOP visit.
//
// Round 1 (odd):  stop at Griller's and Bamboo; pass Patio and Arches at 25.
// Round 2 (even): stop at Patio and Arches; pass Griller's and Bamboo at 25.
// The round begins odd on each new AUTO/navigation session and toggles only
// after a completed traversal of MM000.  Every station uses the same
// direction-aware relative-offset template.
// ---------------------------------------------------------------------------
// Station identity is only its reusable center marker.  All approach and stop
// phases are derived from signed marker-relative offsets at runtime.
struct StationDefinition {
  const char* name;
  uint8_t centerMm;
};
static const StationDefinition STATIONS[] = {
  {"Patio",    15},
  {"Grillers", 63},
  {"Arches",  107},
  {"Bamboo",  157}
};
static const uint8_t STATION_COUNT=(uint8_t)(sizeof(STATIONS)/sizeof(STATIONS[0]));

// Hall evidence duration gate retained for navigation quality during launch.
static const uint32_t LAUNCH_MIN_HALL_EVENT_MS=80UL;

static StationTestPhase stationTestPhase=STATION_TEST_IDLE;
static StationVisitKind stationVisitKind=STATION_VISIT_NONE;
static int8_t stationActiveIndex=-1;
static bool stationFinalRampActive=false;
// Fixed station target remains M+2 in r9.  These variables are retained only
// because the proven departure/final-ramp scaffolding shares them.
static int16_t stationFinalRampOffset=STATION_STOP_OFFSET;
static bool stationDwellClockRunning=false;
// A station departure is a known-location proving ramp.  It never holds at
// a fixed PWM: it continues at 300 ms/PWM through two post-stop Hall hits.
static bool stationDepartureLaunchActive=false;
static uint8_t stationDeparturePostStopHits=0;
static uint32_t stationDwellStartedAtMs=0;
static uint8_t stationCompletedVisits=0;
static bool stationRoundOdd=true; // retained only for backward-compatible telemetry
static bool stationRoundSeenMagnet=false;
static uint8_t stationRoundLastMm=0;

// Hall state
static int baselineCounts=0,northEnterThreshold=0,northExitThreshold=0,southEnterThreshold=0,southExitThreshold=0;
static int32_t baselineCountsQ8=0;
static bool hallEventActive=false; static unsigned long hallEventStartedAtMs=0,hallBaselineReturnAtMs=0; static int hallPeakNorthDelta=0,hallPeakSouthDelta=0;
static int hallBaselineLastRaw=0; static uint8_t hallBaselineStableSamples=0; static uint8_t hallBaselineUnstableSamples=0; static int hallBaselineLastStepCounts=0; static unsigned long hallBaselineTelemLastMs=0; static const char* hallBaselineTrackerState="BOOT"; static const char* hallBaselineFrozenReason="BOOT";
// Navigation evidence state
static int8_t declaredSessionDir=MAP_UNSET,activeMapDir=MAP_UNSET; static bool navReady=false,startIntervalSet=false; static bool startReferenceIsExactMm=false; static uint8_t startIntervalA=0,startIntervalB=0,startNextMm=0;
static bool odomValid=false; static uint8_t odomMm=0,lastExactMm=0;
static PositionState positionState=POSITION_UNKNOWN; static DirectionState directionState=DIRECTION_UNKNOWN;
static uint8_t dnaBuf[DNA_W_MAX]; static uint8_t dnaHead=0,dnaCount=0; static unsigned long lastMagnetTime=0; static bool haveLastMagnet=false;
static MotionTimingState motionTimingState=STOPPED_INFERRED; static uint32_t rampAccum=0; static uint16_t rampSamples=0; static uint8_t rampAvgPwm=0,prevRampAvgPwm=0;
static uint32_t lastMovementEvidenceMs=0;
static bool reversalRehitPending=false; static uint8_t reversalAnchorMm=0,reversalAnchorPol=DNA_UNKNOWN;

// Odometry running + a known travel direction under AUTO/GO is sufficient to
// arm and execute station service.  DNA exact-lock may still correct the
// odometer via DNA_EXACT_LOCK_ODOM_CORRECTION; it does not cancel the
// assigned service.
static bool stationServiceReady(){
  return dispatcherAuto && running && opNav.valid && opNav.mapDir!=MAP_UNSET;
}

// AUTO 5.1 speed feedback needs a known physical segment and direction, not
// exact DNA lock.  The declared start interval anchors the first post-launch
// full magnet-to-magnet segment; later DNA lock remains navigation truth only.
static bool speedGovernorMeasurementReady(){
  return startIntervalSet &&
         declaredSessionDir!=MAP_UNSET &&
         activeMapDir!=MAP_UNSET &&
         odomValid &&
         motionTimingState!=STOPPED_INFERRED &&
         rampCurrent>MOTOR_DEAD_ZONE_PWM;
}

static bool launchEvidenceGateActive(){
  return dispatcherAuto && running;
}

static bool launchHallEvidenceTooWeak(unsigned long hallMs){
  return launchEvidenceGateActive() &&
         (rampCurrent<=MOTOR_DEAD_ZONE_PWM ||
          motionTimingState==STOPPED_INFERRED ||
          hallMs<LAUNCH_MIN_HALL_EVENT_MS);
}

// Forward declarations because AUTO helpers precede MQTT/motor definitions.
static void mqttPublish(const char* t,const char* p,bool retained);
static void mqttPublishInt(const char* t,int v,bool retained);
static void mqttWarn(const char* m);
static void updateMotorAuthority();
static void publishSimpleStates();
static void applyDirectionPin();
static void clearRollingEvidence();
static bool startAutoSequence();
static void ctoService();
static void ctoPublishStatus();
static void ctoPublishState(const char* event,const char* note="");
static void ctoPublishCommand(const char* cmd,uint32_t seq,const char* action,const char* reason);
static void ctoPublishBoot(const char* reason);
static void operationalNavRefresh();
static void operationalNavClear();
static uint8_t ctoFrontBoundaryMm(uint8_t hallMm,int8_t travelDir);
static uint8_t ctoRearBoundaryMm(uint8_t hallMm,int8_t travelDir);
static int16_t stationRelativeOffsetForCenter(uint8_t mm,int8_t dir,uint8_t centerMm);
static uint8_t stationObjectiveHallMm();
static bool trafficOnMagnet(uint8_t mm,int8_t dir);
static float trafficGovernorTargetPkph();
static bool trafficControlsGovernor();
static float effectiveGovernorTargetPkph();
static void serviceTrafficControl();
static float stationGovernorTargetPkph();
static const char* stationPhaseName(StationTestPhase p);
static void stationCompleteDeparture(const char* why);

static const char* speedGovModeName(uint8_t m){
  switch(m){
    case SPEED_GOV_PROVING_MOVEMENT:return "PROVING_MOVEMENT";
    case SPEED_GOV_STATION_DEPARTURE_PROVING:return "STATION_DEPARTURE_PROVING";
    case SPEED_GOV_STARTUP:return "STARTUP";
    case SPEED_GOV_MAINTAIN:return "MAINTAIN";
    default:return "IDLE";
  }
}

static void speedGovClearHistory(){
  speedHistPkph[0]=speedHistPkph[1]=0.0f;
  speedHistCount=0;
  latestSegmentDtMs=0;
  speedRampActive=false;
  speedRampStepMs=0;
  speedRampStepsRemaining=0;
}

// A new magnet is reality.  Abandon any unfinished prediction and begin the
// next decision from the PWM that was actually applied at this magnet.
static void speedGovCancelPendingRamp(){
  speedRampActive=false;
  speedRampStepsRemaining=0;
  commandedPwm=rampCurrent;
  updateMotorAuthority();
}

// ----- CTO r11 operational truth, registry, and proximity helpers ---------
static uint8_t forwardMarkerDistance(uint8_t from,uint8_t to,int8_t dir){
  return (dir==MAP_CW)?routeMod((int32_t)to-from):routeMod((int32_t)from-to);
}

static uint8_t ctoFrontBoundaryMm(uint8_t hallMm,int8_t travelDir){
  return (hallMm==DNA_UNKNOWN || travelDir==MAP_UNSET) ? DNA_UNKNOWN
    : stepMm(hallMm,travelDir,CTO2_STANDARD_FRONT_OFFSET_MM);
}
static uint8_t ctoRearBoundaryMm(uint8_t hallMm,int8_t travelDir){
  return (hallMm==DNA_UNKNOWN || travelDir==MAP_UNSET) ? DNA_UNKNOWN
    : stepMm(hallMm,oppositeMapDir(travelDir),CTO2_STANDARD_REAR_OFFSET_MM);
}
static uint8_t operationalFrontBoundaryMm(){
  return opNav.valid ? ctoFrontBoundaryMm(opNav.mm,opNav.mapDir) : DNA_UNKNOWN;
}
static uint8_t operationalRearBoundaryMm(){
  return opNav.valid ? ctoRearBoundaryMm(opNav.mm,opNav.mapDir) : DNA_UNKNOWN;
}

static void operationalNavSet(uint8_t mm,int8_t dir,uint8_t source){
  if(mm>=DNA_N || dir==MAP_UNSET) return;
  const bool changed=!opNav.valid || opNav.mm!=mm || opNav.mapDir!=dir || opNav.source!=source;
  opNav.valid=true;
  opNav.mm=mm;
  opNav.mapDir=dir;
  opNav.source=source;
  opNav.updatedAtMs=millis();
  if(changed) ctoStatusDirty=true;
}
static bool dispatcherOperationalTruthAvailable(){
  return startIntervalSet && declaredSessionDir!=MAP_UNSET && startNextMm<DNA_N;
}
// startNextMm remains the first marker expected by the sequential odometer.
// The declared traffic reference is different for an interval: it is the last
// marker passed in the selected direction, exactly as agreed.
static uint8_t dispatcherDeclaredReferenceMm(){
  if(!dispatcherOperationalTruthAvailable()) return DNA_UNKNOWN;
  return startReferenceIsExactMm ? startNextMm : stepMm(startNextMm,oppositeMapDir(declaredSessionDir));
}
static void operationalNavClear(){
  opNav.valid=false;
  opNav.mm=DNA_UNKNOWN;
  opNav.mapDir=MAP_UNSET;
  opNav.source=CTO_TRUTH_NONE;
  opNav.updatedAtMs=millis();
  ctoStatusDirty=true;
}
static void operationalNavRefresh(){
  // Current odometry is the best operational marker reference.  It is already
  // progressed directionally for every accepted Hall event even when DNA is
  // still ambiguous or a contrary polarity is being logged.
  if(odomValid && activeMapDir!=MAP_UNSET){
    operationalNavSet(odomMm,activeMapDir,
      positionState==POSITION_EXACT_LOCKED ? CTO_TRUTH_DNA_REFINED : CTO_TRUTH_MAGNET_COUNTED);
    return;
  }
  // Setup declaration is valid truth, not a provisional permission request.
  if(dispatcherOperationalTruthAvailable()){
    operationalNavSet(dispatcherDeclaredReferenceMm(),declaredSessionDir,CTO_TRUTH_DISPATCHER_DECLARED);
  }
}

static bool peerFreshIndex(uint8_t idx){
  if(idx>=MAX_CTO_PEERS) return false;
  const PeerEntry& p=peerRegistry[idx];
  return p.seen && (uint32_t)(millis()-p.receivedAtMs)<=CTO2_PEER_STALE_MS;
}
static int8_t peerIndexById(uint32_t locoId){
  for(uint8_t i=0;i<MAX_CTO_PEERS;i++) if(peerRegistry[i].seen && peerRegistry[i].locoId==locoId) return (int8_t)i;
  return -1;
}
static int8_t peerSlotForId(uint32_t locoId){
  const int8_t existing=peerIndexById(locoId);
  if(existing>=0) return existing;
  for(uint8_t i=0;i<MAX_CTO_PEERS;i++) if(!peerRegistry[i].seen) return (int8_t)i;
  // Registry is deliberately bounded.  Reclaim the stalest record only when
  // full; all normal NGR traffic remains below this capacity.
  uint8_t stalest=0;
  uint32_t oldestAge=0;
  for(uint8_t i=0;i<MAX_CTO_PEERS;i++){
    const uint32_t age=millis()-peerRegistry[i].receivedAtMs;
    if(age>=oldestAge){ oldestAge=age; stalest=i; }
  }
  return (int8_t)stalest;
}
static uint8_t freshPeerCount(){
  uint8_t count=0;
  for(uint8_t i=0;i<MAX_CTO_PEERS;i++) if(peerFreshIndex(i)) count++;
  return count;
}

// Peer ahead/behind classification uses the two boundary gaps around the loop.
// A peer is ahead only when the forward gap from self front to peer rear is
// smaller than the opposite gap from peer front back to self rear.
static bool peerIsAheadByBoundaries(uint8_t idx){
  if(idx>=MAX_CTO_PEERS || !opNav.valid || opNav.mapDir==MAP_UNSET) return false;
  const PeerEntry& p=peerRegistry[idx];
  if(!peerFreshIndex(idx) || p.mapDir!=opNav.mapDir ||
     p.rearBoundaryMm==DNA_UNKNOWN || p.frontBoundaryMm==DNA_UNKNOWN) return false;
  const uint8_t selfFront=operationalFrontBoundaryMm();
  const uint8_t selfRear=operationalRearBoundaryMm();
  if(selfFront==DNA_UNKNOWN || selfRear==DNA_UNKNOWN) return false;
  const uint8_t forwardGap=forwardMarkerDistance(selfFront,p.rearBoundaryMm,opNav.mapDir);
  const uint8_t reverseGap=forwardMarkerDistance(p.frontBoundaryMm,selfRear,opNav.mapDir);
  return forwardGap<reverseGap;
}

static bool peerGenuinelyStopped(uint8_t idx){
  if(idx>=MAX_CTO_PEERS || !peerFreshIndex(idx)) return false;
  const PeerEntry& p=peerRegistry[idx];
  if(p.running) return false;
  if(p.rampPwm>MOTOR_DEAD_ZONE_PWM) return false;
  if(p.motionState!=STOPPED_INFERRED) return false;
  return true;
}

// Nearest same-direction occupancy ahead is selected from physical boundaries,
// not a role, pair, or assigned partner.  All fresh locomotives are relevant.
static int8_t nearestSameDirectionAheadIndex(){
  if(!opNav.valid || opNav.mapDir==MAP_UNSET) return -1;
  const uint8_t selfFront=operationalFrontBoundaryMm();
  int8_t best=-1;
  uint8_t bestGap=DNA_UNKNOWN;
  for(uint8_t i=0;i<MAX_CTO_PEERS;i++){
    const PeerEntry& p=peerRegistry[i];
    if(!peerFreshIndex(i) || p.mapDir!=opNav.mapDir || p.rearBoundaryMm==DNA_UNKNOWN) continue;
    if(!peerIsAheadByBoundaries(i)) continue;
    const uint8_t gap=forwardMarkerDistance(selfFront,p.rearBoundaryMm,opNav.mapDir);
    if(best<0 || gap<bestGap){ best=(int8_t)i; bestGap=gap; }
  }
  return best;
}
static int16_t peerBoundaryGapMm(uint8_t idx){
  if(idx>=MAX_CTO_PEERS) return -1;
  const PeerEntry& p=peerRegistry[idx];
  if(!opNav.valid || opNav.mapDir==MAP_UNSET || p.rearBoundaryMm==DNA_UNKNOWN) return -1;
  return (int16_t)forwardMarkerDistance(operationalFrontBoundaryMm(),p.rearBoundaryMm,opNav.mapDir);
}
static uint8_t peerHallGapMm(uint8_t idx){
  if(idx>=MAX_CTO_PEERS) return DNA_UNKNOWN;
  const PeerEntry& p=peerRegistry[idx];
  return (!opNav.valid || p.mm==DNA_UNKNOWN || p.mapDir!=opNav.mapDir) ? DNA_UNKNOWN
    : forwardMarkerDistance(opNav.mm,p.mm,opNav.mapDir);
}

// MHE topology intentionally ignores MHE- locomotives on both sides.  It is
// used only after this locomotive has reached a normal station stop.
static int8_t nearestMheAheadIndex(){
  if(!opNav.valid || !mustHoldEligible) return -1;
  int8_t best=-1; uint8_t bestDist=DNA_UNKNOWN;
  for(uint8_t i=0;i<MAX_CTO_PEERS;i++){
    const PeerEntry& p=peerRegistry[i];
    if(!peerFreshIndex(i) || !p.mustHoldEligible || p.mapDir!=opNav.mapDir || p.mm==DNA_UNKNOWN) continue;
    const uint8_t d=forwardMarkerDistance(opNav.mm,p.mm,opNav.mapDir);
    if(d==0) continue;
    if(best<0 || d<bestDist){best=(int8_t)i;bestDist=d;}
  }
  return best;
}
static int8_t nearestMheBehindIndex(){
  if(!opNav.valid || !mustHoldEligible) return -1;
  int8_t best=-1; uint8_t bestDist=DNA_UNKNOWN;
  for(uint8_t i=0;i<MAX_CTO_PEERS;i++){
    const PeerEntry& p=peerRegistry[i];
    if(!peerFreshIndex(i) || !p.mustHoldEligible || p.mapDir!=opNav.mapDir || p.mm==DNA_UNKNOWN) continue;
    const uint8_t d=forwardMarkerDistance(p.mm,opNav.mm,opNav.mapDir);
    if(d==0) continue;
    if(best<0 || d<bestDist){best=(int8_t)i;bestDist=d;}
  }
  return best;
}
static bool stationShouldHoldForRear(uint32_t* rearId){
  if(rearId) *rearId=0;
  if(!mustHoldEligible || !opNav.valid) return false;
  const int8_t rear=nearestMheBehindIndex();
  const int8_t ahead=nearestMheAheadIndex();
  if(rear<0 || ahead<0) return false;
  const uint8_t rearDistance=forwardMarkerDistance(peerRegistry[rear].mm,opNav.mm,opNav.mapDir);
  const uint8_t aheadDistance=forwardMarkerDistance(opNav.mm,peerRegistry[ahead].mm,opNav.mapDir);
  if(rearDistance<aheadDistance){
    if(rearId) *rearId=peerRegistry[rear].locoId;
    return true;
  }
  return false;
}
static bool stationRearArrivalConfirmed(){
  const int8_t idx=peerIndexById(stationHoldRearId);
  if(idx<0) return false;
  const PeerEntry& p=peerRegistry[idx];
  return peerFreshIndex((uint8_t)idx) && !p.running && p.trafficPhase==TRAFFIC_WAIT_FOR_CLEARANCE &&
         p.trafficStopForId==LOCO_ID;
}

static const char* trafficPhaseName(uint8_t p){
  switch(p){
    case TRAFFIC_STOPPED_LEAD_APPROACH:return "STOPPED_LEAD_APPROACH";
    case TRAFFIC_FINAL_RAMP:return "FINAL_RAMP";
    case TRAFFIC_WAIT_FOR_CLEARANCE:return "WAIT_FOR_CLEARANCE";
    default:return "CLEAR";
  }
}

static void ctoPublishState(const char* event,const char* note){
  const int8_t nearest=nearestSameDirectionAheadIndex();
  const uint32_t nearestId=(nearest>=0)?peerRegistry[nearest].locoId:0;
  const int16_t nearestGap=(nearest>=0)?peerBoundaryGapMm((uint8_t)nearest):-1;
  char b[460];
  snprintf(b,sizeof(b),
    "{\"event\":\"%s\",\"self_mm\":%u,\"self_front_mm\":%u,\"self_rear_mm\":%u,\"self_truth_source\":\"%s\",\"self_dir\":\"%s\",\"fresh_peer_count\":%u,\"nearest_ahead_id\":%lu,\"nearest_ahead_gap_mm\":%d,\"traffic_phase\":\"%s\",\"mhe\":%u,\"station_holding_for_rear\":%u,\"station_hold_rear_id\":%lu,\"note\":\"%s\"}",
    event,(unsigned)(opNav.valid?opNav.mm:DNA_UNKNOWN),(unsigned)operationalFrontBoundaryMm(),(unsigned)operationalRearBoundaryMm(),
    ctoTruthSourceName(opNav.source),mapDirName(opNav.mapDir),(unsigned)freshPeerCount(),(unsigned long)nearestId,(int)nearestGap,
    trafficPhaseName(trafficPhase),mustHoldEligible?1:0,stationHoldingForRear?1:0,(unsigned long)stationHoldRearId,note);
  mqttPublish(TOPIC_STATE_CTO,b,false);
  mqttPublish(TOPIC_STATE_CTO_BOUNDARY,b,false);
  Serial.printf("[CTO2] %s\n",b);
}

static void ctoPublishCommand(const char* cmd,uint32_t seq,const char* action,const char* reason){
  char b[420];
  snprintf(b,sizeof(b),
    "{\"event\":\"DISPATCHER_CMD\",\"cmd\":\"%s\",\"seq\":%lu,\"action\":\"%s\",\"reason\":\"%s\",\"auto\":%u,\"running\":%u,\"station_phase\":\"%s\",\"traffic_phase\":\"%s\",\"gov_phase\":\"%s\",\"ramp_pwm\":%d}",
    cmd,(unsigned long)seq,action,reason,dispatcherAuto?1:0,running?1:0,
    stationPhaseName(stationTestPhase),trafficPhaseName(trafficPhase),speedGovModeName(speedGovMode),rampCurrent);
  mqttPublish(TOPIC_STATE_CTO,b,false);
  Serial.printf("[CMD] %s\n",b);
}

static void ctoPublishBoot(const char* reason){
  char b[760];
  snprintf(b,sizeof(b),
    "{\"event\":\"BOOT_STATUS\",\"reason\":\"%s\",\"sketch\":\"%s\",\"loco_id\":%lu,\"cto_version\":%u,\"packet_bytes\":%u,\"hall_deadband\":%u,\"hall_entry_margin\":%u,\"hall_min_peak\":%u,\"hall_dominance_percent\":%u,\"hall_min_event_ms\":%lu,\"hall_baseline_track_k\":%ld,\"hall_baseline_track_window\":%u,\"hall_baseline_stable_delta\":%u,\"hall_baseline_stable_samples\":%u,\"station_stop_offset\":%d,\"launch_min_hall_ms\":%lu}",
    reason,SKETCH_NAME,(unsigned long)LOCO_ID,(unsigned)CTO2_VERSION,(unsigned)sizeof(CtoPeerPacket),
    (unsigned)HALL_DEADBAND_COUNTS,(unsigned)HALL_ENTRY_MARGIN_COUNTS,(unsigned)HALL_MIN_PEAK_DELTA,(unsigned)HALL_DOMINANCE_PERCENT,
    (unsigned long)HALL_MIN_EVENT_MS,(long)HALL_BASELINE_TRACK_K,(unsigned)HALL_BASELINE_TRACK_WINDOW_COUNTS,
    (unsigned)HALL_BASELINE_STABLE_DELTA_COUNTS,(unsigned)HALL_BASELINE_STABLE_SAMPLES,
    (int)STATION_STOP_OFFSET,(unsigned long)LAUNCH_MIN_HALL_EVENT_MS);
  mqttPublish(TOPIC_STATE_BOOT,b,true);
  mqttPublish(TOPIC_STATE_CTO,b,false);
  Serial.printf("[BOOT] %s\n",b);
}

static void ctoPublishStatus(){
  operationalNavRefresh();
  if(!opNav.valid || opNav.mapDir==MAP_UNSET) return;
  CtoPeerPacket pkt={};
  pkt.magic=CTO2_MAGIC;
  pkt.version=CTO2_VERSION;
  pkt.senderId=LOCO_ID;
  pkt.sequence=++ctoTxSequence;
  pkt.hallMm=opNav.mm;
  pkt.frontBoundaryMm=operationalFrontBoundaryMm();
  pkt.rearBoundaryMm=operationalRearBoundaryMm();
  pkt.mapDir=opNav.mapDir;
  pkt.autoMode=dispatcherAuto?1:0;
  pkt.running=running?1:0;
  pkt.motionState=(uint8_t)motionTimingState;
  pkt.rampPwm=(uint8_t)constrain(rampCurrent,0,255);
  pkt.speedValid=(speedHistCount>0 && motionTimingState!=STOPPED_INFERRED && rampCurrent>MOTOR_DEAD_ZONE_PWM)?1:0;
  pkt.lastMoveAgeDs=lastMovementEvidenceMs?
    (uint16_t)constrain((int)((millis()-lastMovementEvidenceMs)/100UL),0,65535):65535;
  const float sp=(speedHistCount?speedHistPkph[(speedHistCount-1)&1]:0.0f);
  pkt.speedX10=(uint16_t)constrain((int)lroundf(sp*10.0f),0,65535);
  pkt.frontOffset=CTO2_STANDARD_FRONT_OFFSET_MM;
  pkt.rearOffset=CTO2_STANDARD_REAR_OFFSET_MM;
  pkt.truthSource=(uint8_t)opNav.source;
  pkt.stationPhase=(uint8_t)stationTestPhase;
  pkt.trafficPhase=(uint8_t)trafficPhase;
  pkt.mustHoldEligible=mustHoldEligible?1:0;
  pkt.trafficStopForId=trafficBlockerId;
  pkt.senderRxAccepted=ctoRxAccepted;
  pkt.senderTxAttempts=ctoTxAttempts+1;
  pkt.senderTxImmediateErrors=ctoTxImmediateErrors;
  const uint8_t bcast[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  ctoTxAttempts++;
  const esp_err_t tx=esp_now_send(bcast,(uint8_t*)&pkt,sizeof(pkt));
  if(tx!=ESP_OK){
    ctoTxImmediateErrors++;
    Serial.printf("[CTO2 TX ERROR] esp_now_send=%d\n",(int)tx);
  }
  ctoLastStatusTxMs=millis();
  ctoStatusDirty=false;
}

static void ctoPublishRadioHealth(){
  const int8_t nearest=nearestSameDirectionAheadIndex();
  char b[420];
  snprintf(b,sizeof(b),
    "{\"tx_attempts\":%lu,\"tx_immediate_errors\":%lu,\"rx_accepted\":%lu,\"rx_bad_magic_or_version\":%lu,\"rx_self\":%lu,\"rx_bad_length\":%lu,\"truth_valid\":%u,\"truth_source\":\"%s\",\"truth_hall_mm\":%u,\"fresh_peer_count\":%u,\"nearest_ahead_id\":%lu,\"mhe\":%u,\"cto_version\":%u,\"packet_bytes\":%u}",
    (unsigned long)ctoTxAttempts,(unsigned long)ctoTxImmediateErrors,(unsigned long)ctoRxAccepted,
    (unsigned long)ctoRxBadMagicVersion,(unsigned long)ctoRxSelf,(unsigned long)ctoRxBadLength,
    opNav.valid?1:0,ctoTruthSourceName(opNav.source),(unsigned)opNav.mm,(unsigned)freshPeerCount(),
    (unsigned long)(nearest>=0?peerRegistry[nearest].locoId:0),mustHoldEligible?1:0,(unsigned)CTO2_VERSION,(unsigned)sizeof(CtoPeerPacket));
  mqttPublish(TOPIC_STATE_CTO_RADIO,b,false);
  Serial.printf("[CTO2 RADIO] %s\n",b);
}
static void servicePeerFreshness(){
  for(uint8_t i=0;i<MAX_CTO_PEERS;i++){
    PeerEntry& p=peerRegistry[i];
    if(!p.seen) continue;
    const bool fresh=peerFreshIndex(i);
    if(p.freshPreviously && !fresh){
      p.freshPreviously=false;
      ctoStatusDirty=true;
      ctoPublishState("PEER_STALE","RADIO_FRESHNESS_EXPIRED");
    }
  }
}
static void ctoService(){
  operationalNavRefresh();
  servicePeerFreshness();
  const int8_t nearest=nearestSameDirectionAheadIndex();
  const uint32_t nearestId=(nearest>=0)?peerRegistry[nearest].locoId:0;
  if(nearestId!=trafficLastNearestAheadId){
    trafficLastNearestAheadId=nearestId;
    ctoStatusDirty=true;
    ctoPublishState("NEAREST_AHEAD_CHANGED","REGISTRY_BOUNDARY_TOPOLOGY");
  }
  // Always-on broadcaster: no AUTO/manual, station, motion, e-stop, or peer
  // condition gates this service.  Only operational truth is required.
  if(opNav.valid && (ctoStatusDirty || (uint32_t)(millis()-ctoLastStatusTxMs)>=CTO2_STATUS_INTERVAL_MS)) ctoPublishStatus();
  if((uint32_t)(millis()-ctoLastRadioHealthMs)>=CTO2_RADIO_HEALTH_INTERVAL_MS){
    ctoPublishRadioHealth();
    ctoLastRadioHealthMs=millis();
  }
}

// ----- Traffic: temporary station-mission interruption --------------------
static uint8_t trafficHallTargetFromBlockerRear(uint8_t rearBoundaryMm,int8_t dir){
  return (rearBoundaryMm==DNA_UNKNOWN || dir==MAP_UNSET) ? DNA_UNKNOWN
    : stepMm(rearBoundaryMm,oppositeMapDir(dir),CTO2_STANDARD_FRONT_OFFSET_MM);
}
static int16_t trafficOffsetFromFinal(uint8_t selfHallMm,int8_t dir,uint8_t finalHallMm){
  return stationRelativeOffsetForCenter(selfHallMm,dir,finalHallMm);
}
static int8_t trafficStoppedOccupancyAheadBeforeStation(){
  const int8_t idx=nearestSameDirectionAheadIndex();
  if(idx<0) return -1;
  const PeerEntry& p=peerRegistry[idx];
  if(!peerGenuinelyStopped((uint8_t)idx) || p.rearBoundaryMm==DNA_UNKNOWN) return -1;
  const uint8_t stationTarget=stationObjectiveHallMm();
  if(stationTarget!=DNA_UNKNOWN && opNav.valid){
    const uint8_t peerDistance=forwardMarkerDistance(opNav.mm,p.rearBoundaryMm,opNav.mapDir);
    const uint8_t stationDistance=forwardMarkerDistance(opNav.mm,stationTarget,opNav.mapDir);
    if(peerDistance>stationDistance) return -1;
  }
  return idx;
}
static int16_t trafficBoundaryGapMm(){
  const int8_t idx=peerIndexById(trafficBlockerId);
  if(idx<0) return -1;
  return peerBoundaryGapMm((uint8_t)idx);
}
static void trafficPublish(const char* event,const char* note=""){
  const int8_t idx=peerIndexById(trafficBlockerId);
  const PeerEntry* p=(idx>=0)?&peerRegistry[idx]:nullptr;
  char b[640];
  snprintf(b,sizeof(b),
    "{\"event\":\"%s\",\"phase\":\"%s\",\"self_hall_mm\":%u,\"self_front_boundary_mm\":%u,\"self_rear_boundary_mm\":%u,\"blocker_id\":%lu,\"blocker_hall_mm\":%u,\"blocker_rear_boundary_mm\":%u,\"blocker_running\":%u,\"blocker_ramp_pwm\":%u,\"blocker_motion\":\"%s\",\"blocker_seq\":%lu,\"final_hall_target_mm\":%u,\"offset_from_final_mm\":%d,\"boundary_gap_mm\":%d,\"touch_hall_gap_mm\":%u,\"restart_hall_gap_mm\":%u,\"decel_start_edge_gap_mm\":%u,\"note\":\"%s\"}",
    event,trafficPhaseName(trafficPhase),(unsigned)(opNav.valid?opNav.mm:DNA_UNKNOWN),
    (unsigned)operationalFrontBoundaryMm(),(unsigned)operationalRearBoundaryMm(),(unsigned long)trafficBlockerId,
    (unsigned)(p?p->mm:DNA_UNKNOWN),(unsigned)(p?p->rearBoundaryMm:DNA_UNKNOWN),(p&&p->running)?1:0,
    (unsigned)(p?p->rampPwm:0),p?motionName((MotionTimingState)p->motionState):"UNKNOWN",(unsigned long)(p?p->sequence:0),
    (unsigned)trafficFinalHallMm,(int)trafficLastOffsetMm,(int)trafficBoundaryGapMm(),
    (unsigned)TRAFFIC_TOUCH_HALL_GAP_MM,(unsigned)TRAFFIC_RESTART_HALL_GAP_MM,
    (unsigned)TRAFFIC_DECEL_START_EDGE_GAP_MM,note);
  mqttPublish(TOPIC_STATE_TRAFFIC,b,false);
  Serial.printf("[TRAFFIC] %s\n",b);
}
static void trafficReset(const char* why){
  const bool hadTraffic=(trafficPhase!=TRAFFIC_CLEAR || trafficBlockerId!=0);
  trafficPhase=TRAFFIC_CLEAR;
  trafficFinalRampActive=false;
  trafficBlockerId=0;
  trafficBlockerRearBoundaryMm=DNA_UNKNOWN;
  trafficFinalHallMm=DNA_UNKNOWN;
  trafficLastOffsetMm=999;
  trafficStoppedAtMs=0;
  trafficStopBlockerSequence=0;
  trafficStopBlockerHallMm=DNA_UNKNOWN;
  trafficStopBlockerRearBoundaryMm=DNA_UNKNOWN;
  if(hadTraffic) trafficPublish("RESET",why);
}
static void trafficArmStoppedOccupancy(uint8_t peerIdx){
  if(peerIdx>=MAX_CTO_PEERS) return;
  const PeerEntry& p=peerRegistry[peerIdx];
  trafficBlockerId=p.locoId;
  trafficBlockerRearBoundaryMm=p.rearBoundaryMm;
  trafficFinalHallMm=trafficHallTargetFromBlockerRear(trafficBlockerRearBoundaryMm,opNav.mapDir);
  trafficLastOffsetMm=trafficOffsetFromFinal(opNav.mm,opNav.mapDir,trafficFinalHallMm);
  trafficPhase=TRAFFIC_STOPPED_LEAD_APPROACH;
  speedGovMode=SPEED_GOV_MAINTAIN;
  ctoStatusDirty=true;
  trafficPublish("TRAFFIC_ARMED","STOPPED_NEAREST_OCCUPANCY_AHEAD");
}
static void trafficBeginFinalRamp(){
  speedGovCancelPendingRamp();
  speedGovMode=SPEED_GOV_IDLE;
  trafficPhase=TRAFFIC_FINAL_RAMP;
  trafficFinalRampActive=true;
  commandedPwm=0;
  updateMotorAuthority();
  ctoStatusDirty=true;
  trafficPublish("FINAL_RAMP_BEGIN","PACED_GOLDCORE_ZERO_RAMP_AT_10_HALL_TOUCH");
}
static bool trafficControlsGovernor(){
  return trafficPhase==TRAFFIC_STOPPED_LEAD_APPROACH;
}
static bool trafficOnMagnet(uint8_t mm,int8_t dir){
  (void)mm;
  if(!dispatcherAuto || !running || !opNav.valid || dir==MAP_UNSET) return false;
  if(trafficPhase==TRAFFIC_CLEAR){
    const int8_t idx=trafficStoppedOccupancyAheadBeforeStation();
    if(idx<0) return false;
    const PeerEntry& p=peerRegistry[idx];
    const uint8_t finalHall=trafficHallTargetFromBlockerRear(p.rearBoundaryMm,dir);
    const int16_t offset=trafficOffsetFromFinal(opNav.mm,dir,finalHall);
    if(offset>=TRAFFIC_DECEL_START_OFFSET_MM && offset<=0){
      trafficArmStoppedOccupancy((uint8_t)idx);
      if(trafficLastOffsetMm>=0){
        trafficPublish("LATE_BLOCKER_ACQUIRE","FINAL_TARGET_ALREADY_REACHED");
        trafficBeginFinalRamp();
        return true;
      }
    }
  }
  if(trafficPhase==TRAFFIC_STOPPED_LEAD_APPROACH){
    trafficLastOffsetMm=trafficOffsetFromFinal(opNav.mm,dir,trafficFinalHallMm);
    if(trafficLastOffsetMm>=0){
      trafficBeginFinalRamp();
      return true;
    }
    trafficPublish("APPROACH_MARKER","STOPPED_OCCUPANCY_TARGET_ACTIVE");
  }
  return trafficPhase==TRAFFIC_FINAL_RAMP || trafficPhase==TRAFFIC_WAIT_FOR_CLEARANCE;
}
static float trafficGovernorTargetPkph(){
  if(!trafficControlsGovernor() || !opNav.valid) return AUTO_TARGET_PKPH;
  const int16_t offset=trafficOffsetFromFinal(opNav.mm,opNav.mapDir,trafficFinalHallMm);
  const int16_t nextOffset=(offset==999)?999:(int16_t)(offset+1);
  if(nextOffset>=-7 && nextOffset<=-2) return STATION_SPEED_PKPH;
  if(nextOffset==-1) return STATION_M_PLUS_1_PKPH;
  if(nextOffset==0) return STATION_M_PLUS_2_PKPH;
  return AUTO_TARGET_PKPH;
}
static float effectiveGovernorTargetPkph(){
  const float stationTarget=stationGovernorTargetPkph();
  const float trafficTarget=trafficGovernorTargetPkph();
  return (trafficTarget<stationTarget)?trafficTarget:stationTarget;
}
static void trafficResumeTowardStation(){
  trafficPhase=TRAFFIC_CLEAR;
  trafficFinalRampActive=false;
  trafficBlockerId=0;
  trafficBlockerRearBoundaryMm=DNA_UNKNOWN;
  trafficFinalHallMm=DNA_UNKNOWN;
  trafficLastOffsetMm=999;
  trafficStoppedAtMs=0;
  trafficStopBlockerSequence=0;
  trafficStopBlockerHallMm=DNA_UNKNOWN;
  trafficStopBlockerRearBoundaryMm=DNA_UNKNOWN;
  if(eStopEngaged || !dispatcherAuto) return;
  direction=DIRECTION_FORWARD;
  applyDirectionPin();
  running=true;
  speedGovClearHistory();
  speedGovMode=SPEED_GOV_PROVING_MOVEMENT;
  commandedPwm=LAUNCH_NO_MAGNET_PWM_LIMIT;
  updateMotorAuthority();
  ctoStatusDirty=true;
  trafficPublish("TRAFFIC_RESTART","12_HALL_TO_HALL_CLEARANCE_RESUME_STATION_MISSION");
  publishSimpleStates();
}
static void serviceTrafficControl(){
  if(trafficPhase==TRAFFIC_FINAL_RAMP && !trafficFinalRampActive && rampCurrent==0 && rampTarget==0){
    running=false;
    speedGovMode=SPEED_GOV_IDLE;
    speedGovClearHistory();
    trafficPhase=TRAFFIC_WAIT_FOR_CLEARANCE;
    trafficStoppedAtMs=millis();
    {
      const int8_t idx=peerIndexById(trafficBlockerId);
      if(idx>=0){
        const PeerEntry& p=peerRegistry[idx];
        trafficStopBlockerSequence=p.sequence;
        trafficStopBlockerHallMm=p.mm;
        trafficStopBlockerRearBoundaryMm=p.rearBoundaryMm;
      }
    }
    ctoStatusDirty=true;
    trafficPublish("TRAFFIC_STOPPED","WAITING_FOR_FRESH_POST_STOP_CLEARANCE");
    publishSimpleStates();
    return;
  }
  if(trafficPhase!=TRAFFIC_WAIT_FOR_CLEARANCE || !opNav.valid) return;
  const int8_t idx=peerIndexById(trafficBlockerId);
  if(idx<0) return;
  const PeerEntry& p=peerRegistry[idx];
  if(!peerFreshIndex((uint8_t)idx) || !p.running || p.mapDir!=opNav.mapDir || p.mm==DNA_UNKNOWN) return;
  if(p.receivedAtMs<=trafficStoppedAtMs || p.sequence==trafficStopBlockerSequence) return;
  const bool blockerMoved=(p.mm!=trafficStopBlockerHallMm || p.rearBoundaryMm!=trafficStopBlockerRearBoundaryMm);
  if(!blockerMoved) return;
  const uint8_t hallGap=peerHallGapMm((uint8_t)idx);
  if(hallGap!=DNA_UNKNOWN && hallGap>=TRAFFIC_RESTART_HALL_GAP_MM){
    trafficResumeTowardStation();
  }
}

static const char* stationPhaseName(StationTestPhase p){
  switch(p){
    case STATION_TEST_STOP_SERVICE:return "STOP_SERVICE";
    case STATION_TEST_FINAL_RAMP:return "FINAL_RAMP";
    case STATION_TEST_DWELL:return "DWELL";
    case STATION_TEST_DEPARTING:return "DEPARTING";
    case STATION_TEST_THROUGH_SERVICE:return "THROUGH_SERVICE";
    default:return "IDLE";
  }
}
static const char* stationVisitName(StationVisitKind v){
  return v==STATION_VISIT_STOP?"STOP":(v==STATION_VISIT_THROUGH?"THROUGH":"NONE");
}
static const char* stationRoundName(){ return stationRoundOdd?"ODD":"EVEN"; }
static const StationDefinition* stationActive(){
  return (stationActiveIndex>=0 && stationActiveIndex<(int8_t)STATION_COUNT)?&STATIONS[stationActiveIndex]:nullptr;
}
static int16_t stationRelativeOffsetForCenter(uint8_t mm,int8_t dir,uint8_t centerMm){
  if(dir==MAP_UNSET)return 999;
  const uint8_t forward=(dir==MAP_CW)
    ?routeMod((int32_t)mm-centerMm)
    :routeMod((int32_t)centerMm-mm);
  if(forward<=20)return (int16_t)forward;
  if(forward>=DNA_N-20)return (int16_t)forward-(int16_t)DNA_N;
  return 999;
}
static int16_t stationRelativeOffset(uint8_t mm,int8_t dir){
  const StationDefinition* st=stationActive();
  return st?stationRelativeOffsetForCenter(mm,dir,st->centerMm):999;
}
static bool stationScheduledStop(uint8_t index){
  // r21_CTO: alternating service removed — every station is a STOP.
  (void)index;
  return true;
}
static uint8_t stationObjectiveHallMm(){
  if(!opNav.valid || opNav.mapDir==MAP_UNSET) return DNA_UNKNOWN;
  const StationDefinition* active=stationActive();
  if(active && (stationTestPhase==STATION_TEST_STOP_SERVICE ||
                stationTestPhase==STATION_TEST_FINAL_RAMP ||
                stationTestPhase==STATION_TEST_DWELL)){
    return stepMm(active->centerMm,opNav.mapDir,(uint8_t)STATION_STOP_OFFSET);
  }
  int8_t best=-1;
  uint8_t bestDistance=DNA_UNKNOWN;
  for(uint8_t i=0;i<STATION_COUNT;i++){
    const uint8_t target=stepMm(STATIONS[i].centerMm,opNav.mapDir,(uint8_t)STATION_STOP_OFFSET);
    const uint8_t distance=forwardMarkerDistance(opNav.mm,target,opNav.mapDir);
    if(distance==0) continue;
    if(best<0 || distance<bestDistance){best=(int8_t)i;bestDistance=distance;}
  }
  return best>=0 ? stepMm(STATIONS[best].centerMm,opNav.mapDir,(uint8_t)STATION_STOP_OFFSET) : DNA_UNKNOWN;
}

static void stationPublish(const char* event,int16_t offset,float ignoredTarget,const char* note=""){
  (void)ignoredTarget;
  const StationDefinition* st=stationActive();
  char b[420];
  snprintf(b,sizeof(b),
    "{\"event\":\"%s\",\"phase\":\"%s\",\"station\":\"%s\",\"center_mm\":%u,\"dir\":\"%s\",\"offset\":%d,\"commanded_pwm\":%d,\"actual_pwm\":%d,\"ramp_step_ms\":%u,\"note\":\"%s\"}",
    event,stationPhaseName(stationTestPhase),st?st->name:"NONE",
    st?(unsigned)st->centerMm:999U,mapDirName(opNav.mapDir),(int)offset,
    commandedPwm,rampCurrent,(unsigned)prescribedRampStepMs,note);
  mqttPublish(TOPIC_STATE_STATION,b,false);
  Serial.printf("[STATION] %s
",b);
}
static void stationResetTestCycle(const char* why){
  stationTestPhase=STATION_TEST_IDLE;
  stationVisitKind=STATION_VISIT_NONE;
  stationActiveIndex=-1;
  stationFinalRampActive=false;
  stationFinalRampOffset=FINAL_ZERO_TRIGGER_OFFSET;
  stationDwellClockRunning=false;
  stationHoldingForRear=false;
  stationHoldRearId=0;
  stationDepartureLaunchActive=false;
  stationDeparturePostStopHits=0;
  stationDwellStartedAtMs=0;
  stationCompletedVisits=0;
  stationRoundSeenMagnet=false;
  stationRoundLastMm=0;
  stationPublish("RESET",999,0.0f,why);
}

// Stop service always carries the local train to its own station-offset target.  Traffic
// does not change this station target; it can only temporarily interrupt motion.
static bool stationControlsGovernor(){ return false; }
static float stationGovernorTargetPkph(){ return 0.0f; }

static uint8_t approachTargetForOffset(int16_t offset){
  // M-10=84, M-9=78, M-8=72, M-7=66, M-6=60.
  if(offset<APPROACH_START_OFFSET) return CRUISE_PWM;
  if(offset>=-6) return STATION_ZONE_PWM;
  return (uint8_t)(CRUISE_PWM - (uint8_t)((offset-APPROACH_START_OFFSET+1)*6));
}

static void stationBeginFinalRamp(int16_t offset){
  stationTestPhase=STATION_TEST_FINAL_RAMP;
  stationFinalRampActive=true;
  requestPrescribedPwm(0,FINAL_STOP_STEP_MS);
  stationPublish("FINAL_ZERO_RAMP_BEGIN",offset,0.0f,"M_PLUS_2_300_MS_PER_PWM_TO_ZERO");
}

// Mandatory-stop, prescribed-PWM station service. Hall position selects the
// profile stage; Hall dt and diagnostic pKPH never alter a PWM target.
static bool stationOnMagnet(uint8_t mm,int8_t dir){
  if(!stationServiceReady()) return false;

  if(stationTestPhase==STATION_TEST_DEPARTING){
    const int16_t o=stationRelativeOffset(mm,dir);
    if(o>=5){
      requestPrescribedPwm(CRUISE_PWM,NORMAL_PROFILE_STEP_MS);
      stationCompleteDeparture("CLEARED_STATION_ZONE_AT_M_PLUS_5");
    }else{
      requestPrescribedPwm(STATION_ZONE_PWM,NORMAL_PROFILE_STEP_MS);
    }
    return false;
  }

  if(stationTestPhase==STATION_TEST_FINAL_RAMP || stationTestPhase==STATION_TEST_DWELL) return true;

  if(stationTestPhase==STATION_TEST_IDLE){
    for(uint8_t i=0;i<STATION_COUNT;i++){
      const int16_t o=stationRelativeOffsetForCenter(mm,dir,STATIONS[i].centerMm);
      if(o==APPROACH_START_OFFSET){
        stationActiveIndex=(int8_t)i;
        stationVisitKind=STATION_VISIT_STOP;
        stationTestPhase=STATION_TEST_STOP_SERVICE;
        requestPrescribedPwm(approachTargetForOffset(o),NORMAL_PROFILE_STEP_MS);
        stationPublish("MANDATORY_STOP_ARMED",o,0.0f,"PRESCRIBED_PWM_PROFILE");
        return false;
      }
    }
    return false;
  }

  const int16_t o=stationRelativeOffset(mm,dir);
  if(stationTestPhase==STATION_TEST_STOP_SERVICE){
    if(o>=APPROACH_START_OFFSET && o<=-6){
      requestPrescribedPwm(approachTargetForOffset(o),NORMAL_PROFILE_STEP_MS);
      stationPublish("APPROACH_PWM_TARGET",o,0.0f,"SUBTRACT_6_PWM_PER_MARKER");
    }else if(o>=STATION_ZONE_START_OFFSET && o<FINAL_APPROACH_START_OFFSET){
      requestPrescribedPwm(STATION_ZONE_PWM,NORMAL_PROFILE_STEP_MS);
      stationPublish("STATION_ZONE_HOLD",o,0.0f,"HOLD_PWM_60");
    }else if(o==FINAL_APPROACH_START_OFFSET){
      // Begin at the station midpoint and allow two marker intervals to reach 25.
      requestPrescribedPwm(FINAL_APPROACH_PWM,NORMAL_PROFILE_STEP_MS);
      stationPublish("FINAL_APPROACH_BEGIN",o,0.0f,"RAMP_60_TO_25_BY_M_PLUS_2");
    }else if(o==FINAL_ZERO_TRIGGER_OFFSET){
      stationBeginFinalRamp(o);
      return true;
    }
  }
  return false;
}

static void stationLaunchFromDwell(){
  if(eStopEngaged || !dispatcherAuto){mqttWarn("DEPARTURE DENIED — E-STOP OR MANUAL AUTHORITY");return;}
  direction=DIRECTION_FORWARD;
  applyDirectionPin();
  running=true;
  stationDepartureLaunchActive=true;
  stationTestPhase=STATION_TEST_DEPARTING;
  requestPrescribedPwm(STATION_ZONE_PWM,NORMAL_PROFILE_STEP_MS);
  stationDwellClockRunning=false;
  stationPublish("DWELL_COMPLETE",stationRelativeOffset(opNav.mm,opNav.mapDir),0.0f,"DEPART_TO_PWM_60");
  publishSimpleStates();
}

static void stationCompleteDeparture(const char* why){
  if(stationTestPhase!=STATION_TEST_DEPARTING) return;
  stationPublish("DEPARTURE_COMPLETE",stationRelativeOffset(opNav.mm,opNav.mapDir),0.0f,why);
  stationTestPhase=STATION_TEST_IDLE;
  stationVisitKind=STATION_VISIT_NONE;
  stationActiveIndex=-1;
  stationFinalRampOffset=FINAL_ZERO_TRIGGER_OFFSET;
  stationDepartureLaunchActive=false;
  stationDeparturePostStopHits=0;
}

static void serviceStationTest(){
  if(stationTestPhase==STATION_TEST_FINAL_RAMP){
    if(!stationFinalRampActive && rampCurrent==0 && rampTarget==0){
      running=false;
      stationTestPhase=STATION_TEST_DWELL;
      stationCompletedVisits++;
      stationDwellClockRunning=true;
      stationDwellStartedAtMs=millis();
      stationPublish("DWELL_BEGIN",stationRelativeOffset(opNav.mm,opNav.mapDir),0.0f,"15_SECOND_FIXED_DWELL");
      publishSimpleStates();
    }
    return;
  }
  if(stationTestPhase!=STATION_TEST_DWELL) return;
  if((uint32_t)(millis()-stationDwellStartedAtMs)<STATION_DWELL_MS) return;
  stationLaunchFromDwell();
}

static uint32_t speedGovPredictNextDtMs(uint8_t fromMm, int8_t dir, float currentPkph){
  if(dir==MAP_UNSET || currentPkph<=0.1f) return latestSegmentDtMs?latestSegmentDtMs:1000UL;
  const uint8_t nextMm=stepMm(fromMm,dir);
  const uint16_t nextSegMm=segmentDistanceMm(fromMm,nextMm,dir);
  float trend=0.0f;
  if(speedHistCount>=2) trend=speedHistPkph[1]-speedHistPkph[0];
  // Extrapolate one segment ahead, but clamp so a noisy interval cannot
  // create an absurdly short or long ramp schedule.
  float predictedPkph=currentPkph+trend;
  predictedPkph=constrain(predictedPkph,currentPkph*0.65f,currentPkph*1.35f);
  if(predictedPkph<1.0f) predictedPkph=1.0f;
  uint32_t dt=(uint32_t)lroundf(((float)nextSegMm*1000.0f)/(predictedPkph*PKPH_MM_PER_SEC));
  return constrain(dt,150UL,6000UL);
}

// Schedule delta PWM as individual one-PWM steps across one predicted physical
// interval.  The target is derived from actual rampCurrent, never a prior plan.
static void speedGovScheduleDelta(int delta,uint32_t predictedDtMs){
  speedGovCancelPendingRamp();
  if(delta==0){
    Serial.printf("[SPEED] HOLD pwm=%d\n",rampCurrent);
    return;
  }
  const int target=constrain(rampCurrent+delta,0,(int)AUTO_PWM_MAX);
  const uint8_t steps=(uint8_t)abs(target-rampCurrent);
  if(steps==0)return;
  commandedPwm=target;
  updateMotorAuthority();
  speedRampActive=true;
  speedRampStepsRemaining=steps;
  speedRampStepMs=max(20UL,predictedDtMs/(uint32_t)steps);
  lastRampMs=millis();
  Serial.printf("[SPEED] SCHEDULE from=%d to=%d delta=%+d steps=%u nextDt=%lu stepMs=%lu\n",
                rampCurrent,target,delta,(unsigned)steps,(unsigned long)predictedDtMs,(unsigned long)speedRampStepMs);
}

static void speedGovPublish(uint8_t fromMm,uint8_t toMm,uint16_t segMm,uint32_t dt,float segPkph,float avgPkph,float trend,int delta,uint32_t nextDt){
  char b[430];
  snprintf(b,sizeof(b),
    "{\"mode\":\"MEASURED_SPEED\",\"phase\":\"%s\",\"from_mm\":%u,\"to_mm\":%u,\"segment_mm\":%u,\"dt_ms\":%lu,\"segment_pkph\":%.2f,\"avg_pkph\":%.2f,\"trend_pkph\":%.2f,\"target_pkph\":%.1f,\"actual_pwm\":%d,\"pwm_delta\":%d,\"next_dt_ms\":%lu}",
    speedGovModeName(speedGovMode),(unsigned)fromMm,(unsigned)toMm,(unsigned)segMm,(unsigned long)dt,
    segPkph,avgPkph,trend,effectiveGovernorTargetPkph(),rampCurrent,delta,(unsigned long)nextDt);
  mqttPublish(TOPIC_STATE_GOV,b,false);
  char speedJson[220];
  snprintf(speedJson,sizeof(speedJson),"{\"pkph\":%.2f,\"source\":\"SEGMENT_MEASURED\",\"samples\":%u,\"from_mm\":%u,\"to_mm\":%u,\"segment_mm\":%u,\"dt_ms\":%lu,\"target_pkph\":%.1f}",
    segPkph,(unsigned)speedHistCount,(unsigned)fromMm,(unsigned)toMm,(unsigned)segMm,(unsigned long)dt,effectiveGovernorTargetPkph());
  mqttPublish(TOPIC_MM_SPEED,speedJson,true);
}

static void speedGovOnMeasuredSegment(uint8_t fromMm,uint8_t toMm,int8_t dir,uint16_t segMm,uint32_t dt,float segmentPkph){
  // Do not stack a fresh adjustment on a ramp that did not finish.
  speedGovCancelPendingRamp();
  latestSegmentDtMs=dt;
  lastMovementEvidenceMs=millis();
  if(speedHistCount<2) speedHistPkph[speedHistCount++]=segmentPkph;
  else {speedHistPkph[0]=speedHistPkph[1];speedHistPkph[1]=segmentPkph;}

  const float avgPkph=(speedHistCount==2)?(speedHistPkph[0]+speedHistPkph[1])*0.5f:segmentPkph;
  const float trend=(speedHistCount==2)?(speedHistPkph[1]-speedHistPkph[0]):0.0f;
  const uint32_t nextDt=speedGovPredictNextDtMs(toMm,dir,segmentPkph);
  const float targetPkph=effectiveGovernorTargetPkph();
  int delta=0;

  // Station departure deliberately keeps ramping through two post-stop Hall
  // events.  The first event establishes time zero; this second event supplies
  // the first usable physical segment measurement and hands control to STARTUP.
  if(speedGovMode==SPEED_GOV_STATION_DEPARTURE_PROVING){
    if(stationDeparturePostStopHits<2) return;
    speedGovMode=SPEED_GOV_STARTUP;
    stationCompleteDeparture("TWO_VALID_POST_STOP_SEGMENTS");
  }

  if(speedGovMode==SPEED_GOV_PROVING_MOVEMENT){
    speedGovMode=(segmentPkph >= targetPkph*STARTUP_HANDOFF_FRACTION)?SPEED_GOV_MAINTAIN:SPEED_GOV_STARTUP;
  }
  if(speedGovMode==SPEED_GOV_STARTUP){
    if(segmentPkph < targetPkph*STARTUP_HANDOFF_FRACTION){
      // Deliberately two-stage: brisk proportional-to-target startup, then
      // fine maintenance.  Do not taper this during startup.
      delta=max(1,(int)lroundf(targetPkph/5.0f));
    } else {
      speedGovMode=SPEED_GOV_MAINTAIN;
    }
  }
  if(speedGovMode==SPEED_GOV_MAINTAIN){
    if(stationControlsGovernor() || trafficControlsGovernor()){
      // Station and stopped-traffic arrival both use the established
      // next-arrival correction: compare the completed physical segment to
      // the requested next marker speed and distribute the bidirectional PWM
      // correction across the predicted next physical segment.
      delta=stationArrivalTargetDelta(targetPkph,segmentPkph);
    } else {
      const float error=targetPkph-avgPkph;
      const float absError=fabsf(error);
      // 5.1 correction tiers: hold <0.5; +/-1 through 1.5; +/-3 through 3.0; +/-4 through 5.0; +/-5 above 5.0 pKPH error.
      if(absError<MAINTAIN_HOLD_BAND_PKPH) delta=0;
      else if(absError<=MAINTAIN_ONE_PWM_BAND_PKPH) delta=(error>0.0f)?1:-1;
      else if(absError<=MAINTAIN_THREE_PWM_BAND_PKPH) delta=(error>0.0f)?3:-3;
      else if(absError<=MAINTAIN_FOUR_PWM_BAND_PKPH) delta=(error>0.0f)?4:-4;
      else delta=(error>0.0f)?5:-5;
      // Trend may soften a correction when the train is already moving back
      // toward target; it never reverses the correction.
      if(delta>0 && trend>=TREND_VETO_PKPH) delta=max(0,delta-1);
      if(delta<0 && trend<=-TREND_VETO_PKPH) delta=min(0,delta+1);
    }
  }

  speedGovPublish(fromMm,toMm,segMm,dt,segmentPkph,avgPkph,trend,delta,nextDt);
  Serial.printf("[SPEED] %s %03u->%03u speed=%.2f avg=%.2f trend=%+.2f target=%.1f pwm=%d delta=%+d\n",
    speedGovModeName(speedGovMode),(unsigned)fromMm,(unsigned)toMm,segmentPkph,avgPkph,trend,targetPkph,rampCurrent,delta);
  speedGovScheduleDelta(delta,nextDt);
}

static void serviceGovernor(){
  if(!dispatcherAuto || !running)return;
  if((speedGovMode==SPEED_GOV_PROVING_MOVEMENT ||
      speedGovMode==SPEED_GOV_STATION_DEPARTURE_PROVING) &&
      rampCurrent>=LAUNCH_NO_MAGNET_PWM_LIMIT){
    const bool stationDeparture=(speedGovMode==SPEED_GOV_STATION_DEPARTURE_PROVING);
    running=false;
    commandedPwm=0;
    speedRampActive=false;
    stationDepartureLaunchActive=false;
    updateMotorAuthority();
    mqttWarn(stationDeparture
      ? "STATION DEPARTURE ABORT — PWM 100 WITH NO VALID MAGNET INTERVAL"
      : "LAUNCH ABORT — PWM 100 WITH NO VALID MAGNET INTERVAL");
    Serial.println(stationDeparture
      ? "[SPEED] STATION DEPARTURE ABORT — PWM 100 before a valid movement interval."
      : "[SPEED] LAUNCH ABORT — PWM 100 before a valid movement interval.");
    publishSimpleStates();
  }
}

// Hall-silence OBSERVER (formerly the stall guard).
//
// Continuity philosophy: a silent Hall front-end is MISSING EVIDENCE, not proof
// the locomotive stopped.  The MM030 shutdown proved the old rule wrong — Otto
// was still moving; only the Hall input had gone deaf, and the guard seized a
// running locomotive.  This observer LOGS the silence and lets Otto keep his
// route and station commitment.  It never touches the motor.
//
// A genuine mechanical stall CAN still occur (obstruction, derail against a
// wall) and would draw sustained locked-rotor current.  If stall protection is
// ever wanted, the correct signal is motor CURRENT from the INA219, not Hall
// silence — and per your standing rule it must be discussed and approved before
// any such authority-seizing code is added.  This observer deliberately does
// not implement it.
static const uint32_t HALL_SILENCE_OBSERVE_MS = 10000UL;
static const uint8_t  HALL_SILENCE_PWM_CONTEXT = 80;
static bool hallSilenceReported=false;
static void serviceHallSilenceObserver(){
  // The final 300 ms/PWM station coast legitimately runs quiet; not a concern.
  if(stationFinalRampActive || trafficFinalRampActive){ hallSilenceReported=false; return; }
  if(!dispatcherAuto || !running || rampCurrent<=HALL_SILENCE_PWM_CONTEXT || !haveLastMagnet){
    hallSilenceReported=false; return;
  }
  if((uint32_t)(millis()-lastMagnetTime) < HALL_SILENCE_OBSERVE_MS){
    hallSilenceReported=false; return;   // fresh Hall traffic — clear the latch
  }
  if(hallSilenceReported) return;         // report once per silence episode
  hallSilenceReported=true;
  char b[176];
  snprintf(b,sizeof(b),
    "{\"event\":\"HALL_SILENCE_OBSERVED\",\"pwm\":%d,\"silent_ms\":%lu,\"last_mm\":%u,\"action\":\"CONTINUE_ROUTE\"}",
    rampCurrent,(unsigned long)(millis()-lastMagnetTime),(unsigned)odomMm);
  mqttPublish(TOPIC_DNA_EVENT,b,false);
  Serial.printf("[OBSERVE] %s\n",b);
}

static bool startAutoSequence(){
  if(eStopEngaged || !dispatcherAuto){mqttWarn("AUTO GO DENIED — E-STOP OR MANUAL AUTHORITY");return false;}
  if(running || stationTestPhase!=STATION_TEST_IDLE){mqttWarn("DUPLICATE GO IGNORED — ACTIVE AUTO STATE");return false;}
  direction=DIRECTION_FORWARD;
  applyDirectionPin();
  running=true;
  requestPrescribedPwm(CRUISE_PWM,NORMAL_PROFILE_STEP_MS);
  mqttWarn("AUTO PWM PROFILE ACTIVE — CRUISE PWM 90");
  Serial.println("[AUTO] GO — prescribed ramp to PWM 90; Hall timing is diagnostic only.");
  publishSimpleStates();
  return true;
}

static void stopAutoSequence(){
  stationResetTestCycle("DISPATCHER_STOP");
  running=false;
  requestPrescribedPwm(0,NORMAL_PROFILE_STEP_MS);
  Serial.println("[AUTO] STOP — prescribed down-ramp to zero.");
}

static void enterDispatcherAutoMode(){
  if(dispatcherAuto){
    ctoPublishState("AUTO_ENROLL_IGNORED","ALREADY_IN_AUTO_NO_RESET");
    publishSimpleStates();
    mqttWarn("");
    return;
  }
  if(eStopEngaged){mqttWarn("AUTO DENIED — E-STOP");return;}
  if(rampCurrent>MOTOR_DEAD_ZONE_PWM || rampTarget!=0){mqttWarn("AUTO DENIED — STOP LOCOMOTIVE FIRST");return;}
  dispatcherAuto=true;running=false;commandedPwm=0;prescribedRampStepMs=NORMAL_PROFILE_STEP_MS;
  trafficReset("AUTO_ENROLL");
  stationResetTestCycle("AUTO_ENROLL");
  updateMotorAuthority();publishSimpleStates();mqttWarn("");
  Serial.println("[AUTO] LL PWM DNA AUTO enrolled — prescribed PWM profile awaits GO");
}

static void leaveDispatcherAutoMode(){
  stopAutoSequence();dispatcherAuto=false;
  publishSimpleStates(); ctoStatusDirty=true; ctoPublishStatus(); mqttWarn("");Serial.println("[AUTO] released — MANUAL authority restored");
}

// INA
static Adafruit_INA219 ina219; static bool ina219Available=false; static unsigned long lastInaTelemMs=0; static float lastBusVoltageV=0,lastCurrentA=0,lastPowerW=0; static bool lastLowVolt=false;

static float dashboardConfidence(){switch(positionState){case POSITION_EXACT_LOCKED:return 1.0f;case POSITION_DECLARED:return 0.40f;case POSITION_FAULT:return 0.0f;default:return 0.0f;}}
static void mqttPublish(const char* t,const char* p,bool retained=true){if(mqtt.connected())mqtt.publish(t,p,retained);} static void mqttPublishInt(const char* t,int v,bool r=true){char b[16];snprintf(b,sizeof(b),"%d",v);mqttPublish(t,b,r);} static void mqttWarn(const char* m){mqttPublish(TOPIC_STATE_WARNING,m,false);Serial.printf("[WARN] %s\n",m);}
static void mqttSetupTopics(){
  const char* id=LOCO_NAME;
  snprintf(TOPIC_ONLINE,sizeof(TOPIC_ONLINE),"ngr/loco/%s/online",id);
  snprintf(TOPIC_CMD_THROTTLE,sizeof(TOPIC_CMD_THROTTLE),"ngr/loco/%s/cmd/throttle",id);
  snprintf(TOPIC_CMD_DIRECTION,sizeof(TOPIC_CMD_DIRECTION),"ngr/loco/%s/cmd/direction",id);
  snprintf(TOPIC_CMD_BRAKE,sizeof(TOPIC_CMD_BRAKE),"ngr/loco/%s/cmd/brake",id);
  snprintf(TOPIC_CMD_ESTOP,sizeof(TOPIC_CMD_ESTOP),"ngr/loco/%s/cmd/estop",id);
  snprintf(TOPIC_CMD_AUTO,sizeof(TOPIC_CMD_AUTO),"ngr/loco/%s/cmd/auto",id);
  snprintf(TOPIC_CMD_DISPATCHER_RELEASE,sizeof(TOPIC_CMD_DISPATCHER_RELEASE),"ngr/loco/%s/cmd/dispatcher_release",id);
  snprintf(TOPIC_CMD_SESSION_DIRECTION,sizeof(TOPIC_CMD_SESSION_DIRECTION),"ngr/loco/%s/cmd/session_direction",id);
  snprintf(TOPIC_CMD_START_INTERVAL,sizeof(TOPIC_CMD_START_INTERVAL),"ngr/loco/%s/cmd/start_interval",id);
  snprintf(TOPIC_CMD_START_MM,sizeof(TOPIC_CMD_START_MM),"ngr/loco/%s/cmd/start_mm",id);
  snprintf(TOPIC_CMD_DNA_RESET,sizeof(TOPIC_CMD_DNA_RESET),"ngr/loco/%s/cmd/dna_reset",id);
  snprintf(TOPIC_CMD_MHE,sizeof(TOPIC_CMD_MHE),"ngr/loco/%s/cmd/must_hold_eligible",id);
  snprintf(TOPIC_STATE_THROTTLE,sizeof(TOPIC_STATE_THROTTLE),"ngr/loco/%s/state/throttle",id);
  snprintf(TOPIC_STATE_DIRECTION,sizeof(TOPIC_STATE_DIRECTION),"ngr/loco/%s/state/direction",id);
  snprintf(TOPIC_STATE_BRAKE,sizeof(TOPIC_STATE_BRAKE),"ngr/loco/%s/state/brake",id);
  snprintf(TOPIC_STATE_ESTOP,sizeof(TOPIC_STATE_ESTOP),"ngr/loco/%s/state/estop",id);
  snprintf(TOPIC_STATE_AUTO,sizeof(TOPIC_STATE_AUTO),"ngr/loco/%s/state/auto",id);
  snprintf(TOPIC_STATE_GOV,sizeof(TOPIC_STATE_GOV),"ngr/loco/%s/state/gov",id);
  snprintf(TOPIC_STATE_STATION,sizeof(TOPIC_STATE_STATION),"ngr/loco/%s/state/station",id);
  snprintf(TOPIC_STATE_SESSION_DIRECTION,sizeof(TOPIC_STATE_SESSION_DIRECTION),"ngr/loco/%s/state/session_direction",id);
  snprintf(TOPIC_STATE_START_INTERVAL,sizeof(TOPIC_STATE_START_INTERVAL),"ngr/loco/%s/state/start_interval",id);
  snprintf(TOPIC_STATE_START_MM,sizeof(TOPIC_STATE_START_MM),"ngr/loco/%s/state/start_mm",id);
  snprintf(TOPIC_STATE_MHE,sizeof(TOPIC_STATE_MHE),"ngr/loco/%s/state/must_hold_eligible",id);
  snprintf(TOPIC_STATE_WARNING,sizeof(TOPIC_STATE_WARNING),"ngr/loco/%s/state/warning",id);
  snprintf(TOPIC_STATE_DNA,sizeof(TOPIC_STATE_DNA),"ngr/loco/%s/state/dna",id);
  snprintf(TOPIC_STATE_HALL,sizeof(TOPIC_STATE_HALL),"ngr/loco/%s/state/hall",id);
  snprintf(TOPIC_DNA_EVENT,sizeof(TOPIC_DNA_EVENT),"ngr/loco/%s/dna/event",id);
  snprintf(TOPIC_MM_MAG,sizeof(TOPIC_MM_MAG),"ngr/loco/%s/mm/mag",id);
  snprintf(TOPIC_MM_POS,sizeof(TOPIC_MM_POS),"ngr/loco/%s/mm/pos",id);
  snprintf(TOPIC_MM_MOTION,sizeof(TOPIC_MM_MOTION),"ngr/loco/%s/mm/motion",id);
  snprintf(TOPIC_MM_SPEED,sizeof(TOPIC_MM_SPEED),"ngr/loco/%s/mm/speed",id);
  snprintf(TOPIC_TELEM_VOLTAGE,sizeof(TOPIC_TELEM_VOLTAGE),"ngr/loco/%s/telem/voltage",id);
  snprintf(TOPIC_TELEM_CURRENT,sizeof(TOPIC_TELEM_CURRENT),"ngr/loco/%s/telem/current",id);
  snprintf(TOPIC_TELEM_POWER,sizeof(TOPIC_TELEM_POWER),"ngr/loco/%s/telem/power",id);
  snprintf(TOPIC_STATE_LOWVOLT,sizeof(TOPIC_STATE_LOWVOLT),"ngr/loco/%s/state/lowvolt",id);
  snprintf(TOPIC_STATE_CTO,sizeof(TOPIC_STATE_CTO),"ngr/loco/%s/state/cto2",id);
  snprintf(TOPIC_STATE_CTO_BOUNDARY,sizeof(TOPIC_STATE_CTO_BOUNDARY),"ngr/loco/%s/state/cto2_boundary",id);
  snprintf(TOPIC_STATE_TRAFFIC,sizeof(TOPIC_STATE_TRAFFIC),"ngr/loco/%s/state/traffic",id);
  snprintf(TOPIC_STATE_CTO_RADIO,sizeof(TOPIC_STATE_CTO_RADIO),"ngr/loco/%s/state/cto2_radio",id);
  snprintf(TOPIC_STATE_CTO_LEGACY,sizeof(TOPIC_STATE_CTO_LEGACY),"ngr/loco/%s/state/cto",id);
  snprintf(TOPIC_STATE_BOOT,sizeof(TOPIC_STATE_BOOT),"ngr/loco/%s/state/boot",id);
}
static void publishSimpleStates(){mqttPublishInt(TOPIC_STATE_THROTTLE,commandedPwm);mqttPublishInt(TOPIC_STATE_DIRECTION,direction);mqttPublishInt(TOPIC_STATE_BRAKE,brakeValue);mqttPublishInt(TOPIC_STATE_ESTOP,eStopEngaged?1:0);mqttPublishInt(TOPIC_STATE_AUTO,dispatcherAuto?1:0);mqttPublish(TOPIC_STATE_SESSION_DIRECTION,mapDirName(declaredSessionDir));char b[24];snprintf(b,sizeof(b),"%03u-%03u",startIntervalA,startIntervalB);mqttPublish(TOPIC_STATE_START_INTERVAL,b);mqttPublishInt(TOPIC_STATE_START_MM,startNextMm);mqttPublishInt(TOPIC_STATE_MHE,mustHoldEligible?1:0);}

static inline void pwmAttachCompat(){ledcAttach(MOTOR_PWM_PIN,PWM_FREQUENCY,PWM_RESOLUTION);} static inline void pwmWriteCompat(int v){ledcWrite(MOTOR_PWM_PIN,constrain(v,0,255));}
static void applyDirectionPin(){digitalWrite(MOTOR_DIR_PIN,(direction==DIRECTION_FORWARD)?HIGH:LOW);} 
static void updateMotorAuthority(){rampTarget=(eStopEngaged||direction==DIRECTION_NEUTRAL)?0:commandedPwm;}
static void serviceRamp(){
  const unsigned long now=millis();
  uint32_t wait=prescribedRampStepMs;
  if(now-lastRampMs<wait)return;
  lastRampMs=now;
  if(rampCurrent<rampTarget) rampCurrent++;
  else if(rampCurrent>rampTarget) rampCurrent--;
  if(stationFinalRampActive && rampCurrent<=rampTarget) stationFinalRampActive=false;
  pwmWriteCompat(rampCurrent);
}
static int readAveragedADC(){uint32_t sum=0;for(uint8_t i=0;i<ADC_SAMPLES;i++)sum+=analogRead(HALL_PIN);return (int)(sum/ADC_SAMPLES);}
static void recomputeHallThresholds(){northEnterThreshold=baselineCounts+HALL_DEADBAND_COUNTS+HALL_ENTRY_MARGIN_COUNTS;northExitThreshold=baselineCounts+HALL_DEADBAND_COUNTS;southEnterThreshold=baselineCounts-HALL_DEADBAND_COUNTS-HALL_ENTRY_MARGIN_COUNTS;southExitThreshold=baselineCounts-HALL_DEADBAND_COUNTS;}
static bool hallInsideBaselineBand(int raw){return raw<=northExitThreshold&&raw>=southExitThreshold;}
static HallState classifyCompletedHallEvent(int n,int s){if(n<(int)HALL_MIN_PEAK_DELTA&&s<(int)HALL_MIN_PEAK_DELTA)return HALL_NONE;if(n>=HALL_MIN_PEAK_DELTA && (uint32_t)n*100U>=(uint32_t)s*HALL_DOMINANCE_PERCENT)return HALL_NORTH;if(s>=HALL_MIN_PEAK_DELTA && (uint32_t)s*100U>=(uint32_t)n*HALL_DOMINANCE_PERCENT)return HALL_SOUTH;return HALL_NONE;}
static void publishHallBaselineTelemetry(const char* state,const char* reason,int raw){char b[360];snprintf(b,sizeof(b),"{\"event\":\"HALL_BASELINE\",\"state\":\"%s\",\"reason\":\"%s\",\"raw\":%d,\"baseline\":%d,\"delta\":%d,\"north_enter\":%d,\"north_exit\":%d,\"south_enter\":%d,\"south_exit\":%d,\"stable_samples\":%u,\"unstable_samples\":%u,\"last_step\":%d}",state,reason,raw,baselineCounts,raw-baselineCounts,northEnterThreshold,northExitThreshold,southEnterThreshold,southExitThreshold,(unsigned)hallBaselineStableSamples,(unsigned)hallBaselineUnstableSamples,hallBaselineLastStepCounts);mqttPublish(TOPIC_STATE_HALL,b,false);}
static void serviceHallBaselineTelemetry(int raw){unsigned long now=millis();if(now-hallBaselineTelemLastMs<HALL_BASELINE_TELEM_INTERVAL_MS)return;hallBaselineTelemLastMs=now;publishHallBaselineTelemetry(hallBaselineTrackerState,hallBaselineFrozenReason,raw);}
static bool hallBaselineSignalStable(int raw){if(hallBaselineLastRaw==0){hallBaselineLastRaw=raw;hallBaselineStableSamples=0;hallBaselineUnstableSamples=0;hallBaselineLastStepCounts=0;return false;}int step=raw-hallBaselineLastRaw;hallBaselineLastStepCounts=step;int absStep=step<0?-step:step;hallBaselineLastRaw=raw;if(absStep<=(int)HALL_BASELINE_STABLE_DELTA_COUNTS){if(hallBaselineStableSamples<255)hallBaselineStableSamples++;hallBaselineUnstableSamples=0;}else{hallBaselineStableSamples=0;if(hallBaselineUnstableSamples<255)hallBaselineUnstableSamples++;}return hallBaselineStableSamples>=HALL_BASELINE_STABLE_SAMPLES;}
static bool hallBaselineTrackIfIdle(int raw){bool stable=hallBaselineSignalStable(raw);int err=raw-baselineCounts;int absErr=err<0?-err:err;if(stable && absErr<=(int)HALL_BASELINE_TRACK_WINDOW_COUNTS){int32_t target=((int32_t)raw)<<8;baselineCountsQ8 += (target-baselineCountsQ8)/(int32_t)HALL_BASELINE_TRACK_K;int newBaseline=(int)((baselineCountsQ8+128)>>8);if(newBaseline!=baselineCounts){baselineCounts=newBaseline;recomputeHallThresholds();}hallBaselineTrackerState="TRACKING";hallBaselineFrozenReason="NONE";return true;}hallBaselineTrackerState="FROZEN";hallBaselineFrozenReason=stable?"OUTSIDE_TRACK_WINDOW":"UNSTABLE";return false;}
static void calibrateBaseline(){unsigned long start=millis();uint32_t sum=0,count=0;while(millis()-start<CALIBRATION_TIME_MS){sum+=readAveragedADC();count++;delay(5);}baselineCounts=count?(int)(sum/count):readAveragedADC();baselineCountsQ8=((int32_t)baselineCounts)<<8;hallBaselineLastRaw=baselineCounts;hallBaselineStableSamples=0;hallBaselineUnstableSamples=0;hallBaselineLastStepCounts=0;recomputeHallThresholds();Serial.printf("[DNA] Hall thresholds deadband=%d entryMargin=%d minPeak=%d dominance=%u baseline=%d Nenter=%d Senter=%d trackK=%ld trackWindow=%u stableDelta=%u stableSamples=%u\n",HALL_DEADBAND_COUNTS,HALL_ENTRY_MARGIN_COUNTS,HALL_MIN_PEAK_DELTA,HALL_DOMINANCE_PERCENT,baselineCounts,northEnterThreshold,southEnterThreshold,(long)HALL_BASELINE_TRACK_K,(unsigned)HALL_BASELINE_TRACK_WINDOW_COUNTS,(unsigned)HALL_BASELINE_STABLE_DELTA_COUNTS,(unsigned)HALL_BASELINE_STABLE_SAMPLES);}

static void clearRollingEvidence(){dnaHead=0;dnaCount=0;haveLastMagnet=false;lastMagnetTime=0;}
static void dnaPush(uint8_t p){dnaBuf[dnaHead]=p;dnaHead=(dnaHead+1)%DNA_W_MAX;if(dnaCount<DNA_W_MAX)dnaCount++;}
static uint8_t dnaGetOldest(uint8_t i){uint8_t start=(dnaHead+DNA_W_MAX-dnaCount)%DNA_W_MAX;return dnaBuf[(start+i)%DNA_W_MAX];}
static bool patternMatchesAt(uint8_t current,int8_t dir,uint8_t len){for(uint8_t i=0;i<len;i++){uint8_t back=len-1-i;uint8_t mm=(dir==MAP_CW)?routeMod((int32_t)current-back):routeMod((int32_t)current+back);if(dnaGetOldest(dnaCount-len+i)!=dnaAt(mm))return false;}return true;}
static DnaMatchResult matchExactFromBuffer(){
  DnaMatchResult r={0,{0,MAP_UNSET}};
  if(dnaCount<DNA_W_LOCK)return r;

  // r12 architectural rule: the commanded/active direction is a physical
  // constraint, not a candidate to be inferred from Hall data.  The inactive
  // direction never participates in normal matching or reacquisition.
  int8_t d=(activeMapDir!=MAP_UNSET)?activeMapDir:declaredSessionDir;
  if(d==MAP_UNSET)return r;

  for(uint8_t mm=0;mm<DNA_N;mm++){
    if(patternMatchesAt(mm,d,DNA_W_LOCK)){
      if(r.count==0)r.first={mm,d};
      r.count++;
    }
  }
  return r;
}
static void publishNavState(const char* event,const DnaMatchResult* m,unsigned long dt){char b[320];uint8_t candidate=(m&&m->count)?m->first.mm:0;int8_t cdir=(m&&m->count)?m->first.dir:MAP_UNSET;snprintf(b,sizeof(b),"{\"event\":\"%s\",\"position_state\":\"%s\",\"direction_state\":\"%s\",\"certainty\":%.3f,\"odom\":%u,\"last_exact\":%u,\"dir\":\"%s\",\"candidate_mm\":%u,\"candidate_dir\":\"%s\",\"cands\":%u,\"dt\":%lu,\"motion\":\"%s\"}",event,posStateName(positionState),dirStateName(directionState),dashboardConfidence(),(unsigned)(odomValid?odomMm:0),(unsigned)lastExactMm,mapDirName(activeMapDir),(unsigned)candidate,mapDirName(cdir),(unsigned)(m?m->count:0),dt,motionName(motionTimingState));mqttPublish(TOPIC_STATE_DNA,b,true);mqttPublish(TOPIC_DNA_EVENT,b,false);Serial.printf("[DNA] %s\n",b);}
// ---------------------------------------------------------------------------
// DNA correction gate — Sam/Claude design, r7.
//
// The fundamental principle: a DNA match is new evidence, not ground truth.
// The loco already knows where it is from sequential physical magnet hits.
// A DNA correction is only accepted when it is consistent with that knowledge.
//
// Three classes of correction, each with its own acceptance rule:
//
// 1. SAME direction, SMALL correction (<=NEARBY_REPAIR_MARKERS):
//    Apply immediately.  A small same-direction nudge is almost certainly real.
//
// 2. SAME direction, LARGE correction (>NEARBY_REPAIR_MARKERS):
//    Log as candidate.  Require the NEXT DNA match to be physically consecutive
//    with this one before accepting.  A large same-direction teleport is more
//    likely a false DNA match on a repeated pattern than a real correction.
//
// 3. OPPOSITE direction:
//    Log as candidate.  Require the NEXT DNA match to be in the same direction
//    AND at the physically expected next marker.  Never alter odomMm, lastExactMm,
//    or activeMapDir until confirmed.
//
// In all pending cases the operational position is UNCHANGED.  The loco
// continues on its current route and station commitment.
// ---------------------------------------------------------------------------
static const uint8_t NEARBY_REPAIR_MARKERS = 5; // same-dir corrections within this are applied immediately

// Pending correction state — cleared on any confirmed acceptance or same-dir small repair.
static uint8_t  pendingCandidateMm  = 0;
static int8_t   pendingCandidateDir = MAP_UNSET;
static bool     pendingCorrectionActive = false;
// Sequence counter for accepted Hall events (events that passed the interval
// reject filter and entered onMagnetEvent).  A pending correction is only
// confirmable on the immediately next accepted event — not some later one after
// a run of noisy or ambiguous Hall hits.
static uint32_t acceptedHallEventSeq = 0;
static uint32_t pendingCandidateSeq  = 0;

static void clearPendingCorrection(){
  pendingCorrectionActive=false;
  pendingCandidateMm=0;
  pendingCandidateDir=MAP_UNSET;
  pendingCandidateSeq=0;
}

// Apply a confirmed DNA correction. Normal odometry advances in odomOnMagnet().
static void applyCorrection(const DnaMatchResult& m, unsigned long dt, bool dirChanged, bool posChanged){
  clearPendingCorrection();
  activeMapDir=m.first.dir; odomMm=m.first.mm; odomValid=true; lastExactMm=odomMm;
  positionState=POSITION_EXACT_LOCKED; directionState=DIRECTION_CONFIRMED;
  const char* ev = dirChanged  ? "DNA_EXACT_LOCK_DIR_CORRECTION"  :
                   posChanged  ? "DNA_EXACT_LOCK_ODOM_CORRECTION" :
                                 "DNA_EXACT_LOCK";
  publishNavState(ev,&m,dt);
}

static void acceptExact(const DnaMatchResult& m, unsigned long dt){
  // NOTE (r11): acceptedHallEventSeq is now incremented in onMagnetEvent() for
  // EVERY accepted Hall event (after the 700ms reject, after reversal-rehit
  // handling), not only for unique exact matches.  This closes the r9 gate
  // bypass where a run of ambiguous/outlier/buffering events between a pending
  // correction and its confirming match left the counter unchanged, letting a
  // match arriving many events later still satisfy (pendingSeq + 1 == seq).
  const bool dirChanged  = (activeMapDir!=MAP_UNSET && activeMapDir!=m.first.dir);
  const bool posChanged  = (odomValid && odomMm!=m.first.mm);

  // Not running — accept any exact match immediately with no constraints.
  if(!running){
    applyCorrection(m,dt,dirChanged,posChanged);
    return;
  }

  // ---- DIRECTION REVERSAL ------------------------------------------------
  if(dirChanged){
    // Compute what the physically expected next marker would be if a reversal
    // candidate from the last event is continuing.
    bool isConsecutive = false;
    if(pendingCorrectionActive && pendingCandidateDir==m.first.dir){
      // Confirmation requires: (a) immediately next accepted Hall event,
      // (b) physically consecutive candidate marker.
      bool isNextEvent = (pendingCandidateSeq + 1 == acceptedHallEventSeq);
      uint8_t expected = stepMm(pendingCandidateMm, pendingCandidateDir);
      isConsecutive = isNextEvent && (m.first.mm == expected);
    }

    if(isConsecutive){
      // Two immediately consecutive opposite-direction matches — confirmed reversal.
      char b[192];
      snprintf(b,sizeof(b),
        "{\"event\":\"DNA_DIR_REVERSAL_CONFIRMED\",\"from_dir\":\"%s\",\"to_dir\":\"%s\",\"candidate_mm\":%u,\"confirm_mm\":%u}",
        mapDirName(activeMapDir),mapDirName(m.first.dir),
        (unsigned)pendingCandidateMm,(unsigned)m.first.mm);
      mqttPublish(TOPIC_DNA_EVENT,b,false);
      Serial.printf("[DNA] DIR_REVERSAL_CONFIRMED\n");
      applyCorrection(m,dt,true,true);
    } else {
      // First opposite-direction candidate — log only, do not touch state.
      pendingCandidateMm=m.first.mm;
      pendingCandidateDir=m.first.dir;
      pendingCandidateSeq=acceptedHallEventSeq;
      pendingCorrectionActive=true;
      char b[192];
      snprintf(b,sizeof(b),
        "{\"event\":\"DNA_DIR_REVERSAL_PENDING\",\"current_dir\":\"%s\",\"candidate_dir\":\"%s\",\"candidate_mm\":%u,\"action\":\"OBSERVE_ONLY\"}",
        mapDirName(activeMapDir),mapDirName(m.first.dir),(unsigned)m.first.mm);
      mqttPublish(TOPIC_DNA_EVENT,b,false);
      Serial.printf("[DNA] DIR_REVERSAL_PENDING — no state change, watching\n");
    }
    return;
  }

  // ---- SAME DIRECTION ----------------------------------------------------
  // Clear any stale opposite-direction pending — a same-dir match disproves it.
  if(pendingCorrectionActive && pendingCandidateDir!=m.first.dir){
    clearPendingCorrection();
  }

  if(!posChanged){
    // No correction needed — clean lock, just confirm.
    clearPendingCorrection();
    applyCorrection(m,dt,false,false);
    return;
  }

  // Compute correction distance.
  uint8_t dist = (uint8_t)min(
    (int)routeMod((int32_t)m.first.mm - odomMm),
    (int)routeMod((int32_t)odomMm - m.first.mm));

  if(dist <= NEARBY_REPAIR_MARKERS){
    // Small same-direction correction — apply immediately.
    clearPendingCorrection();
    applyCorrection(m,dt,false,true);
    return;
  }

  // Large same-direction correction — require consecutive confirmation.
  bool isConsecutive = false;
  if(pendingCorrectionActive && pendingCandidateDir==m.first.dir){
    bool isNextEvent = (pendingCandidateSeq + 1 == acceptedHallEventSeq);
    uint8_t expected = stepMm(pendingCandidateMm, pendingCandidateDir);
    isConsecutive = isNextEvent && (m.first.mm == expected);
  }

  if(isConsecutive){
    // Confirmed large relocation — immediately consecutive candidate.
    char b[192];
    snprintf(b,sizeof(b),
      "{\"event\":\"DNA_LARGE_CORRECTION_CONFIRMED\",\"from_mm\":%u,\"to_mm\":%u,\"dist\":%u,\"dir\":\"%s\"}",
      (unsigned)odomMm,(unsigned)m.first.mm,(unsigned)dist,mapDirName(m.first.dir));
    mqttPublish(TOPIC_DNA_EVENT,b,false);
    Serial.printf("[DNA] LARGE_CORRECTION_CONFIRMED dist=%u\n",(unsigned)dist);
    applyCorrection(m,dt,false,true);
  } else {
    // First large-correction candidate — log only, do not touch state.
    pendingCandidateMm=m.first.mm;
    pendingCandidateDir=m.first.dir;
    pendingCandidateSeq=acceptedHallEventSeq;
    pendingCorrectionActive=true;
    char b[192];
    snprintf(b,sizeof(b),
      "{\"event\":\"DNA_LARGE_CORRECTION_PENDING\",\"current_mm\":%u,\"candidate_mm\":%u,\"dist\":%u,\"dir\":\"%s\",\"action\":\"OBSERVE_ONLY\"}",
      (unsigned)odomMm,(unsigned)m.first.mm,(unsigned)dist,mapDirName(m.first.dir));
    mqttPublish(TOPIC_DNA_EVENT,b,false);
    Serial.printf("[DNA] LARGE_CORRECTION_PENDING dist=%u — no state change\n",(unsigned)dist);
  }
}
static void odomOnMagnet(){int8_t d=(activeMapDir!=MAP_UNSET)?activeMapDir:declaredSessionDir;if(!odomValid){odomMm=startNextMm;odomValid=true;}else if(d!=MAP_UNSET)odomMm=stepMm(odomMm,d);}
// Minimum physical interval between accepted magnet events.
// The shortest NGR segment is ~300mm.  At the maximum realistic operating
// speed (~80 pKPH) that segment takes ~700ms.  Anything arriving faster is
// a pole-transition double-trigger — the trailing magnetic pole re-entering
// the Hall deadband as a second event.  The r5 field log proved that ghost
// peaks are strong enough to pass peak-delta filtering; timing is the only
// reliable discriminator.  400ms was too low — a 402ms ghost slipped through
// at Arches and produced a -133 PWM delta.  700ms covers all realistic speeds.
static const uint32_t MIN_MAGNET_INTERVAL_MS = 700UL;

// ---------------------------------------------------------------------------
// r12 continuity-first local repair.
//
// Once an exact position is established, each Hall event is evaluated first
// against the one physically expected next marker.  A single contradiction
// opens a small local episode; it does not revoke direction, position history,
// station service, or motor authority.
//
// Three local explanations are compared over the next observations:
//   NORMAL:    every Hall event advanced one marker; one polarity was wrong.
//   SKIP_ONE:  one physical marker was missed before the first observation.
//   DUP_ONE:   the first observation was a duplicate and did not advance.
//
// Only compelling repeated evidence applies a +/-1 odometer correction.
// Otherwise normal continuity wins with at most one polarity outlier.  If five
// observations remain strongly contradictory, exact lock is released and the
// existing whole-map matcher is allowed to reacquire -- in the commanded
// direction only.
// ---------------------------------------------------------------------------
static const uint8_t CONTINUITY_WINDOW_MAX = 5;
static bool continuityDiscrepancyActive=false;
static uint8_t continuityAnchorMm=0;
static uint8_t continuityObs[CONTINUITY_WINDOW_MAX];
static uint8_t continuityObsCount=0;

static void clearContinuityDiscrepancy(){
  continuityDiscrepancyActive=false;
  continuityAnchorMm=0;
  continuityObsCount=0;
}

static uint8_t continuityMismatchCount(uint8_t hypothesis){
  // 0=NORMAL, 1=SKIP_ONE, 2=DUP_ONE
  if(!continuityDiscrepancyActive || activeMapDir==MAP_UNSET)return 255;
  uint8_t bad=0;
  for(uint8_t i=0;i<continuityObsCount;i++){
    uint8_t steps;
    if(hypothesis==1)steps=(uint8_t)(i+2);       // missed one marker
    else if(hypothesis==2)steps=(i==0)?0:i;      // first event duplicated anchor
    else steps=(uint8_t)(i+1);                   // ordinary continuity
    uint8_t mm=stepMm(continuityAnchorMm,activeMapDir,steps);
    if(continuityObs[i]!=dnaAt(mm))bad++;
  }
  return bad;
}

static void publishContinuityEvent(const char* event,uint8_t normalBad,uint8_t skipBad,uint8_t dupBad,unsigned long dt){
  char b[320];
  snprintf(b,sizeof(b),
    "{\"event\":\"%s\",\"anchor_mm\":%u,\"odom\":%u,\"dir\":\"%s\",\"observations\":%u,\"normal_bad\":%u,\"skip_bad\":%u,\"duplicate_bad\":%u,\"dt\":%lu}",
    event,(unsigned)continuityAnchorMm,(unsigned)odomMm,mapDirName(activeMapDir),
    (unsigned)continuityObsCount,(unsigned)normalBad,(unsigned)skipBad,
    (unsigned)dupBad,dt);
  mqttPublish(TOPIC_DNA_EVENT,b,false);
  Serial.printf("[DNA] %s\n",b);
}

static void continuityStart(uint8_t pol,unsigned long dt){
  continuityDiscrepancyActive=true;
  continuityAnchorMm=odomMm;      // last confirmed position remains the anchor
  continuityObsCount=1;
  continuityObs[0]=pol;

  // Operational odometry follows the predicted normal path while evidence
  // accumulates.  This preserves station/traffic continuity; the anchor above
  // remains available for a bounded repair.
  odomMm=stepMm(continuityAnchorMm,activeMapDir,1);
  publishContinuityEvent("CONTINUITY_DISCREPANCY_OPEN",1,
    continuityMismatchCount(1),continuityMismatchCount(2),dt);
}

static void continuityConsume(uint8_t pol,unsigned long dt){
  if(!continuityDiscrepancyActive || activeMapDir==MAP_UNSET)return;

  if(continuityObsCount<CONTINUITY_WINDOW_MAX){
    continuityObs[continuityObsCount++]=pol;
  }

  // Continue the normal predicted path until local evidence resolves it.
  odomMm=stepMm(continuityAnchorMm,activeMapDir,continuityObsCount);

  uint8_t normalBad=continuityMismatchCount(0);
  uint8_t skipBad=continuityMismatchCount(1);
  uint8_t dupBad=continuityMismatchCount(2);

  // A compelling missed-marker explanation: two or more observations fit the
  // +1 path exactly while normal continuity has accumulated two contradictions.
  if(continuityObsCount>=2 && skipBad==0 && normalBad>=2 && skipBad<dupBad){
    odomMm=stepMm(continuityAnchorMm,activeMapDir,(uint8_t)(continuityObsCount+1));
    lastExactMm=odomMm;
    positionState=POSITION_EXACT_LOCKED;
    directionState=DIRECTION_CONFIRMED;
    publishContinuityEvent("CONTINUITY_REPAIR_MISSED_MARKER",normalBad,skipBad,dupBad,dt);
    clearContinuityDiscrepancy();
    return;
  }

  // A compelling duplicate-event explanation.
  if(continuityObsCount>=2 && dupBad==0 && normalBad>=2 && dupBad<skipBad){
    odomMm=stepMm(continuityAnchorMm,activeMapDir,(uint8_t)(continuityObsCount-1));
    lastExactMm=odomMm;
    positionState=POSITION_EXACT_LOCKED;
    directionState=DIRECTION_CONFIRMED;
    publishContinuityEvent("CONTINUITY_REPAIR_DUPLICATE_EVENT",normalBad,skipBad,dupBad,dt);
    clearContinuityDiscrepancy();
    return;
  }

  // Continuity votes: after three observations, retain the predicted route
  // when it contains no more than the original single outlier and is at least
  // as good as either displacement hypothesis.
  if(continuityObsCount>=3 && normalBad<=1 && normalBad<=skipBad && normalBad<=dupBad){
    lastExactMm=odomMm;
    positionState=POSITION_EXACT_LOCKED;
    directionState=DIRECTION_CONFIRMED;
    publishContinuityEvent("CONTINUITY_RETAINED_OUTLIER_AGED",normalBad,skipBad,dupBad,dt);
    clearContinuityDiscrepancy();
    return;
  }

  if(continuityObsCount>=CONTINUITY_WINDOW_MAX){
    // Sustained contradiction: exact location is now genuinely uncertain.
    // Direction remains fixed.  Rolling evidence is retained so same-direction
    // W12 reacquisition can occur on following Hall events.
    positionState=POSITION_DECLARED;
    directionState=DIRECTION_CONFIRMED;
    publishContinuityEvent("CONTINUITY_LOST_REACQUIRE_SAME_DIRECTION",normalBad,skipBad,dupBad,dt);
    clearContinuityDiscrepancy();
    return;
  }

  publishContinuityEvent("CONTINUITY_DISCREPANCY_ACCUMULATING",normalBad,skipBad,dupBad,dt);
}

static void onMagnetEvent(HallState hallPol,int peakN,int peakS,unsigned long hallMs){
  unsigned long now=millis();
  uint8_t pol=(hallPol==HALL_NORTH)?1:0;
  unsigned long dt=haveLastMagnet?(now-lastMagnetTime):0;

  // Reject impossibly short Hall events by duration before any navigation
  // consumption. Must return before ANY state mutation: no odometry, no DNA,
  // no speed-governor sample, no station/traffic/CTO2 truth, and crucially
  // WITHOUT updating lastMagnetTime (a write here would corrupt the dt of the
  // next real magnet). Logged so ignored events remain visible. This test
  // intentionally precedes interval rejection so 20-25ms threshold flickers
  // are reported as short events rather than hidden as timing rejects.
  if(hallMs < HALL_MIN_EVENT_MS){
    char b[240];
    snprintf(b,sizeof(b),
      "{\"event\":\"HALL_SHORT_EVENT_IGNORED\",\"hallms\":%lu,\"min_ms\":%lu,\"peakN\":%d,\"peakS\":%d,\"obs\":\"%c\",\"ramp_pwm\":%d,\"motion\":\"%s\",\"cand_mm\":%u,\"action\":\"NO_ODOM_NO_SPEED_NO_DNA\"}",
      hallMs,(unsigned long)HALL_MIN_EVENT_MS,peakN,peakS,dnaPolChar(pol),
      rampCurrent,motionName(motionTimingState),(unsigned)(odomValid?odomMm:0));
    mqttPublish(TOPIC_DNA_EVENT,b,false);
    Serial.printf("[HALL] SHORT_EVENT_IGNORED hallms=%lu min=%lu peakN=%d peakS=%d motion=%s\n",
      hallMs,(unsigned long)HALL_MIN_EVENT_MS,peakN,peakS,motionName(motionTimingState));
    return;
  }

  // Reject pole-transition double-triggers before touching any navigation state.
  if(haveLastMagnet && dt < MIN_MAGNET_INTERVAL_MS){
    Serial.printf("[HALL] INTERVAL_REJECT dt=%lu peakN=%d peakS=%d\n",dt,peakN,peakS);
    return;
  }

  if(launchHallEvidenceTooWeak(hallMs)){
    char b[240];
    snprintf(b,sizeof(b),
      "{\"event\":\"LAUNCH_HALL_EVIDENCE_IGNORED\",\"ramp_pwm\":%d,\"motion\":\"%s\",\"hallms\":%lu,\"peakN\":%d,\"peakS\":%d,\"action\":\"NO_ODOM_NO_SPEED\"}",
      rampCurrent,motionName(motionTimingState),hallMs,peakN,peakS);
    mqttPublish(TOPIC_DNA_EVENT,b,false);
    Serial.printf("[HALL] LAUNCH_EVIDENCE_IGNORED ramp=%d motion=%s hallms=%lu peakN=%d peakS=%d\n",
      rampCurrent,motionName(motionTimingState),hallMs,peakN,peakS);
    return;
  }

  // Hall navigation belongs to AUTO only.  Manual running never changes
  // odometry, DNA history, station logic, speed feedback, or route confidence.
  if(!dispatcherAuto){
    Serial.printf("[HALL] IGNORED_MANUAL obs=%c peakN=%d peakS=%d hallms=%lu\n",
      dnaPolChar(pol),peakN,peakS,hallMs);
    return;
  }

  if(reversalRehitPending){
    bool same=(pol==reversalAnchorPol);
    reversalRehitPending=false;
    if(same){
      lastMagnetTime=now;haveLastMagnet=true;
      char b[120];
      snprintf(b,sizeof(b),"REVERSAL_REHIT anchor=MM%03u obs=%c odom_held=1",reversalAnchorMm,dnaPolChar(pol));
      mqttPublish(TOPIC_MM_POS,b,false);mqttPublish(TOPIC_MM_MAG,b,false);Serial.printf("[DNA] %s\n",b);
      return;
    }
  }

  lastMagnetTime=now;haveLastMagnet=true;

  // r11: Count EVERY accepted Hall event exactly once, here — after the 700ms
  // pole-transition reject and after reversal-rehit handling (a rehit returns
  // early above and is deliberately NOT counted, since it holds odom rather
  // than advancing navigation).  Every event that reaches the DNA matching /
  // correction logic below now advances the sequence, so a pending correction
  // can only be confirmed by the genuinely-next accepted event — including when
  // the intervening events are ambiguous, outlier, or buffering.
  acceptedHallEventSeq++;

  // A known-location station departure remains in its proving ramp through the
  // first post-stop Hall hit and hands off only on the second.  The first has
  // dt=0 by design after a complete stop; the second yields the first usable
  // magnet-to-magnet speed interval.
  if(stationDepartureLaunchActive && stationDeparturePostStopHits<2){
    stationDeparturePostStopHits++;
  }

  dnaPush(pol);
  rampAvgPwm=rampSamples?(uint8_t)(rampAccum/rampSamples):(uint8_t)rampCurrent;
  prevRampAvgPwm=rampAvgPwm;rampAccum=0;rampSamples=0;
  if(motionTimingState==RESTARTING) motionTimingState=MOVING;

  DnaMatchResult m={0,{0,MAP_UNSET}};

  if(odomValid && positionState==POSITION_EXACT_LOCKED && activeMapDir!=MAP_UNSET){
    // Normal navigation is prediction first.  Do not search the railroad.
    if(continuityDiscrepancyActive){
      continuityConsume(pol,dt);
    }else{
      uint8_t expectedMm=stepMm(odomMm,activeMapDir);
      if(pol==dnaAt(expectedMm)){
        odomMm=expectedMm;
        lastExactMm=odomMm;
        directionState=DIRECTION_CONFIRMED;
      }else{
        continuityStart(pol,dt);
      }
    }
  }else{
    // Startup or genuine loss only: advance declared odometry and permit a
    // whole-map W12 search, restricted to the commanded direction.
    if(odomValid || (startIntervalSet && declaredSessionDir!=MAP_UNSET))odomOnMagnet();
    m=matchExactFromBuffer();
    if(m.count==1){
      acceptExact(m,dt);
    }else if(m.count>1){
      publishNavState("DNA_AMBIGUOUS_SAME_DIRECTION",&m,dt);
    }else{
      publishNavState("DNA_BUFFERING_SAME_DIRECTION",&m,dt);
    }
  }

  uint16_t seg=0;float speedMmS=0.0f;uint8_t from=DNA_UNKNOWN;
  if(odomValid&&dt>0&&activeMapDir!=MAP_UNSET){
    from=(activeMapDir==MAP_CW)?routeMod((int32_t)odomMm-1):routeMod((int32_t)odomMm+1);
    seg=segmentDistanceMm(from,odomMm,activeMapDir);
    if(seg)speedMmS=(float)seg*1000.0f/(float)dt;
  }

  // Refresh exact operational location, then select a prescribed station PWM
  // profile strictly from marker position. No Hall timing enters motor control.
  operationalNavRefresh();
  stationOnMagnet(opNav.mm,opNav.mapDir);

  // Diagnostic speed remains visible for later IR comparison only.
  if(from!=DNA_UNKNOWN && seg>0 && dt>0){
    const float diagnosticPkph=speedMmS/PKPH_MM_PER_SEC;
    char sb[260];
    snprintf(sb,sizeof(sb),
      "{\"source\":\"HALL_DIAGNOSTIC_ONLY\",\"from_mm\":%u,\"to_mm\":%u,\"segment_mm\":%u,\"dt_ms\":%lu,\"diagnostic_pkph\":%.2f,\"motor_authority\":false}",
      (unsigned)from,(unsigned)odomMm,(unsigned)seg,dt,diagnosticPkph);
    mqttPublish(TOPIC_MM_SPEED,sb,false);
  }

  char mag[360];
  snprintf(mag,sizeof(mag),"MM%03u %s obs=%c expected=%c pos=%s dir=%s dt=%lu timing_mode=%s effective_pwm_avg=%u seg_mm=%u speed_mm_s=%.1f peakN=%d peakS=%d hallms=%lu",(unsigned)(odomValid?odomMm:0),dnaLandmarkName(odomMm),dnaPolChar(pol),odomValid?dnaPolChar(dnaAt(odomMm)):'?',posStateName(positionState),dirStateName(directionState),dt,motionName(motionTimingState),(unsigned)rampAvgPwm,(unsigned)seg,speedMmS,peakN,peakS,hallMs);
  mqttPublish(TOPIC_MM_MAG,mag,false);Serial.printf("[MAG] %s\n",mag);

}

static void serviceHall(){
  int raw=readAveragedADC();
  unsigned long now=millis();

  if(!hallEventActive){
    bool tracked=hallBaselineTrackIfIdle(raw);
    serviceHallBaselineTelemetry(raw);
    int n=max(0,raw-baselineCounts),s=max(0,baselineCounts-raw);

    // Stable signal inside the broad tracking window is treated as baseline
    // drift, not as a magnet start. This avoids the bootstrap trap where a
    // sustained shifted quiescent level lies outside the narrow decoder band.
    if(tracked) return;

    if(raw>=northEnterThreshold||raw<=southEnterThreshold){
      int err=raw-baselineCounts;
      int absErr=err<0?-err:err;
      // A single abrupt sample inside the broad tracking window can be the
      // first view of a shifted quiescent baseline after a bad seed. Do not
      // start an event until the signal shows continued peaky movement, or
      // until it exceeds the broad window where it can only be a magnet.
      if(absErr>(int)HALL_BASELINE_TRACK_WINDOW_COUNTS || hallBaselineUnstableSamples>=2){
        hallEventActive=true;
        hallBaselineTrackerState="FROZEN";
        hallBaselineFrozenReason="EVENT";
        hallEventStartedAtMs=now;hallBaselineReturnAtMs=0;
        hallPeakNorthDelta=n;hallPeakSouthDelta=s;
        Serial.printf("[HALL] START raw=%d baseline=%d\n",raw,baselineCounts);
      }else{
        hallBaselineTrackerState="FROZEN";
        hallBaselineFrozenReason="SETTLING";
      }
    }
    return;
  }

  hallBaselineTrackerState="FROZEN";
  hallBaselineFrozenReason="EVENT";
  serviceHallBaselineTelemetry(raw);
  int n=max(0,raw-baselineCounts),s=max(0,baselineCounts-raw);
  hallPeakNorthDelta=max(hallPeakNorthDelta,n);
  hallPeakSouthDelta=max(hallPeakSouthDelta,s);
  if(hallInsideBaselineBand(raw)){
    if(hallBaselineReturnAtMs==0)hallBaselineReturnAtMs=now;
    if(now-hallBaselineReturnAtMs>=HALL_EVENT_EXIT_HOLD_MS){
      HallState h=classifyCompletedHallEvent(hallPeakNorthDelta,hallPeakSouthDelta);
      unsigned long dur=now-hallEventStartedAtMs;
      hallEventActive=false;
      hallBaselineStableSamples=0;
      hallBaselineUnstableSamples=0;
      hallBaselineLastStepCounts=0;
      hallBaselineLastRaw=raw;
      if(h!=HALL_NONE){
        Serial.printf("[HALL] END peakN=%d peakS=%d result=%c\n",hallPeakNorthDelta,hallPeakSouthDelta,h==HALL_NORTH?'N':'S');
        onMagnetEvent(h,hallPeakNorthDelta,hallPeakSouthDelta,dur);
      }else Serial.printf("[HALL] REJECT peakN=%d peakS=%d\n",hallPeakNorthDelta,hallPeakSouthDelta);
    }
  }else hallBaselineReturnAtMs=0;
}

static void updateMotionState(){
  MotionTimingState old=motionTimingState;
  if(rampCurrent<=MOTOR_DEAD_ZONE_PWM){
    motionTimingState=STOPPED_INFERRED;
  }else if(old==STOPPED_INFERRED){
    motionTimingState=RESTARTING;
    haveLastMagnet=false;
  }else if(stationDepartureLaunchActive && stationDeparturePostStopHits==0){
    // PWM above the dead zone is only an attempted departure.  Physical
    // movement is not asserted until the first post-stop Hall event arrives.
    motionTimingState=RESTARTING;
  }else if(prevRampAvgPwm>0 &&
           abs((int)rampCurrent-(int)prevRampAvgPwm)>(int)(prevRampAvgPwm/6)){
    motionTimingState=SPEED_TRANSITION;
  }else{
    motionTimingState=MOVING;
  }
  if(old!=motionTimingState){
    char b[160];
    snprintf(b,sizeof(b),"{\"state\":\"%s\",\"ramp_pwm\":%d,\"last_mm\":%u}",
             motionName(motionTimingState),rampCurrent,(unsigned)odomMm);
    mqttPublish(TOPIC_MM_MOTION,b,false);
    Serial.printf("[MOTION] %s\n",b);
  }
}
static bool parseStartIntervalPayload(const char* s,uint8_t* a,uint8_t* b){int x=-1,y=-1;if(sscanf(s,"%d-%d",&x,&y)!=2&&sscanf(s,"%d %d",&x,&y)!=2)return false;if(x<0||x>=DNA_N||y<0||y>=DNA_N)return false;uint8_t dxy=routeMod(y-x),dyx=routeMod(x-y);if(dxy!=1&&dyx!=1)return false;*a=x;*b=y;return true;}
static void deriveStartNextFromInterval(){if(!startIntervalSet||declaredSessionDir==MAP_UNSET)return;bool aToBcw=routeMod((int32_t)startIntervalB-startIntervalA)==1;startNextMm=(declaredSessionDir==MAP_CW)?(aToBcw?startIntervalB:startIntervalA):(aToBcw?startIntervalA:startIntervalB);}
static int8_t mapDirForMotorDirectionValue(int d){if(declaredSessionDir==MAP_UNSET)return MAP_UNSET;return d==DIRECTION_FORWARD?declaredSessionDir:(d==DIRECTION_REVERSE?oppositeMapDir(declaredSessionDir):MAP_UNSET);}
static void prepareForMapDirectionChange(int8_t newDir){if(newDir==MAP_UNSET)return;clearRollingEvidence();clearPendingCorrection();clearContinuityDiscrepancy();activeMapDir=newDir;directionState=DIRECTION_DECLARED;reversalRehitPending=odomValid;reversalAnchorMm=odomMm;reversalAnchorPol=odomValid?dnaAt(odomMm):DNA_UNKNOWN;char b[190];snprintf(b,sizeof(b),"DIRECTION_CHANGE map=%s position=%s anchor=MM%03u",mapDirName(newDir),posStateName(positionState),(unsigned)reversalAnchorMm);mqttPublish(TOPIC_MM_POS,b,false);mqttPublish(TOPIC_DNA_EVENT,b,false);Serial.printf("[DNA] %s\n",b);}
static void resetDnaRuntime(bool keepDeclaration){
  clearRollingEvidence();
  clearPendingCorrection();
  clearContinuityDiscrepancy();
  odomValid=false;
  lastExactMm=0;
  activeMapDir=declaredSessionDir;
  positionState=keepDeclaration?POSITION_DECLARED:POSITION_UNKNOWN;
  directionState=keepDeclaration?DIRECTION_DECLARED:DIRECTION_UNKNOWN;
  reversalRehitPending=false;
  // A new setup declaration replaces any old placement immediately.
  operationalNavClear();
  if(keepDeclaration){
    operationalNavRefresh();
    if(opNav.valid){
      // One setup event for TeleMonitor: this is a declaration, not a fresh
      // packet stream and not a motor command.
      ctoPublishState("LOCAL_TRUTH_DECLARED","DISPATCHER_INTERVAL_PLUS_DIRECTION");
    }
  }
}

static void ina219Setup(){if(ina219.begin()){ina219Available=true;Serial.println("[INA] ready");}else Serial.println("[INA] unavailable");}
static void serviceInaTelemetry(){if(!ina219Available||millis()-lastInaTelemMs<INA219_TELEM_INTERVAL_MS)return;lastInaTelemMs=millis();lastBusVoltageV=ina219.getBusVoltage_V();lastCurrentA=ina219.getCurrent_mA()/1000.0f;lastPowerW=ina219.getPower_mW()/1000.0f;bool newLowVolt=lastBusVoltageV>0.1f&&lastBusVoltageV<LOW_VOLTAGE_THRESHOLD_V;char b[24];snprintf(b,sizeof(b),"%.2f",lastBusVoltageV);mqttPublish(TOPIC_TELEM_VOLTAGE,b);snprintf(b,sizeof(b),"%.2f",lastCurrentA);mqttPublish(TOPIC_TELEM_CURRENT,b);snprintf(b,sizeof(b),"%.2f",lastPowerW);mqttPublish(TOPIC_TELEM_POWER,b);if(newLowVolt!=lastLowVolt){lastLowVolt=newLowVolt;mqttPublishInt(TOPIC_STATE_LOWVOLT,lastLowVolt?1:0);}}

static void onMQTTMessage(char* topic,byte* payload,unsigned int len){
  char b[96]; if(len>=sizeof(b))len=sizeof(b)-1; memcpy(b,payload,len); b[len]='\0'; int val=atoi(b);
  if(strcmp(topic,TOPIC_CMD_AUTO)==0){ if(val==1)enterDispatcherAutoMode(); else {mqttWarn("AUTO DISCHARGE — USE DISPATCHER RELEASE");publishSimpleStates();} return; }
  if(strcmp(topic,TOPIC_CMD_DISPATCHER_RELEASE)==0){ if(val==1)leaveDispatcherAutoMode(); return; }
  if(strcmp(topic,TOPIC_CMD_SESSION_DIRECTION)==0){if(!strcasecmp(b,"CW")||!strcmp(b,"1"))declaredSessionDir=MAP_CW;else if(!strcasecmp(b,"CCW")||!strcmp(b,"2"))declaredSessionDir=MAP_CCW;else{mqttWarn("SESSION_DIRECTION MUST BE CW OR CCW");return;}navReady=true;deriveStartNextFromInterval();resetDnaRuntime(startIntervalSet);stationResetTestCycle("SESSION_DIRECTION_SET");publishSimpleStates();return;}
  if(strcmp(topic,TOPIC_CMD_START_INTERVAL)==0){uint8_t a,z;if(!parseStartIntervalPayload(b,&a,&z)){mqttWarn("START_INTERVAL MUST BE ADJACENT LIKE 063-064");return;}startIntervalA=a;startIntervalB=z;startIntervalSet=true;startReferenceIsExactMm=false;deriveStartNextFromInterval();resetDnaRuntime(true);stationResetTestCycle("START_INTERVAL_SET");publishSimpleStates();return;}
  if(strcmp(topic,TOPIC_CMD_START_MM)==0){if(val<0||val>=DNA_N){mqttWarn("START_MM MUST BE 0-170");return;}startNextMm=val;startIntervalSet=true;startReferenceIsExactMm=true;resetDnaRuntime(true);stationResetTestCycle("START_MM_SET");publishSimpleStates();return;}
  if(strcmp(topic,TOPIC_CMD_DNA_RESET)==0){resetDnaRuntime(navReady&&startIntervalSet);stationResetTestCycle("DNA_RESET");publishNavState("DNA_RESET",nullptr,0);return;}
  if(strcmp(topic,TOPIC_CMD_MHE)==0){mqttWarn("MHE/CTO DISABLED IN LL_PWM_DNA_AUTO_1_0");return;}
  if(strcmp(topic,TOPIC_CMD_DIRECTION)==0){if(dispatcherAuto){mqttWarn("DISPATCHER IN CONTROL");return;}if(rampCurrent>MOTOR_DEAD_ZONE_PWM){mqttWarn("WAIT FOR COMPLETE STOP BEFORE CHANGING DIRECTION");return;}int oldDir=direction;direction=constrain(val,0,2);int8_t oldMap=mapDirForMotorDirectionValue(oldDir),newMap=mapDirForMotorDirectionValue(direction);if(direction==DIRECTION_NEUTRAL){commandedPwm=0;}else if(oldMap!=MAP_UNSET&&newMap!=MAP_UNSET&&oldMap!=newMap)prepareForMapDirectionChange(newMap);applyDirectionPin();updateMotorAuthority();publishSimpleStates();return;}
  if(strcmp(topic,TOPIC_CMD_THROTTLE)==0){if(dispatcherAuto){mqttWarn("DISPATCHER IN CONTROL");return;}if(eStopEngaged){mqttWarn("E-STOP ENGAGED");return;}if(!navReady||!startIntervalSet){mqttWarn("SET SESSION_DIRECTION AND START_INTERVAL BEFORE MOVING");return;}commandedPwm=constrain(val,0,255);updateMotorAuthority();publishSimpleStates();return;}
  if(strcmp(topic,TOPIC_CMD_BRAKE)==0){if(dispatcherAuto){mqttWarn("DISPATCHER IN CONTROL");return;}brakeValue=constrain(val,0,255);publishSimpleStates();return;}
  if(strcmp(topic,TOPIC_CMD_ESTOP)==0){eStopEngaged=val!=0;if(eStopEngaged){stationResetTestCycle("MQTT_ESTOP");running=false;commandedPwm=0;rampCurrent=0;pwmWriteCompat(0);}updateMotorAuthority();publishSimpleStates();return;}
}

static void serviceMQTTReconnect(){
  if(mqtt.connected())return; static unsigned long last=0; if(millis()-last<3000)return; last=millis();
  if(mqtt.connect(MQTT_CLIENT_ID,NULL,NULL,TOPIC_ONLINE,0,true,"0")){
    mqtt.publish(TOPIC_ONLINE,"1",true);
    mqtt.subscribe(TOPIC_CMD_THROTTLE); mqtt.subscribe(TOPIC_CMD_DIRECTION); mqtt.subscribe(TOPIC_CMD_BRAKE); mqtt.subscribe(TOPIC_CMD_ESTOP);
    mqtt.subscribe(TOPIC_CMD_AUTO); mqtt.subscribe(TOPIC_CMD_DISPATCHER_RELEASE);
    mqtt.subscribe(TOPIC_CMD_SESSION_DIRECTION); mqtt.subscribe(TOPIC_CMD_START_INTERVAL); mqtt.subscribe(TOPIC_CMD_START_MM); mqtt.subscribe(TOPIC_CMD_DNA_RESET); mqtt.subscribe(TOPIC_CMD_MHE);
    publishSimpleStates();
    mqtt.publish(TOPIC_STATE_CTO_LEGACY,"",true);
    ctoPublishBoot("MQTT_CONNECTED");
    Serial.println("[MQTT] connected");
  }
}

static void serviceSerial(){
  if(!Serial.available())return; char c=(char)Serial.read();
  if(c=='p'){const int8_t n=nearestSameDirectionAheadIndex();Serial.printf("[STATE] LL_PWM_DNA_AUTO_1_0 auto=%d running=%d truth=%s/MM%03u dir=%s peers=%u nearest=%lu gov=%s target=%.1f traffic=%s blocker=%lu final=MM%03u station=%s mhe=%d hold=%d holdRear=%lu cmd=%d ramp=%d motion=%s\n",dispatcherAuto,running,ctoTruthSourceName(opNav.source),(unsigned)opNav.mm,mapDirName(opNav.mapDir),(unsigned)freshPeerCount(),(unsigned long)(n>=0?peerRegistry[n].locoId:0),speedGovModeName(speedGovMode),effectiveGovernorTargetPkph(),trafficPhaseName(trafficPhase),(unsigned long)trafficBlockerId,(unsigned)trafficFinalHallMm,stationPhaseName(stationTestPhase),mustHoldEligible?1:0,stationHoldingForRear?1:0,(unsigned long)stationHoldRearId,commandedPwm,rampCurrent,motionName(motionTimingState));}
  if(c=='r')resetDnaRuntime(navReady&&startIntervalSet);
}

static void onReceive(const esp_now_recv_info_t *info,const uint8_t *data,int len){
  (void)info;
  if(len==(int)sizeof(CtoPeerPacket)){
    // Deliberately ignore all locomotive peer packets in single-train 1.0.
    return;
    CtoPeerPacket cp={};
    memcpy(&cp,data,sizeof(cp));
    if(cp.magic==CTO2_MAGIC && cp.version==CTO2_VERSION){
      if(cp.senderId==LOCO_ID){ctoRxSelf++;return;}
      const int8_t slot=peerSlotForId(cp.senderId);
      PeerEntry& p=peerRegistry[slot];
      const bool newPeer=!p.seen || p.locoId!=cp.senderId;
      const bool wasFresh=peerFreshIndex((uint8_t)slot);
      p.seen=true;
      p.locoId=cp.senderId;
      p.sequence=cp.sequence;
      p.mm=cp.hallMm;
      p.frontBoundaryMm=cp.frontBoundaryMm;
      p.rearBoundaryMm=cp.rearBoundaryMm;
      p.mapDir=cp.mapDir;
      p.autoMode=cp.autoMode!=0;
      p.running=cp.running!=0;
      p.motionState=cp.motionState;
      p.rampPwm=cp.rampPwm;
      p.speedValid=cp.speedValid!=0;
      p.lastMoveAgeDs=cp.lastMoveAgeDs;
      p.speedPkph=(float)cp.speedX10/10.0f;
      p.frontOffset=cp.frontOffset;
      p.rearOffset=cp.rearOffset;
      p.truthSource=cp.truthSource;
      p.stationPhase=(StationTestPhase)cp.stationPhase;
      p.trafficPhase=(TrafficPhase)cp.trafficPhase;
      p.mustHoldEligible=cp.mustHoldEligible!=0;
      p.trafficStopForId=cp.trafficStopForId;
      p.peerRxAccepted=cp.senderRxAccepted;
      p.peerTxAttempts=cp.senderTxAttempts;
      p.peerTxImmediateErrors=cp.senderTxImmediateErrors;
      p.receivedAtMs=millis();
      p.freshPreviously=true;
      ctoRxAccepted++;
      if(newPeer || !wasFresh){
        ctoStatusDirty=true;
        ctoPublishState(newPeer?"PEER_ADDED":"PEER_REFRESHED","UNIVERSAL_TRAFFIC_TRUTH_ACCEPTED");
      }
      return;
    }
    ctoRxBadMagicVersion++;
    return;
  }
  if(len!=(int)sizeof(TrainPacket)){ctoRxBadLength++;return;}
  TrainPacket pkt; memcpy(&pkt,data,sizeof(pkt));
  if(pkt.magic!=ESPNOW_MAGIC || pkt.version!=ESPNOW_VERSION)return;
  if(pkt.locoId!=ESPNOW_BROADCAST_ID && pkt.locoId!=LOCO_ID)return;
  if(pkt.msgType==ESPNOW_MSG_ESTOP && pkt.cmd==ESPNOW_CMD_ESTOP){
    eStopEngaged=true; stationResetTestCycle("DISPATCHER_ESTOP"); running=false; commandedPwm=0; rampCurrent=0; pwmWriteCompat(0); updateMotorAuthority(); publishSimpleStates(); Serial.println("[AUTO] Dispatcher E-STOP"); return;
  }
  if(pkt.msgType!=ESPNOW_MSG_MANUAL)return;
  if(pkt.cmd==ESPNOW_CMD_START){
    if(dispatcherAuto){
      const bool accepted=startAutoSequence();
      ctoPublishCommand("GO",pkt.sequenceNumber,accepted?"ACCEPTED":"IGNORED",accepted?"LAUNCH_STARTED":"IDEMPOTENT_OR_DENIED");
    }else{
      ctoPublishCommand("GO",pkt.sequenceNumber,"IGNORED","MANUAL_AUTHORITY");
      Serial.println("[AUTO] GO ignored — manual authority");
    }
  }
  else if(pkt.cmd==ESPNOW_CMD_STOP){
    if(dispatcherAuto){stopAutoSequence();ctoPublishCommand("STOP",pkt.sequenceNumber,"ACCEPTED","STOP_SEQUENCE");}
    else ctoPublishCommand("STOP",pkt.sequenceNumber,"IGNORED","MANUAL_AUTHORITY");
  }
}

void setup(){Serial.begin(115200);delay(200);Serial.printf("\n[DNA] %s - %s booting packet=%u version=%u\n",SKETCH_NAME,LOCO_NAME,(unsigned)sizeof(CtoPeerPacket),(unsigned)CTO2_VERSION);mqttSetupTopics();pinMode(MOTOR_DIR_PIN,OUTPUT);applyDirectionPin();pwmAttachCompat();pwmWriteCompat(0);calibrateBaseline();ina219Setup();WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(true);WiFi.setSleep(false);WiFi.begin(WIFI_SSID,WIFI_PASS);mqtt.setServer(MQTT_BROKER,MQTT_PORT);mqtt.setBufferSize(8192);mqtt.setCallback(onMQTTMessage);if(esp_now_init()!=ESP_OK){Serial.println("[AUTO] ESP-NOW init failed");}else{
  esp_now_register_recv_cb(onReceive);
  {
    esp_now_peer_info_t pi={};
    const uint8_t bcast[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    memcpy(pi.peer_addr,bcast,6);
    pi.channel=0;
    pi.encrypt=false;
    if(!esp_now_is_peer_exist(bcast)){
      esp_err_t peerResult=esp_now_add_peer(&pi);
      if(peerResult!=ESP_OK) Serial.printf("[AUTO] ESP-NOW broadcast peer add failed: %d\n",(int)peerResult);
    }
  }
  Serial.println("[AUTO] ESP-NOW Dispatcher receiver ready; peer interaction disabled");
}resetDnaRuntime(false);stationResetTestCycle("BOOT");Serial.println("[AUTO] LL_PWM_DNA_AUTO_1_0 single-train prescribed-PWM service ready.");}
void loop(){
  mqtt.loop();
  serviceMQTTReconnect();
  serviceHall();
  serviceInaTelemetry();
  serviceRamp();
  updateMotionState();
  serviceStationTest();
  serviceHallSilenceObserver();
  serviceSerial();
  if(rampCurrent>MOTOR_DEAD_ZONE_PWM){
    rampAccum+=(uint32_t)rampCurrent;
    if(rampSamples<0xFFFF)rampSamples++;
  }
}
