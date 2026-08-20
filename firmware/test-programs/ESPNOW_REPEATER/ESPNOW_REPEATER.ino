/*
 * NGR ESPNOW_REPEATER 1.0 — third-node ESP-NOW listening post and relay.
 *
 * ROLE: diagnostic instrument (firmware/README.md catalog rules). This is NOT
 * a QUORUM change and never becomes one implicitly. It carries no navigation,
 * no authority, and no command path to a locomotive.
 *
 * THE QUESTION IT EXISTS TO ANSWER
 * --------------------------------
 * The locomotives lose each other's ESP-NOW status packets in a way that is
 * not explained by separation distance (99.9% delivery at 72 markers on
 * 2026-08-19, at the same separation that gave 0% earlier the same evening),
 * and not fully explained by a pairwise line-of-sight rule (that rule accounts
 * for 7 of 34 STALE events). Every model so far was fitted to convenience data
 * gathered from inside the two failing endpoints. This node is the independent
 * third observer: it hears both locomotives from a location the operator
 * chooses, records what it heard and how strongly, and — when told to — relays
 * what the other locomotive missed.
 *
 * THREE MODES, one runtime switch (topic ngr/survey/<NODE>/cmd):
 *   listen  — receive and record only. No ESP-NOW transmission of any kind.
 *   shadow  — identical to listen, but ALSO counts and publishes every frame
 *             it WOULD have relayed, and why. Nothing goes on the air.
 *   repeat  — relays qualifying frames on the broadcast address.
 * MODE_DEFAULT below sets the boot mode. It is `repeat` because the operator
 * asked for a repeater; set it to MODE_SHADOW for a survey-only deployment.
 *
 * THE DECISIVE CRITERION (operator, 2026-08-20) — load-bearing, unchanged by
 * the fact that relaying is now switched on: it is not enough for this node to
 * hear "most" packets. It must hear the locomotive whose packet the OTHER
 * locomotive is missing, during those specific multi-second gaps. That is why
 * every mode, including `repeat`, keeps the full per-frame record: relaying
 * without measuring would hide the very evidence the relay was built to get.
 *
 * WHY THE RELAY IS CONSERVATIVE (read before loosening any of it)
 * --------------------------------------------------------------
 * QUORUM's `ctoAcceptPeer()` applies NO sequence-monotonicity test: whatever
 * CtoPeerPacket arrives last becomes peer truth, and peer hallMm/boundaries
 * drive the 18/12/6 decel ladder and the follower hold. A relay that repeated
 * a LATE or DUPLICATE frame would therefore be able to roll a peer's believed
 * position BACKWARDS, or to distort `rampFalling`, which compares consecutive
 * samples. Three gates prevent that, and none of them is cosmetic:
 *   1. FRESHNESS  — a frame older than FORWARD_MAX_AGE_MS when it reaches the
 *                   forwarding decision is dropped, never relayed late.
 *   2. NOVELTY    — per source, only a STRICTLY GREATER sequence is relayed,
 *                   so no duplicate and no rollback can leave this node.
 *   3. RATE       — a hard floor between transmissions and a per-second cap,
 *                   so a fault here cannot flood the channel the locomotives
 *                   depend on.
 * Role echoes (0xC5) carry no sequence, so they are relayed at most once per
 * ECHO_MIN_INTERVAL_MS per source.
 *
 * SINGLE-RELAY ASSUMPTION: with one repeater there is no relay loop, because
 * locomotives never forward. If a SECOND repeater is ever added, the novelty
 * gate still stops each node from re-relaying a sequence it already relayed,
 * but the pair must be reviewed before deployment — two relays hearing each
 * other is a different problem from the one this sketch was measured against.
 *
 * WIRE FORMAT: read-only, and relayed byte-for-byte. The CTO structs below are
 * copied field-for-field from QUORUM.ino (CTO2_VERSION 3 / CTO3_ECHO_VERSION 1)
 * to decode and to validate. Nothing here may change ESPNOW_VERSION,
 * CTO2_VERSION or CTO3_ECHO_VERSION, and no MQTT topic or payload used by
 * QUORUM or the console is touched — this node publishes only under its own
 * ngr/survey/<NODE_NAME>/ namespace.
 *
 * CHANNEL: ESP-NOW only works on the channel the radio is parked on. The
 * locomotives are on channel 11 (telemetry 2026-08-19: ch=11, chg=0, txf=0).
 * This node joins the same AP and so lands on the same channel by the same
 * mechanism; it then MEASURES the channel and complains loudly on divergence.
 * A monitor on the wrong channel hears nothing and looks exactly like a
 * perfect radio shadow — the one failure mode that would corrupt the whole
 * investigation.
 *
 * PLACEMENT: do not treat the dispatcher position (mm 116-160) as the survey
 * location of record. It sits inside the observed shadow and shares the
 * Bamboo/Arches blind spot; a relay there adds no path diversity.
 */
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <PubSubClient.h>
#include <time.h>

