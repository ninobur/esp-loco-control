// ============================================================================
// LL_MQ_CO_GC_DI_1_2
// CHANGES FROM LL_MQ_CO_GC_DI_1_1
// ---------------------------------
//   - Added MQTT station (leading/unpaired) tuning topics:
//       ngr/dispatcher/tune/station_brake_delay  -> PARAM_RAMP_DELAY,  ROLE_ID_STATION
//       ngr/dispatcher/tune/station_dwell        -> PARAM_DWELL,       ROLE_ID_STATION
//
// CHANGES FROM LL_MQ_CO_GC_DI_1_0
// ---------------------------------
//   - Added MQTT tuning parameter topics (7 parameters)
//   - Added MQTT clearall topic
//   - Serial parser, CTO/CE, ESP-NOW behavior unchanged
//
// CHANGES FROM LL_CE_GC_Dispatcher_v1_2
// --------------------------------------
//   - Added Dispatcher-only automatic Circuit Express request timer.
//   - New serial command:
//       ceauto10
//     Enables automatic CE requests every 10 minutes.
//   - New serial command:
//       ceoff
//     Disables automatic CE requests.
//   - The automatic timer sends the same existing CMD_CE_START broadcast
//     used by the manual "ce" command.
//   - Locomotive sketches remain authoritative:
//       CE begins only if an eligible CTO pair already exists.
//       If no valid CTO pair exists, the CE request is ignored normally.
//   - No locomotive behavior, CTO logic, CE entry rules, protocol structure,
//     or packet definitions changed.// This did not change from v1.1. Bulid Based on LL_CE_GC
// NGR Lowline — Close Train Operating Dispatcher v1.4D
// Flash to dedicated ESP32 dispatcher/auditor node.
//
// CHANGES FROM LL_CE_GC_Dispatcher_v1_2
// --------------------------------------
//   - Added passive CE-exit diagnostic reporting support.
//   - New STATUS event:
//       ST_CE_LOCAL_EXIT_TO_CTO_LEADER
//   - Dispatcher now decodes and prints a dedicated CE EXIT line when the
//     former CE Local clears CE state and returns to ordinary CTO as the
//     new leader.
//   - Diagnostic values printed:
//       cruise PWM, current PWM, target PWM, ceActive, and ceRole.
//   - Reporting only. No dispatcher control behavior, train motion logic,
//     command handling, or packet structure changed.
// ============================================================================

//#define BLYNK_PRINT Serial
//#define BLYNK_TEMPLATE_ID   "TMPL2mRnCSgqs"
//#define BLYNK_TEMPLATE_NAME "LL LBO Dispatcher"
//#define BLYNK_AUTH_TOKEN    "OLD_DISPATCHER_BLYNK_TOKEN"

#include <Arduino.h>
#include "LL_LocoConfig_Dispatcher.h"
#include <WiFi.h>
#include <esp_now.h>
//#include <BlynkSimpleEsp32.h>
#include <PubSubClient.h>

// ============================================================================
// BLYNK VIRTUAL PINS
// ============================================================================
#define VPIN_WZ_PATIO    V20
#define VPIN_WZ_BAMBOO   V21
#define VPIN_WZ_ARCHES   V22
#define VPIN_WZ_GRILLERS V23
#define VPIN_WZ_CLEAR    V24
#define VPIN_WZ_STATUS   V25

// ============================================================================
// NODE IDENTITIES
// ============================================================================
static const uint32_t DISPATCHER_ID = LOCO_ID;
static const uint32_t BROADCAST_ID  = 0UL;
static const uint32_t OTTO          = 9950011UL;
static const uint32_t TOBY          = 9950012UL;


// ============================================================================
// MQTT — Dispatcher web-command intake from Flask/Mosquitto
// The Dispatcher remains the ESP-NOW translator for CTO/CE commands.
// ============================================================================
#define MQTT_BROKER "192.168.68.142"
#define MQTT_PORT   1883
#define MQTT_CLIENT_ID "ngr-dispatcher"

static const char* TOPIC_DISP_GO_BOTH      = "ngr/dispatcher/cmd/go";
static const char* TOPIC_DISP_STOP_BOTH    = "ngr/dispatcher/cmd/stop";
static const char* TOPIC_DISP_GO_OTTO      = "ngr/dispatcher/cmd/go/9950011";
static const char* TOPIC_DISP_GO_TOBY      = "ngr/dispatcher/cmd/go/9950012";
static const char* TOPIC_DISP_STOP_OTTO    = "ngr/dispatcher/cmd/stop/9950011";
static const char* TOPIC_DISP_STOP_TOBY    = "ngr/dispatcher/cmd/stop/9950012";
static const char* TOPIC_DISP_CE           = "ngr/dispatcher/cmd/ce";
static const char* TOPIC_DISP_ESTOP        = "ngr/dispatcher/cmd/estop";
static const char* TOPIC_DISP_CLEARALL     = "ngr/dispatcher/cmd/clearall";

