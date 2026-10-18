#pragma once
#include <cstdint>
#include <algorithm>

// No observer include, pointer, result, or callback enters this module.
class ManualControl {
 public:
  int pwm() const { return applied_; }
  int target() const { return target_; }
  int direction() const { return direction_; }
  int brake() const { return brake_; }
  bool estopped() const { return estop_; }
  bool lowVoltage() const { return low_; }
  bool throttle(int value) {
    if (estop_ || low_) return false;
    target_=std::max(0,std::min(255,value)); downMs_=0; return true;
  }
  void stop() { target_=0; downMs_=31; }
  void setBrake(int value) { brake_=std::max(0,std::min(255,value)); }
  bool setDirection(int value) {
    if(value<0 || value>2 || applied_ || target_) return false;
    direction_=value; return true;
  }
  void emergency(bool asserted) {
    estop_=asserted; applied_=target_=0;
    direction_=1; // clearing also requires a deliberate direction selection
  }
  // Called at the established five-second electrical sampling cadence.
  void voltage(float value) {
    if(value<12.5f || value>=13.25f) lowCount_=0;
    else if(lowCount_<5) ++lowCount_;
    if(lowCount_>=5) { low_=true; stop(); }
    if(low_ && value>=14.f) { low_=false; lowCount_=0; }
  }
  void tick(uint32_t now) {
    if(estop_ || direction_==1) { applied_=target_=0; return; }
    if(low_) stop();
    const unsigned step=target_<applied_ ? (downMs_ ? downMs_ : 400-385*brake_/255) : 150;
    if(now-lastStep_<step) return;
    lastStep_=now;
    if(applied_<target_) ++applied_;
    else if(applied_>target_) --applied_;
  }
 private:
  int applied_=0,target_=0,direction_=1,brake_=0,lowCount_=0;
  bool estop_=false,low_=false;
  unsigned downMs_=0;
  uint32_t lastStep_=0;
};
