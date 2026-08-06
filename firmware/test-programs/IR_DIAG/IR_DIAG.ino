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
 *   PULSE #   42  int=  187ms  w=  41ms  peak= +312  raw=2054 rm= +410 fm= +480 base=2742 span= 980  OK
 *   IDLE          raw=2741  env=2260/3240  seen=2231/3271  base=2750 span= 980  thr= +163/ -163  pulses=42
 *   STATS 10s  n=52 rate=5.2/s | int med=190 min=181 max=203 jit=6 | w med=42 |
 *              peak med=308 | rm med=+410 p10=+330 | fm med=+480 p10=+390 |
 *              sat=0 miss=0 latch=0 closs=0 drops=0 | ~121mm/s
 *   PHASE  0: rm +412 fm +788  n=41       (one line per spoke phase, after STATS)
 *   LATCH #  1  w=2501ms fm= -210 rm= -105 base=2750 span= 980 — pulse DISCARDED, no event
 *
 * fm is the falling-edge margin (thrLow minus raw at the fall), rm the rising
 * margin (raw minus thrHigh at the rise): how far each crossing cleared its
 * threshold. Small positive margins mean noise is deciding which edges
 * complete; which of the two sits lower tells you which edge governs. The
 * PHASE lines break both down by spoke position — seven uniform spokes
 * should be flat, and a phase-locked dip is runout, sensor angle, or one bad
 * spoke, seen directly. env= is the rolling-percentile envelope, seen= the
 * true min/max of the same window; when they disagree, the envelope is
 * stale. closs= counts contrast-loss discards (distinct from latch=).
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
 * ARCHITECTURE — inherited from IR_TEST (formerly Spoke_IR_RSSI_survey_v2), unchanged
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
  int      fallMargin;     // thrLow - rawAtEdge at the fall: how much below
                           // the threshold the signal actually went. Small
                           // positive = marginal; this number predicts a latch
  int      riseMargin;     // rawAtRise - thrHigh at the rise: how far the
                           // first supra-threshold sample cleared the rising
                           // threshold. The rising-edge counterpart of
                           // fallMargin — which edge governs is the open
                           // question the daylight run answers
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
// 7-spoke finescale steel wheel — the committed production target, and the
// first to produce uniform intervals across every phase position. UNCONFIRMED
// until counted: hand-turn one full revolution and check pulseCount advances
// by exactly 7 before trusting any speed figure this sketch prints.
const int   SPOKES_PER_WHEEL = 7;

// Placeholder. The ~109 mm figure derived on 2026-08-05 was the LGB wheel
// and does NOT carry over to the finescale wheel; measure this wheel before
// trusting mm/s or pkph. fm, rm, int, w and the PHASE breakdown are all
// calibration-independent and correct regardless — only speed is wrong.
const float WHEEL_CIRCUMFERENCE_MM = 115.0f;

const uint32_t SENSOR_TICK_MS = 1;       // 1 kHz
const uint32_t DEBOUNCE_US    = 15000;   // 15 ms — edge-doubling guard

const int      ADC_MAX          = 4095;
const int      SATURATION_LEVEL = 4000;  // above this the sensor is blinded
const int      MIN_USABLE_SPAN  = 120;   // below this, refuse to emit edges
const int      MARGINAL_SPAN    = 300;   // below this, flag the read

// ---------------------------------------------------------------------------
// ENVELOPE — rolling percentiles, NOT latched extremes.
//
// The previous envelope latched the all-time min/max and relaxed them toward
// the midpoint by DECAY_STEP (2) counts per 250 ms. One spurious extreme
// poisoned it for minutes: on the 2026-08-05 finescale-wheel run a boot/wheel-
// change transient anchored runMin ~2100 counts below anything the sensor was
// actually seeing (base 1827, span 3528 => runMin ~63, against a signal that
// never went below ~1900). Because both thresholds are fractions of span, the
// inflated span dragged thrLow up to ~2415 against a real dark state of
// ~2200 — a falling-edge margin of +164 median, +21 at p10, across 1170
// pulses. Noise decided which falls completed; ~10% did not, and each miss
// latched inPulse through every following spoke (observed widths 2621 and
// 4073 ms).
//
// Now the bounds are the 5th and 95th PERCENTILES of the raw samples in a
// rolling ~2 s window, recomputed every ENV_UPDATE_MS from a 256-bucket
// histogram (O(1) per sample in/out, O(256) per update). Chosen over fast
// asymmetric decay because it is robust BY CONSTRUCTION, not by tuned rate:
// a single outlier sample moves a 5th percentile of 2048 samples by nothing
// at all, and every sample ages out of the window in <= ~2 s. Replaying the
// same 1170 pulses against these bounds gives a falling-edge margin of
// ~+774 median, ~+603 at p10.
//
// A stationary wheel presents a flat trace, the percentiles converge, span
// drops below MIN_USABLE_SPAN, and the sketch honestly reports NO USABLE
// CONTRAST instead of emitting noise edges — for a diagnostic that is the
// correct answer, and the moment the wheel turns the window refills within
// two seconds.
// ---------------------------------------------------------------------------
const int      ENV_WIN_N     = 2048;     // samples in the window (~2 s @ 1 kHz)
const int      ENV_PRIME_N   = 512;      // samples before edges may be emitted
const uint32_t ENV_UPDATE_MS = 250;      // percentile recompute cadence
const int      ENV_PCT_LO    = 5;        // runMin = this percentile
const int      ENV_PCT_HI    = 95;       // runMax = this percentile