// Tuning parameters — all broadcast to both locos
static const char* TOPIC_TUNE_ARRIVAL      = "ngr/dispatcher/tune/arrival_style";
static const char* TOPIC_TUNE_DEPARTURE    = "ngr/dispatcher/tune/departure_style";
static const char* TOPIC_TUNE_PULLUP       = "ngr/dispatcher/tune/pullup_delay";
static const char* TOPIC_TUNE_CREEP_DUR    = "ngr/dispatcher/tune/creep_duration";
static const char* TOPIC_TUNE_CREEP_SPD    = "ngr/dispatcher/tune/creep_speed";
static const char* TOPIC_TUNE_PLATFORM     = "ngr/dispatcher/tune/platform_brake_delay";
static const char* TOPIC_TUNE_TRAILING_BRK = "ngr/dispatcher/tune/trailing_brake_delay";
static const char* TOPIC_TUNE_STATION_BRAKE = "ngr/dispatcher/tune/station_brake_delay";
static const char* TOPIC_TUNE_STATION_DWELL = "ngr/dispatcher/tune/station_dwell";

WiFiClient mqttWifiClient;
PubSubClient mqtt(mqttWifiClient);

// ============================================================================
// PROTOCOL — must match loco sketches exactly
// ============================================================================
static const uint8_t MAGIC   = 0x4E;
static const uint8_t VERSION = 14;      // CE GoldCore v1.1 — all nodes must match

static const uint8_t BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

enum MsgType : uint8_t {
  MSG_TYPE_BLOCK           = 1,
  MSG_TYPE_ESTOP           = 3,
  MSG_TYPE_MANUAL          = 4,
  MSG_TYPE_ENCOUNTER_START = 5,
  MSG_TYPE_RELEASE         = 6,
  MSG_TYPE_DEPARTING       = 7,
  MSG_TYPE_STATUS          = 8    // NEW: loco narrates internal state
};

// STATUS event codes — carried in pkt.cmd for MSG_TYPE_STATUS packets
enum StatusEvent : uint8_t {
  ST_BLOCK_DETECTED              = 1,
  ST_ROLE_ASSIGNED               = 2,
  ST_RAMP_DELAY_START            = 3,
  ST_BRAKING                     = 4,
  ST_CONFIRMED_STOP              = 5,
  ST_RELEASE_SENT                = 6,
  ST_WAIT_LEADER                 = 7,
  ST_ADVANCE_START               = 8,
  ST_ADVANCE_END                 = 9,
  ST_PLATFORM_STOP               = 10,
  ST_PLATFORM_DWELL              = 11,
  ST_DEPARTING                   = 12,
  ST_LEADER_WAITING              = 13,
  ST_LEADER_RELEASED             = 14,
  ST_CE_LOCAL_EXIT_TO_CTO_LEADER = 15 //added 3-13
};

enum CmdType : uint8_t {
  CMD_NONE           = 0,
  CMD_START          = 1,
  CMD_STOP           = 2,
  CMD_ESTOP          = 3,
  CMD_SET_PWM        = 10,
  CMD_SET_ROLE_PARAM = 14,
  CMD_CLEAR_ALL      = 13,
  CMD_CE_START       = 20
};

static const uint8_t ROLE_ID_DEFAULT   = 0;
static const uint8_t ROLE_ID_STATION   = 1;
static const uint8_t ROLE_ID_TRAILING  = 2;
static const uint8_t ROLE_ID_LEADER    = 3;

static const uint8_t PARAM_RAMP_DELAY      = 1;
static const uint8_t PARAM_RAMP_RATE       = 2;
static const uint8_t PARAM_DWELL          = 3;
static const uint8_t PARAM_FOLLOW_DELAY   = 4;
static const uint8_t PARAM_ADVANCE_MS     = 5;
static const uint8_t PARAM_PLATFORM_DWELL = 6;
static const uint8_t PARAM_RAMP_UP_RATE   = 7;
static const uint8_t PARAM_ADVANCE_PWM    = 8;

enum BlockId : uint8_t {
  BLOCK_UNKNOWN  = 0,
  BLOCK_PATIO    = 1,
  BLOCK_BAMBOO   = 2,
  BLOCK_ARCHES   = 3,
  BLOCK_GRILLERS = 4
};

// ============================================================================
// PACKET STRUCTURE — identical to loco sketch
// ============================================================================
typedef struct __attribute__((packed)) {
  uint8_t  magic;
  uint8_t  version;
  uint8_t  msgType;
  uint32_t senderId;
  uint32_t locoId;
  uint8_t  blockId;
  uint8_t  cmd;
  uint8_t  flags;
  uint32_t sequenceNumber;
  uint32_t encounterId;
  uint16_t value;
  uint8_t  roleId;
  uint8_t  paramId;
} TrainPacket;

// ============================================================================
// AUDITOR PEER TABLE
// ============================================================================
static const int MAX_PEERS = 8;

struct PeerState {
  uint32_t id;
  uint8_t  lastBlock;
  uint32_t lastSeq;
  unsigned long lastSeenMs;
  bool active;
};

PeerState peers[MAX_PEERS];
uint32_t txSequence   = 0;
uint8_t workZoneBlock = BLOCK_UNKNOWN;

