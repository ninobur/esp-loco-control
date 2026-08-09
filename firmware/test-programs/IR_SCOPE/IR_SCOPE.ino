/*
 * ============================================================================
 * IR_SCOPE  —  NGR IR wheel-sensor virtual oscilloscope  (data car, ESP32)
 * ============================================================================
 * Companion to IR_DIAG. That instrument reports EVENTS (one line per pulse);
 * this one streams the RAW 1 kHz WAVEFORM together with the exact thresholds
 * and detector state that interpreted it, so a merged pulse can be seen
 * happening instead of inferred from doubled widths afterwards.
 *
 * THE QUESTION THIS ANSWERS (2026-08-09): event telemetry shows a repeating
 * mixture of normal and ~doubled pulse widths/intervals on the seven-spoke
 * finescale wheel. Hypothesis: the analog signal enters a spoke normally but
 * the inter-spoke trough does not cross thrLow, so the detector never rearms
 * and swallows the next spoke. Only the raw waveform beside the live
 * thresholds can confirm or refute that.
 *
 * THIS RUNS ON THE DATA CAR. No throttle, no navigation, no commands to any
 * locomotive. The only subscriptions are its own marker/control topics and
 * nothing in this file can drive anything.
 *
 * ---------------------------------------------------------------------------
 * DETECTOR — IR_DIAG's, UNCHANGED, ON THE SAME SAMPLES IT STREAMS
 * ---------------------------------------------------------------------------
 * Every constant and expression of the detection path is copied verbatim from
 * IR_DIAG.ino (which per decision 0009 matches production IR_TEST thresholds):
 *
 *   rolling 5th/95th-percentile envelope, 2048-sample window, 256-bucket
 *     histogram, recomputed every 250 ms, primed at 512 samples
 *   thrHigh = runMin + 2*span/3        thrLow = runMin + span/3
 *   MIN_USABLE_SPAN 120 / MARGINAL_SPAN 300 contrast gate
 *   DEBOUNCE_US 15000 (rise-to-rise), LATCH_TIMEOUT_MS 2500 discard
 *   contrast-loss discard of open pulse + interval anchor
 *
 * ONE analogRead() per 1 kHz tick feeds BOTH the stream and the detector.
 * (HallProbe read the ADC separately for raw and avg; that is exactly what a
 * scope must not do — the plotted value would not be the judged value.)
 *
 * The ONE deliberate timing difference from IR_DIAG: the sample tick uses
 * vTaskDelayUntil (fixed-period, drift-free) instead of vTaskDelay, because a
 * scope needs a deterministic sample clock. Per-sample lateness against the
 * nominal clock is measured and published; a stall shows up as late_n /
 * late_us, never as silently stretched data.
 *
 * ---------------------------------------------------------------------------
 * ARCHITECTURE — inherited from IR_DIAG/IR_TEST, unchanged
 * ---------------------------------------------------------------------------
 *   sensorTask   1 kHz, core 0, priority 2. Samples, runs the detector,
 *                packs per-sample records into batches, enqueues full
 *                batches. Never blocks on anything.
 *   batchQueue   25 batches of 200 samples (~5 s). An MQTT outage costs at
 *                most the queue depth, and every dropped batch is COUNTED
 *                (bdrop) and visible as a batch-seq gap. Nothing is joined
 *                silently.
 *   networkTask  owns WiFi/MQTT alone, core 0, priority 1. Peek-publish-
 *                remove with a per-pass cap.
 *   loop()       button marker + 5 s status beat only.
 *
 * ---------------------------------------------------------------------------
 * SAMPLE STREAM FORMAT  (topic ngr/diag/irscope01/samples, ~5 msgs/s)
 * ---------------------------------------------------------------------------
 * One JSON object per 200-sample batch:
 *
 *   {"sid":"a1b2c3d4",          boot session id — never join across sids
 *    "seq":123,                 batch sequence, gap = dropped batch(es)
 *    "first":24600,             sample number of hex[0]; sample N is
 *                               exactly N milliseconds after capture start
 *    "n":200,
 *    "env":[runMin,runMax,thrHigh,thrLow],   in force at sample `first`
 *    "envu":[[off,mn,mx,hi,lo],...],         mid-batch envelope recomputes:
 *                               new values apply from sample first+off
 *    "late_us":40,              worst lateness in this batch vs nominal clock
 *    "late_n":0,"envx":0,       cumulative late samples / env-note overflows
 *    "bdrop":0,                 cumulative batches dropped at the queue
 *    "latch":0,"closs":0,       cumulative latch / contrast-loss discards
 *    "sat":0,"pulses":42,       cumulative saturated samples / rises
 *    "hex":"..."}               n x 4 hex chars, one uint16 per sample:
 *                                 bits 0-11  raw ADC value
 *                                 bit 12     inPulse (state AFTER this sample)
 *                                 bit 13     rise edge fired at this sample
 *                                 bit 14     fall edge fired at this sample
 *                                 bit 15     contrast-valid (primed && span ok)
 *
 * A latch-timeout or contrast-loss discard clears inPulse WITHOUT a fall
 * flag — that is the honest record (no fall edge was emitted) and the latch/
 * closs counters say why.
 *
 * OTHER TOPICS (all under ngr/diag/irscope01/)
 *   status       LWT retained {"online":0}; retained {"online":1,...} on
 *                connect; 5 s JSON health beat (includes wheel metadata:
 *                seven spokes, 27.8 mm — metadata only, the detector never
 *                uses it, and NO speed is computed: speed stays untrusted
 *                until a hand-turn shows exactly 7 pulses per revolution)
 *   marker       {"sid":..,"sample":N,"text":".."} on BOOT press or control
 *   marker/set   subscribe: replaces the marker text
 *   control      subscribe: "marker" = publish marker, "status" = beat now
 *
 * SETUP: copy firmware/config/credentials_template.h beside this sketch as
 * credentials.h (git-ignored), flash, run IR_SCOPE_Plotter.py on the laptop.
 * ============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "credentials.h"     // git-ignored; copy firmware/config/credentials_template.h

#define SKETCH_NAME "IR_SCOPE_1_0"

const char* NODE_NAME   = "irscope01";
const char* MQTT_BROKER = "192.168.68.142";
const int   MQTT_PORT   = 1883;

// ===========================================================================
// HARDWARE / DETECTOR TUNING — verbatim from IR_DIAG.ino. Do not tune here:
// a scope that judges with different rules from the instrument it is
// diagnosing produces unattributable pictures (decision 0009).
// ===========================================================================
const int   SENSOR_PIN = 34;             // IR sensor. 33 is the Hall sensor.

// Wheel METADATA for the status beat only. The detector never reads these,
// and this sketch computes no speed at all: per the standing rule, no speed
// figure is trusted until a hand-turn shows exactly seven pulses/revolution.
const int   SPOKES_PER_WHEEL       = 7;
const float WHEEL_DIAMETER_MM      = 27.8f;
const float WHEEL_CIRCUMFERENCE_MM = 87.34f;   // nominal pi * 27.8

const uint32_t SENSOR_TICK_MS = 1;       // 1 kHz
const uint32_t DEBOUNCE_US    = 15000;   // 15 ms rise-to-rise, as IR_DIAG

const int      ADC_MAX          = 4095;
const int      SATURATION_LEVEL = 4000;
const int      MIN_USABLE_SPAN  = 120;
const int      MARGINAL_SPAN    = 300;

const int      ENV_WIN_N     = 2048;     // ~2 s @ 1 kHz
const int      ENV_PRIME_N   = 512;
const uint32_t ENV_UPDATE_MS = 250;
const int      ENV_PCT_LO    = 5;
const int      ENV_PCT_HI    = 95;

const uint32_t LATCH_TIMEOUT_MS = 2500;

// ===========================================================================
// BATCHING
// ===========================================================================
const int      BATCH_N          = 200;   // 200 ms per batch, 5 batches/s
const int      MAX_ENV_UPDATES  = 4;     // recompute is 250 ms; >1 per batch
                                         // needs a stall, 4 is generous
const int      BATCH_QUEUE_N    = 25;    // ~5 s of outage tolerance
const uint32_t STATUS_PERIOD_MS = 5000;
const uint32_t MQTT_RETRY_MS    = 5000;
const uint8_t  BATCH_DRAIN_CAP  = 4;     // batches per network pass
const uint8_t  PUB_DRAIN_CAP    = 4;     // status/marker msgs per pass

// Per-sample flag bits packed above the 12-bit raw value.
const uint16_t FLAG_INPULSE  = 0x1000;
const uint16_t FLAG_RISE     = 0x2000;
const uint16_t FLAG_FALL     = 0x4000;
const uint16_t FLAG_CONTRAST = 0x8000;

// ===========================================================================
// TYPES — must precede every function (Arduino IDE prototype hoisting).
// ===========================================================================
struct EnvUpdate {
  int16_t at;                    // batch offset from which new values apply
  int16_t runMin, runMax, thrHigh, thrLow;
};

struct SampleBatch {
  uint32_t  seq;                 // batch sequence number since boot
  uint32_t  firstSample;         // sample number of s[0]
  uint16_t  count;
  int16_t   runMin0, runMax0, thrHigh0, thrLow0;   // in force at s[0]
  uint8_t   nEnv;
  EnvUpdate env[MAX_ENV_UPDATES];
  uint32_t  maxLateUs;           // worst sample lateness in this batch
  uint16_t  s[BATCH_N];          // flags | raw
};

struct PubMsg { char topic[64]; char payload[512]; bool retain; };

// ===========================================================================
// SHARED STATE
// ===========================================================================
static QueueHandle_t batchQueue = nullptr;   // sensorTask -> networkTask
static QueueHandle_t pubQueue   = nullptr;   // loop()     -> networkTask

static char T_STATUS[64], T_SAMPLES[64], T_MARKER[64], T_MARKER_SET[64], T_CONTROL[64];

static WiFiClient   espClient;
static PubSubClient mqtt(espClient);

static char sessionId[12] = "--------";      // 8 hex chars from esp_random()

// Counters/mirrors written by sensorTask, read elsewhere. Single writer,
// aligned 32-bit, diagnostic only — volatile is sufficient (as IR_DIAG).
static volatile uint32_t sampleCount    = 0;
static volatile uint32_t pulseCount     = 0;
static volatile uint32_t latchTimeouts  = 0;
static volatile uint32_t contrastLosses = 0;
static volatile uint32_t droppedBatches = 0;
static volatile uint32_t lateSamples    = 0;  // samples > 1 tick late
static volatile uint32_t maxLateUsEver  = 0;
static volatile uint32_t satSamples     = 0;
static volatile uint32_t envNoteOverflows = 0;
static volatile uint32_t batchesBuilt   = 0;
static volatile int      lastRaw = 0, vRunMin = 0, vRunMax = 0;
static volatile int      vThrHigh = 0, vThrLow = 0, vSpan = 0;
static volatile uint8_t  vContrastValid = 0, vInPulse = 0;

// Marker text — replace via ngr/diag/irscope01/marker/set
static char markerText[128] = "marker / position / direction / illumination";
static volatile uint8_t markerRequested = 0;   // set by control, served by loop()
static volatile uint8_t statusRequested = 0;

// ===========================================================================
// SENSOR TASK — 1 kHz, core 0, priority 2.
// Detection path is IR_DIAG's sensorTask verbatim minus the PulseEvent
// bookkeeping that only fed the text lines (peak/headroom/gap-trough); every
// variable that AFFECTS an inPulse transition is kept identically.
// ===========================================================================
static int  runMin = 0, runMax = 0;
static bool envelopePrimed = false;

static uint8_t  envWin[ENV_WIN_N];
static uint16_t envHist[256];
static int      envWinIdx = 0, envWinFilled = 0;

// Batch under construction — sensorTask-private.
static SampleBatch cur;
static bool        batchOpen = false;
static uint32_t    nextBatchSeq = 0;

static uint8_t qualityFromSpan(int span) {
  if (span < MIN_USABLE_SPAN) return 0;
  if (span < MARGINAL_SPAN)   return 1;
  return 2;
}

static inline void openBatch() {
  cur.seq         = nextBatchSeq;
  cur.firstSample = sampleCount;
  cur.count       = 0;
  cur.nEnv        = 0;
  cur.maxLateUs   = 0;
  int span   = runMax - runMin;
  cur.runMin0  = (int16_t)runMin;
  cur.runMax0  = (int16_t)runMax;
  cur.thrHigh0 = (int16_t)(runMin + (span * 2) / 3);
  cur.thrLow0  = (int16_t)(runMin + span / 3);
  batchOpen = true;
}

// Called by the envelope recompute when a batch is mid-fill: the new bounds
// apply from the sample about to be recorded (offset = cur.count).
static inline void noteEnvUpdate() {
  if (!batchOpen || cur.count == 0) return;   // openBatch() captures fresh values
  if (cur.nEnv >= MAX_ENV_UPDATES) { envNoteOverflows = envNoteOverflows + 1; return; }
  int span = runMax - runMin;
  EnvUpdate& u = cur.env[cur.nEnv++];
  u.at      = (int16_t)cur.count;
  u.runMin  = (int16_t)runMin;
  u.runMax  = (int16_t)runMax;
  u.thrHigh = (int16_t)(runMin + (span * 2) / 3);
  u.thrLow  = (int16_t)(runMin + span / 3);
}

static inline void recordSample(int raw, uint16_t flags, uint32_t lateUs) {
  if (!batchOpen) openBatch();
  cur.s[cur.count++] = (uint16_t)((raw & 0x0FFF) | flags);
  if (lateUs > cur.maxLateUs) cur.maxLateUs = lateUs;
  sampleCount = sampleCount + 1;
  if (cur.count >= BATCH_N) {
    if (batchQueue && xQueueSend(batchQueue, &cur, 0) != pdTRUE)
      droppedBatches = droppedBatches + 1;   // counted, never silent
    batchesBuilt = batchesBuilt + 1;
    nextBatchSeq++;                          // seq advances even on a drop,
    batchOpen = false;                       //   so the gap is visible
  }
}

static void sensorTask(void*) {
  bool     inPulse = false;
  uint32_t lastEdgeMicros = 0, pulseStartMs = 0;
  uint32_t prevRiseMs = 0, lastEnvUpdateMs = millis();

  TickType_t lastWake = xTaskGetTickCount();
  uint64_t   t0Us = 0;               // nominal clock anchor, set at sample 0

  for (;;) {
    int raw = analogRead(SENSOR_PIN);      // ONE acquisition per sample —
    uint32_t now = millis();               //   stream and detector share it
    lastRaw = raw;

    // --- sample-clock lateness against the nominal 1 kHz grid -------------
    uint64_t nowUs = esp_timer_get_time();
    if (sampleCount == 0) t0Us = nowUs;
    int64_t lateSigned = (int64_t)(nowUs - t0Us) - (int64_t)sampleCount * 1000;
    uint32_t lateUs = (lateSigned > 0) ? (uint32_t)lateSigned : 0;
    if (lateUs > 1000) lateSamples = lateSamples + 1;
    if (lateUs > maxLateUsEver) maxLateUsEver = lateUs;

    if (raw >= SATURATION_LEVEL) satSamples = satSamples + 1;

    // --- envelope window: sample in, oldest out, histogram in step --------
    uint8_t bucket = (uint8_t)(raw >> 4);
    if (envWinFilled == ENV_WIN_N) envHist[envWin[envWinIdx]]--;
    envWin[envWinIdx] = bucket;
    envHist[bucket]++;
    envWinIdx = (envWinIdx + 1) % ENV_WIN_N;
    if (envWinFilled < ENV_WIN_N) envWinFilled++;

    // --- percentile bounds, recomputed every ENV_UPDATE_MS ----------------
    if (now - lastEnvUpdateMs >= ENV_UPDATE_MS && envWinFilled >= 64) {
      lastEnvUpdateMs = now;
      int total   = envWinFilled;
      int needLo  = (total * ENV_PCT_LO)  / 100;
      int needHi  = (total * ENV_PCT_HI) / 100;
      int cum = 0, pLo = -1, pHi = -1;
      for (int b = 0; b < 256; b++) {
        int c = envHist[b];
        if (c == 0) continue;
        cum += c;
        if (pLo < 0 && cum > needLo)  pLo = b;
        if (pHi < 0 && cum >= needHi) pHi = b;
      }
      if (pLo >= 0 && pHi >= 0) {
        runMin = pLo * 16;                 // bucket floor
        runMax = pHi * 16 + 15;            // bucket ceiling
        noteEnvUpdate();                   // stream reconstruction stays exact
      }
      envelopePrimed = (envWinFilled >= ENV_PRIME_N);
    }

    int span = runMax - runMin;
    vRunMin = runMin; vRunMax = runMax; vSpan = span;
    uint8_t quality = qualityFromSpan(span);

    // Contrast gate — IR_DIAG's discard branch, verbatim: no edges before
    // the window is primed, none from a collapsed span, and the open pulse
    // plus the interval anchor die here so nothing spans a blind period.
    if (!envelopePrimed || quality == 0) {
      if (inPulse || prevRiseMs != 0) {
        contrastLosses = contrastLosses + 1;
        inPulse = false;
        prevRiseMs = 0;
      }
      vContrastValid = 0; vInPulse = 0;
      recordSample(raw, 0, lateUs);        // contrast bit clear; no edges
      vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(SENSOR_TICK_MS));
      continue;
    }

    int thrHigh = runMin + (span * 2) / 3;
    int thrLow  = runMin + span / 3;
    vThrHigh = thrHigh; vThrLow = thrLow;
    uint32_t nowMicros = micros();

    uint16_t flags = FLAG_CONTRAST;

    // --- latch timeout: inPulse cannot stay true forever (IR_DIAG) --------
    // Discarded, not completed: inPulse clears WITHOUT a fall flag, and the
    // latch counter says why.
    if (inPulse && (uint32_t)(now - pulseStartMs) > LATCH_TIMEOUT_MS) {
      inPulse = false;
      prevRiseMs = 0;
      latchTimeouts = latchTimeouts + 1;
    }

    if (!inPulse) {
      if (raw > thrHigh && (nowMicros - lastEdgeMicros) > DEBOUNCE_US) {
        inPulse = true;
        lastEdgeMicros = nowMicros;
        pulseStartMs = now;
        flags |= FLAG_RISE;
        pulseCount = pulseCount + 1;
      }
    } else {
      if (raw < thrLow) {
        inPulse = false;
        flags |= FLAG_FALL;
        prevRiseMs = pulseStartMs;
      }
    }

    if (inPulse) flags |= FLAG_INPULSE;
    vContrastValid = 1; vInPulse = inPulse ? 1 : 0;
    recordSample(raw, flags, lateUs);

    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(SENSOR_TICK_MS));
  }
}

// ===========================================================================
// NETWORK TASK — the only place mqtt.* is called.
// ===========================================================================
static uint32_t nextMqttTryMs = 0;
static char     jsonBuf[1600];           // 200*4 hex + header, networkTask-only

static void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, T_MARKER_SET) == 0 && length > 0 && length < sizeof(markerText)) {
    memcpy(markerText, payload, length);
    markerText[length] = '\0';
    Serial.printf("[SCOPE] marker text: %s\n", markerText);
  } else if (strcmp(topic, T_CONTROL) == 0) {
    if (length == 6 && memcmp(payload, "marker", 6) == 0) markerRequested = 1;
    if (length == 6 && memcmp(payload, "status", 6) == 0) statusRequested = 1;
  }
}

static void attemptReconnect() {
  uint32_t now = millis();
  if (now < nextMqttTryMs) return;
  nextMqttTryMs = now + MQTT_RETRY_MS;      // fixed backoff, never every pass
  if (WiFi.status() != WL_CONNECTED) return;
  String cid = String("ngr-irscope-") + NODE_NAME;
  if (mqtt.connect(cid.c_str(), T_STATUS, 0, true, "{\"online\":0}")) {
    char b[160];
    snprintf(b, sizeof(b), "{\"online\":1,\"fw\":\"%s\",\"sid\":\"%s\",\"rate_hz\":1000}",
             SKETCH_NAME, sessionId);
    mqtt.publish(T_STATUS, b, true);
    mqtt.subscribe(T_MARKER_SET);
    mqtt.subscribe(T_CONTROL);
    Serial.println("[NET] MQTT connected");
  }
}

static bool publishBatch(const SampleBatch& b) {
  int off = snprintf(jsonBuf, sizeof(jsonBuf),
    "{\"sid\":\"%s\",\"seq\":%lu,\"first\":%lu,\"n\":%u,"
    "\"env\":[%d,%d,%d,%d],",
    sessionId, (unsigned long)b.seq, (unsigned long)b.firstSample,
    (unsigned)b.count,
    (int)b.runMin0, (int)b.runMax0, (int)b.thrHigh0, (int)b.thrLow0);
  if (b.nEnv > 0) {
    off += snprintf(jsonBuf + off, sizeof(jsonBuf) - off, "\"envu\":[");
    for (int i = 0; i < b.nEnv; i++) {
      off += snprintf(jsonBuf + off, sizeof(jsonBuf) - off, "%s[%d,%d,%d,%d,%d]",
                      i ? "," : "",
                      (int)b.env[i].at, (int)b.env[i].runMin, (int)b.env[i].runMax,
                      (int)b.env[i].thrHigh, (int)b.env[i].thrLow);
    }
    off += snprintf(jsonBuf + off, sizeof(jsonBuf) - off, "],");
  }
  off += snprintf(jsonBuf + off, sizeof(jsonBuf) - off,
    "\"late_us\":%lu,\"late_n\":%lu,\"envx\":%lu,\"bdrop\":%lu,"
    "\"latch\":%lu,\"closs\":%lu,\"sat\":%lu,\"pulses\":%lu,\"hex\":\"",
    (unsigned long)b.maxLateUs, (unsigned long)lateSamples,
    (unsigned long)envNoteOverflows, (unsigned long)droppedBatches,
    (unsigned long)latchTimeouts, (unsigned long)contrastLosses,
    (unsigned long)satSamples, (unsigned long)pulseCount);
  if (off < 0 || off + b.count * 4 + 3 >= (int)sizeof(jsonBuf)) return false;
  static const char* HEXC = "0123456789abcdef";
  for (int i = 0; i < b.count; i++) {
    uint16_t v = b.s[i];
    jsonBuf[off++] = HEXC[(v >> 12) & 0xF];
    jsonBuf[off++] = HEXC[(v >> 8)  & 0xF];
    jsonBuf[off++] = HEXC[(v >> 4)  & 0xF];
    jsonBuf[off++] = HEXC[v & 0xF];
  }
  jsonBuf[off++] = '"';
  jsonBuf[off++] = '}';
  jsonBuf[off]   = '\0';
  return mqtt.publish(T_SAMPLES, jsonBuf, false);
}

static void networkTask(void*) {
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!mqtt.connected()) {
        attemptReconnect();
      } else {
        mqtt.loop();
        // PEEK-PUBLISH-REMOVE, capped: a batch leaves the queue only after
        // publish() reports success; the cap stops a reconnect flush from
        // stampeding the link that just failed.
        SampleBatch b; uint8_t n = 0;
        while (n < BATCH_DRAIN_CAP && xQueuePeek(batchQueue, &b, 0) == pdTRUE) {
          if (!publishBatch(b)) break;
          xQueueReceive(batchQueue, &b, 0);
          n++;
        }
        PubMsg m; n = 0;
        while (n < PUB_DRAIN_CAP && xQueuePeek(pubQueue, &m, 0) == pdTRUE) {
          if (!mqtt.publish(m.topic, m.payload, m.retain)) break;
          xQueueReceive(pubQueue, &m, 0);
          n++;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// ===========================================================================
// loop() HELPERS — enqueue only; nothing here touches mqtt directly.
// ===========================================================================
static void enqueuePub(const char* topic, const char* payload, bool retain) {
  if (!pubQueue) return;
  PubMsg m;
  strlcpy(m.topic, topic, sizeof(m.topic));
  strlcpy(m.payload, payload, sizeof(m.payload));
  m.retain = retain;
  if (xQueueSend(pubQueue, &m, 0) != pdTRUE) {
    PubMsg discard;
    xQueueReceive(pubQueue, &discard, 0);   // drop oldest; newest status wins
    xQueueSend(pubQueue, &m, 0);
  }
}

static void publishMarker() {
  char b[220];
  snprintf(b, sizeof(b), "{\"sid\":\"%s\",\"sample\":%lu,\"text\":\"%s\"}",
           sessionId, (unsigned long)sampleCount, markerText);
  enqueuePub(T_MARKER, b, false);
  Serial.printf("[SCOPE] MARKER @ sample %lu: %s\n",
                (unsigned long)sampleCount, markerText);
}

static void publishStatus() {
  char b[512];
  snprintf(b, sizeof(b),
    "{\"fw\":\"%s\",\"sid\":\"%s\",\"pin\":%d,\"rate_hz\":1000,"
    "\"spokes\":%d,\"wheel_dia_mm\":%.1f,\"wheel_circ_mm\":%.2f,"
    "\"samples\":%lu,\"batches\":%lu,\"bdrop\":%lu,"
    "\"late_n\":%lu,\"max_late_us\":%lu,\"envx\":%lu,"
    "\"raw\":%d,\"run_min\":%d,\"run_max\":%d,\"span\":%d,"
    "\"thr_high\":%d,\"thr_low\":%d,\"contrast_valid\":%u,\"in_pulse\":%u,"
    "\"pulses\":%lu,\"latch\":%lu,\"closs\":%lu,\"sat\":%lu,"
    "\"rssi\":%d,\"heap\":%lu}",
    SKETCH_NAME, sessionId, SENSOR_PIN,
    SPOKES_PER_WHEEL, WHEEL_DIAMETER_MM, WHEEL_CIRCUMFERENCE_MM,
    (unsigned long)sampleCount, (unsigned long)batchesBuilt,
    (unsigned long)droppedBatches,
    (unsigned long)lateSamples, (unsigned long)maxLateUsEver,
    (unsigned long)envNoteOverflows,
    (int)lastRaw, (int)vRunMin, (int)vRunMax, (int)vSpan,
    (int)vThrHigh, (int)vThrLow, (unsigned)vContrastValid, (unsigned)vInPulse,
    (unsigned long)pulseCount, (unsigned long)latchTimeouts,
    (unsigned long)contrastLosses, (unsigned long)satSamples,
    (int)WiFi.RSSI(), (unsigned long)ESP.getFreeHeap());
  enqueuePub(T_STATUS, b, false);
  Serial.printf("[SCOPE] %s\n", b);
}

// ===========================================================================
// BUTTON — GPIO 0 (BOOT), active LOW, debounced: publish a marker.
// ===========================================================================
const int MARKER_BUTTON_PIN = 0;
const uint32_t BUTTON_DEBOUNCE_MS = 50;
static bool lastButtonState = HIGH;
static uint32_t lastButtonChangeMs = 0;

static void serviceButton() {
  bool reading = digitalRead(MARKER_BUTTON_PIN);
  if (reading != lastButtonState) {
    uint32_t now = millis();
    if (now - lastButtonChangeMs > BUTTON_DEBOUNCE_MS) {
      if (reading == LOW) publishMarker();          // press, not release
      lastButtonChangeMs = now;
    }
    lastButtonState = reading;
  }
}

// ===========================================================================
void setup() {
  Serial.begin(115200);
  delay(400);
  analogReadResolution(12);
  pinMode(SENSOR_PIN, INPUT);
  pinMode(MARKER_BUTTON_PIN, INPUT_PULLUP);

  snprintf(sessionId, sizeof(sessionId), "%08lx", (unsigned long)esp_random());

  snprintf(T_STATUS,     sizeof(T_STATUS),     "ngr/diag/%s/status",     NODE_NAME);
  snprintf(T_SAMPLES,    sizeof(T_SAMPLES),    "ngr/diag/%s/samples",    NODE_NAME);
  snprintf(T_MARKER,     sizeof(T_MARKER),     "ngr/diag/%s/marker",     NODE_NAME);
  snprintf(T_MARKER_SET, sizeof(T_MARKER_SET), "ngr/diag/%s/marker/set", NODE_NAME);
  snprintf(T_CONTROL,    sizeof(T_CONTROL),    "ngr/diag/%s/control",    NODE_NAME);

  Serial.printf("=== %s  node=%s  sid=%s  pin=%d  1 kHz, %d-sample batches ===\n",
                SKETCH_NAME, NODE_NAME, sessionId, SENSOR_PIN, BATCH_N);

  batchQueue = xQueueCreate(BATCH_QUEUE_N, sizeof(SampleBatch));
  pubQueue   = xQueueCreate(8, sizeof(PubMsg));
  if (!batchQueue || !pubQueue) {
    Serial.println("[FATAL] queue alloc failed"); while (1) delay(1000);
  }

  // Sampling starts BEFORE the network, so nothing about bringing WiFi up
  // can cost a sample.
  if (xTaskCreatePinnedToCore(sensorTask, "sensorTask", 4096, nullptr, 2, nullptr, 0) != pdPASS) {
    Serial.println("[FATAL] sensor task creation failed"); while (1) delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.setSleep(false);            // modem sleep causes the latency spikes
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setSocketTimeout(2);        // PubSubClient's READ timeout, seconds
  mqtt.setBufferSize(2048);        // batch payloads are ~1.3 KB
  // The TCP connect bound. NOT setSocketTimeout, and NOT setTimeout (the
  // inherited Stream read timeout, in seconds — does nothing here).
  espClient.setConnectionTimeout(3000);   // milliseconds
  if (xTaskCreatePinnedToCore(networkTask, "net", 8192, nullptr, 1, nullptr, 0) != pdPASS) {
    Serial.println("[FATAL] network task creation failed"); while (1) delay(1000);
  }

  Serial.println("[BOOT] streaming 1 kHz sample batches to ngr/diag/irscope01/samples");
  Serial.println("[BOOT] BOOT button = marker; marker/set replaces the text");
}

// ===========================================================================
static uint32_t lastStatusMs = 0;

void loop() {
  serviceButton();

  if (markerRequested) { markerRequested = 0; publishMarker(); }

  uint32_t now = millis();
  if (statusRequested || now - lastStatusMs >= STATUS_PERIOD_MS) {
    statusRequested = 0;
    lastStatusMs = now;
    publishStatus();
  }
  delay(5);
}
