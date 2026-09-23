/*
 * NGR SURVEY CAR: QRE1113 IR spoke sensor + Wi-Fi RSSI survey.
 * One physical car, one identity (NODE_NAME). Two payloads on the same node:
 *   - IR speed telemetry (independent movement detection, M2 groundwork)
 *   - Wi-Fi RSSI at 2 Hz (coverage-map survey to locate radio shadows)
 *
 * IR topics (ngr/spoke/<NODE_NAME>/telem/...):
 *   speed_mmps, speed_pkph, pulse_count, raw_adc,
 *   pulse_interval_ms (time between accepted spokes),
 *   pulse_width_ms    (time signal stays high)
 * RSSI topic:
 *   ngr/spoke/<NODE_NAME>/telem/rssi   (dBm, published every RSSI_PUBLISH_MS)
 *
 * SPOKE COUNT: SPOKES_PER_WHEEL = 2 — the survey wheel carries TWO gaffer-tape
 * flags, opposite one another (180 deg apart). Two flags = two pulses per rev.
 * (History: this sketch was 10, then 5; the physical target is 2. Speed is
 * directly proportional to this constant, so it MUST match the wheel.)
 *
 * SURVEY CORRELATION NOTE: this car has no position sense of its own. RSSI is
 * mapped to position afterward by matching timestamps against the TOW
 * locomotive's navMm in the same log. The RSSI ESP sits ~2 marker-spacings
 * BEHIND the tow loco's Hall sensor, so RSSI at time T corresponds to a track
 * position ~2 markers behind the loco's reported mm at T. Apply that offset
 * during analysis. First survey: start MM40-41 CW.
 *
 * IR signal cable is shielded (coax) sensor -> ESP, pin D34 unchanged.
 */
#include <WiFi.h>
#include <PubSubClient.h>

// WiFi credentials live in credentials.h, which is git-ignored and never
// committed. Copy firmware/config/credentials_template.h into this folder as
// credentials.h and fill in the real values (repo convention — the sketch
// arrived with them inline).
#include "credentials.h"
const char* MQTT_BROKER = "192.168.68.142";
const int MQTT_PORT = 1883;
const char* NODE_NAME = "IR_SPEED_SENSOR";

const int SENSOR_PIN = 34;
const int SPOKES_PER_WHEEL = 2; // TWO gaffer-tape flags, 180 deg apart -> 2 pulses/rev
const float WHEEL_CIRCUMFERENCE_MM = 115.0f;

const unsigned long CALIBRATION_MS = 10000;
const unsigned long DEBOUNCE_US = 15000;
const unsigned long SPEED_TIMEOUT_MS = 1000;
const unsigned long DEBUG_PRINT_MS = 500;
const unsigned long RSSI_PUBLISH_MS = 500;  // survey RSSI cadence (2 Hz)

int thresholdHigh = 1280;
int thresholdLow = 640;
bool sensorState = false;
unsigned long lastEdgeMicros = 0;
unsigned long pulseStartMicros = 0;
unsigned long lastPulseMillis = 0;
unsigned long lastDebugPrint = 0;
unsigned long lastRssiPublish = 0;
volatile unsigned long pulseCount = 0;

unsigned long lastPulseIntervalMs = 0; // interval between pulse starts
unsigned long lastPulseWidthMs = 0;    // time signal is high

float lastKnownSpeedMmPerSec = 0.0f;
float lastKnownPkph = 0.0f;
int lastPulseRawAdc = 0;
float lastPublishedSpeed = -1.0f;
unsigned long lastPublishedPulseCount = 0xFFFFFFFF;

const int WINDOW_SIZE = 5;
unsigned long intervalBuffer[WINDOW_SIZE];
int intervalIndex = 0;
int intervalsFilled = 0;

char TOPIC_ONLINE[64], TOPIC_SPEED[64], TOPIC_PULSECOUNT[64], TOPIC_PKPH[64];
char TOPIC_RAWADC[64], TOPIC_INTERVAL[64], TOPIC_WIDTH[64], TOPIC_RSSI[64];

WiFiClient espClient;
PubSubClient mqtt(espClient);