// Automatic Circuit Express request timer.
// Dispatcher-only feature: periodically sends the same CE request as the
// manual "ce" command. Locomotives still decide whether CE is eligible.
bool          ceAutoEnabled        = false;
unsigned long ceAutoIntervalMs     = 0;
unsigned long ceAutoLastRequestMs  = 0;

// ============================================================================
// UTILITY HELPERS
// ============================================================================
String ts() {
  unsigned long ms = millis(), s = ms/1000, m = s/60, h = m/60;
  s %= 60; m %= 60;
  char buf[16]; sprintf(buf, "%02lu:%02lu:%02lu", h, m, s);
  return String(buf);
}

const char* blockName(uint8_t b) {
  switch (b) {
    case BLOCK_PATIO:    return "PATIO";
    case BLOCK_BAMBOO:   return "BAMBOO";
    case BLOCK_ARCHES:   return "ARCHES";
    case BLOCK_GRILLERS: return "GRILLERS";
    default:             return "UNKNOWN";
  }
}

uint8_t blockFromName(String name) {
  name.toLowerCase();
  if (name == "patio")    return BLOCK_PATIO;
  if (name == "bamboo")   return BLOCK_BAMBOO;
  if (name == "arches")   return BLOCK_ARCHES;
  if (name == "grillers") return BLOCK_GRILLERS;
  return BLOCK_UNKNOWN;
}

const char* locoName(uint32_t id) {
  if (id == OTTO)          return "OTTO";
  if (id == TOBY)          return "TOBY";
  if (id == BROADCAST_ID)  return "BOTH";
  if (id == DISPATCHER_ID) return "DISPATCHER";
  return "UNKNOWN";
}

// ============================================================================
// STATUS EVENT NAME — NEW in v1.4D
// Maps the cmd byte in a STATUS packet to a human-readable event name.
// ============================================================================
const char* statusEventName(uint8_t ev) {
  switch (ev) {
    case ST_BLOCK_DETECTED:   return "BLOCK_DETECTED";
    case ST_ROLE_ASSIGNED:    return "ROLE_ASSIGNED";
    case ST_RAMP_DELAY_START: return "RAMP_DELAY";
    case ST_BRAKING:          return "BRAKING";
    case ST_CONFIRMED_STOP:   return "CONFIRMED_STOP";
    case ST_RELEASE_SENT:     return "RELEASE_SENT";
    case ST_WAIT_LEADER:      return "WAIT_LEADER";
    case ST_ADVANCE_START:    return "ADVANCE_START";
    case ST_ADVANCE_END:      return "ADVANCE_END";
    case ST_PLATFORM_STOP:    return "PLATFORM_STOP";
    case ST_PLATFORM_DWELL:   return "PLATFORM_DWELL";
    case ST_DEPARTING:        return "DEPARTING";
    case ST_LEADER_WAITING:               return "LEADER_WAITING";
    case ST_LEADER_RELEASED:              return "LEADER_RELEASED";
    case ST_CE_LOCAL_EXIT_TO_CTO_LEADER: return "CE_LOCAL_EXIT_TO_CTO";//added 3-13
    default:                              return "ST_UNKNOWN";
  }
}

// ============================================================================
// PEER TABLE MANAGEMENT
// ============================================================================
PeerState* findOrCreatePeer(uint32_t id) {
  for (int i = 0; i < MAX_PEERS; i++)
    if (peers[i].active && peers[i].id == id) return &peers[i];
  for (int i = 0; i < MAX_PEERS; i++) {
    if (!peers[i].active) {
      peers[i] = {id, BLOCK_UNKNOWN, 0, 0, true};
      return &peers[i];
    }
  }
  return nullptr;
}

void printPeerTable() {
  Serial.println("---------------- CTO AUDIT TABLE ----------------");
  bool any = false;
  for (int i = 0; i < MAX_PEERS; i++) {
    if (!peers[i].active) continue; any = true;
    Serial.printf("%s  %-8s id=%-10lu block=%-10s seq=%-6lu age=%lu ms\n",
                  ts().c_str(), locoName(peers[i].id), (unsigned long)peers[i].id,
                  blockName(peers[i].lastBlock), (unsigned long)peers[i].lastSeq,
                  millis() - peers[i].lastSeenMs);
  }
  if (!any) Serial.println("  (empty)");
  Serial.println("-------------------------------------------------");
}

void clearDispatcherPeerTable() {
  for (int i = 0; i < MAX_PEERS; i++) peers[i] = {0, BLOCK_UNKNOWN, 0, 0, false};
}
void updateWorkZoneStatus() {
  //Blynk.virtualWrite(VPIN_WZ_STATUS, "STUBBED IN CTO v1.4D");
}

// ============================================================================
// TRANSMIT FUNCTIONS  (unchanged from v1.3)
// ============================================================================
void sendManual(uint8_t cmd, uint32_t target) {
  TrainPacket pkt = {};
  pkt.magic = MAGIC; pkt.version = VERSION; pkt.msgType = MSG_TYPE_MANUAL;
  pkt.senderId = DISPATCHER_ID; pkt.locoId = target; pkt.cmd = cmd;
  pkt.sequenceNumber = ++txSequence;
  esp_now_send(BROADCAST_MAC, (uint8_t*)&pkt, sizeof(pkt));
  const char* n = (cmd == CMD_START) ? "START" :
                  (cmd == CMD_STOP) ? "STOP" :
                  (cmd == CMD_CE_START) ? "CE" : "CMD";
  Serial.printf("%s TX %-6s -> %s seq=%lu\n",
                ts().c_str(), n, locoName(target), (unsigned long)txSequence);
}

