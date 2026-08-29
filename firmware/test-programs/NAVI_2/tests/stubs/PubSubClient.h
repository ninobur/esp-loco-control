#pragma once
#include "Arduino.h"
#include "WiFi.h"
// Publishes are captured so tests can assert on emitted topics/payloads.
struct HostPub { std::string topic, payload; bool retain; };
extern std::vector<HostPub> hostPublished;
class PubSubClient {
public:
  PubSubClient(WiFiClient&){}
  void setServer(const char*, int){}
  void setSocketTimeout(int){}
  void setCallback(void(*)(char*, uint8_t*, unsigned int)){}
  void setBufferSize(int){}
  bool connect(const char*, const char*, int, bool, const char*){ return false; }
  bool connected(){ return false; }
  void loop(){}
  int state(){ return -1; }
  bool subscribe(const char*){ return true; }
  bool publish(const char* t, const char* p, bool r=false){
    hostPublished.push_back({t?t:"", p?p:"", r}); return true;
  }
};
