/*
 * IR_SPEED_LOCAL 1.2 — lean local IR speed prototype
 *
 * ROLE: diagnostic/prototype. This is NOT QUORUM and cannot drive a motor.
 * It proves the sensor-to-local-speed contract before any production change.
 *
 * Evidence basis (2026-08-09/10):
 *   - black LGB plastic wheel: 10 optical pulses per revolution;
 *   - measured rolling circumference: 3.8 in = 96.52 mm;
 *   - night and daylight Hall-synchronised runs counted repeatably;
 *   - MQTT detail delivery failed badly while the local counter stayed sound.
 *
 * Therefore speed is computed in the sampler task and never depends on WiFi,
 * MQTT, loop(), a publish queue, or received-message counts. MQTT publishes
 * only the latest result at 1 Hz and factual counters at 5 s. There is no
 * per-pulse MQTT stream and no backlog to flush after reconnect.
 *
 * Detection is deliberately small:
 *   rolling 5th/95th-percentile envelope (lighting changes were observed),
 *   2/3 rising and 1/3 falling hysteresis, completed-pulse intervals, and a
 *   five-interval median. No interval is rejected by comparison with prior
 *   intervals. No "missed spoke" is inferred.
 *
 * Guards retained only with evidence or an overwhelming case (decision 0020):
 *   - contrast floor: speed cannot be measured without optical contrast;
 *   - marginal output gate: stationary ADC-noise crossings were observed;
 *   - stale timeout: a governor must not consume old speed as current speed;
 *   - open-pulse abort: open pulses lasting seconds occurred in field data.
 * Every intervention has its own literal counter. "Invalid" means no current
 * trustworthy IR speed; it never means the wheel is stopped.
 */

#define IR_LOCAL_WIFI
// Uncomment only for commissioning. Adds per-pulse SERIAL output, never MQTT.
// #define IR_LOCAL_DEBUG

#include <Arduino.h>
#include <esp_timer.h>
#ifdef IR_LOCAL_WIFI
  #include <WiFi.h>
  #include <PubSubClient.h>
  #include "../../config/credentials.h"
#endif

#define SKETCH_NAME "IR_SPEED_LOCAL_1_2"

// ---------------------------------------------------------------------------
// Proven hardware and rolling calibration
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

// Rolling percentile envelope: already replay- and field-proven by IR_SCOPE.
static constexpr int      ENV_WIN_N               = 2048;
static constexpr int      ENV_PRIME_N             = 512;
static constexpr uint32_t ENV_UPDATE_MS           = 250;
static constexpr int      ENV_PCT_LO              = 5;
static constexpr int      ENV_PCT_HI              = 95;

static constexpr int      SPEED_WINDOW_N          = 5;

#ifdef IR_LOCAL_WIFI
static const char* NODE_NAME   = "IR_SPEED_SENSOR";
static const char* MQTT_BROKER = "192.168.68.142";
static constexpr int MQTT_PORT = 1883;
static constexpr uint32_t SPEED_PUBLISH_MS  = 1000;
static constexpr uint32_t STATUS_PUBLISH_MS = 5000;
static constexpr uint32_t MQTT_RETRY_MS     = 5000;
// PubSubClient's default 256-byte packet cannot hold topic + status JSON.
// The missing status beat was observed immediately after the first 1.1 flash.
static constexpr uint16_t MQTT_BUFFER_BYTES = 512;
#endif

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

#ifdef IR_LOCAL_WIFI
static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);
static char topicOnline[72], topicSpeed[72], topicStatus[72];
static uint32_t mqttAttempts = 0, mqttConnects = 0, publishFailures = 0;
#endif