void sendConfig(uint8_t cmd, uint32_t target, uint16_t value) {
  TrainPacket pkt = {};
  pkt.magic = MAGIC; pkt.version = VERSION; pkt.msgType = MSG_TYPE_MANUAL;
  pkt.senderId = DISPATCHER_ID; pkt.locoId = target; pkt.cmd = cmd; pkt.value = value;
  pkt.sequenceNumber = ++txSequence;
  esp_now_send(BROADCAST_MAC, (uint8_t*)&pkt, sizeof(pkt));
  Serial.printf("%s TX PWM %u -> %s seq=%lu\n",
                ts().c_str(), value, locoName(target), (unsigned long)txSequence);
}

void sendRoleParam(uint32_t target, uint8_t roleId, uint8_t paramId, uint16_t value) {
  TrainPacket pkt = {};
  pkt.magic = MAGIC; pkt.version = VERSION; pkt.msgType = MSG_TYPE_MANUAL;
  pkt.senderId = DISPATCHER_ID; pkt.locoId = target;
  pkt.cmd = CMD_SET_ROLE_PARAM; pkt.value = value;
  pkt.roleId = roleId; pkt.paramId = paramId;
  pkt.sequenceNumber = ++txSequence;
  esp_now_send(BROADCAST_MAC, (uint8_t*)&pkt, sizeof(pkt));

  const char* paramStr =
    (paramId == PARAM_RAMP_DELAY)      ? "rampDelay"     :
    (paramId == PARAM_RAMP_RATE)       ? "rampRate(ALL)" :
    (paramId == PARAM_DWELL)           ? "dwell"         :
    (paramId == PARAM_FOLLOW_DELAY)    ? "followDelay"   :
    (paramId == PARAM_ADVANCE_MS)      ? "advanceMs"     :
    (paramId == PARAM_PLATFORM_DWELL)  ? "platformDwell" :
    (paramId == PARAM_RAMP_UP_RATE)    ? "rampUpRate"    :
    (paramId == PARAM_ADVANCE_PWM)     ? "advancePwm"    : "?";

  const char* roleStr =
    (roleId == ROLE_ID_STATION)  ? "STATION"  :
    (roleId == ROLE_ID_TRAILING) ? "TRAILING" :
    (roleId == ROLE_ID_LEADER)   ? "LEADER"   : "DEFAULT";

  Serial.printf("%s TX PARAM %s.%s=%u -> %s seq=%lu\n",
                ts().c_str(), roleStr, paramStr, value,
                locoName(target), (unsigned long)txSequence);
}

void sendEstop() {
  TrainPacket pkt = {};
  pkt.magic = MAGIC; pkt.version = VERSION; pkt.msgType = MSG_TYPE_ESTOP;
  pkt.senderId = DISPATCHER_ID; pkt.locoId = BROADCAST_ID; pkt.cmd = CMD_ESTOP;
  pkt.sequenceNumber = ++txSequence;
  esp_now_send(BROADCAST_MAC, (uint8_t*)&pkt, sizeof(pkt));
  Serial.printf("%s TX E-STOP BROADCAST seq=%lu\n",
                ts().c_str(), (unsigned long)txSequence);
}

void sendClearAll() {
  TrainPacket pkt = {};
  pkt.magic = MAGIC; pkt.version = VERSION; pkt.msgType = MSG_TYPE_MANUAL;
  pkt.senderId = DISPATCHER_ID; pkt.locoId = BROADCAST_ID; pkt.cmd = CMD_CLEAR_ALL;
  pkt.sequenceNumber = ++txSequence;
  esp_now_send(BROADCAST_MAC, (uint8_t*)&pkt, sizeof(pkt));
  clearDispatcherPeerTable(); updateWorkZoneStatus();
  Serial.printf("%s TX CLEAR ALL seq=%lu\n", ts().c_str(), (unsigned long)txSequence);
}

