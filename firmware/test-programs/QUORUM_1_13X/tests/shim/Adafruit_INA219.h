// Host shim for Adafruit_INA219.h — replay harness only, never flashed.
// begin() fails, matching a locomotive with no INA219 fitted. The navigator
// then publishes bus voltage as null, which is the path the replay exercises;
// no adjudication logic reads current or power.
#pragma once
#include <Arduino.h>
#include <Wire.h>

class Adafruit_INA219 {
public:
  explicit Adafruit_INA219(uint8_t=0x40){}
  bool  begin(HostTwoWire* = nullptr){ return false; }
  void  setCalibration_32V_2A(){}
  void  setCalibration_32V_1A(){}
  void  setCalibration_16V_400mA(){}
  float getBusVoltage_V(){ return 0.0f; }
  float getShuntVoltage_mV(){ return 0.0f; }
  float getCurrent_mA(){ return 0.0f; }
  float getPower_mW(){ return 0.0f; }
};