#if defined(IR_LOCAL_WIFI) || defined(IR_LOCAL_DEBUG)
static const char* validityName(Validity v) {
  switch (v) {
    case IR_VALID:            return "VALID";
    case IR_MARGINAL:         return "MARGINAL";
    case IR_REACQUIRING:      return "REACQUIRING";
    case IR_STALE:            return "STALE";
    default:                  return "INVALID_CONTRAST";
  }
}
#endif

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
      // If contrast vanished with a pulse open, recovery cannot establish a
      // new rise until the old high episode physically returns below thrLow.
      // Preserve an existing abort wait for the same reason.
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
      const int thresholdHigh = runMin + (span * 2) / 3;
      const int thresholdLow  = runMin + span / 3;

      if (contrastMarginal) {
        strongIntervalCount = 0;
        if (latest.validity != IR_MARGINAL) {
          publishLocalSnapshot(now, localHealth.completedPulses, 0, 0.0f,
                               raw, runMin, runMax, IR_MARGINAL);
        }
      }

      if (inPulse && (uint32_t)(now - riseMs) > OPEN_ABORT_MS) {
        if (!openAbortCounted) localHealth.openAborts++;
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
        // The aborted high episode has physically ended. Only now may a new
        // rise be attributed; never manufacture repeated rises from one
        // plateau that remains above thresholdHigh.
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
              // Keep measuring so the envelope can recover, but none of the
              // stationary-noise band can satisfy the governor's validity
              // contract. Five later strong intervals are required.
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
                                : (intervalCount >= SPEED_WINDOW_N &&
                                   strongIntervalCount >= SPEED_WINDOW_N)
                                  ? IR_VALID : IR_REACQUIRING;
        publishLocalSnapshot(now, localHealth.completedPulses, intervalMs, speed,
                             raw, runMin, runMax, validity);
#ifdef IR_LOCAL_DEBUG
        Serial.printf("[IR] pulse=%lu int=%lu speed=%.2f valid=%s span=%d\n",
                      (unsigned long)localHealth.completedPulses,
                      (unsigned long)intervalMs, speed,
                      validityName(validity), span);
#endif
      }

      if (!contrastMarginal &&
          (lastCompletedMs == 0 ||
           (uint32_t)(now - lastCompletedMs) > SPEED_STALE_MS)) {
        if (latest.validity != IR_STALE) {
          intervalCount = 0;
          intervalIndex = 0;
          strongIntervalCount = 0;
          previousRiseMs = 0; // no interval may span an edge-silence episode
          speed = 0.0f;
          publishLocalSnapshot(now, localHealth.completedPulses, 0, 0.0f,
                               raw, runMin, runMax, IR_STALE);
        }
      }
    }

    // The sampler owns localHealth. Export one coherent diagnostic snapshot
    // at low rate; the network never reads fields while they are being
    // modified, and the 1 kHz path does not take a mutex per sample.
    if ((uint32_t)(now - lastHealthSnapshotMs) >= 100) {
      lastHealthSnapshotMs = now;
      portENTER_CRITICAL(&snapshotMux);
      health = localHealth;
      portEXIT_CRITICAL(&snapshotMux);
    }

    vTaskDelayUntil(&wake, pdMS_TO_TICKS(1));
  }
}

#ifdef IR_LOCAL_WIFI
static bool publishText(const char* topic, const char* payload, bool retained = false) {
  if (!mqtt.publish(topic, payload, retained)) {
    publishFailures++;
    return false;
  }
  return true;
}