void declareWorkZone(uint8_t blockId) {
  workZoneBlock = blockId; updateWorkZoneStatus();
  Serial.printf("%s WORK ZONE %s — stubbed in CTO v1.4D\n",
                ts().c_str(), blockId == BLOCK_UNKNOWN ? "CLEAR" : blockName(blockId));
}
// ============================================================================
// AUTOMATIC CIRCUIT EXPRESS REQUEST SERVICE
//
// Dispatcher-only timer. It periodically sends the same CE command used by
// the manual "ce" command. The locomotive sketches remain authoritative:
// CE begins only if their existing CTO state says the request is valid.
// ============================================================================
void serviceCeAuto() {
  if (!ceAutoEnabled) return;
  if (ceAutoIntervalMs == 0) return;

  unsigned long now = millis();
  if (now - ceAutoLastRequestMs < ceAutoIntervalMs) return;

  ceAutoLastRequestMs = now;
  sendManual(CMD_CE_START, BROADCAST_ID);

  Serial.printf("%s CE AUTO: periodic Circuit Express request sent\n",
                ts().c_str());
}
// ============================================================================
// RECEIVE HANDLER — passive auditor + STATUS logger
// ============================================================================
void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != (int)sizeof(TrainPacket)) return;
  TrainPacket pkt; memcpy(&pkt, data, sizeof(pkt));
  if (pkt.magic != MAGIC || pkt.version != VERSION) return;
  if (pkt.senderId == DISPATCHER_ID) return;

  if (pkt.msgType == MSG_TYPE_BLOCK) {
    PeerState* peer = findOrCreatePeer(pkt.locoId);
    if (!peer) { Serial.println(ts() + " RX BLOCK ignored: peer table full"); return; }
    peer->lastBlock = pkt.blockId; peer->lastSeq = pkt.sequenceNumber;
    peer->lastSeenMs = millis();
    Serial.printf("%s RX BLOCK      %-8s -> %-10s seq=%lu\n",
                  ts().c_str(), locoName(pkt.locoId), blockName(pkt.blockId),
                  (unsigned long)pkt.sequenceNumber);
  }
  else if (pkt.msgType == MSG_TYPE_ENCOUNTER_START) {
    Serial.printf("%s RX ENC_START  trailing=%-8s leader=%-8s block=%-10s enc=%lu seq=%lu\n",
                  ts().c_str(), locoName(pkt.senderId), locoName(pkt.locoId),
                  blockName(pkt.blockId), (unsigned long)pkt.encounterId,
                  (unsigned long)pkt.sequenceNumber);
  }
  else if (pkt.msgType == MSG_TYPE_RELEASE) {
    Serial.printf("%s RX RELEASE    trailing=%-8s leader=%-8s block=%-10s enc=%lu seq=%lu\n",
                  ts().c_str(), locoName(pkt.senderId), locoName(pkt.locoId),
                  blockName(pkt.blockId), (unsigned long)pkt.encounterId,
                  (unsigned long)pkt.sequenceNumber);
  }
  else if (pkt.msgType == MSG_TYPE_DEPARTING) {
    Serial.printf("%s RX DEPARTING  sender=%-8s  block=%-10s enc=%lu seq=%lu\n",
                  ts().c_str(), locoName(pkt.senderId),
                  blockName(pkt.blockId), (unsigned long)pkt.encounterId,
                  (unsigned long)pkt.sequenceNumber);
  }
  else if (pkt.msgType == MSG_TYPE_ESTOP) {
    Serial.printf("%s RX E-STOP/ACK from %s seq=%lu\n",
                  ts().c_str(), locoName(pkt.locoId),
                  (unsigned long)pkt.sequenceNumber);
  }
 else if (pkt.msgType == MSG_TYPE_STATUS) {

  if (pkt.cmd == ST_CE_LOCAL_EXIT_TO_CTO_LEADER) {
    bool ceActiveReported = (pkt.flags & 0x01) != 0;
    uint8_t ceRoleReported = (pkt.flags >> 1) & 0x03;

    Serial.printf(
      "%s RX CE EXIT   %-8s LOCAL->CTO_LEADER blk=%-10s cruise=%-3u current=%-3u target=%-3u ceActive=%d ceRole=%u\n",
      ts().c_str(),
      locoName(pkt.senderId),
      blockName(pkt.blockId),
      (unsigned)pkt.roleId,
      (unsigned)pkt.value,
      (unsigned)pkt.paramId,
      ceActiveReported ? 1 : 0,
      (unsigned)ceRoleReported
    );
    return;
  }

  // -----------------------------------------------------------------------
  // Existing STATUS log
  // Decode the flags byte: bit0=confirmedStopped, bit1=departingReceived
  // -----------------------------------------------------------------------
  bool confirmed  = (pkt.flags & 0x01) != 0;
  bool departing  = (pkt.flags & 0x02) != 0;
  Serial.printf("%s RX STATUS    %-8s %-20s blk=%-10s pwm=%-3u ok=%d dep=%d enc=%lu\n",
                ts().c_str(),
                locoName(pkt.senderId),
                statusEventName(pkt.cmd),
                blockName(pkt.blockId),
                (unsigned)pkt.value,
                confirmed ? 1 : 0,
                departing ? 1 : 0,
                (unsigned long)pkt.encounterId);
}
}
// ============================================================================
// BLYNK HANDLERS
// ============================================================================
//BLYNK_WRITE(VPIN_WZ_PATIO)    { if (param.asInt() == 1) declareWorkZone(BLOCK_PATIO);    }
//BLYNK_WRITE(VPIN_WZ_BAMBOO)   { if (param.asInt() == 1) declareWorkZone(BLOCK_BAMBOO);   }
//BLYNK_WRITE(VPIN_WZ_ARCHES)   { if (param.asInt() == 1) declareWorkZone(BLOCK_ARCHES);   }
//BLYNK_WRITE(VPIN_WZ_GRILLERS) { if (param.asInt() == 1) declareWorkZone(BLOCK_GRILLERS); }
//BLYNK_WRITE(VPIN_WZ_CLEAR)    { if (param.asInt() == 1) declareWorkZone(BLOCK_UNKNOWN);  }

