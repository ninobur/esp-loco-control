// Host shim for Wire.h — replay harness only, never flashed.
#pragma once
#include <Arduino.h>

struct HostTwoWire {
  bool begin(int=-1,int=-1,uint32_t=0){ return true; }
  void beginTransmission(uint8_t){}
  uint8_t endTransmission(bool=true){ return 0; }
};
extern HostTwoWire Wire;