static void networkTask(void*) {
  uint32_t nextConnectMs = 0, lastSpeedMs = 0, lastStatusMs = 0;
  for (;;) {
    uint32_t now = millis();
    if (WiFi.status() == WL_CONNECTED && !mqtt.connected() && now >= nextConnectMs) {
      nextConnectMs = now + MQTT_RETRY_MS;
      mqttAttempts++;
      String clientId = "ngr-ir-local-" + String((uint32_t)ESP.getEfuseMac(), HEX);
      if (mqtt.connect(clientId.c_str(), topicOnline, 0, true, "0")) {
        mqttConnects++;
        publishText(topicOnline, "1", true);
      }
    }

    if (mqtt.connected()) {
      mqtt.loop();
      if ((uint32_t)(now - lastSpeedMs) >= SPEED_PUBLISH_MS) {
        lastSpeedMs = now;
        LocalSpeedSnapshot s;
        portENTER_CRITICAL(&snapshotMux);
        s = latest;
        portEXIT_CRITICAL(&snapshotMux);
        char payload[256];
        if (s.validity == IR_VALID) {
          snprintf(payload, sizeof(payload),
                   "{\"schema\":\"ir-speed-local/1\",\"seq\":%lu,"
                   "\"t_ms\":%lu,\"report_ms\":%lu,\"speed_valid\":1,"
                   "\"speed_mmps\":%.2f,\"span\":%d}",
                   (unsigned long)s.completedPulses,
                   (unsigned long)s.capturedMs, (unsigned long)now,
                   s.speedMmPerSec, s.span);
        } else {
          snprintf(payload, sizeof(payload),
                   "{\"schema\":\"ir-speed-local/1\",\"seq\":%lu,"
                   "\"t_ms\":%lu,\"report_ms\":%lu,\"speed_valid\":0,"
                   "\"speed_mmps\":null,\"state\":\"%s\",\"span\":%d}",
                   (unsigned long)s.completedPulses,
                   (unsigned long)s.capturedMs, (unsigned long)now,
                   validityName(s.validity), s.span);
        }
        publishText(topicSpeed, payload, false);
      }

      if ((uint32_t)(now - lastStatusMs) >= STATUS_PUBLISH_MS) {
        lastStatusMs = now;
        LocalSpeedSnapshot s;
        HealthCounters h;
        portENTER_CRITICAL(&snapshotMux);
        s = latest;
        h = health;
        portEXIT_CRITICAL(&snapshotMux);
        char payload[384];
        snprintf(payload, sizeof(payload),
                 "{\"sketch\":\"%s\",\"spokes\":%d,\"circ_mm\":%.2f,"
                 "\"pulses\":%lu,\"rises\":%lu,\"sample_missed\":%lu,"
                 "\"open_aborts\":%lu,\"contrast_invalid\":%lu,"
                 "\"saturated_samples\":%lu,"
                 "\"span\":%d,\"sample_max_gap_us\":%lu,"
                 "\"mqtt_attempts\":%lu,\"mqtt_connects\":%lu,"
                 "\"publish_failures\":%lu,\"rssi\":%d}",
                 SKETCH_NAME, SPOKES_PER_REV, WHEEL_CIRCUMFERENCE_MM,
                 (unsigned long)h.completedPulses, (unsigned long)h.rises,
                 (unsigned long)h.sampleMissedSlots,
                 (unsigned long)h.openAborts,
                 (unsigned long)h.contrastInvalidEpisodes,
                 (unsigned long)h.saturatedSamples, s.span,
                 (unsigned long)h.sampleMaxGapUs,
                 (unsigned long)mqttAttempts, (unsigned long)mqttConnects,
                 (unsigned long)publishFailures, WiFi.RSSI());
        publishText(topicStatus, payload, false);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
#endif

void setup() {
  Serial.begin(115200);
  delay(300);
  analogReadResolution(12);
  pinMode(SENSOR_PIN, INPUT);

  Serial.printf("=== %s pin=%d spokes=%d circumference=%.2fmm mm/pulse=%.3f ===\n",
                SKETCH_NAME, SENSOR_PIN, SPOKES_PER_REV,
                WHEEL_CIRCUMFERENCE_MM, MM_PER_PULSE);

  if (xTaskCreatePinnedToCore(sensorTask, "ir-local", 6144, nullptr, 2,
                             nullptr, 0) != pdPASS) {
    Serial.println("[FATAL] sensor task creation failed");
    while (true) delay(1000);
  }

#ifdef IR_LOCAL_WIFI
  snprintf(topicOnline, sizeof(topicOnline),
           "ngr/spoke/%s/status/online", NODE_NAME);
  // New contract and cadence: never reuse IR_TEST's per-pulse schema/topic.
  snprintf(topicSpeed, sizeof(topicSpeed),
           "ngr/spoke/%s/telem/speed", NODE_NAME);
  snprintf(topicStatus, sizeof(topicStatus),
           "ngr/spoke/%s/telem/status", NODE_NAME);

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  if (!mqtt.setBufferSize(MQTT_BUFFER_BYTES)) {
    Serial.println("[WARN] MQTT 512-byte buffer allocation failed");
  }
  mqtt.setSocketTimeout(2);
  wifiClient.setConnectionTimeout(3000);

  if (xTaskCreatePinnedToCore(networkTask, "ir-net", 6144, nullptr, 1,
                             nullptr, 1) != pdPASS) {
    Serial.println("[FATAL] network task creation failed");
    while (true) delay(1000);
  }
#endif
}

void loop() {
  // Production consumers will read a LocalSpeedSnapshot locally. This
  // standalone prototype has no actuator and intentionally does nothing here.
  delay(1000);
}
