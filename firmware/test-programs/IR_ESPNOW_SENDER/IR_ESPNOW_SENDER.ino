/*
 * ============================================================================
 * IR_TEST_CAR_ESPNOW 1.0 — Stage-A IR speed sender
 * ============================================================================
 * ROLE (decision 0018): diagnostic/instrument, on the proven IR Test Car
 * ESP32. Samples the 10-spoke wheel, computes qualified speed locally, and
 * sends cumulative evidence to Toby by ESP-NOW. No motor, navigation, MQTT,
 * dispatcher, station, CTO, or locomotive authority of any kind.
 *
 * Spec: docs/IR_TEST_CAR_ESPNOW_FIRMWARE_SPEC.md (Draft 1, 2026-08-15).
 * Parent: docs/QUORUM_IR_INTEGRATION_SPEC.md. Wire: firmware/common/
 * IRSpeedWire.h, 72 bytes, frozen. Receiver: QUORUM_1_16R_IR_TEST_A.
 *
 * LINEAGE (spec §2): the acquisition below is IR_SPEED_LOCAL_1_2's sampler,
 * copied without retuning — same pin, envelope, hysteresis, median, spans,
 * timeouts, validity meanings, task pinning and snapshot discipline. The
 * implementation report must diff the acquisition lines against the frozen
 * reference; radio additions are not acquisition differences.
 *
 * DELIBERATE REMOVALS (spec §3): no IR_LOCAL_WIFI, no PubSubClient, no
 * broker, no credentials.h, no association, no NTP. The radio is WIFI_STA
 * mode WITHOUT association, pinned to the EAP's channel so Toby (an
 * associated station on that channel) can hear unicast frames.
 *
 * THE ONE NEW MEASUREMENT SEMANTIC (spec §2/§6, decision 0036): wire state
 * STOPPED. Derived by the PACKET-PRODUCTION layer — never the sampler — when
 * completedPulses has not advanced for IR_STOPPED_MS. A wheel parked on a
 * spoke holds the optical signal wherever it happens to sit; the acquisition
 * layer may show INVALID_CONTRAST or an open pulse, and the wire still says
 * STOPPED/0.00, because "the wheel has not turned for 2.5 s" is the honest,
 * useful statement. It is an observation, not proof of cause: a blinded
 * sensor imitates it, which is why zero NEVER grants upward motor authority
 * downstream (0036). Acquisition state, envelope, aborts and counters are
 * untouched by this derivation.
 *
 * CUMULATIVE EVIDENCE IS THE CONTRACT (parent spec §5.2): every packet
 * carries the monotonic completedPulses. Lose three packets and the fourth
 * still delivers the entire distance. There is no queue, no retransmission,
 * no catch-up burst — the latest snapshot supersedes everything.
 * ============================================================================
 */

// Uncomment only for commissioning. Adds per-pulse SERIAL output, never MQTT.
// #define IR_LOCAL_DEBUG

#include <Arduino.h>
#include <esp_timer.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include "../../common/IRSpeedWire.h"
#include "ir_espnow_config.h"

#define SKETCH_NAME "IR_TEST_CAR_ESPNOW_1_0"

// ---------------------------------------------------------------------------
// Proven hardware and rolling calibration — IR_SPEED_LOCAL_1_2, unchanged.
// ---------------------------------------------------------------------------
static constexpr int      SENSOR_PIN             = 34;       // ADC1_CH6
static constexpr int      SPOKES_PER_REV          = 10;
static constexpr float    WHEEL_CIRCUMFERENCE_MM  = 96.52f;   // measured 3.8 in
static constexpr float    MM_PER_PULSE            = WHEEL_CIRCUMFERENCE_MM /
                                                     SPOKES_PER_REV;

static constexpr uint32_t SAMPLE_PERIOD_US        = 1000;     // nominal 1 kHz
static constexpr uint32_t SPEED_STALE_MS          = 2500;
static constexpr uint32_t OPEN_ABORT_MS           = 2500;
static constexpr int      MIN_USABLE_SPAN         = 120;
static constexpr int      MARGINAL_SPAN           = 300;