// ============================================================================
// SERIAL COMMAND REFERENCE
// ============================================================================
void printCommands() {
  Serial.println("========================================");
  Serial.println("MOTION");
  Serial.println("  g              GO both");
  Serial.println("  s              STOP both");
  Serial.println("  ce             start Circuit Express from current CTO pair");
 Serial.println("  ceauto10       send automatic CE request every 10 minutes");
 Serial.println("  ceoff          stop automatic CE requests");
 Serial.println("  e              E-STOP broadcast");
  Serial.println("  og             GO Otto only");
  Serial.println("  tg             GO Toby only");
  Serial.println("  os             STOP Otto only");
  Serial.println("  ts             STOP Toby only");
  Serial.println("DIAGNOSTICS");
  Serial.println("  pt             print peer/audit table");
  Serial.println("  c              CLEAR ALL — reset peer tables and CTO state");
  Serial.println("CRUISE PWM  (0-255)");
  Serial.println("  op###          Otto cruise PWM      e.g. op110");
  Serial.println("  tp###          Toby cruise PWM      e.g. tp120");
  Serial.println("  bp###          both cruise PWM      e.g. bp115");
  Serial.println("RAMP RATES  (ms per PWM step)");
  Serial.println("  rr###          shared decel rate    e.g. rr80  (all profiles)");
  Serial.println("  ru###          shared accel rate    e.g. ru120 (all departures)");
  Serial.println("ROLE PROFILES  (ms values)");
  Serial.println("  target: o=Otto  t=Toby  b=both");
  Serial.println("  role:   df=default  st=station  tr=trailing  ld=leader");
  Serial.println("  param:  rd=ramp delay   dw=dwell");
  Serial.println("  format: [target][role][param][value]");
  Serial.println("  e.g.  oldrd6000   Otto LEADER ramp delay 6000ms");
  Serial.println("  e.g.  bstdw5000   both STATION dwell 5000ms");
  Serial.println("TRAILING SEQUENCE  (ms values, up to 30000)");
  Serial.println("  [o|t|b]fd#####   follow delay        e.g. bfd3000");
  Serial.println("  [o|t|b]ad#####   advance duration    e.g. bad10000");
  Serial.println("  [o|t|b]pd#####   platform dwell      e.g. bpd10000");
  Serial.println("  [o|t|b]ap###     advance PWM         e.g. bap60");
  Serial.println("WORK ZONE  (stubbed — no broadcast sent)");
  Serial.println("  wz patio / bamboo / arches / grillers");
  Serial.println("  wzclear");
  Serial.println("========================================");
}

// ============================================================================
// SERIAL PARSER  (unchanged from v1.3)
// ============================================================================
bool parseRoleParam(const String& line) {
  if (line.length() < 6) return false;
  int pos = 0;
  uint32_t target;
  char t = line.charAt(pos++);
  if      (t == 'o') target = OTTO;
  else if (t == 't') target = TOBY;
  else if (t == 'b') target = BROADCAST_ID;
  else return false;

  if (line.length() < (unsigned)(pos + 2)) return false;
  String roleStr = line.substring(pos, pos + 2); pos += 2;
  uint8_t roleId;
  if      (roleStr == "df") roleId = ROLE_ID_DEFAULT;
  else if (roleStr == "st") roleId = ROLE_ID_STATION;
  else if (roleStr == "tr") roleId = ROLE_ID_TRAILING;
  else if (roleStr == "ld") roleId = ROLE_ID_LEADER;
  else return false;

  if (line.length() < (unsigned)(pos + 2)) return false;
  String paramStr = line.substring(pos, pos + 2); pos += 2;
  uint8_t paramId;
  if      (paramStr == "rd") paramId = PARAM_RAMP_DELAY;
  else if (paramStr == "dw") paramId = PARAM_DWELL;
  else return false;

  if (line.length() <= (unsigned)pos) return false;
  uint16_t value = (uint16_t)constrain(line.substring(pos).toInt(), 0, 30000);
  sendRoleParam(target, roleId, paramId, value);
  return true;
}

bool parseTrailingParam(const String& line) {
  if (line.length() < 4) return false;
  uint32_t target;
  char t = line.charAt(0);
  if      (t == 'o') target = OTTO;
  else if (t == 't') target = TOBY;
  else if (t == 'b') target = BROADCAST_ID;
  else return false;

  String param = line.substring(1, 3);
  uint8_t paramId;
  if      (param == "fd") paramId = PARAM_FOLLOW_DELAY;
  else if (param == "ad") paramId = PARAM_ADVANCE_MS;
  else if (param == "pd") paramId = PARAM_PLATFORM_DWELL;
  else if (param == "ap") paramId = PARAM_ADVANCE_PWM;
  else return false;

  if (line.length() <= 3) return false;
  uint16_t value = (uint16_t)constrain(line.substring(3).toInt(), 0, 30000);
  sendRoleParam(target, ROLE_ID_TRAILING, paramId, value);
  return true;
}

