// QUORUM host replay harness — never flashed.
//
// This file #includes QUORUM.ino and runs the REAL navigator on the
// development host. The replay therefore exercises the shipped adjudication
// code — scoreEntry(), decideEvaluation(), the timing gate, adoption and its
// validation, enterNoQuorum() and the snapshot builder — rather than a
// transcription that could drift from it.
//
// What is faithful:
//   * every navigator decision, because it is the firmware's own code;
//   * marker timing, because millis() is driven from the captured intervals;
//   * every publication, because the firmware's own queues are drained.
//
// What is NOT faithful, and must not be asserted on:
//   * the Hall ISR, detector thresholds and queue-drop behaviour — the replay
//     starts at navOnMarker(), downstream of all of it;
//   * WiFi/MQTT delivery — nothing connects, by design;
//   * task scheduling and real-time interleaving.
//
// Protocol: commands on stdin, one per line. Results as JSON lines on stdout.
//   dir CW|CCW              set session direction (and derive navDir)
//   declare <mm>            operator position declaration
//   auto 0|1                leave/enter the AUTO chamber
//   event <dt_ms> <N|S> <pwmActual> <pwmCommanded> <peak> <durMs> <drift>
//   mark <label>            emit a marker line, for locating phases in output
//   dump                    emit navigator state
//   snapshot                emit the retained NO_QUORUM snapshot buffer
// Publications are emitted as they are produced, after each command.

#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <string>
#include <iostream>
#include <sstream>

// --- shim globals the stubs declare extern ---------------------------------
unsigned long g_hostMillis = 0;
HostSerial    Serial;
HostWiFiClass WiFi;
HostTwoWire   Wire;

// The Arduino build generates prototypes for every function in a .ino before
// compiling it, so the sketch may legally call ahead of a definition. A plain
// C++ translation unit gets no such service, so the forward declarations the
// sketch relies on are restated here. These must stay a pure mirror of
// QUORUM.ino's signatures — the harness may not change what it is testing.
static void publishWarning(const char* text);

// The navigator under test. Included, not linked: QUORUM.ino's file-scope
// statics are the harness's to drive directly, which is the whole point.
#include "../QUORUM.ino"

// --- JSON string escaping for the output protocol --------------------------
static std::string esc(const char* s){
  std::string o;
  for(const char* p=s; *p; ++p){
    switch(*p){
      case '"':  o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n";  break;
      case '\r': o += "\\r";  break;
      case '\t': o += "\\t";  break;
      default:   o += *p;
    }
  }
  return o;
}

// Drain both publication queues in the order the firmware filled them. The
// marker path carries decision events (§5.1), so it is drained too.
static void drainPublications(){
  PubMsg m;
  while(markerPubQueue && xQueueReceive(markerPubQueue,&m,0)==pdTRUE)
    printf("{\"pub\":\"%s\",\"payload\":\"%s\"}\n", m.topic?m.topic:"", esc(m.payload).c_str());
  while(pubQueue && xQueueReceive(pubQueue,&m,0)==pdTRUE)
    printf("{\"pub\":\"%s\",\"payload\":\"%s\",\"retain\":%s}\n",
           m.topic?m.topic:"", esc(m.payload).c_str(), m.retain?"true":"false");
}

static void emitState(){
  printf("{\"state\":\"%s\",\"mm\":%u,\"dir\":\"%s\",\"miss_streak\":%u,"
         "\"eval\":%u,\"ring_len\":%u,\"lc_mm\":%u,\"since\":%u,"
         "\"pending_validation\":%s,\"adopted_offset\":%d,\"agree\":%u,\"disagree\":%u,"
         // CODEX 1.16 review finding 1: the harness never calls
         // serviceStations(), so the resume interlock is only testable if the
         // flag itself is visible. Every dump now reports it.
         "\"gate\":\"%s\",\"auto\":%u}\n",
         navStateName(), navMm, dirName(navDir), (unsigned)missStreak,
         (unsigned)evalCount, (unsigned)evRingLen, lastConfirmedMm,
         (unsigned)markersSinceConfirmed,
         adoptionPendingValidation?"true":"false",
         adoptedOffset==NO_ADOPTED_OFFSET?999:(int)adoptedOffset,
         (unsigned)navAgree,(unsigned)navDisagree,
         lastTimingGate, autoRunning?1u:0u);
}

static void emitSnapshot(){
  printf("{\"snapshot_desired\":%u,\"snapshot\":\"%s\"}\n",
         (unsigned)desiredRetainedNoQuorum, esc(noQuorumSnapshot).c_str());
}

// The retained slot is emitted whenever the firmware commits a new one, rather
// than when a fixture remembers to ask. noQuorumGeneration is incremented under
// the mux by every commit — buildNoQuorumSnapshot() and navDeclare() alike — so
// this catches each terminal record at the instant it is built.
//
// Asking at a fixed point in the command stream is not good enough: the capture
// publishes mm/no_quorum BEFORE the marker record that provoked it, so a
// fixture-driven 'snapshot' lands one event early and misses the last incident
// of a run entirely.
static uint32_t lastSeenGeneration = 0;
static void emitSnapshotIfChanged(){
  if(noQuorumGeneration != lastSeenGeneration){
    lastSeenGeneration = noQuorumGeneration;
    emitSnapshot();
  }
}

// setup() is not called: it starts tasks and a radio. The harness creates
// exactly the queues the navigator publishes into, and nothing else.
static void harnessInit(){
  eventQueue     = xQueueCreate(256,sizeof(MarkerEvent));
  pubQueue       = xQueueCreate(32,sizeof(PubMsg));
  markerPubQueue = xQueueCreate(128,sizeof(PubMsg));
  cmdQueue       = xQueueCreate(16,sizeof(CmdMsg));
  buildTopics();
  navState = NAV_UNSET;
}