static constexpr int      ENV_WIN_N               = 2048;
static constexpr int      ENV_PRIME_N             = 512;
static constexpr uint32_t ENV_UPDATE_MS           = 250;
static constexpr int      ENV_PCT_LO              = 5;
static constexpr int      ENV_PCT_HI              = 95;

static constexpr int      SPEED_WINDOW_N          = 5;
// Intervals required before the first VALID after an edge-silent episode.
// median5() already medians over whatever count the ring holds, so smoothing
// deepens 3 -> 5 on its own as samples arrive; this only sets how early the
// measurement is allowed to be called VALID.
//
// THREE IS A FLOOR, NOT A PREFERENCE. Median-of-3 rejects one outlier;
// median-of-2 returns the larger of two, so a single bad interval would be
// reported as the speed. The bad interval is not hypothetical — see the
// edge-silence reset below.
//
// Measured 2026-08-18 (Otto, 55 stops, ~74 min of motion): IR held VALID for
// 98% of moving time and recovered from every stop within one 1 Hz sample,
// reporting down to 26 mm/s. This shortens an already-short window; it does
// not rescue a broken one.
static constexpr int      SPEED_WINDOW_MIN        = 3;

// Acquisition validity — IR_SPEED_LOCAL_1_2's enum, unchanged. STOPPED is
// NOT here: it is a wire state, derived at packet production (spec §2).
enum Validity : uint8_t {
  IR_INVALID_CONTRAST,
  IR_MARGINAL,
  IR_REACQUIRING,
  IR_VALID,
  IR_STALE
};

struct LocalSpeedSnapshot {
  uint32_t capturedMs;
  uint32_t completedPulses;
  uint32_t lastIntervalMs;
  float    speedMmPerSec;
  int      raw;
  int      runMin;
  int      runMax;
  int      span;
  Validity validity;
};

struct HealthCounters {
  uint32_t rises;
  uint32_t completedPulses;
  uint32_t sampleMissedSlots;
  uint32_t openAborts;
  // Wheel parked with a spoke across the beam. Split out of openAborts so that
  // counter can go back to meaning "a fault happened". LOCAL ONLY — this is
  // deliberately not on the wire: IrSpeedPacketV1 is frozen at 72 bytes with
  // pinned field offsets, and a diagnostic counter does not justify a V2
  // packet. It reaches the operator on the 5 s Serial health line.
  uint32_t spokeParks;
  uint32_t contrastInvalidEpisodes;
  uint32_t saturatedSamples;
  uint32_t sampleMaxGapUs;
};

static portMUX_TYPE snapshotMux = portMUX_INITIALIZER_UNLOCKED;
static LocalSpeedSnapshot latest = {};
static HealthCounters health = {};

// Sampler-owned envelope.
static uint8_t  envWin[ENV_WIN_N];
static uint16_t envHist[256];
static int envIndex = 0;
static int envFilled = 0;
static int runMin = 0, runMax = 0;
static bool envelopePrimed = false;

static const char* validityName(uint8_t v) {
  switch (v) {
    case IR_V_VALID:       return "VALID";
    case IR_V_MARGINAL:    return "MARGINAL";
    case IR_V_REACQUIRING: return "REACQUIRING";
    case IR_V_STALE:       return "STALE";
    case IR_V_STOPPED:     return "STOPPED";
    default:               return "INVALID_CONTRAST";
  }
}

static uint32_t median5(const uint32_t* values, int count) {
  uint32_t sorted[SPEED_WINDOW_N];
  for (int i = 0; i < count; ++i) sorted[i] = values[i];
  for (int i = 1; i < count; ++i) {
    uint32_t key = sorted[i];
    int j = i - 1;
    while (j >= 0 && sorted[j] > key) {
      sorted[j + 1] = sorted[j];
      --j;
    }
    sorted[j + 1] = key;
  }
  return count ? sorted[count / 2] : 0;
}