// LATCH TIMEOUT — inPulse must not be able to stay true forever. Every
// genuine width in the 2026-08-05 log is under ~210 ms; the two latch
// artefacts were 2621 and 4073 ms. 2500 ms sits an order of magnitude above
// any real pulse (even the stale 2-flag constants at a 20 mm/s creep bound
// give ~2.9 s for a HALF revolution, and no optical feature spans half the
// wheel) while catching every latch this sensor has produced. On expiry the
// incomplete pulse is DISCARDED — no event, no interval, no width. A
// 4-second "pulse" entering the record is worse than no data.
const uint32_t LATCH_TIMEOUT_MS = 2500;

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

// Envelope bounds and the ACTUAL observed extremes of the same window, both
// for the IDLE line — a stale bound is visible at a glance when env and seen
// disagree, instead of being inferred from arithmetic three hours later.
static volatile int lastRunMin = 0, lastRunMax = 0;   // percentile bounds
static volatile int lastSeenMin = 0, lastSeenMax = 0; // true window min/max

// Latch-timeout accounting. Counter is monotonic and published in STATS;
// loop() watches it to print a LATCH line with the duration.
//
// INTERPRETATION CAVEAT, 2026-08-05: without a motion witness, a stop with
// a spoke parked in front of the sensor trips this timer and is
// indistinguishable from a real sensor latch. For the daylight run,
// latchTimeouts is UNINTERPRETABLE as a fault count — the abort itself is
// still doing the job that matters (the state machine cannot wedge), and
// the LATCH line carries the numbers (width, fm, rm, baseline, span) from
// which a genuine stop and a real latch CAN be told apart, so this run
// accumulates the evidence to design the split properly: bounded abort
// with floor and ceiling, separate from fault classification requiring a
// witness. (A 20 x median timeout was proposed and withdrawn: the median
// stops updating during the very latch being diagnosed — a guard taking
// its input from the thing that has failed.)
static volatile uint32_t latchTimeouts  = 0;
static volatile uint32_t lastLatchDurMs = 0;
static volatile int      lastLatchFm    = 0;   // thrLow - raw at the abort
static volatile int      lastLatchRm    = 0;   // raw - thrHigh at the abort
static volatile int      lastLatchBase  = 0;
static volatile int      lastLatchSpan  = 0;

// Contrast-loss discards: the envelope collapsed (or was not yet primed)
// while a pulse was open or an interval anchor stood, and both were thrown
// away rather than letting the first post-blind pulse report an interval
// spanning the outage. Distinct from latchTimeouts. Published in STATS.
static volatile uint32_t contrastLosses = 0;

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
static int  runMin = 0, runMax = 0;      // percentile bounds; valid once primed
static bool envelopePrimed = false;      // ENV_PRIME_N samples in the window

// Rolling-window histogram. Samples are stored as 256-bucket indices
// (raw >> 4, 16 counts per bucket) so the window costs 2 KB and a percentile
// is an O(256) cumulative scan. Bucket resolution (16 counts) is noise-level
// against margins measured in hundreds.
static uint8_t  envWin[ENV_WIN_N];
static uint16_t envHist[256];
static int      envWinIdx = 0, envWinFilled = 0;

static uint8_t qualityFromSpan(int span) {
  if (span < MIN_USABLE_SPAN) return 0;
  if (span < MARGINAL_SPAN)   return 1;
  return 2;
}