void serviceSerial() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim(); line.toLowerCase();
  if (line.length() == 0) return;

if (line == "g")  { sendManual(CMD_START, BROADCAST_ID); return; }
if (line == "s")  { sendManual(CMD_STOP,  BROADCAST_ID); return; }
if (line == "ce") { sendManual(CMD_CE_START, BROADCAST_ID); return; }

if (line == "ceauto10") {
  ceAutoEnabled       = true;
  ceAutoIntervalMs    = 10UL * 60UL * 1000UL;
  ceAutoLastRequestMs = millis();

  Serial.printf("%s CE AUTO: enabled — next automatic CE request in 10 minutes\n",
                ts().c_str());
  return;
}

if (line == "ceoff") {
  ceAutoEnabled       = false;
  ceAutoIntervalMs    = 0;
  ceAutoLastRequestMs = 0;

  Serial.printf("%s CE AUTO: disabled\n", ts().c_str());
  return;
}

if (line == "e")  { sendEstop();                          return; }
  if (line == "c")  { sendClearAll();                       return; }
  if (line == "pt") { printPeerTable();                     return; }
  if (line == "og") { sendManual(CMD_START, OTTO);          return; }
  if (line == "tg") { sendManual(CMD_START, TOBY);          return; }
  if (line == "os") { sendManual(CMD_STOP,  OTTO);          return; }
  if (line == "ts") { sendManual(CMD_STOP,  TOBY);          return; }

  if (line == "wzclear") { declareWorkZone(BLOCK_UNKNOWN); return; }
  if (line.startsWith("wz ")) {
    uint8_t b = blockFromName(line.substring(3));
    if (b != BLOCK_UNKNOWN) declareWorkZone(b);
    else Serial.println("Unknown block. Use: patio bamboo arches grillers");
    return;
  }

  if (line.startsWith("op") && line.length() > 2) {
    sendConfig(CMD_SET_PWM, OTTO, (uint16_t)constrain(line.substring(2).toInt(), 0, 255)); return;
  }
  if (line.startsWith("tp") && line.length() > 2) {
    sendConfig(CMD_SET_PWM, TOBY, (uint16_t)constrain(line.substring(2).toInt(), 0, 255)); return;
  }
  if (line.startsWith("bp") && line.length() > 2) {
    sendConfig(CMD_SET_PWM, BROADCAST_ID, (uint16_t)constrain(line.substring(2).toInt(), 0, 255)); return;
  }

  if (line.startsWith("rr") && line.length() > 2) {
    uint16_t val = (uint16_t)constrain(line.substring(2).toInt(), 10, 2000);
    sendRoleParam(BROADCAST_ID, ROLE_ID_TRAILING, PARAM_RAMP_RATE, val);
    return;
  }

  if (line.startsWith("ru") && line.length() > 2) {
    uint16_t val = (uint16_t)constrain(line.substring(2).toInt(), 10, 2000);
    sendRoleParam(BROADCAST_ID, ROLE_ID_DEFAULT, PARAM_RAMP_UP_RATE, val);
    return;
  }

  if (line.length() > 3) {
    char tgt = line.charAt(0);
    if (tgt == 'o' || tgt == 't' || tgt == 'b') {
      if (parseTrailingParam(line)) return;
    }
  }

  if (parseRoleParam(line)) return;

  Serial.printf("%s Unknown command: '%s'\n", ts().c_str(), line.c_str());
  printCommands();
}

