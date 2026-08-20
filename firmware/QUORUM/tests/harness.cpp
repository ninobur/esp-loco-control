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
// IR TEST A (spec §11.2): the harness enables the IR layer regardless of the
// compiled profile so the receiver paths are testable. A separate build
// WITHOUT this define is the Otto-inert / byte-identity check.
#ifndef HARNESS_IR_OFF
#define IR_TEST_A_ENABLED 1
#define IR_SENSOR_MAC_BYTES {0x24,0x22,0x33,0x44,0x55,0x66}
#endif
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

static uint32_t g_peerSeq = 0;

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
      // Mirror drainMarkers()'s call, which is the navigator's real entry —
      // including the IR Test A observation pair around it (inert when off).
      irObserveEventPre(e);
      navOnMarker(e);
      irObserveEventPost(e);
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
    } else if(cmd=="wifi_channel"){
      // 1.16Ra: drive the station channel the drift watch reads. Outside
      // 1..14 the firmware must treat it as unknown, which is what the
      // negative and out-of-band cases in the fixture assert.
      int c; in >> c; hostWifiChannel=c;
    } else if(cmd=="cto"){
      // One ctoService() pass. The navigator fixtures never call this, so
      // every existing replay is untouched; only a fixture that asks for CTO
      // gets CTO. Brings the radio up on first use, exactly as networkTask
      // does on the locomotive.
      ctoRadioInit();
      ctoService();
    } else if(cmd=="cto_cmd"){
      // Route through the REAL operator command handler. The old 'cto_off'
      // set ctoEnabled directly and therefore never executed ctoHandleClear's
      // dissolve — so "all four pre-existing dissolutions are tested" was
      // untrue for clear and off (review, 2026-08-16).
      std::string payload; std::getline(in, payload);
      size_t b=payload.find_first_not_of(" \t");
      if(b!=std::string::npos) payload=payload.substr(b); else payload.clear();
      ctoHandleClear(payload.c_str());
#ifdef CE_MISSION_PRESENT
    } else if(cmd=="ce"){
      // Route through the REAL dispatcher entry point, not ceMission directly:
      // the refusal-when-unrolled path and the severance are part of what is
      // under test, and setting the state by hand would skip both.
      ceBegin();
    } else if(cmd=="ce_dump"){
      // partner and traffic included because "mission == NONE" alone hid a P1:
      // CE cleared without a pairing ever forming, and the test still passed.
      std::printf("{\"ce\":true,\"mission\":\"%s\",\"cruise\":%d,"
                  "\"dwell\":%lu,\"seq\":%u,\"role\":\"%s\","
                  "\"partner\":%lu,\"traffic\":%u}\n",
                  ceMissionName(), ceCruisePwm(),
                  (unsigned long)ctoDwellMs(), (unsigned)ceStationSeq,
                  ctoRole==CTO_ROLE_LEADER?"LEADER":
                  ctoRole==CTO_ROLE_FOLLOWER?"FOLLOWER":"NONE",
                  (unsigned long)ctoPartnerId, (unsigned)ctoTraffic);
#endif
    } else if(cmd=="cto_off"){
      ctoEnabled=false;
    } else if(cmd=="advance"){
      // Move the clock without producing a marker: the channel watch is rate
      // limited to 1 Hz and must be given real time to pass.
      unsigned long ms; in >> ms; g_hostMillis += ms;
    } else if(cmd=="peer"){
      // 0037: inject a CTO peer status packet, exactly as the radio would
      // deliver one. CTO role formation had NO test coverage before this —
      // the esp_now shim drops every send and no callback ever fires, so the
      // registry was permanently empty and every pairing path was dead code
      // in replay. Args: <id> <mm> <CW|CCW> [truth]  (truth: 2=NORMAL default,
      // 1=EVALUATING, 0=NO_QUORUM). Bounds are derived the same way the
      // sender derives its own, so fixtures read in positions, not arcs.
      unsigned long id; int mm; std::string d; int truth=2;
      in >> id >> mm >> d;
      if(!(in >> truth)) truth=2;
      int8_t pdir = (d=="CW") ? MAP_CW : MAP_CCW;
      CtoPeerPacket p{};
      p.magic=CTO2_MAGIC; p.version=CTO2_VERSION;
      p.senderId=(uint32_t)id; p.sequence=++g_peerSeq;
      p.hallMm=(uint8_t)mm; p.mapDir=pdir;
      p.frontBoundaryMm=routeMod((int32_t)mm + pdir*CONSIST_EXTENT_FRONT_MARKERS);
      p.rearBoundaryMm =routeMod((int32_t)mm - pdir*CONSIST_EXTENT_REAR_MARKERS);
      p.frontOffset=CONSIST_EXTENT_FRONT_MARKERS;
      p.rearOffset=CONSIST_EXTENT_REAR_MARKERS;
      p.truthSource=(uint8_t)truth;
      p.autoMode=1; p.running=1; p.motionState=3;
      ctoAcceptPeer(p);
    } else if(cmd=="peer_echo"){
      // The reciprocal role echo (0032). Without it ctoEchoConfirmed stays
      // false and the follower dwell never applies — which is itself worth
      // asserting, so it is a separate command rather than implied by 'peer'.
      unsigned long id, partner; std::string r;
      in >> id >> r >> partner;
      Cto3RoleEcho e{};
      e.magic=CTO3_ECHO_MAGIC; e.version=CTO3_ECHO_VERSION;
      e.senderId=(uint32_t)id; e.partnerId=(uint32_t)partner;
      e.role=(r=="LEADER")?(uint8_t)CTO_ROLE_LEADER:(uint8_t)CTO_ROLE_FOLLOWER;
      e.pairEpochMs=g_hostMillis;
      ctoAcceptEcho(e);
    } else if(cmd=="cto_dump"){
      printf("{\"cto\":true,\"role\":\"%s\",\"partner\":%lu,\"expected\":%lu,"
             "\"fleet_hold\":%d,\"dwell_ms\":%lu,\"echo_confirmed\":%d}\n",
             ctoRole==CTO_ROLE_LEADER?"LEADER":ctoRole==CTO_ROLE_FOLLOWER?"FOLLOWER":"NONE",
             (unsigned long)ctoPartnerId,(unsigned long)ctoExpectedId,
             ctoFleetHold?1:0,(unsigned long)ctoDwellMs(),ctoEchoConfirmed?1:0);
    } else if(cmd=="ir"){
#if IR_TEST_A_ON
      // Inject one IrSpeedPacketV1 through irAcceptFrame() — the exact
      // validation chain the radio callback uses, then serviceIrRx().
      // Args: <seq> <boot> <pulses> <validity 0-5> <speed_x100|-1=auto> [age_ms_back]
      // auto encoding: canonical for the given validity. age_ms_back sets the
      // rx timestamp N ms BEFORE the current harness clock (for staleness).
      unsigned long seq,boot,pulses; int validity; long spd; long back=0;
      in >> seq >> boot >> pulses >> validity >> spd;
      if(!(in >> back)) back=0;
      IrSpeedPacketV1 p{};
      p.magic=IR_SPEED_MAGIC; p.version=IR_SPEED_VERSION; p.bytes=sizeof(p);
      p.sensorId=IR_SENSOR_ID_IR01; p.targetLocoId=LOCO_ID;
      p.bootId=(uint32_t)boot; p.txSequence=(uint32_t)seq;
      p.capturedMs=g_hostMillis; p.completedPulses=(uint32_t)pulses;
      p.lastIntervalMs=100; p.opticalSpan=400; p.reserved=0;
      p.validity=(uint8_t)validity;
      if(spd<0){
        p.speedMmpsX100=(validity==IR_V_VALID)?25000UL
                       :(validity==IR_V_STOPPED)?0UL:IR_SPEED_INVALID_X100;
      }else p.speedMmpsX100=(uint32_t)spd;
      static const uint8_t SRC[6]={0x24,0x22,0x33,0x44,0x55,0x66};
      irEnsureQueue();
      irAcceptFrame(SRC,(const uint8_t*)&p,(int)sizeof(p),
                    (uint32_t)(g_hostMillis-(unsigned long)back));
      serviceIrRx();
#endif
    } else if(cmd=="ir_raw"){
#if IR_TEST_A_ON
      // Deliberately malformed frames for the reject counters.
      // Args: <mutation>  len|magic|ver|bytes|resv|src|sensor|target|enum|enc
      std::string mut; in >> mut;
      IrSpeedPacketV1 p{};
      p.magic=IR_SPEED_MAGIC; p.version=IR_SPEED_VERSION; p.bytes=sizeof(p);
      p.sensorId=IR_SENSOR_ID_IR01; p.targetLocoId=LOCO_ID;
      p.bootId=7; p.txSequence=1; p.completedPulses=10;
      p.validity=IR_V_VALID; p.speedMmpsX100=1000;
      uint8_t src[6]={0x24,0x22,0x33,0x44,0x55,0x66};
      int len=(int)sizeof(p);
      if(mut=="len") len=40;
      else if(mut=="magic") p.magic=0xC7;
      else if(mut=="ver") p.version=9;
      else if(mut=="bytes") p.bytes=71;
      else if(mut=="resv") p.reserved=1;
      else if(mut=="src") src[0]=0xAA;
      else if(mut=="sensor") p.sensorId=0xDEADBEEF;
      else if(mut=="target") p.targetLocoId=1234;
      else if(mut=="enum") p.validity=6;
      else if(mut=="enc") p.speedMmpsX100=IR_SPEED_INVALID_X100; // VALID w/ sentinel
      irEnsureQueue();
      irAcceptFrame(src,(const uint8_t*)&p,len,(uint32_t)g_hostMillis);
      serviceIrRx();
#endif
    } else if(cmd=="ir_telem"){
#if IR_TEST_A_ON
      // Force both telemetry cadences due now, then run the service — the
      // review found the §9.4 authority strings had zero runtime coverage.
      irLastSpeedPubMs = (uint32_t)g_hostMillis - 2000;
      irLastStatusPubMs = (uint32_t)g_hostMillis - 6000;
      serviceIrTelemetry();
#endif
    } else if(cmd=="ir_dump"){
#if IR_TEST_A_ON
      IrView v=irCurrentView();
      char sp[16]; if(v.numeric) snprintf(sp,16,"%.2f",(double)v.mmps); else strcpy(sp,"null");
      printf("{\"ir\":true,\"state\":\"%s\",\"numeric\":%d,\"mmps\":%s,\"age\":%lu,"
             "\"acc\":%lu,\"gaps\":%lu,\"dup\":%lu,\"ooo\":%lu,\"regress\":%lu,"
             "\"reboot\":%lu,\"blen\":%lu,\"bver\":%lu,\"bwire\":%lu,\"bsrc\":%lu,"
             "\"bsen\":%lu,\"btgt\":%lu,\"benum\":%lu,\"benc\":%lu,"
             "\"mm_valid\":%d,\"mm_mmps\":%.1f,\"anchor\":%d}\n",
             v.state,v.numeric?1:0,sp,(unsigned long)v.ageMs,
             (unsigned long)irAccepted,(unsigned long)irSeqGaps,(unsigned long)irDups,
             (unsigned long)irOutOfOrder,(unsigned long)irPulseRegressions,
             (unsigned long)irSensorReboots,
             (unsigned long)irRejBadLen,(unsigned long)irRejBadMagicVer,
             (unsigned long)irRejBadWire,(unsigned long)irRejBadSource,
             (unsigned long)irRejBadSensor,(unsigned long)irRejBadTarget,
             (unsigned long)irRejBadEnum,(unsigned long)irRejBadEncoding,
             mmSpeedValid?1:0,(double)mmSpeedMmps,irAnchorValid?1:0);
#endif
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
