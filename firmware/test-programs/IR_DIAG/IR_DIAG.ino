/*
 * ============================================================================
 * IR_DIAG  —  NGR IR wheel-sensor diagnostic  (data car, ESP32)
 * ============================================================================
 * Companion to HALL_DIAG. That one diagnoses the marker sensor; this one
 * diagnoses the QRE1113 IR wheel sensor. Same idea: show what the sensor is
 * actually seeing, in plain text, pulse by pulse.
 *
 * THIS RUNS ON THE DATA CAR. It has no throttle, no navigation, no commands,
 * and no part in locomotive control. It only watches a wheel and reports.
 * There is deliberately no subscribe() and no command handler anywhere in
 * this file, so it is structurally incapable of driving anything.
 *
 * NO RSSI. The five-hour coverage survey is done and the radio is not the
 * question any more. Every RSSI topic, field and timer is gone.
 *
 * ---------------------------------------------------------------------------
 * OUTPUT — human-readable, identical on both pipes
 * ---------------------------------------------------------------------------
 * Every line goes to BOTH the USB serial console and MQTT, in the same
 * fixed-column text. Not JSON: this is meant to be read by a person on a
 * terminal while the wheel turns.
 *
 *   On the car:   idle | screen
 *   On the Pi:    mosquitto_sub -h 127.0.0.1 -t 'ngr/spoke/+/diag' -v
 *   To log it:    mosquitto_sub -h 127.0.0.1 -t 'ngr/spoke/+/diag' \
 *                   | tee -a ~/ir_diag_$(date +%Y%m%d_%H%M).log
 *
 * (ngr_runlog.py now subscribes to ngr/# rather than ngr/loco/+/#, so once it
 * is deployed on the Pi these lines are captured automatically as well.)
 *
 * Three kinds of line:
 *
 *   PULSE #   42  int=  187ms  w=  41ms  peak=+312  raw=2054 base=1742 span= 480  OK
 *   IDLE          raw= 1741  base= 1742  span=  478  thr=+160/-160  pulses=42
 *   STATS 10s  n=52 rate=5.2/s | int med=190 min=181 max=203 jit=6 | w med=42 |
 *              peak med=308 min=214 | sat=0 miss=0 | ~121mm/s
 *
 * PULSE is the event. IDLE proves the sensor is alive between events — the
 * single most useful line when nothing is happening, because "no output" and
 * "no pulses" look identical without it. STATS is the ten-second summary that
 * turns a stream into a verdict.
 *
 * ---------------------------------------------------------------------------
 * WHAT HEALTHY AND UNHEALTHY LOOK LIKE
 * ---------------------------------------------------------------------------
 *   healthy   span comfortably above MARGINAL_SPAN, peaks consistent, jit
 *             small relative to int, sat=0, miss=0, quality OK
 *   weak      span shrinking toward MIN_USABLE_SPAN — a dirty lens and a
 *             sunny afternoon both present as a shrinking gap
 *   blinded   sat>0 — raw is pinned near 4095 and the emitter's own light can
 *             no longer be distinguished. THIS is the real sunlight failure
 *   dead      IDLE lines continue but no PULSE lines while the wheel turns —
 *             optical path broken, tape gone, standoff wrong
 *   doubling  n roughly twice the expected count and int alternating short/
 *             long — edge doubling, the bare-spoke failure from the notes
 *   dropping  miss>0 — an interval beyond 1.8x the running median, i.e. a
 *             flag that went past unseen
 *
 * The v1 survey car's "lockouts" were none of these: pulse_count kept rising
 * and the ADC never passed 2511 of 4095. That was a blocked loop() publishing
 * a CPU-time measurement as a pulse width. This sketch cannot reproduce that
 * error — see the architecture note below.
 *
 * ---------------------------------------------------------------------------
 * ARCHITECTURE — inherited from Spoke_IR_RSSI_survey_v2, unchanged
 * ---------------------------------------------------------------------------
 *   sensorTask   1 kHz sampling, core 0, priority 2 (above the network).
 *                Adaptive threshold, debounced edges, BOTH edge timestamps
 *                taken here. Cannot be starved by loop() or by the radio, so
 *                a width can only ever measure light.
 *   eventQueue   256 timestamped pulses. A network outage costs nothing.
 *   loop()       drains events, formats text, emits to serial and MQTT.
 *   networkTask  owns WiFi/MQTT alone. Non-blocking connect, fixed backoff,
 *                peek-publish-remove capped at 8 per pass so a reconnect
 *                flush cannot stampede the link.
 *
 * PIN: 34 (ADC1_CH6) — the IR sensor. Pin 33 is the HALL sensor; do not
 * confuse them, it produces a convincing "dead sensor" reading.
 * ============================================================================
 */