// credentials.h is git-ignored. Copy firmware/config/credentials_template.h
// into this folder as credentials.h and fill in the real values.
#include "credentials.h"

// ---- identity and endpoints ------------------------------------------------
// NODE_NAME is the survey identity, not a location. Set SURVEY_SITE from the
// operator's placement before each run so the record says where it listened;
// a coverage survey whose samples cannot be attributed to a position is scrap.
static const char* NODE_NAME   = "ESPNOW_REP_1";
// SURVEY_SITE is SETTABLE AT RUNTIME (`site <name>` on the cmd topic) and not a
// compile-time constant, because it changed four times in the first hour of use
// while a compile-time value silently labelled every record MAC_BENCH. A survey
// sample whose position label is wrong is worse than one with no label: it
// looks attributable and is not.
static char SURVEY_SITE[24] = "MAC_BENCH";      // e.g. "ALCOVE_MOUTH", "MM_120"
static const char* MQTT_BROKER = "192.168.68.142";
static const int   MQTT_PORT   = 1883;

// The channel the locomotives are on. Measured, not assumed.
static const uint8_t EXPECTED_CHANNEL = 11;

enum RepMode : uint8_t { MODE_LISTEN=0, MODE_SHADOW=1, MODE_REPEAT=2 };
static const RepMode MODE_DEFAULT = MODE_REPEAT;
static RepMode repMode = MODE_DEFAULT;

// ---- wire formats (READ-ONLY copies of QUORUM's frozen structs) ------------
static const uint8_t CTO2_MAGIC         = 0xC4;
static const uint8_t CTO2_VERSION       = 3;
static const uint8_t CTO3_ECHO_MAGIC    = 0xC5;
static const uint8_t CTO3_ECHO_VERSION  = 1;
typedef struct __attribute__((packed)) {
  uint8_t  magic; uint8_t version;
  uint32_t senderId; uint32_t sequence;
  uint8_t  hallMm; uint8_t frontBoundaryMm; uint8_t rearBoundaryMm;
  int8_t   mapDir;
  uint8_t  autoMode; uint8_t running; uint8_t motionState; uint8_t rampPwm;
  uint8_t  speedValid; uint16_t lastMoveAgeDs; uint16_t speedX10;
  uint8_t  frontOffset; uint8_t rearOffset;
  uint8_t  truthSource;
  uint8_t  stationPhase;
  uint8_t  trafficPhase;
  uint8_t  mustHoldEligible;
  uint32_t trafficStopForId;
  uint32_t senderRxAccepted; uint32_t senderTxAttempts; uint32_t senderTxImmediateErrors;
} CtoPeerPacket;
typedef struct __attribute__((packed)) {
  uint8_t magic; uint8_t version;
  uint32_t senderId;
  uint8_t  role;
  uint32_t partnerId;
  uint32_t pairEpochMs;
} Cto3RoleEcho;

static const uint8_t CTO_BCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// ---- tunables --------------------------------------------------------------
// STALE_MS mirrors QUORUM's CTO_PEER_STALE_MS exactly so this node's gap
// episodes are directly comparable with a locomotive's. It is inherited from
// r12 and has never been measured; that is a separate open item, not this
// sketch's business.
static const uint32_t STALE_MS            = 3000;
static const uint32_t SENDER_PERIOD_MS    = 500;   // CTO_TX_INTERVAL_MS: 2 Hz
static const uint32_t SUMMARY_MS          = 1000;  // per-source summary cadence
static const uint32_t WINDOW_MS           = 10000; // rolling delivery window
static const uint16_t WINDOW_SLOTS        = (WINDOW_MS / SENDER_PERIOD_MS) + 4;
static const uint8_t  MAX_SOURCES         = 8;
// Relay gates. FORWARD_MAX_AGE_MS is the freshness gate: a status frame is
// 500 ms of truth, so 100 ms of relay delay is a fifth of its life and the
// position error it can inject is bounded by whatever the locomotive travels
// in that time. Do not raise it to "rescue" late frames — a late frame that
// overwrites newer peer truth is precisely the hazard.
static const uint32_t FORWARD_MAX_AGE_MS  = 100;
static const uint32_t ECHO_MIN_INTERVAL_MS= 900;   // echoes carry no sequence
static const uint32_t TX_MIN_SPACING_MS   = 15;    // channel courtesy floor
static const uint8_t  TX_MAX_PER_SEC      = 24;    // hard cap; fault containment