static void publishLocalSnapshot(uint32_t nowMs,
                                 uint32_t completed,
                                 uint32_t intervalMs,
                                 float speed,
                                 int raw,
                                 int lo,
                                 int hi,
                                 Validity validity) {
  portENTER_CRITICAL(&snapshotMux);
  latest.capturedMs = nowMs;
  latest.completedPulses = completed;
  latest.lastIntervalMs = intervalMs;
  latest.speedMmPerSec = speed;
  latest.raw = raw;
  latest.runMin = lo;
  latest.runMax = hi;
  latest.span = hi - lo;
  latest.validity = validity;
  portEXIT_CRITICAL(&snapshotMux);
}

static void updateEnvelope(int raw, uint32_t nowMs, uint32_t& lastUpdateMs) {
  uint8_t bucket = (uint8_t)(raw >> 4);
  if (envFilled == ENV_WIN_N) envHist[envWin[envIndex]]--;
  envWin[envIndex] = bucket;
  envHist[bucket]++;
  envIndex = (envIndex + 1) % ENV_WIN_N;
  if (envFilled < ENV_WIN_N) envFilled++;

  if ((uint32_t)(nowMs - lastUpdateMs) < ENV_UPDATE_MS || envFilled < 64) return;
  lastUpdateMs = nowMs;

  const int needLo = (envFilled * ENV_PCT_LO) / 100;
  const int needHi = (envFilled * ENV_PCT_HI) / 100;
  int cumulative = 0, loBucket = -1, hiBucket = -1;
  for (int b = 0; b < 256; ++b) {
    cumulative += envHist[b];
    if (loBucket < 0 && cumulative > needLo) loBucket = b;
    if (hiBucket < 0 && cumulative >= needHi) {
      hiBucket = b;
      break;
    }
  }
  if (loBucket >= 0 && hiBucket >= 0) {
    runMin = loBucket * 16;
    runMax = hiBucket * 16 + 15;
  }
  envelopePrimed = envFilled >= ENV_PRIME_N;
}

