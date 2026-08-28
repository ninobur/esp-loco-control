// Host shim for PubSubClient.h — replay harness only, never flashed.
// Never connects. The replay captures publications by draining the firmware's
// own pubQueue/markerQueue, so this class only has to exist and refuse.
#pragma once
#include <Arduino.h>
#include <WiFi.h>

typedef void (*MqttCallback)(char*,uint8_t*,unsigned int);

class PubSubClient {
public:
  PubSubClient(){}
  explicit PubSubClient(WiFiClient&){}
  PubSubClient& setServer(const char*,uint16_t){ return *this; }
  PubSubClient& setServer(uint32_t,uint16_t){ return *this; }
  PubSubClient& setCallback(MqttCallback){ return *this; }
  bool setBufferSize(uint16_t){ return true; }
  void setKeepAlive(uint16_t){}
  void setSocketTimeout(uint16_t){}
  bool connect(const char*){ return false; }
  bool connect(const char*,const char*,const char*){ return false; }
  bool connect(const char*,const char*,const char*,const char*,uint8_t,bool,const char*){ return false; }
  // willTopic/willQos/willRetain/willMessage form used by attemptReconnect()
  bool connect(const char*,const char*,uint8_t,bool,const char*){ return false; }
  bool connected(){ return false; }
  int  state(){ return -1; }
  bool loop(){ return false; }
  bool publish(const char*,const char*,bool=false){ return false; }
  bool subscribe(const char*){ return false; }
};