// ---- rx plumbing -----------------------------------------------------------
// The ESP-NOW callback runs in the WiFi task. It copies and leaves — exactly
// the discipline QUORUM uses (ctoOnRecv). All decisions belong to loop().
typedef struct {
  uint8_t  mac[6];
  uint32_t rxMs;
  int8_t   rssi;
  uint8_t  len;
  uint8_t  raw[sizeof(CtoPeerPacket)];
} RxItem;
static QueueHandle_t rxQueue = nullptr;
static uint32_t rxDropped = 0;            // queue full: our fault, not the radio's
static uint32_t rxForeign = 0;            // frames that are not CTO at all
static uint32_t txDone = 0, txFailed = 0;

struct Source {
  bool     used = false;
  uint32_t id = 0;
  uint8_t  mac[6] = {0};
  uint32_t firstSeq = 0, lastSeq = 0;
  uint32_t rxCount = 0, echoCount = 0;
  uint32_t missTotal = 0;          // summed sequence steps beyond 1
  uint16_t missRun = 0;            // consecutive misses in the LAST step
  uint16_t missRunMax = 0;
  uint32_t firstRxMs = 0, lastRxMs = 0;
  uint32_t maxGapMs = 0;
  bool     stale = false;
  uint32_t staleOnsetMs = 0;
  uint32_t staleEpisodes = 0;
  int8_t   rssiLast = 0, rssiMin = 127, rssiMax = -128;
  long     rssiSum = 0; uint32_t rssiN = 0;
  uint8_t  hallMm = 255; int8_t mapDir = 0;
  uint8_t  autoMode = 0, running = 0;
  // relay bookkeeping
  uint32_t lastFwdSeq = 0; bool haveFwd = false;
  uint32_t fwdCount = 0, fwdEchoCount = 0;
  uint32_t dropStale = 0, dropDup = 0, dropRate = 0;
  uint32_t lastEchoFwdMs = 0;
  // rolling window: receive time AND sequence of recent status frames, ring
  // buffer. The sequence is what makes the window figure exact — see
  // windowDelivery().
  uint32_t win[WINDOW_SLOTS]; uint32_t winSeq[WINDOW_SLOTS];
  uint16_t winHead = 0; uint16_t winFill = 0;
};
static Source sources[MAX_SOURCES];

WiFiClient   espClient;
PubSubClient mqtt(espClient);
static char  tOnline[80], tRx[80], tPeer[96], tEvent[80], tHealth[80], tCmd[80], tMode[80];
static uint32_t lastSummary = 0, lastHealth = 0, lastMqttTry = 0;
static uint32_t lastTxMs = 0, txSecWindow = 0; static uint8_t txThisSec = 0;
static uint8_t  radioChannel = 0;
static bool     channelWarned = false;
static bool     timeValid = false;
static bool     radioUp = false;

static const char* modeName(RepMode m){
  return m==MODE_REPEAT ? "repeat" : (m==MODE_SHADOW ? "shadow" : "listen");
}

// ---------------------------------------------------------------------------
static void onEspNowRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len){
  if(len <= 0 || len > (int)sizeof(CtoPeerPacket)) { rxForeign++; return; }
  RxItem it;
  if(info && info->src_addr) memcpy(it.mac, info->src_addr, 6); else memset(it.mac, 0, 6);
  it.rxMs = millis();
  // Per-frame RSSI comes from the packet's own rx_ctrl, NOT WiFi.RSSI().
  // WiFi.RSSI() reports the link to the ACCESS POINT and says nothing about
  // the locomotive-to-here path, which is the entire question here.
  it.rssi = (info && info->rx_ctrl) ? (int8_t)info->rx_ctrl->rssi : 0;
  it.len  = (uint8_t)len;
  memcpy(it.raw, data, len);
  if(rxQueue && xQueueSend(rxQueue, &it, 0) != pdTRUE) rxDropped++;
}

// Broadcast is unacknowledged: SUCCESS here means the frame was transmitted,
// NOT that any locomotive heard it. Same caveat QUORUM records for ctoOnSend.
static void onEspNowSend(const wifi_tx_info_t*, esp_now_send_status_t status){
  if(status == ESP_NOW_SEND_SUCCESS) txDone++; else txFailed++;
}

static Source* findSource(uint32_t id){
  for(uint8_t i=0;i<MAX_SOURCES;i++) if(sources[i].used && sources[i].id==id) return &sources[i];
  for(uint8_t i=0;i<MAX_SOURCES;i++) if(!sources[i].used){
    sources[i] = Source(); sources[i].used = true; sources[i].id = id; return &sources[i];
  }
  return nullptr;   // more than MAX_SOURCES senders: report, never overwrite
}