static void sensorTask(void*) {
  bool inPulse = false;
  bool awaitLowAfterAbort = false;
  bool contrastInvalid = true;
  bool openAbortCounted = false;
  uint32_t riseMs = 0, previousRiseMs = 0, lastCompletedMs = 0;
  uint32_t lastEnvelopeUpdateMs = millis();
  uint32_t intervalRing[SPEED_WINDOW_N] = {};
  int intervalIndex = 0, intervalCount = 0;
  int strongIntervalCount = 0;
  float speed = 0.0f;
  HealthCounters localHealth = {};
  uint32_t lastHealthSnapshotMs = 0;

  int64_t previousSampleUs = esp_timer_get_time();
  TickType_t wake = xTaskGetTickCount();

  for (;;) {
    const int64_t sampleUs = esp_timer_get_time();
    const uint32_t gapUs = (uint32_t)(sampleUs - previousSampleUs);
    previousSampleUs = sampleUs;
    if (gapUs > localHealth.sampleMaxGapUs) localHealth.sampleMaxGapUs = gapUs;
    if (gapUs > SAMPLE_PERIOD_US + SAMPLE_PERIOD_US / 2) {
      uint32_t slots = (gapUs + SAMPLE_PERIOD_US / 2) / SAMPLE_PERIOD_US;
      if (slots > 1) localHealth.sampleMissedSlots += slots - 1;
      wake = xTaskGetTickCount(); // resynchronise; never fabricate catch-up samples
    }

    const uint32_t now = millis();
    const int raw = analogRead(SENSOR_PIN);
    if (raw >= 4000) localHealth.saturatedSamples++;
    updateEnvelope(raw, now, lastEnvelopeUpdateMs);
    const int span = runMax - runMin;
    const bool contrastGood = envelopePrimed && span >= MIN_USABLE_SPAN;

    if (!contrastGood) {
      if (!contrastInvalid) localHealth.contrastInvalidEpisodes++;
      contrastInvalid = true;
      awaitLowAfterAbort = awaitLowAfterAbort || inPulse;
      inPulse = false;
      openAbortCounted = false;
      previousRiseMs = 0;
      intervalCount = 0;
      intervalIndex = 0;
      strongIntervalCount = 0;
      speed = 0.0f;
      publishLocalSnapshot(now, localHealth.completedPulses, 0, 0.0f,
                           raw, runMin, runMax, IR_INVALID_CONTRAST);
    } else {
      contrastInvalid = false;
      const bool contrastMarginal = span < MARGINAL_SPAN;
      // Declared here rather than beside its first use further down: the
      // spoke-park test below needs it, and both readings must come from the
      // same sample of `now`.
      const bool edgeSilent = (lastCompletedMs == 0 ||
                               (uint32_t)(now - lastCompletedMs) > SPEED_STALE_MS);
      const int thresholdHigh = runMin + (span * 2) / 3;
      const int thresholdLow  = runMin + span / 3;

      if (contrastMarginal) {
        strongIntervalCount = 0;
        if (latest.validity != IR_MARGINAL) {
          publishLocalSnapshot(now, localHealth.completedPulses, 0, 0.0f,
                               raw, runMin, runMax, IR_MARGINAL);
        }
      }

      // A pulse held open past OPEN_ABORT_MS has two quite different causes, and
      // charging both to openAborts made that counter mean "something optical
      // went wrong" when half the time it meant "the train parked."
      //
      //   parked on a spoke : the wheel stopped with a spoke across the beam.
      //                       Ordinary, expected, and self-clearing on the next
      //                       departure — the straddling spoke's rise happened
      //                       before the stop and its fall happens after.
      //   genuine abort     : edges were arriving and then stopped while the
      //                       wheel was still turning. That is a fault.
      //
      // The two are told apart by whether edges had already fallen silent when
      // the pulse hung: a park is edge-silent by definition, an abort is not.
      // Both still reset acquisition identically — only the bookkeeping differs.
      //
      // The straddling spoke stays uncounted, and that is a deliberate
      // under-count of about 9.652 mm per park, not an oversight. At the moment
      // the pulse hangs, that spoke has not finished passing, and this sensor
      // counts edges without direction: a wheel that creeps back off the spoke
      // it was parked on retreats over the same edge it arrived by. Counting on
      // departure would assume a forward completion the sensor cannot see, and
      // inventing distance is worse than owing it.
      if (inPulse && (uint32_t)(now - riseMs) > OPEN_ABORT_MS) {
        if (!openAbortCounted) {
          if (edgeSilent) localHealth.spokeParks++;
          else            localHealth.openAborts++;
        }
        openAbortCounted = true;
        inPulse = false;
        awaitLowAfterAbort = true;
        previousRiseMs = 0;
        intervalCount = 0;
        intervalIndex = 0;
        strongIntervalCount = 0;
        speed = 0.0f;
      }

      if (awaitLowAfterAbort && raw < thresholdLow) {
        awaitLowAfterAbort = false;
      }

      if (!inPulse && !awaitLowAfterAbort && raw > thresholdHigh) {
        inPulse = true;
        openAbortCounted = false;
        riseMs = now;
        localHealth.rises++;
      } else if (inPulse && raw < thresholdLow) {
        inPulse = false;
        localHealth.completedPulses++;
        lastCompletedMs = now;

        uint32_t intervalMs = 0;
        if (previousRiseMs != 0) {
          intervalMs = riseMs - previousRiseMs;
          // Decision 0004: every measured nonzero interval enters.
          if (intervalMs > 0) {
            intervalRing[intervalIndex] = intervalMs;
            intervalIndex = (intervalIndex + 1) % SPEED_WINDOW_N;
            if (intervalCount < SPEED_WINDOW_N) intervalCount++;
            if (contrastMarginal) {
              strongIntervalCount = 0;
            } else if (strongIntervalCount < SPEED_WINDOW_N) {
              strongIntervalCount++;
            }
            uint32_t medMs = median5(intervalRing, intervalCount);
            if (medMs > 0) speed = MM_PER_PULSE * 1000.0f / medMs;
          }
        }
        previousRiseMs = riseMs;

        const Validity validity = contrastMarginal ? IR_MARGINAL
                                : (intervalCount >= SPEED_WINDOW_MIN &&
                                   strongIntervalCount >= SPEED_WINDOW_MIN)
                                  ? IR_VALID : IR_REACQUIRING;
        publishLocalSnapshot(now, localHealth.completedPulses, intervalMs, speed,
                             raw, runMin, runMax, validity);
#ifdef IR_LOCAL_DEBUG
        Serial.printf("[IR] pulse=%lu int=%lu speed=%.2f valid=%u span=%d\n",
                      (unsigned long)localHealth.completedPulses,
                      (unsigned long)intervalMs, speed,
                      (unsigned)validity, span);
#endif
      }

      // "No interval may span an edge-silence episode" — now enforced whatever
      // the contrast is. This reset used to sit behind !contrastMarginal, which
      // withheld it in exactly the case it was written for: a stopped wheel
      // settles the optical signal onto one plateau, the rolling envelope
      // narrows toward that plateau, span drops into the marginal band, and the
      // guard stops running. previousRiseMs then survived the whole dwell, so
      // the first interval measured after departure was the length of the STOP
      // — 26 s at a Bamboo station stand — and only failed to poison the output
      // because median-of-5 outvoted it. That made SPEED_WINDOW_N load-bearing
      // for correctness by accident rather than for smoothing by design, and it
      // is why SPEED_WINDOW_MIN could not otherwise be lowered.
      //
      // Field evidence 2026-08-18 (Otto, 55 stops): abrt=0 and cinv=0, i.e. no
      // stop that session took the open-abort or contrast-invalid path. Every
      // one of them came through here.
      if (edgeSilent && previousRiseMs != 0) {
        previousRiseMs = 0;
        intervalCount = 0;
        intervalIndex = 0;
        strongIntervalCount = 0;
        speed = 0.0f;
      }

      if (!contrastMarginal && edgeSilent) {
        if (latest.validity != IR_STALE) {
          publishLocalSnapshot(now, localHealth.completedPulses, 0, 0.0f,
                               raw, runMin, runMax, IR_STALE);
        }
      }
    }

    if ((uint32_t)(now - lastHealthSnapshotMs) >= 100) {
      lastHealthSnapshotMs = now;
      portENTER_CRITICAL(&snapshotMux);
      health = localHealth;
      portEXIT_CRITICAL(&snapshotMux);
    }

    vTaskDelayUntil(&wake, pdMS_TO_TICKS(1));
  }
}
// --------------------------- end of IR_SPEED_LOCAL_1_2 acquisition ----------