static void sensorTask(void*) {
  bool     inPulse = false;
  uint32_t lastEdgeMicros = 0, pulseStartMicros = 0, pulseStartMs = 0;
  uint32_t prevRiseMs = 0, lastEnvUpdateMs = millis();
  int      peakDelta = 0;
  int      riseMarginAtRise = 0;   // captured at the rise, travels to the emit
  bool     sawSaturation = false;

  for (;;) {
    int raw = analogRead(SENSOR_PIN);      // a TASK, not an ISR — safe
    uint32_t now = millis();
    lastRaw = raw;

    if (raw >= SATURATION_LEVEL) { satSamples++; sawSaturation = true; }

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
      int cum = 0, pLo = -1, pHi = -1, seenLo = -1, seenHi = -1;
      for (int b = 0; b < 256; b++) {
        int c = envHist[b];
        if (c == 0) continue;
        if (seenLo < 0) seenLo = b;
        seenHi = b;
        cum += c;
        if (pLo < 0 && cum > needLo)  pLo = b;
        if (pHi < 0 && cum >= needHi) pHi = b;
      }
      if (pLo >= 0 && pHi >= 0) {
        runMin = pLo * 16;                 // bucket floor
        runMax = pHi * 16 + 15;            // bucket ceiling
        lastRunMin  = runMin;  lastRunMax  = runMax;
        lastSeenMin = seenLo * 16;         // true observed extremes of the
        lastSeenMax = seenHi * 16 + 15;    //   same window, for the IDLE line
      }
      envelopePrimed = (envWinFilled >= ENV_PRIME_N);
    }

    int span     = runMax - runMin;
    int baseline = (runMin + runMax) / 2;
    lastSpan = span; lastBaseline = baseline;
    uint8_t quality = qualityFromSpan(span);

    // No edges before the window has a real population, and none from a
    // collapsed span. A flat trace (stationary wheel, dead optical path)
    // converges the percentiles, span drops below the floor, and the IDLE
    // line reports NO USABLE CONTRAST — which for a diagnostic is the truth,
    // not a failure to be papered over.
    //
    // The interval anchor dies here too. Clearing only inPulse left
    // prevRiseMs pointing at the last pulse BEFORE the blind period, so the
    // first pulse after it reported an interval spanning the whole outage —
    // arriving dressed as a normal measurement. Corrupting the record is
    // worse than failing. Counted once per episode (the guard below is
    // false again after the clear), distinct from latchTimeouts.
    if (!envelopePrimed || quality == 0) {
      if (inPulse || prevRiseMs != 0) {
        contrastLosses = contrastLosses + 1;   // plain add: volatile-safe
        inPulse = false;
        prevRiseMs = 0;
      }
      vTaskDelay(pdMS_TO_TICKS(SENSOR_TICK_MS));
      continue;
    }

    // Symmetric about the midpoint, matching the production sketch (IR_TEST
    // uses these exact expressions): rise at 2/3 of range, fall at 1/3. A
    // diagnostic that validates production behaviour must not use different
    // detection rules from production. The previous asymmetric pair
    // (baseline + span/3 rising, baseline + span/6 falling — 0.83/0.67 of
    // range, both in the upper third) was undocumented and nobody could
    // establish it was deliberate.
    int thrHigh = runMin + (span * 2) / 3;
    int thrLow  = runMin + span / 3;
    uint32_t nowMicros = micros();
    int delta = raw - baseline;

    // --- latch timeout: inPulse cannot stay true forever ------------------
    // The incomplete pulse is DISCARDED: no event, and prevRiseMs is cleared
    // so the next completed pulse cannot compute an interval spanning the
    // latch. The eventual artificial fall finds inPulse already false and
    // emits nothing.
    if (inPulse && (uint32_t)(now - pulseStartMs) > LATCH_TIMEOUT_MS) {
      inPulse = false;
      prevRiseMs = 0;
      lastLatchDurMs = now - pulseStartMs;
      lastLatchFm    = thrLow - raw;    // where the signal sat vs both
      lastLatchRm    = raw - thrHigh;   //   thresholds at the abort
      lastLatchBase  = baseline;
      lastLatchSpan  = span;
      latchTimeouts  = latchTimeouts + 1;   // plain add: volatile-safe
    }

    if (!inPulse) {
      if (raw > thrHigh && (nowMicros - lastEdgeMicros) > DEBOUNCE_US) {
        inPulse = true;
        lastEdgeMicros = nowMicros;
        pulseStartMicros = nowMicros;
        pulseStartMs = now;
        peakDelta = delta;
        riseMarginAtRise = raw - thrHigh;   // margin at the moment of crossing
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
        e.fallMargin   = thrLow - raw;     // the number that predicts a latch
        e.riseMargin   = riseMarginAtRise;
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
static int      winPeak[WIN], winFallM[WIN], winRiseM[WIN];
static int      winLen = 0, winIdx = 0;
static uint32_t statPulses = 0, statSat = 0, statMiss = 0;

// ---------------------------------------------------------------------------
// SPOKE-PHASE BREAKDOWN — the instrument that decides the daylight run.
//
// A rolling phase index (0..SPOKES_PER_WHEEL-1) advances on every consumed
// pulse and RESETS on any discard (latch timeout or contrast loss), so a
// phase histogram is never silently assembled across a blind period —
// alignment before and after a gap is unknowable, and mixing them would
// smear a phase-locked weakness flat. Absolute alignment to a physical spoke
// is arbitrary (there is no index mark); the RELATIVE profile is the finding:
// seven uniform spokes should be flat, and runout, sensor angle, or one bad
// spoke shows as one or two phases with markedly lower margins — directly,
// instead of being inferred from interval ratios hours later.
//
// Per-phase rm/fm accumulate over each STATS period (last PHASE_BUF of each
// kept for the median), printed as one PHASE line per phase, then cleared.
// ---------------------------------------------------------------------------
const int PHASE_BUF = 32;
static int      phaseRm[SPOKES_PER_WHEEL][PHASE_BUF];
static int      phaseFm[SPOKES_PER_WHEEL][PHASE_BUF];
static uint32_t phaseN[SPOKES_PER_WHEEL];
static int      phaseIdx = 0;
static bool     phaseRestarted = false;   // a discard occurred this window

static void phaseReset(bool clearStats) {
  phaseIdx = 0;
  if (clearStats) {
    for (int p = 0; p < SPOKES_PER_WHEEL; p++) phaseN[p] = 0;
  }
}

// Signed percentile over the window — used for fallMargin, where the sign IS
// the finding (medInt() takes abs(), which would hide a negative margin).
static int pctIntSigned(int* src, int n, int pct) {
  if (n <= 0) return 0;
  int t[WIN];
  for (int i = 0; i < n; i++) t[i] = src[i];
  for (int i = 1; i < n; i++) { int k = t[i]; int j = i-1;
    while (j >= 0 && t[j] > k) { t[j+1] = t[j]; j--; } t[j+1] = k; }
  return t[(n * pct) / 100];
}

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
  winFallM[winIdx] = e.fallMargin;
  winRiseM[winIdx] = e.riseMargin;
  winIdx = (winIdx + 1) % WIN;
  if (winLen < WIN) winLen++;
  statPulses++;
  if (e.saturated) statSat++;

  // Phase accumulation: this pulse belongs to the current phase slot, then
  // the index advances. Ring-overwrite keeps the last PHASE_BUF per phase.
  int p = phaseIdx;
  phaseRm[p][phaseN[p] % PHASE_BUF] = e.riseMargin;
  phaseFm[p][phaseN[p] % PHASE_BUF] = e.fallMargin;
  phaseN[p]++;
  phaseIdx = (phaseIdx + 1) % SPOKES_PER_WHEEL;
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

  // DISCARD WATCHER — before the drain, so a blind episode that just ended
  // resets the phase index before the first post-blind pulse is indexed.
  // Latch timeouts also print their line here (only loop() may emit; the
  // discard itself already happened at the tick, and nothing about the
  // aborted pulse entered the record). Contrast losses reset phase silently;
  // their count rides STATS as closs=.
  {
    static uint32_t seenLatchTimeouts = 0;
    uint32_t lt = latchTimeouts, cl = contrastLosses;   // one read each
    if (lt != seenLatchTimeouts) {
      seenLatchTimeouts = lt;
      // Everything a later design needs to split "stopped with a spoke in
      // the beam" from "sensor latched": a stop shows raw parked mid-band
      // with a healthy span; a real latch shows the band itself wrong.
      snprintf(b, sizeof(b),
        "LATCH #%3lu  w=%lums fm=%+5d rm=%+5d base=%4d span=%4d — pulse DISCARDED, no event",
        (unsigned long)lt, (unsigned long)lastLatchDurMs,
        (int)lastLatchFm, (int)lastLatchRm,
        (int)lastLatchBase, (int)lastLatchSpan);
      emit(b);
    }
    if (lt != 0 || cl != 0) {
      static uint32_t seenAny = 0;
      if (lt + cl != seenAny) {
        seenAny = lt + cl;
        // Phase alignment across a gap is unknowable: restart the index and
        // drop this window's per-phase data rather than smear a phase-locked
        // weakness flat with misaligned samples.
        phaseReset(true);
        phaseRestarted = true;
      }
    }
  }

  while (eventQueue && xQueueReceive(eventQueue, &e, 0) == pdTRUE) {
    windowPush(e);
    lastPulseMs = millis();
    const char* q = (e.quality == 2) ? "OK" : (e.quality == 1 ? "MARGINAL" : "UNAVAIL");
    snprintf(b, sizeof(b),
      "PULSE #%5lu  int=%5lums  w=%4lums  peak=%+5d  raw=%4d rm=%+5d fm=%+5d base=%4d span=%4d  %s%s",
      (unsigned long)e.seq, (unsigned long)e.intervalMs, (unsigned long)e.widthMs,
      e.peakDelta, e.rawAtEdge, e.riseMargin, e.fallMargin, e.baseline, e.span, q,
      e.saturated ? "  *SATURATED*" : "");
    emit(b);
  }

  uint32_t now = millis();

  // IDLE — proves the sensor is alive between pulses. Without this line, "no
  // output" and "no pulses" are indistinguishable, which is exactly how a
  // dead optical path gets mistaken for a stopped wheel. env= is the
  // percentile envelope; seen= is the true min/max of the same window — when
  // they disagree by more than a bucket or two, the envelope is stale, and
  // that is visible at a glance instead of via arithmetic three hours later.
  if (now - lastIdlePrint >= IDLE_PRINT_MS) {
    lastIdlePrint = now;
    int span = lastSpan, base = lastBaseline;
    snprintf(b, sizeof(b),
      "IDLE          raw=%4d  env=%4d/%4d  seen=%4d/%4d  base=%4d span=%4d  thr=%+5d/%+5d  pulses=%lu%s",
      (int)lastRaw, (int)lastRunMin, (int)lastRunMax,
      (int)lastSeenMin, (int)lastSeenMax, base, span, span/6, -(span/6),
      (unsigned long)pulseCount,
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
      "peak med=%d | rm med=%+d p10=%+d | fm med=%+d p10=%+d | sat=%lu miss=%lu latch=%lu closs=%lu drops=%lu | ~%.0fmm/s ~%.1fpkph",
      (unsigned long)(elapsed/1000), (unsigned long)statPulses, rate,
      (unsigned long)medInt_, (unsigned long)mn, (unsigned long)mx,
      (unsigned long)(mx > mn ? mx - mn : 0), (unsigned long)medW,
      medP, pctIntSigned(winRiseM, winLen, 50), pctIntSigned(winRiseM, winLen, 10),
      pctIntSigned(winFallM, winLen, 50), pctIntSigned(winFallM, winLen, 10),
      (unsigned long)statSat, (unsigned long)statMiss,
      (unsigned long)latchTimeouts, (unsigned long)contrastLosses,
      (unsigned long)eventDrops,
      mmps, mmps / PKPH_MM_PER_SEC);
    emit(b);

    // PHASE — one line per spoke phase: median rm/fm and the sample count.
    // Only meaningful while pulses were contiguous; if a discard restarted
    // the index this window, say so rather than print misaligned data.
    if (phaseRestarted) {
      emit("PHASE  note: index restarted after a discard this window — per-phase data dropped");
      phaseRestarted = false;
    }
    for (int p = 0; p < SPOKES_PER_WHEEL; p++) {
      int n = (phaseN[p] > (uint32_t)PHASE_BUF) ? PHASE_BUF : (int)phaseN[p];
      if (n == 0) continue;
      snprintf(b, sizeof(b), "PHASE %2d: rm %+4d fm %+4d  n=%lu",
               p, pctIntSigned(phaseRm[p], n, 50), pctIntSigned(phaseFm[p], n, 50),
               (unsigned long)phaseN[p]);
      emit(b);
      phaseN[p] = 0;   // windowed, like the STATS counters
    }
    statPulses = 0; statSat = 0; statMiss = 0;
  }

  delay(2);
}