// Rolling-window delivery over the last WINDOW_MS, computed from SEQUENCE
// NUMBERS, not from elapsed time. First bench run, 2026-08-20: a time-based
// denominator (span / 500 ms) read 105.6% because the window's start is the
// first frame IN it, not the instant the window opens, so it systematically
// under-counted what the sender sent. Sequence arithmetic has no such bias:
// the sender tells us exactly how many frames it emitted between the oldest
// frame in the window and the newest. This is the figure directly comparable
// with the locomotives' own percentage readings.
static float windowDelivery(const Source& s, uint32_t now){
  if(s.winFill == 0) return 0.0f;
  uint32_t cutoff = (now > WINDOW_MS) ? now - WINDOW_MS : 0;
  uint16_t n = 0; uint32_t oldestSeq = 0; bool haveOldest = false;
  for(uint16_t i=0;i<s.winFill;i++){
    if(s.win[i] < cutoff) continue;
    n++;
    if(!haveOldest || s.winSeq[i] < oldestSeq){ oldestSeq = s.winSeq[i]; haveOldest = true; }
  }
  if(!haveOldest || n == 0) return 0.0f;
  if(s.lastSeq < oldestSeq) return 0.0f;          // sender restarted mid-window
  uint32_t expected = s.lastSeq - oldestSeq + 1;
  if(expected < n) expected = n;                  // never report above 100%
  return 100.0f * (float)n / (float)expected;
}

static void isoStamp(char* out, size_t n){
  if(!timeValid){ out[0]='\0'; return; }
  time_t t = time(nullptr); struct tm tmv;
  localtime_r(&t, &tmv);
  strftime(out, n, "%Y-%m-%dT%H:%M:%S", &tmv);
}

// ---- the relay decision ----------------------------------------------------
// Returns a short reason string; "SENT" means it went on the air, "WOULD"
// means shadow mode counted it and stayed silent, anything else is a drop.
static const char* forwardDecide(Source& s, const RxItem& it, uint32_t seq, bool isEcho, uint32_t now){
  if(repMode == MODE_LISTEN) return "LISTEN";
  if(!radioUp)               return "NORADIO";
  uint32_t age = now - it.rxMs;
  if(age > FORWARD_MAX_AGE_MS){ s.dropStale++; return "STALE"; }
  if(isEcho){
    if(s.lastEchoFwdMs && (now - s.lastEchoFwdMs) < ECHO_MIN_INTERVAL_MS){ s.dropDup++; return "ECHO_RATE"; }
  } else {
    // NOVELTY: strictly greater only. Equal is a duplicate (heard twice, or a
    // second relay); less is a rollback and is exactly what must never reach a
    // locomotive that has no monotonicity test of its own.
    if(s.haveFwd && seq <= s.lastFwdSeq){ s.dropDup++; return "DUP"; }
  }
  if(txSecWindow != now/1000){ txSecWindow = now/1000; txThisSec = 0; }
  if(txThisSec >= TX_MAX_PER_SEC){ s.dropRate++; return "RATE"; }
  // Spacing DEFERS, it does not discard. First bench run, 2026-08-20: the two
  // locomotives' 2 Hz streams are ~10 ms apart, so a spacing gate that dropped
  // was silently refusing to relay roughly every second Otto frame ("SPACING"
  // in the first capture). Waiting out the remainder costs a few ms against a
  // 100 ms freshness budget; refusing costs the frame the relay exists for.
  if(lastTxMs){
    uint32_t since = now - lastTxMs;
    if(since < TX_MIN_SPACING_MS){
      uint32_t wait = TX_MIN_SPACING_MS - since;
      if(wait > TX_MIN_SPACING_MS) wait = TX_MIN_SPACING_MS;   // clock skew guard
      delay(wait);
      now = millis();
      if((now - it.rxMs) > FORWARD_MAX_AGE_MS){ s.dropStale++; return "STALE"; }
    }
  }

  if(repMode == MODE_SHADOW){
    if(isEcho) s.lastEchoFwdMs = now; else { s.lastFwdSeq = seq; s.haveFwd = true; }
    if(isEcho) s.fwdEchoCount++; else s.fwdCount++;
    return "WOULD";
  }
  esp_err_t r = esp_now_send(CTO_BCAST, it.raw, it.len);
  lastTxMs = now; txThisSec++;
  if(r != ESP_OK) return "TXERR";
  if(isEcho){ s.lastEchoFwdMs = now; s.fwdEchoCount++; }
  else      { s.lastFwdSeq = seq; s.haveFwd = true; s.fwdCount++; }
  return "SENT";
}