// ---------------------------------------------------------------------------
// Stage-A radio (spec §4, §6). Everything below is new; nothing above changed.
// ---------------------------------------------------------------------------
static bool     radioUp = false;
static uint32_t bootId = 0;
static uint32_t txSequence = 0;
static uint32_t txAttempts = 0, txImmediateErrors = 0;
static volatile uint32_t txDeliveryFailures = 0;   // send-callback context
static uint32_t speedRangeClips = 0;               // valid speed too big for wire
static uint32_t lastTxMs = 0, lastHealthLineMs = 0;

// STOPPED derivation state — packet-production layer only (spec §6).
static uint32_t lastPulseCount = 0;
static uint32_t lastPulseProgressMs = 0;

// WiFi-task context: bounded counter update only. Broadcast would report
// unconditional success; this is UNICAST, so FAIL means no MAC-layer ACK —
// the one delivery fact ESP-NOW can actually give us (unlike the CTO
// broadcast path, where txd is only "reached the air"; see 1.16Ra).
static void onSend(const wifi_tx_info_t*, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) txDeliveryFailures++;
}

static bool macUnusable(const uint8_t* m) {
  bool allZero = true;
  for (int i = 0; i < 6; ++i) if (m[i] != 0) allZero = false;
  const bool multicast = (m[0] & 0x01) != 0;     // includes broadcast
  return allZero || multicast;
}

