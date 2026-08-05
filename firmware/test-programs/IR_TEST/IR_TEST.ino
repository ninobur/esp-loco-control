/*
 * ============================================================================
 * IR_TEST  —  NGR survey car: QRE1113 IR wheel sensor
 *             + Wi-Fi RSSI coverage survey
 * ============================================================================
 *
 * Formerly Spoke_IR_RSSI_survey_v2, renamed 2026-08-05. Same hardware, same
 * job, same lineage: it succeeded Spoke_IR_RSSI_survey and was rebuilt around
 * the failure analysed in docs/IR_SENSOR_NOTES.md.
 *
 * NODE_NAME stays IR_SPEED_SENSOR. That string is the MQTT topic identity
 * (ngr/spoke/IR_SPEED_SENSOR/...), not the sketch name — renaming it would
 * move every topic and break the Pi-side parsers and every archived record.
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
 *   * UNCONDITIONAL FIFO IN, MEDIAN OF INTERVALS OUT. Every measured interval
 *     enters the ring; robustness lives in the output computation, not in an
 *     admission rule. The original 3x-median gate wedged in the field, and
 *     the revolution-time summing that followed stretched one glitch across
 *     14 output pulses at ten spokes — see the LAYER 2 header for both
 *     post-mortems.
 *   * BUFFER RESET ON TIMEOUT. v1 zeroed reported speed but left the buffer
 *     and its fill count intact, so the first pulse after a stop averaged
 *     against pre-stop garbage.
 *   * ADAPTIVE THRESHOLD, not one calibration at boot. A slowly decaying
 *     running min AND max with thresholds at fixed fractions of the gap
 *     adapts to rising ambient and fading contrast at once — a dirty lens and
 *     a sunny afternoon both present as a shrinking gap. Calibrating once and
 *     never adapting is the defect that cost weeks on the Hall sensor.
 *   * DECAY GATED ON MOTION, AND THE ENVELOPE PERSISTED. A stationary wheel
 *     presents a constant, so decaying against it collapses the span to zero
 *     — measured at 7.9 counts/second, blind after four minutes at a station.
 *     Decay now runs only while the wheel is turning, and the envelope is
 *     restored from NVS at boot (widened, never narrowed) so the car does not
 *     have to move before it can see. Expansion stays unconditional, so a
 *     lighting change is still tracked while stopped.
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
 * SPOKE COUNT: SPOKES_PER_WHEEL = 10 — the factory LGB moulded spoked wheel
 * itself is now the target: ten spokes, evenly spaced by manufacture, ten
 * pulses per revolution. (History: 10 hand flags, then 5, then 2 tape flags;
 * the tape era ended 2026-08-05 when the operator switched to the bare wheel.
 * Speed is directly proportional to this constant, so it MUST match the
 * wheel.)
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

// ---------------------------------------------------------------------------
// IR_TELEMETRY_WIFI — the one build switch.
//
// DEFINED (default): WiFi + MQTT + the 2 Hz RSSI survey. This is the survey
//   build and the one that produces telemetry.
// COMMENTED OUT: no WiFi is compiled in at all — no radio, no MQTT, no
//   reconnect path, nothing that can stall. Pulses log over USB only.
//
// This exists because validating capture and network in the same first run
// leaves a bad result unattributable. Run the USB build first: if pulses are
// clean with no network in the binary, capture is proven, and anything that
// appears afterwards belongs to the radio. Stage 1 of the staged plan in
// docs/IR_SENSOR_NOTES.md asks for exactly this.
// ---------------------------------------------------------------------------
#define IR_TELEMETRY_WIFI

#include <Arduino.h>
#include <Preferences.h>   // NVS-backed envelope persistence — no radio needed
#ifdef IR_TELEMETRY_WIFI
  #include <WiFi.h>
  #include <PubSubClient.h>
  // Credentials live in credentials.h, git-ignored and never committed. Copy
  // firmware/config/credentials_template.h into this folder as credentials.h.
  #include "credentials.h"
#endif

#define SKETCH_NAME "IR_TEST"

const char* NODE_NAME   = "IR_SPEED_SENSOR";
#ifdef IR_TELEMETRY_WIFI
const char* MQTT_BROKER = "192.168.68.142";
const int   MQTT_PORT   = 1883;
#endif

// RSSI is only meaningful with a radio. In the USB build it publishes as 0 so
// the payload shape is identical between builds and one parser reads both.
// PUB_HAS_ROOM is the outbound backpressure test (see loop()); with no radio
// there is no outbound queue to back up, so it is always true.
#ifdef IR_TELEMETRY_WIFI
  #define CURRENT_RSSI   ((int)WiFi.RSSI())
  #define PUB_HAS_ROOM   (uxQueueSpacesAvailable(pubQueue) > 4)
  #define MQTT_ATTEMPTS  ((unsigned long)mqttAttempts)
#else
  #define CURRENT_RSSI   0
  #define PUB_HAS_ROOM   true
  #define MQTT_ATTEMPTS  0UL
#endif

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
  uint8_t  intervalValid;// 0 = do not feed the speed filter: no prior rise,
                         // or the gap spans an edge-silence longer than
                         // SPEED_TIMEOUT_MS (a dwell, an outage of sight).
                         // intervalMs still carries the measured value for
                         // the log — the dwell length is diagnostic data.
};

// SENSOR STATE — the honest answer to "can this number be trusted?"
//
// Published on every pulse and every status beat. The 2026-08-05 run showed
// why it must exist: the sensor was blind for 479 s of a 30-minute session
// while publishing quality "OK" throughout, and 24% of pulses reported
// speed 0.00 while the car was demonstrably under power. A published 0.00 is
// a lie a downstream governor cannot distinguish from a real stop.
//
//   UNAVAILABLE   cannot see; no trustworthy output
//   REACQUIRING   seeing edges, median window not yet filled; a PROVISIONAL
//                 estimate is published once SPEED_PROVISIONAL_N intervals
//                 are in, but speed_valid stays 0
//   VALID         window filled, output trustworthy
//   STOPPED       no edges AND independent evidence agrees the wheel is
//                 stationary. The survey car HAS no independent witness, so
//                 this state is currently UNREACHABLE by design — an IR
//                 timeout alone means the sensor stopped SEEING, not that
//                 the wheel stopped TURNING. On the production locomotive,
//                 motionWitnessSaysStopped() gains PWM and Hall inputs and
//                 this state becomes reachable without touching the machine.
enum SpeedState : uint8_t { SS_UNAVAILABLE, SS_REACQUIRING, SS_VALID, SS_STOPPED };

#ifdef IR_TELEMETRY_WIFI
// 384: the pulse payload gained state/speed_valid fields and null-able speed;
// worst case is ~310 chars (arithmetic at the snprintf), over the old 256.
struct PubMsg { const char* topic; char payload[384]; bool retain; };
#endif

// ===========================================================================
// HARDWARE / TUNING
// ===========================================================================
const int   SENSOR_PIN            = 34;      // ADC1_CH6 — must be ADC1 with WiFi up
const int   SPOKES_PER_WHEEL      = 10;       //  10 lgb spokes
const float WHEEL_CIRCUMFERENCE_MM = 115.0f; // MEASURE on the production car

const uint32_t SENSOR_TICK_MS  = 1;          // 1 kHz sampling

// 2.5 ms edge-doubling guard. Measured rise-to-rise (lastEdgeMicros is written
// only in the rising branch), so it bounds the INTERVAL, not the width. The
// fastest genuine ten-spoke interval observed is 22 ms — a comfortable 9x
// above the guard — while 2.5 ms still swallows contact-style chatter on a
// single optical transition. (Was 15 ms in the two-flag era; at ten spokes a
// 15 ms guard would sit only 1.5x under the fastest real interval, one grade
// of speed away from suppressing genuine rises — and a suppressed rise leaves
// sensorState false, so the ENTIRE pulse vanishes, not just an edge.)
const uint32_t DEBOUNCE_US     = 2500;

// SPEED_TIMEOUT_MS is tied to SPOKES_PER_WHEEL and must be rederived every
// time the spoke count changes — it has been wrong twice now for exactly that
// reason (1000 ms set from ten-flag data was wrong for two flags; 4000 ms set
// from two-flag arithmetic was wrong for ten spokes).
//
// Ten-spoke derivation: 115 mm circumference / 10 spokes = 11.5 mm per
// interval. At the 20 mm/s creep bound the genuine interval is 575 ms. The
// timeout must also survive consecutive MISSED spokes at creep — the measured
// miss rate reaches 30% at low speed, so runs of misses are routine, and a
// timeout that fires on three missed spokes would cycle the filter through
// reset/reacquire in normal running. Tolerate three consecutive misses:
// 4 x 575 = 2300 ms. Round up: 2500.
//
// What this buys over the stale 4000: edge silence is declared in 2.5 s
// instead of 4, which shrinks the window where a blind-but-moving sensor is
// still publishing stale speed, and shortens every recovery in Layer 2.
//
// IMPORTANT: exceeding this timeout does NOT mean the wheel is stopped. It
// means the sensor has stopped SEEING. On the survey car there is no
// independent motion witness, so the state machine reports UNAVAILABLE, never
// STOPPED — see SpeedState below.
const uint32_t SPEED_TIMEOUT_MS = 2500;

// LATCH TIMEOUT — the bound on how long a single pulse may stay HIGH.
//
// On 2026-08-05 sensorState latched true for 105.3 SECONDS: the signal rose
// through thresholdHigh, then sat inside the hysteresis band without ever
// crossing thresholdLow. The state machine entered a pulse and never left it,
// and the sensor was silently blind the whole time.
//
// Same value as SPEED_TIMEOUT_MS, same physics: at ~50% duty a pulse lasts
// half a spoke period, so if the wheel is above the 20 mm/s creep bound no
// genuine pulse can be even half this long — anything past it is a latch, not
// a flag. On expiry the incomplete pulse is DISCARDED: it never becomes an
// event, and the artificial "fall" that eventually follows must never be
// mistaken for an edge — a 105-second "pulse" entering the filter is worse
// than no data at all.
const uint32_t LATCH_TIMEOUT_MS = SPEED_TIMEOUT_MS;

// Adaptive threshold. runMin/runMax track the signal and decay toward each
// other so the window follows rising ambient and fading contrast alike.
const int      MIN_USABLE_SPAN = 120;   // below this, declare UNAVAILABLE
const int      MARGINAL_SPAN   = 300;   // below this, flag the read as MARGINAL
const uint32_t DECAY_INTERVAL_MS = 250; // how often the envelope relaxes
const int      DECAY_STEP        = 2;   // counts per relaxation step

// DECAY IS GATED ON MOTION. Decaying a wall clock against a stationary wheel
// collapses the envelope to nothing: a stopped wheel presents a CONSTANT to
// the ADC, so there is no information to adapt to and the relaxation just
// walks runMin and runMax together until they meet. Measured twice in the
// field at 7.9 counts/second:
//     11:45:18 span 1911   11:47:18 span 951   11:48:38 span 314
//     11:49:18 span 11  -> quality UNAVAILABLE
// Four minutes at a station and the sensor is blind. It recovers on motion,
// but the first pulses after the dwell arrive through a squeezed window —
// which is what manufactured the 41 ms interval that wedged the old speed
// filter. Station dwell is normal operation, so the two faults compounded.
//
// This is a design error, not a badly chosen time constant: NO value of
// DECAY_STEP avoids it, because the input carries no information to track.
//
// 2000 ms sits deliberately just UNDER SPEED_TIMEOUT_MS (2500, rederived for
// ten spokes). The envelope therefore stops decaying BEFORE edge silence is
// declared, so what gets frozen — and what gets written to NVS — is by
// construction the envelope that was working while edges were still flowing,
// never one that has already started collapsing in the ambiguous window
// before the timeout.
const uint32_t ENVELOPE_DECAY_MOTION_WINDOW_MS = 2000;

// Restored envelopes are WIDENED, never narrowed. A too-wide envelope costs a
// missed pulse or two; a too-narrow one manufactures noise-driven edges, which
// is the failure that started all of this.
const int      ENVELOPE_RESTORE_WIDEN_PCT = 10;

const char* NVS_NAMESPACE = "irsense";
const char* NVS_KEY_MIN   = "envmin";
const char* NVS_KEY_MAX   = "envmax";

const uint32_t RSSI_PUBLISH_MS = 500;   // survey cadence, 2 Hz
const uint32_t STATUS_PUBLISH_MS = 5000;// health/diagnostic beat
const uint32_t MQTT_RETRY_MS   = 5000;  // fixed backoff, NOT every loop pass
const uint8_t  PUB_DRAIN_CAP   = 8;     // outbound publishes per network pass

// pKPH SCALE — SETTLED, matched to the navigation firmware.
//
// The navigation lineage divides mm/s by a single empirical constant:
//     static const float PKPH_MM_PER_SEC = 5.37325f;
//     measuredPkph = speedMmS / PKPH_MM_PER_SEC;
// in both NGR_LL_DNA_CTO2_r12 (line 414 / 2100) and SOLONAV 1.x. That is the
// number the mm/speed topic has always carried, so it is the number the IR
// sensor must produce for the two to be comparable at all — which is the whole
// point of an independent witness.
//
// It equals 0.18611 per mm/s, i.e. roughly 1:51.7 — a house unit calibrated
// empirically, NOT a geometric scale factor. That is fine, but it means the
// other values in circulation are all wrong for cross-comparison:
//     v1 of this sketch  0.21    (~1:58)   <- matched nothing
//     Pi dashboard       0.162   (1:45)    <- separate lineage, still disagrees
//     geometric 1:22.5   0.081
//     geometric 1:29     0.104
// The dashboard discrepancy is real and is NOT fixed here — it is a dashboard
// question, flagged in IR_SENSOR_NOTES.md for a separate decision.
//
// speed_mmps is published alongside and remains the scale-free truth.
const float PKPH_MM_PER_SEC = 5.37325f;   // navigation firmware constant, verbatim

// ===========================================================================
// SHARED STATE
// ===========================================================================
static QueueHandle_t eventQueue = nullptr;   // sensorTask -> loop()
#ifdef IR_TELEMETRY_WIFI
static QueueHandle_t pubQueue   = nullptr;   // loop() -> networkTask
#endif

// Counters written by sensorTask, read by loop() for telemetry. Single writer,
// aligned 32-bit, diagnostic only — volatile is sufficient.
static volatile uint32_t pulseCount     = 0;
static volatile uint32_t eventDrops     = 0; // events lost to a full eventQueue
static volatile int      lastRaw        = 0;
static volatile int      lastSpan       = 0;
static volatile uint32_t sensorTaskMaxGapMs = 0;
static uint32_t pubDrops = 0;                // publishes lost to a full pubQueue

// SAMPLER-OWNED last-rising-edge time, for loop()'s timeout detection. The
// old detection used lastPulseSeenMs — the time loop() CONSUMED an event —
// which stops advancing during network backpressure even while the wheel
// turns, so an outage could fabricate a stop, reset the filter, and write NVS
// with the car moving. This timestamp is written at the edge by the task that
// saw it and cannot be stalled by the network. 0 = no rise since boot.
static volatile uint32_t lastRiseMs = 0;

// Latch-timeout accounting (see LATCH_TIMEOUT_MS). latchTimeouts is the
// monotonic published counter; loop() watches it to force reacquisition.
// latchedNow/latchStartMs let the status beat report an in-progress latch
// BEFORE it times out — a latch is visible from its first second.
static volatile uint32_t latchTimeouts = 0;
static volatile uint8_t  latchedNow    = 0;
static volatile uint32_t latchStartMs  = 0;

#ifdef IR_TELEMETRY_WIFI
static char T_ONLINE[64], T_PULSE[64], T_RSSI[64], T_STATUS[64];

static WiFiClient   espClient;
static PubSubClient mqtt(espClient);

// ---------------------------------------------------------------------------
// pub() ENQUEUES and returns. It must never touch mqtt: networkTask is the
// only place a message leaves the radio. Drop-oldest, because for status the
// newest value is the truth and evicting a stale copy is correct.
//
// Pulse events are NOT protected by this eviction rule — they are held in
// eventQueue instead and only converted to a publish when there is room (see
// loop()). A pulse happens once and cannot be re-derived; a status value can.
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
#else
// USB build: no radio exists. Every payload goes to the serial log instead,
// so the two builds produce the same records through different pipes.
static void pub(const char* topic, const char* payload, bool retain = false) {
  (void)retain;
  Serial.printf("[USB] %s %s\n", topic, payload);
}
static const char* T_PULSE  = "telem/pulse";
static const char* T_STATUS = "telem/status";
#endif

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

// Mirrors of the envelope for loop() to read when it writes NVS on a stop.
// runMin/runMax themselves are sensorTask's private working state; these are
// the same single-writer/volatile convention already used by lastRaw/lastSpan,
// which keeps the NVS write off sensorTask entirely. Plain assignment, so no
// compound-operation-on-volatile deprecation.
static volatile int lastRunMin = 4095, lastRunMax = 0;

static Preferences envPrefs;

static uint8_t qualityFromSpan(int span) {
  if (span < MIN_USABLE_SPAN) return 0;   // UNAVAILABLE
  if (span < MARGINAL_SPAN)   return 1;   // MARGINAL
  return 2;                               // OK
}

// ---------------------------------------------------------------------------
// ENVELOPE PERSISTENCE
//
// Requiring the car to move before its baseline is usable is not practical in
// service: it boots blind, and the pulses it needs in order to become sighted
// are the very pulses it cannot see. So the envelope survives a power cycle.
//
// The write happens on the moving->stopped transition ONLY. NVS has finite
// write endurance and the 1 kHz sampler must never touch it, so nothing on
// the capture path and nothing per-pulse ever reaches flash — a stop is a
// once-per-station event, and the ESP-IDF NVS layer additionally skips the
// write entirely when the stored value is unchanged.
//
// Restore WIDENS the stored envelope rather than narrowing it. A too-wide
// envelope costs a missed pulse or two while expansion re-converges; a
// too-narrow one manufactures edges out of noise, which is the failure that
// produced the 41 ms interval that wedged the old speed filter. When in
// doubt, be less sensitive, not more.
// ---------------------------------------------------------------------------
static void restoreEnvelope() {
  if (!envPrefs.begin(NVS_NAMESPACE, true)) {          // read-only
    Serial.println("[ENV] no stored envelope (first boot) — cold prime from first sample");
    return;
  }
  int storedMin = envPrefs.getInt(NVS_KEY_MIN, -1);
  int storedMax = envPrefs.getInt(NVS_KEY_MAX, -1);
  envPrefs.end();

  // Every way a stored pair can be absent, truncated or nonsense. ANY failure
  // falls through to the existing cold-prime path in sensorTask, unchanged —
  // a corrupt NVS entry must never be able to produce a working-looking but
  // wrong envelope.
  if (storedMin < 0 || storedMax < 0 || storedMin > 4095 || storedMax > 4095 ||
      storedMax <= storedMin) {
    Serial.printf("[ENV] stored envelope absent or invalid (min=%d max=%d) — cold prime\n",
                  storedMin, storedMax);
    return;
  }
  int storedSpan = storedMax - storedMin;
  if (storedSpan < MIN_USABLE_SPAN) {
    Serial.printf("[ENV] stored span %d below MIN_USABLE_SPAN %d — cold prime\n",
                  storedSpan, MIN_USABLE_SPAN);
    return;
  }

  int margin = (storedSpan * ENVELOPE_RESTORE_WIDEN_PCT) / 100;
  runMin = storedMin - margin; if (runMin < 0)    runMin = 0;
  runMax = storedMax + margin; if (runMax > 4095) runMax = 4095;
  lastRunMin = runMin; lastRunMax = runMax;
  envelopePrimed = true;    // suppress the single-sample cold prime
  Serial.printf("[ENV] restored min=%d max=%d span=%d (stored %d/%d, widened %d%%)\n",
                runMin, runMax, runMax - runMin, storedMin, storedMax,
                ENVELOPE_RESTORE_WIDEN_PCT);
}

static void saveEnvelope() {
  int mn = lastRunMin, mx = lastRunMax;
  if (mx - mn < MIN_USABLE_SPAN) {
    Serial.printf("[ENV] not saved: span %d below MIN_USABLE_SPAN %d\n",
                  mx - mn, MIN_USABLE_SPAN);
    return;   // never persist an envelope that is already unusable
  }
  if (!envPrefs.begin(NVS_NAMESPACE, false)) {
    Serial.println("[ENV] NVS open for write failed — envelope not persisted");
    return;
  }
  envPrefs.putInt(NVS_KEY_MIN, mn);
  envPrefs.putInt(NVS_KEY_MAX, mx);
  envPrefs.end();
  Serial.printf("[ENV] saved on stop: min=%d max=%d span=%d\n", mn, mx, mx - mn);
}

static void sensorTask(void*) {
  bool     sensorState     = false;
  uint32_t lastEdgeMicros  = 0;
  uint32_t pulseStartMicros = 0;
  uint32_t pulseStartMs    = 0;
  uint32_t prevRiseMs      = 0;   // previous pulse's rising edge, for interval
  uint32_t lastPulseMs     = 0;   // last rising edge — 0 = none since boot
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
    // EXPANSION IS UNCONDITIONAL and stays above the quality gate, so a
    // lighting change is still tracked while stationary and a collapsed
    // envelope can always recover. Only the DECAY is gated.
    if (!envelopePrimed) { runMin = runMax = raw; envelopePrimed = true; }
    if (raw < runMin) runMin = raw;
    if (raw > runMax) runMax = raw;

    // Decay only while the wheel is known to be turning — see
    // ENVELOPE_DECAY_MOTION_WINDOW_MS. lastPulseMs == 0 means no pulse has
    // been seen since boot, which must NOT read as "just moved" during the
    // first few seconds of uptime.
    bool recentMotion = (lastPulseMs != 0) &&
                        (now - lastPulseMs < ENVELOPE_DECAY_MOTION_WINDOW_MS);
    if (recentMotion && now - lastDecayMs >= DECAY_INTERVAL_MS) {
      lastDecayMs = now;
      // Relax the envelope toward the middle so it can follow a rising ambient
      // or a fading target instead of latching onto a one-off extreme.
      int mid = (runMin + runMax) / 2;
      if (runMin < mid - DECAY_STEP) runMin += DECAY_STEP;
      if (runMax > mid + DECAY_STEP) runMax -= DECAY_STEP;
    } else if (!recentMotion) {
      // Hold the decay phase, so standing for ten minutes does not earn a
      // burst of catch-up relaxation the moment the car moves again.
      lastDecayMs = now;
    }

    int span = runMax - runMin;
    lastSpan   = span;
    lastRunMin = runMin;
    lastRunMax = runMax;
    uint8_t quality = qualityFromSpan(span);

    // A collapsed envelope means peeling tape, a dirty lens or no target.
    // Emitting edges from noise at high rate is worse than saying nothing:
    // hold the state machine idle until real contrast returns.
    if (quality == 0) { vTaskDelay(pdMS_TO_TICKS(SENSOR_TICK_MS)); continue; }

    // Thresholds at fixed fractions of the live gap, with hysteresis.
    int thresholdHigh = runMin + (span * 2) / 3;
    int thresholdLow  = runMin + span / 3;

    uint32_t nowMicros = micros();

    // LATCH TIMEOUT. sensorState has been true for longer than any physical
    // pulse can last: the signal is parked inside the hysteresis band. Abort
    // the pulse — it is DISCARDED, never emitted — and clear the interval
    // anchor so the next completed pulse starts a fresh measurement rather
    // than spanning the latch. The state machine is immediately eligible to
    // rise again; if the signal is still above thresholdHigh it will re-arm,
    // re-latch, and re-count, which is honest: each increment of
    // latchTimeouts is another LATCH_TIMEOUT_MS of confirmed blindness.
    if (sensorState && (uint32_t)(now - pulseStartMs) > LATCH_TIMEOUT_MS) {
      sensorState = false;
      prevRiseMs  = 0;                        // aborted rise anchors nothing
      latchTimeouts = latchTimeouts + 1;      // plain add: volatile-safe
      latchedNow    = 0;
    }
    latchedNow   = sensorState ? 1 : 0;
    latchStartMs = pulseStartMs;

    if (!sensorState && raw > thresholdHigh) {
      // RISING edge — the flag has entered the beam.
      if (nowMicros - lastEdgeMicros > DEBOUNCE_US) {
        sensorState      = true;
        lastEdgeMicros   = nowMicros;
        pulseStartMicros = nowMicros;
        pulseStartMs     = now;
        lastPulseMs      = now;   // the wheel is turning: decay may run
        lastRiseMs       = now;   // sampler-owned, for loop()'s timeout
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
      e.intervalValid = 0;
      e.rawAtEdge    = raw;
      e.span         = span;
      e.quality      = quality;

      if (prevRiseMs != 0) {
        // The interval is ALWAYS reported — after a dwell it carries the
        // dwell length, which is diagnostic data. But it only feeds the speed
        // filter when it is a plausible spoke-to-spoke time: a gap longer
        // than SPEED_TIMEOUT_MS spans an edge-silence (a stop, or a blind
        // episode) and is not a speed measurement. This is the "treat the
        // next pulse as having no valid prior interval" rule, implemented as
        // a flag at measurement time rather than clearing prevRiseMs at
        // timeout time — identical filter behaviour, one task-local owner,
        // and the measured value survives into the log. NOTHING fabricated
        // is ever fed to the FIFO: an invalid interval simply never arrives.
        e.intervalMs    = pulseStartMs - prevRiseMs;
        e.intervalValid = (e.intervalMs <= SPEED_TIMEOUT_MS) ? 1 : 0;
      }
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
// LAYER 2 — SPEED: unconditional FIFO in, median of INTERVALS out
// ---------------------------------------------------------------------------
// TWO DEAD ARCHITECTURES, AND WHY THIS ONE
//
// FIRST: admission used to be gated on a multiple of the buffer's OWN median
// (refuse beyond 3x). That wedges permanently, and did, in the field: a
// spurious 41 ms interval seeded the buffer, the median became 41 ms, the
// gate became 123 ms, every genuine interval (232-620 ms) was refused, and
// refused intervals never enter so the median could not move. Speed froze at
// 1402.44 mm/s while publishing quality "OK". ANY filter whose admission rule
// reads its own contents has this failure mode. Admission is therefore
// UNCONDITIONAL and stays that way — that property was field-proven on
// 2026-08-05 when a 261632 ms interval entered the ring, corrupted a few
// outputs, and aged out on its own. Under the gate it would have wedged.
//
// SECOND: intervals were then summed into sliding REVOLUTION times
// (SPOKES_PER_WHEEL consecutive intervals) and the median taken over those.
// The summing existed to cancel unequal spacing of hand-applied tape flags.
// Two things killed it:
//   * The target is now the factory LGB 10-spoke wheel — moulded, evenly
//     spaced by manufacture. Autocorrelation of interval deviations at lag 10
//     is -0.025 across 125 field pulses: the wheel runs true. The problem the
//     summing solved no longer exists.
//   * The summing is what made contamination LONG. One bad interval sits in
//     SPOKES_PER_WHEEL consecutive revolution times, so at ten spokes a
//     single glitch poisons ten of the five median slots — the median can
//     never hold a clean majority, and a bad edge stayed visible for
//     SPOKES_PER_WHEEL + 5 - 1 = 14 pulses. Measured on 2026-08-05: after
//     each dwell, 8 pulses of 0.00 then 2 of garbage before the output was
//     right again.
//
// NOW: median over the last SPEED_MEDIAN_N raw intervals, and speed is
//     circumference / (median_interval * SPOKES_PER_WHEEL).
// One bad interval is one bad slot: the contamination window is exactly ONE
// interval, and the median outvotes it immediately (2 good of 3, 3 of 5).
// Nothing sums, so nothing spreads.
//
// This cannot wedge because no state outlives the buffer: every sample ages
// out within SPEED_MEDIAN_N pulses regardless of what precedes or follows it.
// ===========================================================================
const int SPEED_MEDIAN_N      = 5;  // intervals in the median window
const int SPEED_PROVISIONAL_N = 3;  // intervals before a provisional estimate

static SpeedState speedState = SS_UNAVAILABLE;   // boot: cannot see yet
static uint32_t   reacqCount = 0;                // UNAVAILABLE->REACQUIRING transitions

static const char* speedStateName(SpeedState s) {
  switch (s) {
    case SS_REACQUIRING: return "REACQUIRING";
    case SS_VALID:       return "VALID";
    case SS_STOPPED:     return "STOPPED";
    default:             return "UNAVAILABLE";
  }
}

// THE MOTION WITNESS HOOK. STOPPED requires independent evidence that the
// wheel is stationary — an IR timeout alone only proves the sensor stopped
// seeing. The survey car has no such evidence, so this returns false and
// SS_STOPPED is unreachable. On the production locomotive this is where PWM
// and Hall/marker state plug in; the state machine above needs no other
// change.
static bool motionWitnessSaysStopped() {
  return false;   // survey car: no witness exists
}

static uint32_t intervalRing[SPEED_MEDIAN_N];
static int      intervalIndex   = 0;
static int      intervalsFilled = 0;

static float    lastSpeedMmPerSec = 0.0f;
static float    lastPkph = 0.0f;

// Still called on the stop timeout. It is no longer load-bearing for wedge
// recovery — nothing can wedge now — but clearing stale data on a stop
// remains correct.
static void resetIntervalBuffer() {
  intervalIndex = 0; intervalsFilled = 0;
  lastSpeedMmPerSec = 0.0f; lastPkph = 0.0f;
}

// Median over the FILLED slots only, so it can never read an uninitialised
// entry during the warm-up after boot or after a reset.
static uint32_t medianIntervalMs() {
  if (intervalsFilled == 0) return 0;
  uint32_t t[SPEED_MEDIAN_N];
  for (int i = 0; i < intervalsFilled; i++) t[i] = intervalRing[i];
  for (int i = 1; i < intervalsFilled; i++) {      // insertion sort, n<=5
    uint32_t k = t[i]; int j = i - 1;
    while (j >= 0 && t[j] > k) { t[j+1] = t[j]; j--; }
    t[j+1] = k;
  }
  return t[intervalsFilled / 2];
}

// Returns true if the argument was a measured interval and entered the ring.
//
// The ONLY false case is the zero sentinel, which sensorTask emits when there
// is no previous rising edge to measure against (the first pulse after boot).
// Zero is the ABSENCE of a measurement, not a measurement being judged — no
// path in here inspects buffer contents to decide admission, which is the
// property that makes the wedge above structurally impossible.
static bool acceptInterval(uint32_t intervalMs) {
  if (intervalMs == 0) return false;

  intervalRing[intervalIndex] = intervalMs;
  intervalIndex = (intervalIndex + 1) % SPEED_MEDIAN_N;
  if (intervalsFilled < SPEED_MEDIAN_N) intervalsFilled++;

  uint32_t medMs = medianIntervalMs();   // >= 1: only nonzero intervals enter

  // One spoke passes per interval, so a wheel revolution takes
  // SPOKES_PER_WHEEL median intervals.
  lastSpeedMmPerSec = WHEEL_CIRCUMFERENCE_MM /
                      (((float)medMs / 1000.0f) * SPOKES_PER_WHEEL);
  lastPkph          = lastSpeedMmPerSec / PKPH_MM_PER_SEC;   // navigation's conversion
  return true;
}

// ===========================================================================
// LAYER 3 — NETWORK TASK. The ONLY place mqtt.* is called.
// ===========================================================================
#ifdef IR_TELEMETRY_WIFI
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
        // PEEK-PUBLISH-REMOVE, CAPPED. Two separate hazards, one loop:
        //
        // (1) A message leaves the queue only after mqtt.publish() reports
        //     success. The locomotive firmware removed first and ignored the
        //     result, so a publish that failed locally deleted the evidence
        //     with the drop counter still reading 0 — markers 24 and 23
        //     vanished exactly that way in the 2026-07-31 outage test. Here a
        //     local failure leaves the message queued, stops this pass, and
        //     retries on the next one.
        //
        // (2) A 30 s outage buffers ~105 pulses. Flushing them as individual
        //     blocking writes on reconnect stampedes the link that just proved
        //     marginal, recreating the congestion this design exists to
        //     survive. 8 per pass at a 5 ms tick clears a full backlog in
        //     about a second while leaving mqtt.loop() its turn between
        //     writes, so inbound is never starved by outbound.
        PubMsg m;
        uint8_t n = 0;
        while (n < PUB_DRAIN_CAP && xQueuePeek(pubQueue, &m, 0) == pdTRUE) {
          if (!mqtt.publish(m.topic, m.payload, m.retain)) break;  // leave it queued
          xQueueReceive(pubQueue, &m, 0);                          // remove ONLY on success
          n++;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
#endif  // IR_TELEMETRY_WIFI

// ===========================================================================
#ifdef IR_TELEMETRY_WIFI
static void buildTopics() {
  snprintf(T_ONLINE, sizeof(T_ONLINE), "ngr/spoke/%s/status/online", NODE_NAME);
  snprintf(T_PULSE,  sizeof(T_PULSE),  "ngr/spoke/%s/telem/pulse",   NODE_NAME);
  snprintf(T_RSSI,   sizeof(T_RSSI),   "ngr/spoke/%s/telem/rssi",    NODE_NAME);
  snprintf(T_STATUS, sizeof(T_STATUS), "ngr/spoke/%s/telem/status",  NODE_NAME);
}
#endif

void setup() {
  Serial.begin(115200);
  delay(300);
#ifdef IR_TELEMETRY_WIFI
  Serial.printf("\n[BOOT] %s — node %s — WIFI TELEMETRY BUILD\n", SKETCH_NAME, NODE_NAME);
#else
  Serial.printf("\n[BOOT] %s — node %s — USB-ONLY BUILD (no radio compiled in)\n",
                SKETCH_NAME, NODE_NAME);
#endif

  analogReadResolution(12);
  pinMode(SENSOR_PIN, INPUT);

  // BEFORE sensorTask exists, so there is no writer to race with: the task is
  // the sole owner of runMin/runMax from the moment it is created below.
  restoreEnvelope();

  eventQueue = xQueueCreate(256, sizeof(PulseEvent));   // ~6 KB, >30 s of pulses
  if (!eventQueue) {
    Serial.println("[FATAL] event queue alloc failed");
    while (1) delay(1000);
  }
#ifdef IR_TELEMETRY_WIFI
  buildTopics();
  pubQueue = xQueueCreate(32, sizeof(PubMsg));          // ~8 KB
  if (!pubQueue) {
    Serial.println("[FATAL] pub queue alloc failed");
    while (1) delay(1000);
  }
#endif

  // Sampling starts BEFORE the network exists, so nothing about bringing WiFi
  // up can cost a pulse. Priority 2 pinned to core 0, above networkTask's 1.
  if (xTaskCreatePinnedToCore(sensorTask, "sensorTask", 4096, nullptr, 2, nullptr, 0) != pdPASS) {
    Serial.println("[FATAL] sensor task creation failed");
    while (1) delay(1000);
  }

#ifdef IR_TELEMETRY_WIFI
  // NON-BLOCKING network setup: no while loop anywhere in the connect path.
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  // Modem sleep is ON by default in STA mode and produces exactly the latency
  // spikes seen in the v1 logs.
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setSocketTimeout(2);           // PubSubClient's own READ timeout, seconds

  // THE TCP CONNECT BOUND. Three similarly-named calls, only one of which does
  // this job — the distinction has cost this project debugging time before:
  //   mqtt.setSocketTimeout(s)          PubSubClient's read timeout. NOT connect.
  //   espClient.setTimeout(s)           the inherited Stream READ timeout, in
  //                                     SECONDS. Does nothing for connect().
  //   espClient.setConnectionTimeout(ms) <- this one, in MILLISECONDS.
  // On this core (3.3.11) WiFiClient is a typedef of NetworkClient, whose
  // connect() timeout comes only from setConnectionTimeout(). The core default
  // is already 3000 ms, so this is belt-and-braces and makes the bound explicit
  // and version-independent.
  espClient.setConnectionTimeout(3000);

  if (xTaskCreatePinnedToCore(networkTask, "net", 8192, nullptr, 1, nullptr, 0) != pdPASS) {
    Serial.println("[FATAL] network task creation failed");
    while (1) delay(1000);
  }
  Serial.println("[BOOT] ready — sampling at 1 kHz, publishing when the link allows");
#else
  Serial.println("[BOOT] ready — sampling at 1 kHz, logging over USB");
#endif
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
  // BACKPRESSURE, not a guard: events are only converted into publishes while
  // there is outbound room. When the link is down networkTask drains nothing,
  // pubQueue fills, and events then STAY in the 256-slot timestamped
  // eventQueue — which is the outage buffer and is sized for it. Without this
  // the 32-slot pubQueue would evict pulses (drop-oldest) long before the
  // event buffer was anywhere near full, throwing away exactly the record the
  // buffer exists to keep. The 4-slot margin leaves room for status and RSSI.
  while (eventQueue && PUB_HAS_ROOM && xQueueReceive(eventQueue, &e, 0) == pdTRUE) {
    bool accepted = e.intervalValid && acceptInterval(e.intervalMs);
    lastPulseSeenMs = millis();

    // State: edges are arriving, so we are at least reacquiring; VALID only
    // when the median window is full. Entering REACQUIRING from dark counts
    // as a reacquisition.
    if (speedState == SS_UNAVAILABLE || speedState == SS_STOPPED) {
      speedState = SS_REACQUIRING;
      reacqCount++;
    }
    if (intervalsFilled >= SPEED_MEDIAN_N) speedState = SS_VALID;
    bool haveEstimate = (intervalsFilled >= SPEED_PROVISIONAL_N);
    bool speedValid   = (speedState == SS_VALID);

    // The speed fields are NULL until a provisional estimate exists. The old
    // payload published 0.00 through the whole warm-up — 24% of all pulses in
    // the 2026-08-05 run — which a downstream consumer cannot tell from a
    // real stop. null is parseable and unambiguous: "no estimate", never "0".
    char spd[48];
    if (haveEstimate)
      snprintf(spd, sizeof(spd), "\"speed_mmps\":%.2f,\"pkph\":%.2f",
               lastSpeedMmPerSec, lastPkph);
    else
      snprintf(spd, sizeof(spd), "\"speed_mmps\":null,\"pkph\":null");

    // ONE atomic JSON payload per pulse. Six separate topics per wedge is what
    // v1 did — ~38 blocking writes/second — and a torn set of five topics is
    // how v1 logged interval values that belonged to the previous pulse.
    //
    // Buffer arithmetic, worst case, every field at its widest output:
    // seq/t_ms/interval_ms/width_ms 4x u32 ("4294967295"); speed/pkph at
    // "-99999.99" or null; state "REACQUIRING"; speed_valid 1; raw "-4095";
    // span "4095"; quality "UNAVAILABLE"; accepted 1; rssi "-100"; ev_drops
    // u32; rej literal 0  ->  ~310 chars + NUL <= PubMsg::payload[384].
    // Checked, not assumed: a payload that silently truncates is invalid
    // JSON that a replay cannot parse.
    //
    // "rej" is retained at a hard 0 so the Pi-side log schema keeps its
    // shape. Nothing is rejected any more; the field stays for parsers and
    // archived records.
    const char* q = (e.quality == 2) ? "OK" : (e.quality == 1 ? "MARGINAL" : "UNAVAILABLE");
    char b[384];
    snprintf(b, sizeof(b),
      "{\"seq\":%lu,\"t_ms\":%lu,\"interval_ms\":%lu,\"width_ms\":%lu,"
      "\"state\":\"%s\",\"speed_valid\":%d,%s,\"raw\":%d,\"span\":%d,"
      "\"quality\":\"%s\",\"accepted\":%d,\"rssi\":%d,"
      "\"ev_drops\":%lu,\"rej\":0}",
      (unsigned long)e.seq, (unsigned long)e.detectedAtMs,
      (unsigned long)e.intervalMs, (unsigned long)e.widthMs,
      speedStateName(speedState), speedValid ? 1 : 0, spd,
      e.rawAtEdge, e.span,
      q, accepted ? 1 : 0, CURRENT_RSSI,
      (unsigned long)eventDrops);
    pub(T_PULSE, b, false);

    // "accepted":0 means the interval did not feed the filter: no prior rise
    // (first pulse after boot) or a gap spanning an edge-silence (flagged by
    // the sampler, intervalValid == 0).
    Serial.printf("[PULSE] #%lu int=%lums w=%lums %s %.1f mm/s raw=%d span=%d %s%s\n",
      (unsigned long)e.seq, (unsigned long)e.intervalMs, (unsigned long)e.widthMs,
      speedStateName(speedState), haveEstimate ? lastSpeedMmPerSec : 0.0f,
      e.rawAtEdge, e.span, q, accepted ? "" : " [interval not fed to filter]");
  }

  // LATCH-TIMEOUT reaction. The sampler discards the latched pulse itself
  // (nothing is ever emitted for it — see sensorTask); what remains for this
  // side is the trust consequence: whatever the filter held before the latch
  // is stale, so clear it and reacquire from scratch. No NVS write here — the
  // envelope did not cause the latch verdict and a latch proves nothing about
  // its quality either way.
  {
    static uint32_t seenLatchTimeouts = 0;
    uint32_t lt = latchTimeouts;   // one volatile read
    if (lt != seenLatchTimeouts) {
      seenLatchTimeouts = lt;
      resetIntervalBuffer();
      speedState = SS_UNAVAILABLE;
      char b[192];
      snprintf(b, sizeof(b),
        "{\"seq\":%lu,\"event\":\"LATCH_TIMEOUT\",\"state\":\"UNAVAILABLE\","
        "\"speed_valid\":0,\"speed_mmps\":null,\"pkph\":null,"
        "\"latch_timeouts\":%lu,\"rssi\":%d}",
        (unsigned long)pulseCount, (unsigned long)lt, CURRENT_RSSI);
      pub(T_PULSE, b, false);
      Serial.printf("[PULSE] LATCH_TIMEOUT #%lu — pulse discarded, filter reset\n",
                    (unsigned long)lt);
    }
  }

  // EDGE-SILENCE detection, from the SAMPLER-OWNED rise timestamp — not from
  // lastPulseSeenMs, which measures when loop() consumed an event and stops
  // advancing under network backpressure. With the old source, an MQTT outage
  // longer than the timeout would fabricate a stop while the wheel turned:
  // reset the filter, publish STOPPED, and write NVS mid-motion.
  //
  // A timeout here means the sensor stopped SEEING. Whether the wheel stopped
  // TURNING is a separate question only an independent witness can answer —
  // and the survey car has none, so this declares UNAVAILABLE (with STOPPED
  // reserved for a witness-equipped build; see motionWitnessSaysStopped()).
  {
    uint32_t lr = lastRiseMs;   // one volatile read
    if (lr != 0 && (uint32_t)(millis() - lr) > SPEED_TIMEOUT_MS &&
        speedState != SS_UNAVAILABLE && speedState != SS_STOPPED) {
      resetIntervalBuffer();
      speedState = motionWitnessSaysStopped() ? SS_STOPPED : SS_UNAVAILABLE;

      // The edge-silence transition is the only place NVS is written. The
      // state guard above goes false on this pass and cannot go true again
      // until a new pulse arrives, so this fires exactly once per silence —
      // never in a loop, never on the capture path.
      saveEnvelope();

      char b[192];
      snprintf(b, sizeof(b),
        "{\"seq\":%lu,\"event\":\"TIMEOUT\",\"state\":\"%s\","
        "\"speed_valid\":0,\"speed_mmps\":null,\"pkph\":null,\"rssi\":%d}",
        (unsigned long)pulseCount, speedStateName(speedState), CURRENT_RSSI);
      pub(T_PULSE, b, false);
      Serial.printf("[PULSE] TIMEOUT — no edges for %lu ms, state %s, filter reset\n",
                    (unsigned long)SPEED_TIMEOUT_MS, speedStateName(speedState));
    }
  }

  uint32_t now = millis();
#ifdef IR_TELEMETRY_WIFI
  // Survey payload: RSSI on a fixed 2 Hz cadence independent of pulse
  // activity, so coverage is mapped even while stopped. RSSI also rides every
  // pulse above, which is what lets dropouts be correlated with position.
  // This is the survey's whole purpose and exists only in the WiFi build.
  if (now - lastRssiPublish >= RSSI_PUBLISH_MS) {
    lastRssiPublish = now;
    if (WiFi.status() == WL_CONNECTED) {
      char b[64];
      snprintf(b, sizeof(b), "%d", (int)WiFi.RSSI());
      pub(T_RSSI, b, false);
    }
  }
#endif

  // Health beat: the numbers that would explain a suspicious record. A flat
  // pulse count with a healthy span and no drops is a real optical failure —
  // the opposite signature from v1's artefact, and now distinguishable.
  if (now - lastStatusPublish >= STATUS_PUBLISH_MS) {
    lastStatusPublish = now;
    char b[384];
    snprintf(b, sizeof(b),
      "{\"pulses\":%lu,\"raw\":%d,\"span\":%d,\"quality\":\"%s\","
      "\"state\":\"%s\",\"reacq\":%lu,\"latch_timeouts\":%lu,"
      "\"ev_drops\":%lu,\"pub_drops\":%lu,\"rej\":0,"
      "\"task_max_gap_ms\":%lu,\"mqtt_attempts\":%lu,\"rssi\":%d,\"heap\":%lu}",
      (unsigned long)pulseCount, (int)lastRaw, (int)lastSpan,
      qualityFromSpan(lastSpan) == 2 ? "OK" : (qualityFromSpan(lastSpan) == 1 ? "MARGINAL" : "UNAVAILABLE"),
      speedStateName(speedState), (unsigned long)reacqCount, (unsigned long)latchTimeouts,
      (unsigned long)eventDrops, (unsigned long)pubDrops,
      (unsigned long)sensorTaskMaxGapMs, MQTT_ATTEMPTS,
      CURRENT_RSSI, (unsigned long)ESP.getFreeHeap());
    pub(T_STATUS, b, false);
    sensorTaskMaxGapMs = 0;   // windowed, like the locomotive's loopstat
  }

  delay(2);
}
