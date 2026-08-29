#pragma once
class Adafruit_INA219 {
public:
  bool begin(void* = nullptr){ return false; }   // absent sensor on host
  float getBusVoltage_V(){ return 0.0f; }
  float getCurrent_mA(){ return 0.0f; }
  float getPower_mW(){ return 0.0f; }
};