// ---------------------------------------------------------------------------
// IR_DIAG_WIFI — build switch.
//   DEFINED (default): stream to MQTT *and* serial.
//   COMMENTED OUT: no radio compiled in at all — serial only, and nothing
//   exists that could stall capture. Use this on the bench first.
// ---------------------------------------------------------------------------
#define IR_DIAG_WIFI

#include <Arduino.h>
#ifdef IR_DIAG_WIFI
  #include <WiFi.h>
  #include <PubSubClient.h>
  #include "credentials.h"     // git-ignored; copy firmware/config/credentials_template.h
#endif

#define SKETCH_NAME "IR_DIAG_1_0"

const char* NODE_NAME = "IR_SPEED_SENSOR";
#ifdef IR_DIAG_WIFI
const char* MQTT_BROKER = "192.168.68.142";
const int   MQTT_PORT   = 1883;
#endif

// ===========================================================================
// TYPES — must precede every function. The Arduino IDE hoists generated
// prototypes above anything declared later in the file.
// ===========================================================================
struct PulseEvent {
  uint32_t seq;
  uint32_t detectedAtMs;   // millis() at the RISING edge
  uint32_t intervalMs;     // rise-to-rise (0 on the first pulse)
  uint32_t widthMs;        // rise-to-fall of THIS pulse
  int      peakDelta;      // signed peak excursion from baseline
  int      rawAtEdge;
  int      baseline;
  int      span;           // live contrast at the edge
  uint8_t  quality;        // 0=UNAVAILABLE 1=MARGINAL 2=OK
  uint8_t  saturated;      // raw hit the ADC ceiling during this pulse
};

#ifdef IR_DIAG_WIFI
struct PubMsg { char payload[224]; };
#endif

// ===========================================================================
// HARDWARE / TUNING
// ===========================================================================
const int   SENSOR_PIN = 34;             // IR sensor. 33 is the Hall sensor.
const int   SPOKES_PER_WHEEL = 2;        // TWO tape flags, 180 deg apart
const float WHEEL_CIRCUMFERENCE_MM = 115.0f;   // MEASURE on the production car

const uint32_t SENSOR_TICK_MS = 1;       // 1 kHz
const uint32_t DEBOUNCE_US    = 15000;   // 15 ms — edge-doubling guard

const int      ADC_MAX          = 4095;
const int      SATURATION_LEVEL = 4000;  // above this the sensor is blinded
const int      MIN_USABLE_SPAN  = 120;   // below this, refuse to emit edges
const int      MARGINAL_SPAN    = 300;   // below this, flag the read
const uint32_t DECAY_INTERVAL_MS = 250;  // envelope relaxation cadence
const int      DECAY_STEP        = 2;

const uint32_t IDLE_PRINT_MS  = 2000;    // "sensor is alive" heartbeat
const uint32_t STATS_PRINT_MS = 10000;   // rolling summary
const uint32_t MQTT_RETRY_MS  = 5000;
const uint8_t  PUB_DRAIN_CAP  = 8;

// Navigation's empirical conversion, matched so IR speed is comparable with
// the locomotive's: NGR_LL_DNA_CTO2_r12:414 and SOLONAV 1.x both use
// pkph = mm/s / 5.37325. Not a geometric G factor; a house unit (~1:51.7).
const float PKPH_MM_PER_SEC = 5.37325f;

// ===========================================================================
// SHARED STATE
// ===========================================================================
static QueueHandle_t eventQueue = nullptr;
#ifdef IR_DIAG_WIFI
static QueueHandle_t pubQueue = nullptr;
static char T_ONLINE[64], T_DIAG[64];
static WiFiClient   espClient;
static PubSubClient mqtt(espClient);
#endif

static volatile uint32_t pulseCount = 0;
static volatile uint32_t eventDrops = 0;
static volatile int      lastRaw = 0;
static volatile int      lastBaseline = 0;
static volatile int      lastSpan = 0;
static volatile uint32_t satSamples = 0;   // ADC ceiling hits since boot