static void publishFrame(const Source& s, const RxItem& it, uint32_t seq, bool isEcho,
                         const char* fwd, uint32_t fwdAgeMs){
  if(!mqtt.connected()) return;
  char iso[32]; isoStamp(iso, sizeof(iso));
  char mac[18];
  snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
           it.mac[0],it.mac[1],it.mac[2],it.mac[3],it.mac[4],it.mac[5]);
  char mm[8];
  if(s.hallMm==255) snprintf(mm,sizeof(mm),"null"); else snprintf(mm,sizeof(mm),"%u",(unsigned)s.hallMm);
  char payload[400];
  snprintf(payload, sizeof(payload),
    "{\"site\":\"%s\",\"mode\":\"%s\",\"src\":%lu,\"seq\":%lu,\"rxMs\":%lu,\"iso\":\"%s\","
    "\"rssi\":%d,\"len\":%u,\"type\":\"%s\",\"mac\":\"%s\",\"mm\":%s,\"dir\":%d,"
    "\"ch\":%u,\"missRun\":%u,\"fwd\":\"%s\",\"fwdAgeMs\":%lu}",
    SURVEY_SITE, modeName(repMode), (unsigned long)s.id, (unsigned long)seq,
    (unsigned long)it.rxMs, iso, (int)it.rssi, (unsigned)it.len,
    isEcho ? "echo" : "cto", mac, mm, (int)s.mapDir,
    (unsigned)radioChannel, (unsigned)s.missRun, fwd, (unsigned long)fwdAgeMs);
  mqtt.publish(tRx, payload, false);
}

static void publishEvent(const char* kind, const Source& s, uint32_t gapMs){
  if(!mqtt.connected()) return;
  char iso[32]; isoStamp(iso, sizeof(iso));
  char mm[8];
  if(s.hallMm==255) snprintf(mm,sizeof(mm),"null"); else snprintf(mm,sizeof(mm),"%u",(unsigned)s.hallMm);
  char payload[288];
  snprintf(payload, sizeof(payload),
    "{\"site\":\"%s\",\"event\":\"%s\",\"src\":%lu,\"gapMs\":%lu,\"iso\":\"%s\","
    "\"rxMs\":%lu,\"lastSeq\":%lu,\"rssiLast\":%d,\"mm\":%s,\"mode\":\"%s\"}",
    SURVEY_SITE, kind, (unsigned long)s.id, (unsigned long)gapMs, iso,
    (unsigned long)millis(), (unsigned long)s.lastSeq, (int)s.rssiLast, mm,
    modeName(repMode));
  mqtt.publish(tEvent, payload, false);
  Serial.printf("EVT,%lu,%s,%lu,%lu\n",
                (unsigned long)millis(), kind, (unsigned long)s.id, (unsigned long)gapMs);
}

static void publishSummary(Source& s, uint32_t now){
  uint32_t gap = now - s.lastRxMs;
  uint32_t span = (s.lastSeq >= s.firstSeq) ? (s.lastSeq - s.firstSeq + 1) : 0;
  float cum = span ? (100.0f * (float)s.rxCount / (float)span) : 0.0f;
  float win = windowDelivery(s, now);
  float rssiMean = s.rssiN ? ((float)s.rssiSum / (float)s.rssiN) : 0.0f;
  char topic[112];
  snprintf(topic, sizeof(topic), "%s/%lu", tPeer, (unsigned long)s.id);
  char iso[32]; isoStamp(iso, sizeof(iso));
  char mm[8];
  if(s.hallMm==255) snprintf(mm,sizeof(mm),"null"); else snprintf(mm,sizeof(mm),"%u",(unsigned)s.hallMm);
  char payload[512];
  snprintf(payload, sizeof(payload),
    "{\"site\":\"%s\",\"mode\":\"%s\",\"src\":%lu,\"iso\":\"%s\",\"rx\":%lu,\"echo\":%lu,"
    "\"seqSpan\":%lu,\"missTotal\":%lu,\"missRunMax\":%u,"
    "\"deliveryWin\":%.1f,\"deliveryCum\":%.1f,"
    "\"gapMs\":%lu,\"maxGapMs\":%lu,\"stale\":%u,\"staleEpisodes\":%lu,"
    "\"rssiLast\":%d,\"rssiMin\":%d,\"rssiMax\":%d,\"rssiMean\":%.1f,"
    "\"mm\":%s,\"dir\":%d,\"auto\":%u,\"run\":%u,"
    "\"fwd\":%lu,\"fwdEcho\":%lu,\"dropStale\":%lu,\"dropDup\":%lu,\"dropRate\":%lu}",
    SURVEY_SITE, modeName(repMode), (unsigned long)s.id, iso,
    (unsigned long)s.rxCount, (unsigned long)s.echoCount,
    (unsigned long)span, (unsigned long)s.missTotal, (unsigned)s.missRunMax,
    win, cum,
    (unsigned long)gap, (unsigned long)s.maxGapMs,
    (unsigned)(s.stale?1:0), (unsigned long)s.staleEpisodes,
    (int)s.rssiLast, (int)(s.rssiMin==127?0:s.rssiMin), (int)(s.rssiMax==-128?0:s.rssiMax),
    rssiMean, mm, (int)s.mapDir, (unsigned)s.autoMode, (unsigned)s.running,
    (unsigned long)s.fwdCount, (unsigned long)s.fwdEchoCount,
    (unsigned long)s.dropStale, (unsigned long)s.dropDup, (unsigned long)s.dropRate);
  if(mqtt.connected()) mqtt.publish(topic, payload, false);
  Serial.printf("SUM,%lu,%lu,%.1f,%.1f,%lu,%lu,%u,%d,fwd=%lu\n",
                (unsigned long)now, (unsigned long)s.id, win, cum,
                (unsigned long)gap, (unsigned long)s.maxGapMs,
                (unsigned)s.missRunMax, (int)s.rssiLast, (unsigned long)s.fwdCount);
}

