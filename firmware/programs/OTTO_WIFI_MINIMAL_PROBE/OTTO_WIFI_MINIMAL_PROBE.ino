// Diagnostic only: isolate Otto's Wi-Fi association from EWO tasks and ESP-NOW.
// This sketch never starts the motor, IR, MQTT, or NAVI.
#include <Arduino.h>
#include <WiFi.h>
#include "credentials.h"

static constexpr uint8_t OTTO_MOTOR_PWM_PIN = 4;

static void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    Serial.printf("[PROBE] disconnect_reason=%u\n",
                  static_cast<unsigned>(info.wifi_sta_disconnected.reason));
  } else if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
    Serial.println("[PROBE] station=ASSOCIATED");
  } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    Serial.printf("[PROBE] station=GOT_IP ip=%s\n", WiFi.localIP().toString().c_str());
  }
}

void setup() {
  pinMode(OTTO_MOTOR_PWM_PIN, OUTPUT);
  digitalWrite(OTTO_MOTOR_PWM_PIN, LOW);
  Serial.begin(115200);
  delay(300);
  WiFi.onEvent(onWifiEvent);
  WiFi.mode(WIFI_STA);
  Serial.printf("[PROBE] OTTO_WIFI_MINIMAL_PROBE mac=%s ssid_length=%u\n",
                WiFi.macAddress().c_str(), static_cast<unsigned>(strlen(WIFI_SSID)));
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.println("[PROBE] wifi=CONNECTING");
}

void loop() {
  static uint32_t lastReportMs = 0;
  const uint32_t now = millis();
  if (now - lastReportMs >= 5000) {
    lastReportMs = now;
    Serial.printf("[PROBE] wifi_status=%d connected=%u rssi=%d\n",
                  static_cast<int>(WiFi.status()), WiFi.status() == WL_CONNECTED,
                  WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
  }
  delay(50);
}