// ---------------------------------------------------------------------------
// emit() — ONE line, to BOTH pipes, identically. Serial always; MQTT when a
// radio is compiled in. Never blocks: the MQTT side only enqueues.
// ---------------------------------------------------------------------------
static void emit(const char* line) {
  Serial.println(line);
#ifdef IR_DIAG_WIFI
  if (!pubQueue) return;
  PubMsg m;
  strlcpy(m.payload, line, sizeof(m.payload));
  if (xQueueSend(pubQueue, &m, 0) != pdTRUE) {
    PubMsg discard;
    xQueueReceive(pubQueue, &discard, 0);   // drop oldest; the stream is live
    xQueueSend(pubQueue, &m, 0);
  }
#endif
}

// ===========================================================================
// SENSOR TASK — 1 kHz, core 0, priority 2. Everything about a pulse is
// captured HERE, at the edge, and travels with the event.
// ===========================================================================
static int  runMin = ADC_MAX, runMax = 0;
static bool envelopePrimed = false;

static uint8_t qualityFromSpan(int span) {
  if (span < MIN_USABLE_SPAN) return 0;
  if (span < MARGINAL_SPAN)   return 1;
  return 2;
}

static void sensorTask(void*) {
  bool     inPulse = false;
  uint32_t lastEdgeMicros = 0, pulseStartMicros = 0, pulseStartMs = 0;
  uint32_t prevRiseMs = 0, lastDecayMs = millis();
  int      peakDelta = 0;
  bool     sawSaturation = false;

  for (;;) {
    int raw = analogRead(SENSOR_PIN);      // a TASK, not an ISR — safe
    uint32_t now = millis();
    lastRaw = raw;

    if (raw >= SATURATION_LEVEL) { satSamples++; sawSaturation = true; }

    // --- adaptive envelope: follows rising ambient and fading contrast ---
    if (!envelopePrimed) { runMin = runMax = raw; envelopePrimed = true; }
    if (raw < runMin) runMin = raw;
    if (raw > runMax) runMax = raw;
    if (now - lastDecayMs >= DECAY_INTERVAL_MS) {
      lastDecayMs = now;
      int mid = (runMin + runMax) / 2;
      if (runMin < mid - DECAY_STEP) runMin += DECAY_STEP;
      if (runMax > mid + DECAY_STEP) runMax -= DECAY_STEP;
    }
    int span     = runMax - runMin;
    int baseline = (runMin + runMax) / 2;
    lastSpan = span; lastBaseline = baseline;
    uint8_t quality = qualityFromSpan(span);

    // A collapsed envelope means peeling tape, a dirty lens, or no target.
    // Emitting edges out of noise at high rate is worse than saying nothing;
    // the IDLE line keeps reporting so the failure stays visible.
    if (quality == 0) { inPulse = false; vTaskDelay(pdMS_TO_TICKS(SENSOR_TICK_MS)); continue; }

    int thrHigh = baseline + span / 3;
    int thrLow  = baseline + span / 6;
    uint32_t nowMicros = micros();
    int delta = raw - baseline;

    if (!inPulse) {
      if (raw > thrHigh && (nowMicros - lastEdgeMicros) > DEBOUNCE_US) {
        inPulse = true;
        lastEdgeMicros = nowMicros;
        pulseStartMicros = nowMicros;
        pulseStartMs = now;
        peakDelta = delta;
        sawSaturation = (raw >= SATURATION_LEVEL);
        pulseCount++;
      }
    } else {
      if (abs(delta) > abs(peakDelta)) peakDelta = delta;
      if (raw < thrLow) {
        inPulse = false;
        PulseEvent e;
        e.seq          = pulseCount;
        e.detectedAtMs = pulseStartMs;
        e.widthMs      = (nowMicros - pulseStartMicros) / 1000UL;
        e.intervalMs   = prevRiseMs ? (pulseStartMs - prevRiseMs) : 0;
        e.peakDelta    = peakDelta;
        e.rawAtEdge    = raw;
        e.baseline     = baseline;
        e.span         = span;
        e.quality      = quality;
        e.saturated    = sawSaturation ? 1 : 0;
        prevRiseMs = pulseStartMs;
        if (eventQueue && xQueueSend(eventQueue, &e, 0) != pdTRUE) eventDrops++;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(SENSOR_TICK_MS));
  }
}

// ===========================================================================
// ROLLING WINDOW — the numbers that turn a stream into a verdict.
// ===========================================================================
const int WIN = 64;
static uint32_t winInterval[WIN], winWidth[WIN];
static int      winPeak[WIN];
static int      winLen = 0, winIdx = 0;
static uint32_t statPulses = 0, statSat = 0, statMiss = 0;

static uint32_t medU32(uint32_t* src, int n) {
  if (n <= 0) return 0;
  uint32_t t[WIN];
  for (int i = 0; i < n; i++) t[i] = src[i];
  for (int i = 1; i < n; i++) { uint32_t k = t[i]; int j = i-1;
    while (j >= 0 && t[j] > k) { t[j+1] = t[j]; j--; } t[j+1] = k; }
  return t[n/2];
}
static int medInt(int* src, int n) {
  if (n <= 0) return 0;
  int t[WIN];
  for (int i = 0; i < n; i++) t[i] = abs(src[i]);
  for (int i = 1; i < n; i++) { int k = t[i]; int j = i-1;
    while (j >= 0 && t[j] > k) { t[j+1] = t[j]; j--; } t[j+1] = k; }
  return t[n/2];
}

static void windowPush(const PulseEvent& e) {
  if (e.intervalMs > 0) {
    // A gap well beyond the running median means a flag went past unseen.
    uint32_t med = medU32(winInterval, winLen);
    if (med > 0 && e.intervalMs > (med * 9) / 5) statMiss++;   // >1.8x
    winInterval[winIdx] = e.intervalMs;
  } else {
    winInterval[winIdx] = 0;
  }
  winWidth[winIdx] = e.widthMs;
  winPeak[winIdx]  = e.peakDelta;
  winIdx = (winIdx + 1) % WIN;
  if (winLen < WIN) winLen++;
  statPulses++;
  if (e.saturated) statSat++;
}

// ===========================================================================
// NETWORK TASK — the only place mqtt.* is called.
// ===========================================================================
#ifdef IR_DIAG_WIFI
static uint32_t nextMqttTryMs = 0;

static void attemptReconnect() {
  uint32_t now = millis();
  if (now < nextMqttTryMs) return;
  nextMqttTryMs = now + MQTT_RETRY_MS;      // fixed backoff, never every pass
  if (WiFi.status() != WL_CONNECTED) return;
  String cid = "ngr-irdiag-" + String(NODE_NAME);
  if (mqtt.connect(cid.c_str(), T_ONLINE, 0, true, "0")) {
    mqtt.publish(T_ONLINE, "1", true);
    Serial.println("[NET] MQTT connected");
  }
}

static void networkTask(void*) {
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!mqtt.connected()) {
        attemptReconnect();
      } else {
        mqtt.loop();
        // PEEK-PUBLISH-REMOVE, capped. A line leaves the queue only after
        // publish() reports success, so a locally detectable failure retries
        // instead of vanishing; the cap stops a reconnect flush stampeding
        // the link that just failed.
        PubMsg m; uint8_t n = 0;
        while (n < PUB_DRAIN_CAP && xQueuePeek(pubQueue, &m, 0) == pdTRUE) {
          if (!mqtt.publish(T_DIAG, m.payload, false)) break;
          xQueueReceive(pubQueue, &m, 0);
          n++;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
#endif

// ===========================================================================
void setup() {
  Serial.begin(115200);
  delay(400);
  analogReadResolution(12);
  pinMode(SENSOR_PIN, INPUT);

  char b[224];
  snprintf(b, sizeof(b), "=== %s  node=%s  pin=%d  flags/rev=%d  circ=%.1fmm ===",
           SKETCH_NAME, NODE_NAME, SENSOR_PIN, SPOKES_PER_WHEEL, WHEEL_CIRCUMFERENCE_MM);
  Serial.println(b);
  Serial.println("PULSE = one flag. IDLE = sensor alive. STATS = 10s summary.");

  eventQueue = xQueueCreate(256, sizeof(PulseEvent));
  if (!eventQueue) { Serial.println("[FATAL] event queue alloc failed"); while (1) delay(1000); }

#ifdef IR_DIAG_WIFI
  snprintf(T_ONLINE, sizeof(T_ONLINE), "ngr/spoke/%s/status/online", NODE_NAME);
  snprintf(T_DIAG,   sizeof(T_DIAG),   "ngr/spoke/%s/diag",          NODE_NAME);
  pubQueue = xQueueCreate(64, sizeof(PubMsg));
  if (!pubQueue) { Serial.println("[FATAL] pub queue alloc failed"); while (1) delay(1000); }
#endif

  // Sampling starts BEFORE the network, so nothing about bringing WiFi up can
  // cost a pulse.
  if (xTaskCreatePinnedToCore(sensorTask, "sensorTask", 4096, nullptr, 2, nullptr, 0) != pdPASS) {
    Serial.println("[FATAL] sensor task creation failed"); while (1) delay(1000);
  }

#ifdef IR_DIAG_WIFI
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.setSleep(false);            // modem sleep causes the latency spikes
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setSocketTimeout(2);        // PubSubClient's READ timeout, seconds
  // The TCP connect bound. NOT setSocketTimeout, and NOT setTimeout (which is
  // the inherited Stream read timeout, in seconds, and does nothing here).
  espClient.setConnectionTimeout(3000);   // milliseconds
  if (xTaskCreatePinnedToCore(networkTask, "net", 8192, nullptr, 1, nullptr, 0) != pdPASS) {
    Serial.println("[FATAL] network task creation failed"); while (1) delay(1000);
  }
  Serial.println("[BOOT] streaming to serial and ngr/spoke/<node>/diag");
#else
  Serial.println("[BOOT] serial only — no radio compiled in");
#endif
}

// ===========================================================================
static uint32_t lastIdlePrint = 0, lastStatsPrint = 0, lastPulseMs = 0;

void loop() {
  char b[224];
  PulseEvent e;

  while (eventQueue && xQueueReceive(eventQueue, &e, 0) == pdTRUE) {
    windowPush(e);
    lastPulseMs = millis();
    const char* q = (e.quality == 2) ? "OK" : (e.quality == 1 ? "MARGINAL" : "UNAVAIL");
    snprintf(b, sizeof(b),
      "PULSE #%5lu  int=%5lums  w=%4lums  peak=%+5d  raw=%4d base=%4d span=%4d  %s%s",
      (unsigned long)e.seq, (unsigned long)e.intervalMs, (unsigned long)e.widthMs,
      e.peakDelta, e.rawAtEdge, e.baseline, e.span, q,
      e.saturated ? "  *SATURATED*" : "");
    emit(b);
  }

  uint32_t now = millis();

  // IDLE — proves the sensor is alive between pulses. Without this line, "no
  // output" and "no pulses" are indistinguishable, which is exactly how a
  // dead optical path gets mistaken for a stopped wheel.
  if (now - lastIdlePrint >= IDLE_PRINT_MS) {
    lastIdlePrint = now;
    int span = lastSpan, base = lastBaseline;
    snprintf(b, sizeof(b),
      "IDLE          raw=%4d  base=%4d  span=%4d  thr=%+4d/%+4d  pulses=%lu%s",
      (int)lastRaw, base, span, span/3, span/6, (unsigned long)pulseCount,
      qualityFromSpan(span) == 0 ? "   <-- NO USABLE CONTRAST" : "");
    emit(b);
  }

  // STATS — the ten-second verdict.
  if (now - lastStatsPrint >= STATS_PRINT_MS) {
    uint32_t elapsed = now - lastStatsPrint;
    lastStatsPrint = now;
    uint32_t medInt_ = medU32(winInterval, winLen);
    uint32_t medW    = medU32(winWidth, winLen);
    int      medP    = medInt(winPeak, winLen);
    uint32_t mn = 0, mx = 0;
    for (int i = 0; i < winLen; i++) {
      uint32_t v = winInterval[i];
      if (v == 0) continue;
      if (mn == 0 || v < mn) mn = v;
      if (v > mx) mx = v;
    }
    float rate = (elapsed > 0) ? (statPulses * 1000.0f / elapsed) : 0.0f;
    // Derived, and only as a sanity check: it depends on SPOKES_PER_WHEEL and
    // WHEEL_CIRCUMFERENCE_MM, which are calibration inputs, not measurements.
    float mmps = (medInt_ > 0)
               ? (1000.0f / medInt_) / SPOKES_PER_WHEEL * WHEEL_CIRCUMFERENCE_MM : 0.0f;
    snprintf(b, sizeof(b),
      "STATS %2lus  n=%lu rate=%.1f/s | int med=%lu min=%lu max=%lu jit=%lu | w med=%lu | "
      "peak med=%d | sat=%lu miss=%lu drops=%lu | ~%.0fmm/s ~%.1fpkph",
      (unsigned long)(elapsed/1000), (unsigned long)statPulses, rate,
      (unsigned long)medInt_, (unsigned long)mn, (unsigned long)mx,
      (unsigned long)(mx > mn ? mx - mn : 0), (unsigned long)medW,
      medP, (unsigned long)statSat, (unsigned long)statMiss,
      (unsigned long)eventDrops, mmps, mmps / PKPH_MM_PER_SEC);
    emit(b);
    statPulses = 0; statSat = 0; statMiss = 0;
  }

  delay(2);
}