static void handleFrame(const RxItem& it){
  const uint8_t magic = it.raw[0];
  const uint8_t ver   = it.raw[1];
  const uint32_t now  = millis();
  uint32_t id = 0, seq = 0;
  Source* s = nullptr;

  if(it.len == (int)sizeof(CtoPeerPacket) && magic == CTO2_MAGIC && ver == CTO2_VERSION){
    CtoPeerPacket p; memcpy(&p, it.raw, sizeof(p));
    id = p.senderId; seq = p.sequence;
    s = findSource(id);
    if(!s){ rxForeign++; return; }
    // Relay FIRST: every millisecond spent on statistics or MQTT is added to
    // the relayed frame's age, and age is the hazard being bounded.
    const char* fwd = forwardDecide(*s, it, seq, false, now);
    s->hallMm = p.hallMm; s->mapDir = p.mapDir;
    s->autoMode = p.autoMode; s->running = p.running;

    memcpy(s->mac, it.mac, 6);
    if(s->rxCount == 0){
      s->firstSeq = seq; s->firstRxMs = it.rxMs; s->lastRxMs = it.rxMs; s->missRun = 0;
    } else {
      uint32_t gap = it.rxMs - s->lastRxMs;
      if(gap > s->maxGapMs) s->maxGapMs = gap;
      // Miss run from SEQUENCE arithmetic, not from elapsed time: a sender that
      // was itself stopped or rebooted must not be counted as radio loss. A
      // sequence that goes backwards means the sender restarted; re-baseline
      // rather than record a spurious multi-thousand-packet loss.
      if(seq > s->lastSeq){
        uint32_t step = seq - s->lastSeq;
        uint32_t missed = (step > 1) ? (step - 1) : 0;
        if(missed > 65535) missed = 65535;
        s->missRun = (uint16_t)missed;
        if(s->missRun > s->missRunMax) s->missRunMax = s->missRun;
        s->missTotal += missed;
      } else {
        Serial.printf("NOTE,%lu,sender %lu sequence reset %lu -> %lu; re-baselining\n",
                      (unsigned long)it.rxMs, (unsigned long)id,
                      (unsigned long)s->lastSeq, (unsigned long)seq);
        s->firstSeq = seq; s->rxCount = 0; s->missTotal = 0; s->missRun = 0;
        s->haveFwd = false; s->lastFwdSeq = 0;   // novelty gate re-baselines too
      }
    }
    if(s->stale){
      s->stale = false;
      publishEvent("RECOVERED", *s, it.rxMs - s->staleOnsetMs);
    }
    s->lastSeq = seq; s->lastRxMs = it.rxMs; s->rxCount++;
    s->rssiLast = it.rssi;
    if(it.rssi < s->rssiMin) s->rssiMin = it.rssi;
    if(it.rssi > s->rssiMax) s->rssiMax = it.rssi;
    s->rssiSum += it.rssi; s->rssiN++;
    s->win[s->winHead] = it.rxMs; s->winSeq[s->winHead] = seq;
    s->winHead = (s->winHead + 1) % WINDOW_SLOTS;
    if(s->winFill < WINDOW_SLOTS) s->winFill++;

    publishFrame(*s, it, seq, false, fwd, now - it.rxMs);
    Serial.printf("PKT,%lu,%lu,%lu,%d,%u,%u,%u,%s\n",
                  (unsigned long)it.rxMs, (unsigned long)id, (unsigned long)seq,
                  (int)it.rssi, (unsigned)it.len, (unsigned)s->hallMm,
                  (unsigned)s->missRun, fwd);
    return;
  }

  if(it.len == (int)sizeof(Cto3RoleEcho) && magic == CTO3_ECHO_MAGIC && ver == CTO3_ECHO_VERSION){
    Cto3RoleEcho e; memcpy(&e, it.raw, sizeof(e));
    id = e.senderId;
    s = findSource(id);
    if(!s){ rxForeign++; return; }
    const char* fwd = forwardDecide(*s, it, 0, true, now);
    s->echoCount++;
    memcpy(s->mac, it.mac, 6);
    // The echo is 1 Hz and carries no sequence, so it must NOT touch the gap
    // or delivery statistics — those describe the 2 Hz status stream only. It
    // is recorded because a node that hears echoes but not status frames is a
    // finding in itself.
    publishFrame(*s, it, 0, true, fwd, now - it.rxMs);
    return;
  }

  rxForeign++;   // not ours (the IR sender's frames are 72 bytes and land here)
}