static void radioInit() {
  // Order per spec §4. Any failure in steps 2-5 leaves sampling operational
  // and transmission disabled — one factual line, no reboot loop.
  if (macUnusable(TOBY_STA_MAC)) {
    Serial.println("[RADIO] target MAC unset/unusable — TX DISABLED, measuring locally");
    return;
  }
  if (esp_wifi_set_channel(IR_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    Serial.println("[RADIO] set_channel failed — TX DISABLED");
    return;
  }
  if (esp_now_init() != ESP_OK) {
    Serial.println("[RADIO] esp_now_init failed — TX DISABLED");
    return;
  }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, TOBY_STA_MAC, 6);
  peer.channel = IR_ESPNOW_CHANNEL;
  peer.encrypt = false;   // Stage A: receiver has no authority (spec §4);
                          // encryption is an explicit decision before any
                          // authority stage, never a flag flip.
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("[RADIO] add_peer failed — TX DISABLED");
    return;
  }
  esp_now_register_send_cb(onSend);
  radioUp = true;
}

static void sendSnapshot() {
  LocalSpeedSnapshot s;
  HealthCounters h;
  portENTER_CRITICAL(&snapshotMux);
  s = latest;
  h = health;
  portEXIT_CRITICAL(&snapshotMux);
  // No lock held from here on (spec §6).

  const uint32_t now = millis();
  const bool stopped = (uint32_t)(now - lastPulseProgressMs) >= IR_STOPPED_MS;

  IrSpeedPacketV1 p = {};
  p.magic        = IR_SPEED_MAGIC;
  p.version      = IR_SPEED_VERSION;
  p.bytes        = sizeof(IrSpeedPacketV1);
  p.sensorId     = IR_SENSOR_ID;
  p.targetLocoId = IR_TARGET_LOCO_ID;
  p.bootId       = bootId;
  p.txSequence   = ++txSequence;
  p.capturedMs   = s.capturedMs;
  p.completedPulses = s.completedPulses;
  p.lastIntervalMs  = s.lastIntervalMs;
  p.opticalSpan  = (uint16_t)constrain(s.span, 0, 65535);
  p.reserved     = 0;

  // Canonical encoding (wire header): structural, checked before conversion.
  if (stopped) {
    p.validity = IR_V_STOPPED;
    p.speedMmpsX100 = 0;
  } else if (s.validity == IR_VALID) {
    // Range check before scaling: a speed too large for the wire is
    // transmitted as non-valid, never wrapped (spec §6). Not expected on
    // the railway: the ceiling is ~42.9 million mm/s.
    float scaled = s.speedMmPerSec * 100.0f;
    if (scaled >= 0.0f && scaled < (float)IR_SPEED_INVALID_X100) {
      p.validity = IR_V_VALID;
      p.speedMmpsX100 = (uint32_t)scaled;
    } else {
      speedRangeClips++;
      p.validity = IR_V_REACQUIRING;          // non-valid, honestly
      p.speedMmpsX100 = IR_SPEED_INVALID_X100;
    }
  } else {
    p.validity = (uint8_t)s.validity;         // acquisition enum 0-4 unchanged
    p.speedMmpsX100 = IR_SPEED_INVALID_X100;
  }

  p.sampleMissedSlots       = h.sampleMissedSlots;
  p.openAborts              = h.openAborts;
  p.contrastInvalidEpisodes = h.contrastInvalidEpisodes;
  p.saturatedSamples        = h.saturatedSamples;
  p.sampleMaxGapUs          = h.sampleMaxGapUs;
  p.txAttempts              = txAttempts;
  p.txImmediateErrors       = txImmediateErrors;
  p.txDeliveryFailures      = txDeliveryFailures;

  txAttempts++;
  if (esp_now_send(TOBY_STA_MAC, (const uint8_t*)&p, sizeof(p)) != ESP_OK)
    txImmediateErrors++;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  analogReadResolution(12);
  pinMode(SENSOR_PIN, INPUT);

  bootId = esp_random();
  if (bootId == 0) bootId = 1;   // nonzero per spec §6

  // Radio mode without association (spec §4 step 1).
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setSleep(false);

  Serial.printf("=== %s pin=%d spokes=%d circ=%.2fmm mm/pulse=%.3f boot=%08lX ===\n",
                SKETCH_NAME, SENSOR_PIN, SPOKES_PER_REV,
                WHEEL_CIRCUMFERENCE_MM, MM_PER_PULSE, (unsigned long)bootId);
  // BENCH FIX (2026-08-18): WiFi.macAddress() read immediately after
  // WiFi.mode(WIFI_STA) came back all-zero on this core — the netif had not
  // yet latched the hardware MAC. esp_read_mac() reads the factory-burned
  // eFuse value directly, with no dependency on WiFi stack timing at all.
  uint8_t myMac[6];
  esp_read_mac(myMac, ESP_MAC_WIFI_STA);
  Serial.printf("[RADIO] local MAC %02X:%02X:%02X:%02X:%02X:%02X  target %02X:%02X:%02X:%02X:%02X:%02X  ch=%u  target_loco=%lu\n",
                myMac[0], myMac[1], myMac[2], myMac[3], myMac[4], myMac[5],
                TOBY_STA_MAC[0], TOBY_STA_MAC[1], TOBY_STA_MAC[2],
                TOBY_STA_MAC[3], TOBY_STA_MAC[4], TOBY_STA_MAC[5],
                (unsigned)IR_ESPNOW_CHANNEL, (unsigned long)IR_TARGET_LOCO_ID);

  radioInit();

  // Sampler start (spec §4 step 6). Same task parameters as the reference.
  // Failure is a fatal halt: never pretend to transmit valid data (spec §8).
  if (xTaskCreatePinnedToCore(sensorTask, "ir-local", 6144, nullptr, 2,
                              nullptr, 0) != pdPASS) {
    Serial.println("[FATAL] sensor task creation failed");
    while (true) delay(1000);
  }

  // The no-progress clock starts at sampler startup: a wheel that stays
  // still is honestly STOPPED 2.5 s from now (spec §6).
  lastPulseProgressMs = millis();
}