void setupTopics() {
  snprintf(TOPIC_ONLINE, sizeof(TOPIC_ONLINE), "ngr/spoke/%s/status/online", NODE_NAME);
  snprintf(TOPIC_SPEED, sizeof(TOPIC_SPEED), "ngr/spoke/%s/telem/speed_mmps", NODE_NAME);
  snprintf(TOPIC_PKPH, sizeof(TOPIC_PKPH), "ngr/spoke/%s/telem/speed_pkph", NODE_NAME);
  snprintf(TOPIC_PULSECOUNT, sizeof(TOPIC_PULSECOUNT), "ngr/spoke/%s/telem/pulse_count", NODE_NAME);
  snprintf(TOPIC_RAWADC, sizeof(TOPIC_RAWADC), "ngr/spoke/%s/telem/raw_adc", NODE_NAME);
  snprintf(TOPIC_INTERVAL, sizeof(TOPIC_INTERVAL), "ngr/spoke/%s/telem/pulse_interval_ms", NODE_NAME);
  snprintf(TOPIC_WIDTH, sizeof(TOPIC_WIDTH), "ngr/spoke/%s/telem/pulse_width_ms", NODE_NAME);
  snprintf(TOPIC_RSSI, sizeof(TOPIC_RSSI), "ngr/spoke/%s/telem/rssi", NODE_NAME);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) {
    delay(250); Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED)
    Serial.printf("\nWiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());
  else
    Serial.printf("\nWiFi not yet connected (status=%d)\n", WiFi.status());
}

void connectMQTT() {
  if (WiFi.status() != WL_CONNECTED || mqtt.connected()) return;
  Serial.print("Connecting to MQTT broker...");
  String clientId = "ngr-spoke-" + String(NODE_NAME);
  if (mqtt.connect(clientId.c_str(), TOPIC_ONLINE, 0, true, "0")) {
    Serial.println("connected");
    mqtt.publish(TOPIC_ONLINE, "1", true);
  } else {
    Serial.printf("failed, rc=%d\n", mqtt.state());
  }
}

void calibrateThreshold() {
  Serial.println(">>> CALIBRATING NOW - rotate the wheel by hand for 10 seconds! <<<");
  int minVal = 4095, maxVal = 0;
  unsigned long start = millis(), lastReport = 0;
  while (millis() - start < CALIBRATION_MS) {
    mqtt.loop();
    int val = analogRead(SENSOR_PIN);
    if (val < minVal) minVal = val;
    if (val > maxVal) maxVal = val;
    if (millis() - lastReport > 2000) {
      lastReport = millis();
      Serial.printf("  ...calibrating (%lus remaining) min=%d max=%d so far\n",
        (CALIBRATION_MS - (millis() - start)) / 1000, minVal, maxVal);
    }
    delay(2);
  }
  int mid = (minVal + maxVal) / 2;
  int span = maxVal - minVal;
  int margin = max(150, span / 6);
  thresholdHigh = mid + margin;
  thresholdLow = mid - margin;
  Serial.printf("Calibration done. min=%d max=%d thresholdHigh=%d thresholdLow=%d\n",
    minVal, maxVal, thresholdHigh, thresholdLow);
  if (span < 100) Serial.println("WARNING: essentially no signal swing detected.");
}

void computeSpeed(unsigned long lastIntervalMs) {
  unsigned long sum = 0;
  for (int i = 0; i < intervalsFilled; i++) sum += intervalBuffer[i];
  float avgIntervalMs = (float)sum / intervalsFilled;
  float pulsesPerSec = 1000.0f / avgIntervalMs;
  float revsPerSec = pulsesPerSec / SPOKES_PER_WHEEL;
  lastKnownSpeedMmPerSec = revsPerSec * WHEEL_CIRCUMFERENCE_MM;
  lastKnownPkph = lastKnownSpeedMmPerSec * 0.21f;
  Serial.printf("Pulse #%lu | interval=%lums | avg=%.1fms | speed=%.2f mm/s | pKPH=%.2f | raw=%d\n",
    pulseCount, lastIntervalMs, avgIntervalMs, lastKnownSpeedMmPerSec, lastKnownPkph, lastPulseRawAdc);
}

void publishPulseWidth() {
  if (!mqtt.connected()) return;
  char widthStr[16];
  snprintf(widthStr, sizeof(widthStr), "%lu", lastPulseWidthMs);
  mqtt.publish(TOPIC_WIDTH, widthStr, false);
  Serial.printf("[PULSE] width=%lums\n", lastPulseWidthMs);
}