// ---- MQTT ------------------------------------------------------------------
static void publishMode(){
  if(!mqtt.connected()) return;
  char p[224];
  snprintf(p, sizeof(p),
    "{\"site\":\"%s\",\"node\":\"%s\",\"mode\":\"%s\",\"maxAgeMs\":%lu,\"ch\":%u}",
    SURVEY_SITE, NODE_NAME, modeName(repMode),
    (unsigned long)FORWARD_MAX_AGE_MS, (unsigned)radioChannel);
  mqtt.publish(tMode, p, true);
}

static void onCmd(char* topic, byte* payload, unsigned int len){
  char buf[40]; unsigned n = len < sizeof(buf)-1 ? len : sizeof(buf)-1;
  memcpy(buf, payload, n); buf[n] = '\0';
  if(!strncmp(buf,"site ",5) && buf[5]){
    char old[24]; snprintf(old,sizeof(old),"%s",SURVEY_SITE);
    snprintf(SURVEY_SITE,sizeof(SURVEY_SITE),"%.23s",buf+5);   // truncate, never overflow
    Serial.printf("NOTE,%lu,site %s -> %s\n",(unsigned long)millis(),old,SURVEY_SITE);
    // The move itself is evidence: samples either side of it are from
    // different places and must never be pooled.
    if(mqtt.connected()){
      char p[224];
      snprintf(p,sizeof(p),"{\"event\":\"SITE_CHANGED\",\"from\":\"%s\",\"to\":\"%s\",\"rxMs\":%lu}",
               old,SURVEY_SITE,(unsigned long)millis());
      mqtt.publish(tEvent,p,false);
    }
    publishMode();
    return;
  }
  RepMode was = repMode;
  if(!strcmp(buf,"listen"))      repMode = MODE_LISTEN;
  else if(!strcmp(buf,"shadow")) repMode = MODE_SHADOW;
  else if(!strcmp(buf,"repeat")) repMode = MODE_REPEAT;
  else { Serial.printf("NOTE,%lu,unknown cmd \"%s\" (listen|shadow|repeat|site <name>)\n",
                       (unsigned long)millis(),buf); return; }
  Serial.printf("NOTE,%lu,mode %s -> %s\n",
                (unsigned long)millis(), modeName(was), modeName(repMode));
  publishMode();
}

static void setupTopics(){
  snprintf(tOnline, sizeof(tOnline), "ngr/survey/%s/online",  NODE_NAME);
  snprintf(tRx,     sizeof(tRx),     "ngr/survey/%s/rx",      NODE_NAME);
  snprintf(tPeer,   sizeof(tPeer),   "ngr/survey/%s/peer",    NODE_NAME);
  snprintf(tEvent,  sizeof(tEvent),  "ngr/survey/%s/event",   NODE_NAME);
  snprintf(tHealth, sizeof(tHealth), "ngr/survey/%s/health",  NODE_NAME);
  snprintf(tMode,   sizeof(tMode),   "ngr/survey/%s/mode",    NODE_NAME);
  snprintf(tCmd,    sizeof(tCmd),    "ngr/survey/%s/cmd",     NODE_NAME);
}

static void mqttEnsure(){
  if(mqtt.connected()) return;
  uint32_t now = millis();
  if(now - lastMqttTry < 3000) return;
  lastMqttTry = now;
  if(mqtt.connect(NODE_NAME, tOnline, 0, true, "0")){
    mqtt.publish(tOnline, "1", true);
    mqtt.subscribe(tCmd);
    publishMode();
  }
}

void setup(){
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.printf("[REP] ESPNOW_REPEATER 1.0 — node=%s site=%s boot mode=%s\n",
                NODE_NAME, SURVEY_SITE, modeName(repMode));
  Serial.println("[REP] relay gates: fresh<=100ms, strictly-newer sequence only, "
                 "<=24 tx/s. No authority, no commands to locomotives.");
  Serial.println("PKT,rxMs,src,seq,rssi,len,mm,missRun,fwd");

  rxQueue = xQueueCreate(48, sizeof(RxItem));
  setupTopics();

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);       // modem sleep would drop frames between beacons
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while(WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) delay(250);
  if(WiFi.status() == WL_CONNECTED)
    Serial.printf("[REP] wifi up %s\n", WiFi.localIP().toString().c_str());
  else
    Serial.println("[REP] wifi NOT up — ESP-NOW still runs, but on whatever "
                   "channel the radio idles on. Treat silence as unproven.");

  configTime(0, 0, MQTT_BROKER, "pool.ntp.org");

  int c = WiFi.channel();
  radioChannel = (c > 0 && c <= 14) ? (uint8_t)c : 0;
  if(esp_now_init() != ESP_OK){
    Serial.println("[REP] esp_now init FAILED — nothing recorded, nothing relayed");
  } else {
    esp_now_register_recv_cb(onEspNowRecv);
    esp_now_register_send_cb(onEspNowSend);
    esp_now_peer_info_t pi = {};
    memcpy(pi.peer_addr, CTO_BCAST, 6);
    pi.channel = 0; pi.encrypt = false;   // 0 = current station channel
    if(!esp_now_is_peer_exist(CTO_BCAST)) esp_now_add_peer(&pi);
    radioUp = true;
    Serial.printf("[REP] esp-now up, ch=%u (expected %u)\n",
                  (unsigned)radioChannel, (unsigned)EXPECTED_CHANNEL);
  }
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setBufferSize(512);
  mqtt.setCallback(onCmd);
}