void loop() {
  const uint32_t now = millis();

  // STOPPED derivation: cumulative pulses are the only witness. Maintained
  // HERE, unconditionally — the first draft kept it inside sendSnapshot(),
  // so with the radio disabled (the shipped all-zero-MAC commissioning
  // state) the clock never advanced and the heartbeat claimed STOPPED
  // forever while the wheel was spinning. Found by the adversarial pass;
  // exactly the mode where the operator is watching that line.
  {
    uint32_t pulses;
    portENTER_CRITICAL(&snapshotMux);
    pulses = latest.completedPulses;
    portEXIT_CRITICAL(&snapshotMux);
    if (pulses != lastPulseCount) {
      lastPulseCount = pulses;
      lastPulseProgressMs = now;
    }
  }

  if (radioUp && (uint32_t)(now - lastTxMs) >= IR_TX_INTERVAL_MS) {
    lastTxMs = now;
    sendSnapshot();
  }

  // Field Serial contract (spec §7): one health line each 5 s, nothing else.
  if ((uint32_t)(now - lastHealthLineMs) >= 5000) {
    lastHealthLineMs = now;
    LocalSpeedSnapshot s;
    HealthCounters h;
    portENTER_CRITICAL(&snapshotMux);
    s = latest;
    h = health;
    portEXIT_CRITICAL(&snapshotMux);
    const bool stopped = (uint32_t)(now - lastPulseProgressMs) >= IR_STOPPED_MS;
    Serial.printf("[HB] state=%s pulses=%lu span=%d missed=%lu aborts=%lu parks=%lu "
                  "sat=%lu maxgap=%luus tx=%lu txe=%lu txf=%lu clips=%lu ch=%u radio=%d\n",
                  stopped ? "STOPPED" : validityName((uint8_t)s.validity),
                  (unsigned long)h.completedPulses, s.span,
                  (unsigned long)h.sampleMissedSlots,
                  (unsigned long)h.openAborts,
                  (unsigned long)h.spokeParks,
                  (unsigned long)h.saturatedSamples,
                  (unsigned long)h.sampleMaxGapUs,
                  (unsigned long)txAttempts, (unsigned long)txImmediateErrors,
                  (unsigned long)txDeliveryFailures,
                  (unsigned long)speedRangeClips,
                  (unsigned)IR_ESPNOW_CHANNEL, radioUp ? 1 : 0);
  }

  delay(5);
}
