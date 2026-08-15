// Host shim for WiFi.h — replay harness only, never flashed.
// The replay never brings a link up: WiFi reports permanently disconnected so
// networkTask()'s reconnect path is inert and nothing in the navigator waits
// on the radio.
#pragma once
#include <Arduino.h>

#define WL_CONNECTED    3
#define WL_DISCONNECTED 6
#define WIFI_STA        1

// Fixture-driven, default 0 = unassociated.
static int hostWifiChannel = 0;

struct HostWiFiClass {
  int  status(){ return WL_DISCONNECTED; }
  void begin(const char*,const char*){}
  void mode(int){}
  void persistent(bool){}
  void setAutoReconnect(bool){}
  void setSleep(bool){}
  void disconnect(bool=false){}
  int  RSSI(){ return -50; }
  // 1.16Ra: an unassociated station has no channel, so 0 is the honest
  // default and the drift watch treats it as "don't know yet" — no replay
  // fabricates a CTO_CHANNEL_CHANGED event. A fixture may drive it with the
  // harness 'wifi_channel' command to exercise the watch deliberately, which
  // is the ONLY way the new code gets tested: byte-identity across the
  // capture corpus proves no regression and says nothing about new code.
  int  channel(){ return hostWifiChannel; }
};
extern HostWiFiClass WiFi;

// ESP32 core 3.3.11: the connect timeout lives on the client as
// setConnectionTimeout(ms). setTimeout() is a different, seconds-based
// stream timeout and does NOT gate connection attempts.
struct WiFiClient {
  void setConnectionTimeout(uint32_t){}
  void setTimeout(uint32_t){}
};