void loop(){
  mqttEnsure();
  mqtt.loop();

  RxItem it;
  while(rxQueue && xQueueReceive(rxQueue, &it, 0) == pdTRUE) handleFrame(it);

  uint32_t now = millis();

  // Channel watch. A node on the wrong channel hears nothing and relays
  // nothing, and is indistinguishable from a perfect radio shadow — say so
  // loudly rather than let silence be read as coverage evidence.
  int c = WiFi.channel();
  uint8_t nowCh = (c > 0 && c <= 14) ? (uint8_t)c : 0;
  if(nowCh != radioChannel){
    Serial.printf("NOTE,%lu,channel changed %u -> %u\n",
                  (unsigned long)now, (unsigned)radioChannel, (unsigned)nowCh);
    radioChannel = nowCh; channelWarned = false;
  }
  if(radioChannel != EXPECTED_CHANNEL && !channelWarned){
    channelWarned = true;
    Serial.printf("WARN,%lu,on ch=%u but locomotives are on ch=%u — this node "
                  "cannot hear or relay them; silence here is NOT evidence\n",
                  (unsigned long)now, (unsigned)radioChannel, (unsigned)EXPECTED_CHANNEL);
    if(mqtt.connected()){
      char p[208];
      snprintf(p, sizeof(p),
        "{\"site\":\"%s\",\"event\":\"CHANNEL_MISMATCH\",\"ch\":%u,\"expected\":%u}",
        SURVEY_SITE, (unsigned)radioChannel, (unsigned)EXPECTED_CHANNEL);
      mqtt.publish(tEvent, p, false);
    }
  }

  if(!timeValid && time(nullptr) > 1600000000) timeValid = true;

  // Stale onset detection, at the locomotives' own threshold.
  for(uint8_t i=0;i<MAX_SOURCES;i++){
    Source& s = sources[i];
    if(!s.used || s.rxCount == 0) continue;
    uint32_t gap = now - s.lastRxMs;
    if(gap > s.maxGapMs) s.maxGapMs = gap;
    if(!s.stale && gap >= STALE_MS){
      s.stale = true; s.staleOnsetMs = now; s.staleEpisodes++;
      publishEvent("STALE_ONSET", s, gap);
    }
  }

  if(now - lastSummary >= SUMMARY_MS){
    lastSummary = now;
    for(uint8_t i=0;i<MAX_SOURCES;i++) if(sources[i].used) publishSummary(sources[i], now);
  }

  if(now - lastHealth >= 5000){
    lastHealth = now;
    uint8_t seen = 0;
    for(uint8_t i=0;i<MAX_SOURCES;i++) if(sources[i].used) seen++;
    char p[352];
    snprintf(p, sizeof(p),
      "{\"site\":\"%s\",\"node\":\"%s\",\"mode\":\"%s\",\"upMs\":%lu,\"ch\":%u,"
      "\"sources\":%u,\"apRssi\":%d,\"foreign\":%lu,\"dropped\":%lu,"
      "\"txDone\":%lu,\"txFailed\":%lu,\"heap\":%lu,\"timeValid\":%u}",
      SURVEY_SITE, NODE_NAME, modeName(repMode), (unsigned long)now,
      (unsigned)radioChannel, (unsigned)seen, (int)WiFi.RSSI(),
      (unsigned long)rxForeign, (unsigned long)rxDropped,
      (unsigned long)txDone, (unsigned long)txFailed,
      (unsigned long)ESP.getFreeHeap(), (unsigned)(timeValid?1:0));
    if(mqtt.connected()) mqtt.publish(tHealth, p, false);
    Serial.printf("HLT,%lu,%s,ch=%u,src=%u,ap=%d,foreign=%lu,drop=%lu,tx=%lu/%lu\n",
                  (unsigned long)now, modeName(repMode), (unsigned)radioChannel,
                  (unsigned)seen, (int)WiFi.RSSI(), (unsigned long)rxForeign,
                  (unsigned long)rxDropped, (unsigned long)txDone, (unsigned long)txFailed);
  }
  delay(2);
}
