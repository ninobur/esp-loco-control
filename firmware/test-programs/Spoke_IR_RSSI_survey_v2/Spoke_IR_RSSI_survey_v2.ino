/*
 * ============================================================================
 * Spoke_IR_RSSI_survey_v2  —  NGR survey car: QRE1113 IR wheel sensor
 *                             + Wi-Fi RSSI coverage survey
 * ============================================================================
 *
 * v2 of Spoke_IR_RSSI_survey. Same hardware, same job, rebuilt around the
 * failure analysed in docs/IR_SENSOR_NOTES.md.
 *
 * ---------------------------------------------------------------------------
 * WHAT WAS WRONG WITH v1
 * ---------------------------------------------------------------------------
 * v1 sampled the sensor by polling inside loop(), and loop() also called a
 * connectWiFi() that blocks in a while loop for up to 30 seconds. While it
 * blocked, no ADC read happened, so wedges passed an unwatched sensor and
 * nothing caught up afterwards. Worse, the pulse that was open when the stall
 * began was closed when the loop resumed, and its width was computed as
 * micros()-pulseStartMicros — publishing a 21-SECOND "pulse width" that
 * measured how long the CPU was busy, not how long the sensor saw light.
 *
 * The three largest v1 "lockouts" aligned exactly with MQTT dropouts, and
 * pulse_count advanced +10, +9, +10 across them: the sensor never went blind.
 * The ADC never exceeded 2511 of 4095, so saturation does not fit either.
 * The failure was architectural, not optical.
 *
 * ---------------------------------------------------------------------------
 * WHAT v2 DOES INSTEAD — and why it is a TASK, not an ISR
 * ---------------------------------------------------------------------------
 * This project has already solved this exact problem, in the field, on the
 * Hall sensor. SOLONAV/QUORUM's answer was NOT an interrupt: it was a
 * dedicated FreeRTOS task sampling on a 1 ms tick, pinned to core 0 above the
 * network, feeding a queue that loop() drains. Measured: hall task gaps of
 * 1-4 ms while loop() stalled for 20 SECONDS, and loop_max_gap_ms of 80 ms
 * against 94,033 ms before the change.
 *
 * That is copied here deliberately, and not an ADC-in-ISR design, because:
 *   * Arduino analogRead() is not ISR-safe on ESP32;
 *   * adc1_get_raw() is the DEPRECATED legacy driver on the installed core
 *     (3.3.11 — it lives under driver/deprecated/), and its replacement
 *     adc_oneshot_read() takes a mutex and is not ISR-safe either;
 *   * a 1 ms task tick already gives 20-60 samples across a typical flag,
 *     which is ample, and it costs none of the ISR hazards.
 * A comparator front end plus attachInterrupt() would also be sound, but that
 * hardware does not exist yet. When the sensor moves onto the locomotive's own
 * ESP32, sensorTask() drops in alongside hallTask() unchanged.
 *
 *   1  SENSOR TASK   1 kHz sampling, adaptive threshold, edge detection with
 *                    debounce. Stamps micros()/millis() AT THE EDGE. Cannot be
 *                    starved by loop() or by the network.
 *   2  EVENT QUEUE   256 timestamped pulse events. A 30 s outage costs
 *                    nothing: every pulse is recorded with a correct
 *                    timestamp and flushed when the link returns.
 *   3  LOOP          drains events, computes speed, emits ONE JSON per pulse.
 *   4  NETWORK TASK  owns WiFi and MQTT exclusively. Non-blocking connect with
 *                    a backoff timer, and a CAPPED outbound drain so a
 *                    reconnect flush cannot stampede the link that just failed.
 *
 * Nothing in the capture path can block on the network, and nothing in the
 * network path can touch capture state except through a queue.
 *
 * ---------------------------------------------------------------------------
 * OTHER FIXES FROM docs/IR_SENSOR_NOTES.md "Firmware requirements"
 * ---------------------------------------------------------------------------
 *   * ONE JSON MESSAGE PER PULSE, not six topics. v1 published six topics per
 *     wedge — ~38 blocking TCP writes/second at the observed rate, from a
 *     moving vehicle. That publish rate was likely CAUSING dropouts, not only
 *     suffering them. It also fixes the artefact where interval values were
 *     stale republishes of the previous pulse: one atomic payload cannot tear.
 *   * RSSI ON EVERY PULSE, so dropouts can be correlated against position,
 *     plus the standalone 2 Hz survey topic (which is the point of the car and
 *     is published even while stopped).
 *   * OUTLIER REJECTION on the interval buffer: anything beyond 3x the current
 *     median is refused before it enters, so one stall-length interval cannot
 *     corrupt speed for the next five pulses.
 *   * BUFFER RESET ON TIMEOUT. v1 zeroed reported speed but left the buffer
 *     and its fill count intact, so the first pulse after a stop averaged
 *     against pre-stop garbage.
 *   * ADAPTIVE THRESHOLD, not one calibration at boot. A slowly decaying
 *     running min AND max with thresholds at fixed fractions of the gap
 *     adapts to rising ambient and fading contrast at once — a dirty lens and
 *     a sunny afternoon both present as a shrinking gap. Calibrating once and
 *     never adapting is the defect that cost weeks on the Hall sensor.
 *   * A FLOOR UNDER THE THRESHOLD. A self-adjusting threshold squeezes itself
 *     into the noise if contrast collapses. Below MIN_USABLE_SPAN the sensor
 *     declares itself UNAVAILABLE rather than emitting garbage at high rate.
 *   * SIGNAL QUALITY REPORTED ALONGSIDE THE READING. span and quality ride
 *     every payload, so a narrowing margin is visible BEFORE it fails.
 *
 * ---------------------------------------------------------------------------
 * WHAT THIS DOES NOT FIX
 * ---------------------------------------------------------------------------
 * This makes the sensor HONEST, not sun-proof. A genuine optical failure will
 * still fail — but it will now be measurable, because it will show a FLAT
 * pulse count with sane widths, which is the opposite signature of the v1
 * artefact. The real optical fix is differential sampling (emitter on a GPIO:
 * read dark, read lit, subtract), and that remains gated on whether the
 * emitter is hard-tied to VCC on the breakout. See IR_SENSOR_NOTES.md.
 *
 * SPOKE COUNT: SPOKES_PER_WHEEL = 2 — the survey wheel carries TWO gaffer-tape
 * flags, opposite one another (180 deg apart). Two flags = two pulses per rev.
 * (History: 10, then 5; the physical target is 2. Speed is directly
 * proportional to this constant, so it MUST match the wheel.)
 *
 * SURVEY CORRELATION: this car has no position sense of its own. RSSI is
 * mapped to position afterward by matching timestamps against the TOW
 * locomotive's navMm in the same log. The RSSI ESP sits ~2 marker-spacings
 * BEHIND the tow loco's Hall sensor, so RSSI at time T corresponds to a track
 * position ~2 markers behind the loco's reported mm at T. Apply that offset
 * during analysis. First survey: start MM40-41 CW.
 *
 * IR signal cable is shielded (coax) sensor -> ESP, pin D34 unchanged.
 * GPIO34 is ADC1, which is required: ADC2 cannot be read while WiFi is up.
 * ============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Credentials live in credentials.h, git-ignored and never committed. Copy
// firmware/config/credentials_template.h into this folder as credentials.h.
#include "credentials.h"

#define SKETCH_NAME "Spoke_IR_RSSI_survey_v2"

const char* MQTT_BROKER = "192.168.68.142";
const int   MQTT_PORT   = 1883;
const char* NODE_NAME   = "IR_SPEED_SENSOR";

// ===========================================================================
// TYPES — must precede every function. The Arduino IDE generates prototypes
// and inserts them above anything declared later in the file, so a type used
// in a signature but defined mid-file produces "does not name a type".
// ===========================================================================
struct PulseEvent {
  uint32_t seq;          // monotonic pulse number since boot
  uint32_t detectedAtMs; // millis() at the RISING edge — for log correlation
  uint32_t intervalMs;   // rising edge to rising edge (0 on the first pulse)
  uint32_t widthMs;      // rising to falling edge of THIS pulse
  int      rawAtEdge;    // ADC value that crossed the threshold
  int      span;         // adaptive contrast (runMax-runMin) at the edge
  uint8_t  quality;      // 0=UNAVAILABLE 1=MARGINAL 2=OK
};

struct PubMsg { const char* topic; char payload[256]; bool retain; };

// ===========================================================================
// HARDWARE / TUNING
// ===========================================================================
const int   SENSOR_PIN            = 34;      // ADC1_CH6 — must be ADC1 with WiFi up
const int   SPOKES_PER_WHEEL      = 2;       // TWO tape flags, 180 deg apart
const float WHEEL_CIRCUMFERENCE_MM = 115.0f; // MEASURE on the production car

const uint32_t SENSOR_TICK_MS  = 1;          // 1 kHz sampling
const uint32_t DEBOUNCE_US     = 15000;      // 15 ms — edge doubling guard

// SPEED_TIMEOUT_MS is tied to SPOKES_PER_WHEEL and was wrong in v1. With TWO
// flags the interval is 5x longer than the ten-flag data it was set from: at
// 115 mm circumference, 50 mm/s is already a 1150 ms interval, so v1's 1000 ms
// timeout declared "stopped" while the car was still rolling at creep. At
// 20 mm/s the interval is ~2.9 s, so the timeout must sit above that.
const uint32_t SPEED_TIMEOUT_MS = 4000;

// Adaptive threshold. runMin/runMax track the signal and decay toward each
// other so the window follows rising ambient and fading contrast alike.
const int      MIN_USABLE_SPAN = 120;   // below this, declare UNAVAILABLE
const int      MARGINAL_SPAN   = 300;   // below this, flag the read as MARGINAL
const uint32_t DECAY_INTERVAL_MS = 250; // how often the envelope relaxes
const int      DECAY_STEP        = 2;   // counts per relaxation step

const uint32_t RSSI_PUBLISH_MS = 500;   // survey cadence, 2 Hz
const uint32_t STATUS_PUBLISH_MS = 5000;// health/diagnostic beat
const uint32_t MQTT_RETRY_MS   = 5000;  // fixed backoff, NOT every loop pass
const uint8_t  PUB_DRAIN_CAP   = 8;     // outbound publishes per network pass

// pKPH SCALE FACTOR — UNRESOLVED, see IR_SENSOR_NOTES.md "Known errors".
// v1 used 0.21, which corresponds to roughly 1:58 and matches no G standard
// (1:22.5 would be 0.081, 1:29 would be 0.104). The Pi dashboard currently
// scales measured speed by 0.162 (1:45). Three different numbers are in play,
// so the value is NOT silently changed here — v1's constant is kept so v2
// speeds stay comparable with v1 logs. speed_mmps is the scale-free truth and
// should be preferred for any cross-check until this is settled.
const float PKPH_FACTOR = 0.21f;

// ===========================================================================
// SHARED STATE
// ===========================================================================
static QueueHandle_t eventQueue = nullptr;   // sensorTask -> loop()
static QueueHandle_t pubQueue   = nullptr;   // loop() -> networkTask

// Counters written by sensorTask, read by loop() for telemetry. Single writer,
// aligned 32-bit, diagnostic only — volatile is sufficient.
static volatile uint32_t pulseCount     = 0;
static volatile uint32_t eventDrops     = 0; // events lost to a full eventQueue
static volatile int      lastRaw        = 0;
static volatile int      lastSpan       = 0;
static volatile uint32_t sensorTaskMaxGapMs = 0;
static uint32_t pubDrops = 0;                // publishes lost to a full pubQueue

static char T_ONLINE[64], T_PULSE[64], T_RSSI[64], T_STATUS[64];

static WiFiClient   espClient;
static PubSubClient mqtt(espClient);

// ---------------------------------------------------------------------------
// pub() ENQUEUES and returns. It must never touch mqtt: networkTask is the
// only place a message leaves the radio. Drop-oldest, because for status the
// newest value is the truth. Pulse events do NOT come through here on the
// critical path — they are queued as events first (see loop()).
// ---------------------------------------------------------------------------
static void pub(const char* topic, const char* payload, bool retain = false) {
  if (!pubQueue) return;
  PubMsg m; m.topic = topic; m.retain = retain;
  strlcpy(m.payload, payload, sizeof(m.payload));
  if (xQueueSend(pubQueue, &m, 0) != pdTRUE) {
    PubMsg discard;
    xQueueReceive(pubQueue, &discard, 0);   // evict oldest
    xQueueSend(pubQueue, &m, 0);
    pubDrops++;
  }
}

// ===========================================================================
// LAYER 1 — SENSOR TASK
// ---------------------------------------------------------------------------
// Samples at 1 kHz on core 0 at priority 2 (above networkTask's 1), so neither
// a blocked loop() nor a degraded link can starve it. Everything about a pulse
// — the edge timestamps, the ADC value, the contrast at the moment of
// detection — is captured HERE and travels with the event.
// ===========================================================================
static int  runMin = 4095, runMax = 0;
static bool envelopePrimed = false;

static uint8_t qualityFromSpan(int span) {
  if (span < MIN_USABLE_SPAN) return 0;   // UNAVAILABLE
  if (span < MARGINAL_SPAN)   return 1;   // MARGINAL
  return 2;                               // OK
}

static void sensorTask(void*) {
  bool     sensorState     = false;
  uint32_t lastEdgeMicros  = 0;
  uint32_t pulseStartMicros = 0;
  uint32_t pulseStartMs    = 0;
  uint32_t prevRiseMs      = 0;   // previous pulse's rising edge, for interval
  uint32_t lastDecayMs     = millis();
  uint32_t prevTick        = millis();

  for (;;) {
    uint32_t now = millis();
    uint32_t gap = now - prevTick;
    if (gap > sensorTaskMaxGapMs) sensorTaskMaxGapMs = gap;
    prevTick = now;

    int raw = analogRead(SENSOR_PIN);   // a TASK, not an ISR — this is safe
    lastRaw = raw;

    // --- adaptive envelope -------------------------------------------------
    if (!envelopePrimed) { runMin = runMax = raw; envelopePrimed = true; }
    if (raw < runMin) runMin = raw;
    if (raw > runMax) runMax = raw;
    if (now - lastDecayMs >= DECAY_INTERVAL_MS) {
      lastDecayMs = now;
      // Relax the envelope toward the middle so it can follow a rising ambient
      // or a fading target instead of latching onto a one-off extreme.
      int mid = (runMin + runMax) / 2;
      if (runMin < mid - DECAY_STEP) runMin += DECAY_STEP;
      if (runMax > mid + DECAY_STEP) runMax -= DECAY_STEP;
    }
    int span = runMax - runMin;
    lastSpan = span;
    uint8_t quality = qualityFromSpan(span);

    // A collapsed envelope means peeling tape, a dirty lens or no target.
    // Emitting edges from noise at high rate is worse than saying nothing:
    // hold the state machine idle until real contrast returns.
    if (quality == 0) { vTaskDelay(pdMS_TO_TICKS(SENSOR_TICK_MS)); continue; }

    // Thresholds at fixed fractions of the live gap, with hysteresis.
    int thresholdHigh = runMin + (span * 2) / 3;
    int thresholdLow  = runMin + span / 3;

    uint32_t nowMicros = micros();

    if (!sensorState && raw > thresholdHigh) {
      // RISING edge — the flag has entered the beam.
      if (nowMicros - lastEdgeMicros > DEBOUNCE_US) {
        sensorState      = true;
        lastEdgeMicros   = nowMicros;
        pulseStartMicros = nowMicros;
        pulseStartMs     = now;
        pulseCount++;
      }
    } else if (sensorState && raw < thresholdLow) {
      // FALLING edge — emit the completed pulse. Width is measured between two
      // edges seen by THIS task, which cannot be stretched by a stalled loop()
      // or a blocked socket. That is the v1 defect, structurally removed.
      sensorState = false;
      PulseEvent e;
      e.seq          = pulseCount;
      e.detectedAtMs = pulseStartMs;
      e.widthMs      = (nowMicros - pulseStartMicros) / 1000UL;
      e.intervalMs   = 0;      // filled in below from the previous rise
      e.rawAtEdge    = raw;
      e.span         = span;
      e.quality      = quality;

      if (prevRiseMs != 0) e.intervalMs = pulseStartMs - prevRiseMs;
      prevRiseMs = pulseStartMs;

      if (eventQueue && xQueueSend(eventQueue, &e, 0) != pdTRUE) {
        // Drop-newest and COUNT it. A hole in the record must never be silent.
        eventDrops++;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(SENSOR_TICK_MS));
  }
}

// ===========================================================================
// LAYER 2 — SPEED, with outlier rejection
// ===========================================================================
const int WINDOW_SIZE = 5;
static uint32_t intervalBuffer[WINDOW_SIZE];
static int      intervalIndex = 0;
static int      intervalsFilled = 0;
static float    lastSpeedMmPerSec = 0.0f;
static float    lastPkph = 0.0f;
static uint32_t rejectedIntervals = 0;

static void resetIntervalBuffer() {
  intervalIndex = 0; intervalsFilled = 0;
  lastSpeedMmPerSec = 0.0f; lastPkph = 0.0f;
}

static uint32_t medianInterval() {
  if (intervalsFilled == 0) return 0;
  uint32_t t[WINDOW_SIZE];
  for (int i = 0; i < intervalsFilled; i++) t[i] = intervalBuffer[i];
  for (int i = 1; i < intervalsFilled; i++) {      // insertion sort, n<=5
    uint32_t k = t[i]; int j = i - 1;
    while (j >= 0 && t[j] > k) { t[j+1] = t[j]; j--; }
    t[j+1] = k;
  }
  return t[intervalsFilled / 2];
}

// Returns true if the interval was accepted into the average.
static bool acceptInterval(uint32_t intervalMs) {
  if (intervalMs == 0) return false;
  // A stall-length interval entering the rolling average corrupts speed for
  // the next five pulses. Refuse anything beyond 3x the current median.
  if (intervalsFilled > 0) {
    uint32_t med = medianInterval();
    if (med > 0 && intervalMs > med * 3) { rejectedIntervals++; return false; }
  }
  intervalBuffer[intervalIndex] = intervalMs;
  intervalIndex = (intervalIndex + 1) % WINDOW_SIZE;
  if (intervalsFilled < WINDOW_SIZE) intervalsFilled++;

  uint32_t sum = 0;
  for (int i = 0; i < intervalsFilled; i++) sum += intervalBuffer[i];
  float avgMs = (float)sum / intervalsFilled;
  float pulsesPerSec = 1000.0f / avgMs;
  float revsPerSec   = pulsesPerSec / SPOKES_PER_WHEEL;
  lastSpeedMmPerSec  = revsPerSec * WHEEL_CIRCUMFERENCE_MM;
  lastPkph           = lastSpeedMmPerSec * PKPH_FACTOR;
  return true;
}

// ===========================================================================
// LAYER 3 — NETWORK TASK. The ONLY place mqtt.* is called.
// ===========================================================================
static uint32_t nextMqttTryMs = 0;
static uint32_t mqttAttempts = 0;

static void attemptReconnect() {
  uint32_t now = millis();
  if (now < nextMqttTryMs) return;
  nextMqttTryMs = now + MQTT_RETRY_MS;    // fixed backoff, not every pass
  if (WiFi.status() != WL_CONNECTED) return;
  mqttAttempts++;
  String clientId = "ngr-spoke-" + String(NODE_NAME);
  if (mqtt.connect(clientId.c_str(), T_ONLINE, 0, true, "0")) {
    mqtt.publish(T_ONLINE, "1", true);
    Serial.println("[NET] MQTT connected");
  } else {
    Serial.printf("[NET] MQTT connect failed, rc=%d\n", mqtt.state());
  }
}

static void networkTask(void*) {
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!mqtt.connected()) {
        attemptReconnect();       // may block; harmless on this task
      } else {
        mqtt.loop();
        // CAPPED drain. A 30 s outage buffers ~100+ pulses; flushing them all
        // as individual blocking writes on reconnect would stampede the link
        // that just failed — re-creating the congestion this design exists to
        // survive. 8 per pass at a 5 ms tick clears a full backlog in about a
        // second while leaving room for mqtt.loop() between writes.
        PubMsg m;
        uint8_t n = 0;
        while (n < PUB_DRAIN_CAP && xQueueReceive(pubQueue, &m, 0) == pdTRUE) {
          mqtt.publish(m.topic, m.payload, m.retain);
          n++;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// ===========================================================================
static void buildTopics() {
  snprintf(T_ONLINE, sizeof(T_ONLINE), "ngr/spoke/%s/status/online", NODE_NAME);
  snprintf(T_PULSE,  sizeof(T_PULSE),  "ngr/spoke/%s/telem/pulse",   NODE_NAME);
  snprintf(T_RSSI,   sizeof(T_RSSI),   "ngr/spoke/%s/telem/rssi",    NODE_NAME);
  snprintf(T_STATUS, sizeof(T_STATUS), "ngr/spoke/%s/telem/status",  NODE_NAME);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n[BOOT] %s — node %s\n", SKETCH_NAME, NODE_NAME);

  analogReadResolution(12);
  pinMode(SENSOR_PIN, INPUT);
  buildTopics();

  eventQueue = xQueueCreate(256, sizeof(PulseEvent));   // ~6 KB, >30 s of pulses
  pubQueue   = xQueueCreate(32,  sizeof(PubMsg));       // ~8 KB
  if (!eventQueue || !pubQueue) {
    Serial.println("[FATAL] queue alloc failed");
    while (1) delay(1000);
  }

  // Sampling starts BEFORE the network exists, so nothing about bringing WiFi
  // up can cost a pulse. Priority 2 pinned to core 0, above networkTask's 1.
  if (xTaskCreatePinnedToCore(sensorTask, "sensorTask", 4096, nullptr, 2, nullptr, 0) != pdPASS) {
    Serial.println("[FATAL] sensor task creation failed");
    while (1) delay(1000);
  }

  // NON-BLOCKING network setup: no while loop anywhere in the connect path.
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  // Modem sleep is ON by default in STA mode and produces exactly the latency
  // spikes seen in the v1 logs.
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setSocketTimeout(2);           // PubSubClient's own read timeout
  // And the TCP connect bound, which setSocketTimeout does NOT set. On this
  // core setTimeout() is the inherited Stream read timeout and has no effect
  // on connect(); setConnectionTimeout() takes MILLISECONDS. This distinction
  // cost this project real debugging time on the locomotive firmware.
  espClient.setConnectionTimeout(3000);

  if (xTaskCreatePinnedToCore(networkTask, "net", 8192, nullptr, 1, nullptr, 0) != pdPASS) {
    Serial.println("[FATAL] network task creation failed");
    while (1) delay(1000);
  }

  Serial.println("[BOOT] ready — sampling at 1 kHz, publishing when the link allows");
}

// ===========================================================================
// loop() — drains captured events and publishes. It may stall for any reason
// without costing a pulse: capture is on its own task and the queue holds the
// backlog with correct timestamps.
// ===========================================================================
static uint32_t lastRssiPublish = 0;
static uint32_t lastStatusPublish = 0;
static uint32_t lastPulseSeenMs = 0;

void loop() {
  PulseEvent e;
  while (eventQueue && xQueueReceive(eventQueue, &e, 0) == pdTRUE) {
    bool accepted = acceptInterval(e.intervalMs);
    lastPulseSeenMs = millis();

    // ONE atomic JSON payload per pulse. Six separate topics per wedge is what
    // v1 did — ~38 blocking writes/second — and a torn set of five topics is
    // how v1 logged interval values that belonged to the previous pulse.
    //
    // Buffer arithmetic, worst case, every field at its widest output:
    // seq/t_ms/interval_ms/width_ms 4x u32 = 4x(len+13); speed/pkph at
    // "-99999.99"; raw "-4095"; span "4095"; quality "UNAVAILABLE";
    // rssi "-100"; ev_drops/rej 2x u32  ->  235 chars + NUL = 236 <= 256,
    // and 236 <= PubMsg::payload[256]. Checked, not assumed: a payload that
    // silently truncates is invalid JSON that a replay cannot parse.
    const char* q = (e.quality == 2) ? "OK" : (e.quality == 1 ? "MARGINAL" : "UNAVAILABLE");
    char b[256];
    snprintf(b, sizeof(b),
      "{\"seq\":%lu,\"t_ms\":%lu,\"interval_ms\":%lu,\"width_ms\":%lu,"
      "\"speed_mmps\":%.2f,\"pkph\":%.2f,\"raw\":%d,\"span\":%d,"
      "\"quality\":\"%s\",\"accepted\":%d,\"rssi\":%d,"
      "\"ev_drops\":%lu,\"rej\":%lu}",
      (unsigned long)e.seq, (unsigned long)e.detectedAtMs,
      (unsigned long)e.intervalMs, (unsigned long)e.widthMs,
      lastSpeedMmPerSec, lastPkph, e.rawAtEdge, e.span,
      q, accepted ? 1 : 0, (int)WiFi.RSSI(),
      (unsigned long)eventDrops, (unsigned long)rejectedIntervals);
    pub(T_PULSE, b, false);

    Serial.printf("[PULSE] #%lu int=%lums w=%lums %.1f mm/s raw=%d span=%d %s%s\n",
      (unsigned long)e.seq, (unsigned long)e.intervalMs, (unsigned long)e.widthMs,
      lastSpeedMmPerSec, e.rawAtEdge, e.span, q, accepted ? "" : " [interval rejected]");
  }

  // Stop detection. v1 zeroed the reported speed but left the buffer and its
  // fill count intact, so the first pulse after a stop averaged against
  // pre-stop garbage. Reset the whole thing.
  if (lastPulseSeenMs && millis() - lastPulseSeenMs > SPEED_TIMEOUT_MS &&
      (lastSpeedMmPerSec != 0.0f || intervalsFilled != 0)) {
    resetIntervalBuffer();
    char b[128];
    snprintf(b, sizeof(b),
      "{\"seq\":%lu,\"event\":\"STOPPED\",\"speed_mmps\":0.00,\"pkph\":0.00,\"rssi\":%d}",
      (unsigned long)pulseCount, (int)WiFi.RSSI());
    pub(T_PULSE, b, false);
    Serial.println("[PULSE] STOPPED — interval buffer reset");
  }

  // Survey payload: RSSI on a fixed 2 Hz cadence independent of pulse
  // activity, so coverage is mapped even while stopped. RSSI also rides every
  // pulse above, which is what lets dropouts be correlated with position.
  uint32_t now = millis();
  if (now - lastRssiPublish >= RSSI_PUBLISH_MS) {
    lastRssiPublish = now;
    if (WiFi.status() == WL_CONNECTED) {
      char b[64];
      snprintf(b, sizeof(b), "%d", (int)WiFi.RSSI());
      pub(T_RSSI, b, false);
    }
  }

  // Health beat: the numbers that would explain a suspicious record. A flat
  // pulse count with a healthy span and no drops is a real optical failure —
  // the opposite signature from v1's artefact, and now distinguishable.
  if (now - lastStatusPublish >= STATUS_PUBLISH_MS) {
    lastStatusPublish = now;
    char b[256];
    snprintf(b, sizeof(b),
      "{\"pulses\":%lu,\"raw\":%d,\"span\":%d,\"quality\":\"%s\","
      "\"ev_drops\":%lu,\"pub_drops\":%lu,\"rej\":%lu,"
      "\"task_max_gap_ms\":%lu,\"mqtt_attempts\":%lu,\"rssi\":%d,\"heap\":%lu}",
      (unsigned long)pulseCount, (int)lastRaw, (int)lastSpan,
      qualityFromSpan(lastSpan) == 2 ? "OK" : (qualityFromSpan(lastSpan) == 1 ? "MARGINAL" : "UNAVAILABLE"),
      (unsigned long)eventDrops, (unsigned long)pubDrops, (unsigned long)rejectedIntervals,
      (unsigned long)sensorTaskMaxGapMs, (unsigned long)mqttAttempts,
      (int)WiFi.RSSI(), (unsigned long)ESP.getFreeHeap());
    pub(T_STATUS, b, false);
    sensorTaskMaxGapMs = 0;   // windowed, like the locomotive's loopstat
  }

  delay(2);
}