int main(){
  harnessInit();
  std::string line;
  while(std::getline(std::cin,line)){
    if(line.empty() || line[0]=='#') continue;
    std::istringstream in(line);
    std::string cmd; in >> cmd;

    if(cmd=="dir"){
      std::string d; in >> d;
      sessionDir = (d=="CW") ? MAP_CW : (d=="CCW" ? MAP_CCW : MAP_UNSET);
      applyDirection();
    } else if(cmd=="declare"){
      int mm; in >> mm;
      navDeclare((uint8_t)mm);
    } else if(cmd=="motor"){
      // state/direction values, straight from LocoConfig: 0 REV, 1 NEU, 2 FWD.
      // applyDirection() derives navDir from this and sessionDir, and performs
      // the mid-interval odometer step-back on a genuine reversal.
      int v; in >> v;
      motorDirection = v;
      applyDirection();
    } else if(cmd=="noquorum"){
      // Terminal entry with a given reason. The FORCED_BY_FIXTURE path in the
      // sketch is exactly this call and nothing else (the cmd/force_lost
      // handler calls enterNoQuorum("FORCED_BY_FIXTURE") directly), so driving
      // it here is faithful. SECOND_ADOPTION_FAILED also has a synthetic that
      // reaches it through handleFailedAdoption(), because there the
      // surrounding rebased-ring state is the point.
      std::string reason; in >> reason;
      enterNoQuorum(reason.c_str());
    } else if(cmd=="auto"){
      int a; in >> a;
      autoRunning = (a!=0);
      autoEnrolled = (a!=0);
    } else if(cmd=="event"){
      unsigned long dt; std::string pol;
      int pwmA,pwmC,peak,dur,drift;
      in >> dt >> pol >> pwmA >> pwmC >> peak >> dur >> drift;
      g_hostMillis += dt;
      MarkerEvent e{};
      e.polarity             = (pol=="N") ? 1 : 0;
      e.peak                 = peak;
      e.durationMs           = (uint16_t)dur;
      e.baselineDrift        = (int16_t)drift;
      e.detectedAtMs         = g_hostMillis;
      e.pwmActualAtDetect    = (uint8_t)pwmA;
      e.pwmCommandedAtDetect = (uint8_t)pwmC;
      // Mirror drainMarkers()'s call, which is the navigator's real entry.
      navOnMarker(e);
      // Post-event navigator state. The captured mm/marker record carries the
      // same fields, so verify_replay.py can prove the replay reproduces the
      // firmware's own gate decisions — which is what validates the
      // reconstructed pwmCommandedAtDetect.
      printf("{\"ev\":true,\"mm\":%u,\"gate\":\"%s\",\"dt\":%u,"
             "\"dt_expected\":%u,\"state\":\"%s\"}\n",
             navMm, lastTimingGate, (unsigned)lastSegmentDt,
             (unsigned)lastDtExpected, navStateName());
    } else if(cmd=="constants"){
      // The firmware's own map and advisory parameters, so no test has to keep
      // a second copy of NGR_DNA1 that could drift from it. Emitted through
      // dnaAt()/pgm_read_byte, the same accessor the navigator scores with.
      // ADVISORY_NONE arrived with the advisory. Guarding it keeps this
      // harness buildable against PRE-advisory revisions of the sketch, which
      // verify_inert.py needs in order to diff old against new.
      #ifdef ADVISORY_NONE
      const unsigned advisoryNone = ADVISORY_NONE;
      #else
      const unsigned advisoryNone = 255;
      #endif
      printf("{\"dna_n\":%u,\"dna_w\":%u,\"reacq\":%u,\"quorum_max\":%u,"
             "\"quorum_margin\":%u,\"quorum_trigger\":%u,\"advisory_none\":%u,"
             "\"gate_low_pwm_floor\":%u,"
             // 1.16 era marker: the suite branches its expectations on the
             // presence of these — a legacy build simply does not report them.
             "\"q_floor_ms\":%u,\"suffix_rescue_n\":%u,\"nq_confirm_n\":%u,"
             "\"offsets\":[",
             (unsigned)DNA_N,(unsigned)DNA_W,(unsigned)REACQ_WINDOW_MARKERS,
             (unsigned)QUORUM_MAX,(unsigned)QUORUM_MARGIN,
             (unsigned)QUORUM_TRIGGER,advisoryNone,
             (unsigned)GATE_LOW_PWM_FLOOR,
             (unsigned)Q_FLOOR_MS,(unsigned)SUFFIX_RESCUE_N,(unsigned)NQ_CONFIRM_N);
      for(uint8_t i=0;i<QUORUM_CANDIDATES;i++)
        printf("%s%d", i?",":"", (int)QUORUM_OFFSETS[i]);
      printf("],\"dna\":[");
      for(uint16_t i=0;i<DNA_N;i++)
        printf("%s%u", i?",":"", (unsigned)dnaAt((uint8_t)i));
      printf("]}\n");
    } else if(cmd=="mark"){
      std::string label; in >> label;
      printf("{\"mark\":\"%s\"}\n", esc(label.c_str()).c_str());
    } else if(cmd=="dump"){
      emitState();
    } else if(cmd=="snapshot"){
      emitSnapshot();
    } else {
      printf("{\"error\":\"unknown command\",\"line\":\"%s\"}\n", esc(line.c_str()).c_str());
    }
    emitSnapshotIfChanged();
    drainPublications();
    fflush(stdout);
  }
  return 0;
}