void publishTelemetry() {
  // No heartbeat: only publish when something actually changed (a real
  // pulse, or the stop-timeout dropping speed to 0.00). A stopped, idle
  // node stays silent instead of re-announcing the same value on a timer.
  bool changed = (lastKnownSpeedMmPerSec != lastPublishedSpeed) ||
                 (pulseCount != lastPublishedPulseCount);
  if (!changed) return;

  char speedStr[16], pkphStr[16], countStr[16], rawStr[16], intervalStr[16];
  snprintf(speedStr, sizeof(speedStr), "%.2f", lastKnownSpeedMmPerSec);
  snprintf(pkphStr, sizeof(pkphStr), "%.2f", lastKnownPkph);
  snprintf(countStr, sizeof(countStr), "%lu", pulseCount);
  snprintf(rawStr, sizeof(rawStr), "%d", lastPulseRawAdc);
  snprintf(intervalStr, sizeof(intervalStr), "%lu", lastPulseIntervalMs);

  mqtt.publish(TOPIC_SPEED, speedStr, false);
  mqtt.publish(TOPIC_PKPH, pkphStr, false);
  mqtt.publish(TOPIC_PULSECOUNT, countStr, false);
  mqtt.publish(TOPIC_RAWADC, rawStr, false);
  mqtt.publish(TOPIC_INTERVAL, intervalStr, false);

  lastPublishedSpeed = lastKnownSpeedMmPerSec;
  lastPublishedPulseCount = pulseCount;
  Serial.printf("[MQTT] speed=%s mm/s | pulses=%s | interval=%sms | raw=%s\n",
    speedStr, countStr, intervalStr, rawStr);
}

void publishRssi() {
  // Survey payload: current Wi-Fi RSSI in dBm, on a fixed 2 Hz cadence
  // (independent of pulse activity, so coverage is mapped even when stopped).
  if (WiFi.status() != WL_CONNECTED || !mqtt.connected()) return;
  if (millis() - lastRssiPublish < RSSI_PUBLISH_MS) return;
  lastRssiPublish = millis();
  long rssi = WiFi.RSSI();
  char rssiStr[16];
  snprintf(rssiStr, sizeof(rssiStr), "%ld", rssi);
  mqtt.publish(TOPIC_RSSI, rssiStr, false);
  Serial.printf("[RSSI] %ld dBm\n", rssi);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  analogReadResolution(12);
  pinMode(SENSOR_PIN, INPUT);
  setupTopics();
  connectWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  connectMQTT();
  Serial.println("\n=== QRE1113 Spoke Sensor - MQTT (individual pulse timing) ===");
  calibrateThreshold();
  Serial.printf("Node: %s | Wheel circumference=%.1fmm\n", NODE_NAME, WHEEL_CIRCUMFERENCE_MM);
  sensorState = analogRead(SENSOR_PIN) > thresholdHigh;
  lastPulseMillis = millis();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();

  int raw = analogRead(SENSOR_PIN);
  if (millis() - lastDebugPrint > DEBUG_PRINT_MS) {
    lastDebugPrint = millis();
    Serial.printf("raw=%d (thresholdHigh=%d thresholdLow=%d)\n", raw, thresholdHigh, thresholdLow);
  }

  if (!sensorState && raw > thresholdHigh) {
    unsigned long nowMicros = micros();
    if (nowMicros - lastEdgeMicros > DEBOUNCE_US) {
      sensorState = true;
      lastEdgeMicros = nowMicros;
      pulseStartMicros = nowMicros;
      pulseCount++;
      lastPulseRawAdc = raw;

      unsigned long nowMillis = millis();
      unsigned long interval = nowMillis - lastPulseMillis;
      lastPulseMillis = nowMillis;
      lastPulseIntervalMs = interval;
      intervalBuffer[intervalIndex] = interval;
      intervalIndex = (intervalIndex + 1) % WINDOW_SIZE;
      if (intervalsFilled < WINDOW_SIZE) intervalsFilled++;
      computeSpeed(interval);
    }
  } else if (sensorState && raw < thresholdLow) {
    sensorState = false;
    lastPulseWidthMs = (micros() - pulseStartMicros) / 1000UL;
    publishPulseWidth();
  }

  if (millis() - lastPulseMillis > SPEED_TIMEOUT_MS) {
    lastKnownSpeedMmPerSec = 0.0f;
    lastKnownPkph = 0.0f;
  }
  publishTelemetry();
  publishRssi();
}