// ============================================================================
// MQTT COMMAND HANDLER — Flask Dispatcher Console
// ============================================================================
void onDispatcherMQTTMessage(char* topic, byte* payload, unsigned int length) {
  String t(topic);
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.printf("%s MQTT RX [%s] = %s\n", ts().c_str(), t.c_str(), msg.c_str());

  uint16_t val = (uint16_t)constrain(msg.toInt(), 0, 30000);

  // Operational commands
  if      (t == TOPIC_DISP_GO_BOTH)   sendManual(CMD_START, BROADCAST_ID);
  else if (t == TOPIC_DISP_STOP_BOTH) sendManual(CMD_STOP,  BROADCAST_ID);
  else if (t == TOPIC_DISP_GO_OTTO)   sendManual(CMD_START, OTTO);
  else if (t == TOPIC_DISP_GO_TOBY)   sendManual(CMD_START, TOBY);
  else if (t == TOPIC_DISP_STOP_OTTO) sendManual(CMD_STOP,  OTTO);
  else if (t == TOPIC_DISP_STOP_TOBY) sendManual(CMD_STOP,  TOBY);
  else if (t == TOPIC_DISP_CE)        sendManual(CMD_CE_START, BROADCAST_ID);
  else if (t == TOPIC_DISP_ESTOP)     sendEstop();
  else if (t == TOPIC_DISP_CLEARALL)  sendClearAll();

  // Tuning parameters — broadcast to both locos
  else if (t == TOPIC_TUNE_ARRIVAL)
    sendRoleParam(BROADCAST_ID, ROLE_ID_TRAILING, PARAM_RAMP_RATE,      val);
  else if (t == TOPIC_TUNE_DEPARTURE)
    sendRoleParam(BROADCAST_ID, ROLE_ID_DEFAULT,  PARAM_RAMP_UP_RATE,   val);
  else if (t == TOPIC_TUNE_PULLUP)
    sendRoleParam(BROADCAST_ID, ROLE_ID_TRAILING, PARAM_FOLLOW_DELAY,   val);
  else if (t == TOPIC_TUNE_CREEP_DUR)
    sendRoleParam(BROADCAST_ID, ROLE_ID_TRAILING, PARAM_ADVANCE_MS,     val);
  else if (t == TOPIC_TUNE_CREEP_SPD)
    sendRoleParam(BROADCAST_ID, ROLE_ID_TRAILING, PARAM_ADVANCE_PWM,    val);
  else if (t == TOPIC_TUNE_PLATFORM)
    sendRoleParam(BROADCAST_ID, ROLE_ID_TRAILING, PARAM_PLATFORM_DWELL, val);
  else if (t == TOPIC_TUNE_TRAILING_BRK)
    sendRoleParam(BROADCAST_ID, ROLE_ID_STATION,  PARAM_RAMP_DELAY,     val);
  else if (t == TOPIC_TUNE_STATION_BRAKE)
    sendRoleParam(BROADCAST_ID, ROLE_ID_STATION,  PARAM_RAMP_DELAY,     val);
  else if (t == TOPIC_TUNE_STATION_DWELL)
    sendRoleParam(BROADCAST_ID, ROLE_ID_STATION,  PARAM_DWELL,          val);
}

void serviceDispatcherMQTTReconnect() {
  static unsigned long lastAttemptMs = 0;
  if (mqtt.connected()) return;

  unsigned long now = millis();
  if (now - lastAttemptMs < 5000UL) return;
  lastAttemptMs = now;

  Serial.printf("%s MQTT connecting to %s:%d ...\n", ts().c_str(), MQTT_BROKER, MQTT_PORT);
  if (mqtt.connect(MQTT_CLIENT_ID)) {
    mqtt.subscribe(TOPIC_DISP_GO_BOTH);
    mqtt.subscribe(TOPIC_DISP_STOP_BOTH);
    mqtt.subscribe(TOPIC_DISP_GO_OTTO);
    mqtt.subscribe(TOPIC_DISP_GO_TOBY);
    mqtt.subscribe(TOPIC_DISP_STOP_OTTO);
    mqtt.subscribe(TOPIC_DISP_STOP_TOBY);
    mqtt.subscribe(TOPIC_DISP_CE);
    mqtt.subscribe(TOPIC_DISP_ESTOP);
    mqtt.subscribe(TOPIC_DISP_CLEARALL);
    mqtt.subscribe(TOPIC_TUNE_ARRIVAL);
    mqtt.subscribe(TOPIC_TUNE_DEPARTURE);
    mqtt.subscribe(TOPIC_TUNE_PULLUP);
    mqtt.subscribe(TOPIC_TUNE_CREEP_DUR);
    mqtt.subscribe(TOPIC_TUNE_CREEP_SPD);
    mqtt.subscribe(TOPIC_TUNE_PLATFORM);
    mqtt.subscribe(TOPIC_TUNE_TRAILING_BRK);
    mqtt.subscribe(TOPIC_TUNE_STATION_BRAKE);
    mqtt.subscribe(TOPIC_TUNE_STATION_DWELL);
    Serial.println(ts() + " MQTT connected. All topics subscribed.");
  } else {
    Serial.printf("%s MQTT connect failed rc=%d\n", ts().c_str(), mqtt.state());
  }
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(300);
  clearDispatcherPeerTable();
  workZoneBlock = BLOCK_UNKNOWN;

  WiFi.mode(WIFI_STA);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
    //Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);
  Serial.println();
  Serial.print("WiFi connected. IP=");
  Serial.println(WiFi.localIP());
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(onDispatcherMQTTMessage);
  mqtt.setKeepAlive(60);

  if (esp_now_init() != ESP_OK) {
    Serial.println("FATAL: ESP-NOW init failed. Halting.");
    while (true) delay(1000);
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
  peerInfo.channel = 0; peerInfo.encrypt = false;
  if (!esp_now_is_peer_exist(BROADCAST_MAC)) esp_now_add_peer(&peerInfo);
  esp_now_register_recv_cb(onReceive);
  updateWorkZoneStatus();

  Serial.println("========================================");
  Serial.println("LL_MQ_CO_GC_DI_1_2");
  Serial.println("Station brake delay + dwell tuning topics added");
  Serial.println("MQTT web-command intake + existing ESP-NOW CTO/CE dispatcher");
  Serial.println("STATUS reporting: all loco state transitions logged");
  Serial.println("Deadlock fix: RELEASE accepted block-independently");
  Serial.println("Role: operator panel + passive CTO auditor");
  Serial.println("========================================");
  printCommands();
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
  mqtt.loop();
  serviceDispatcherMQTTReconnect();

  //Blynk.run();
  serviceSerial();
}
