#pragma once
#include "Arduino.h"
#define WIFI_STA 1
#define WL_CONNECTED 3
struct HostWiFi {
  int status(){ return 0; }        // never connected on host: network inert
  void mode(int){}
  void begin(const char*, const char*){}
  void setAutoReconnect(bool){}
  void persistent(bool){}
  void setSleep(bool){}
  int channel(){ return 1; }
  int RSSI(){ return -50; }
  IPAddress localIP(){ return IPAddress(); }
  String macAddress(){ return String("00:00:00:00:00:00"); }
};
extern HostWiFi WiFi;
class WiFiClient {
public:
  void setConnectionTimeout(int){}
};
using NetworkClient = WiFiClient;
