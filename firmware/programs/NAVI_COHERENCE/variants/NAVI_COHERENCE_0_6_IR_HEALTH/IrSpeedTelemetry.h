#pragma once
#include <stdio.h>
#include "IrHealthMonitor.h"

namespace ngr_nav {
// Prototype/house speed, NOT physical km/h. Keep the full canonical divisor.
static constexpr double PKPH_MM_PER_SEC = 5.37325;

struct IrSpeedReading {
  bool valid=false;
  const char* reason="WARMUP";
  double mmps=0;
  uint64_t windowUs=0,deltaPulses=0;
  uint32_t pitchUm=0;
};

// Read-only consumer: no route decisions, PWM writes or Hall-speed fallback.
class IrSpeedTelemetry {
 public:
  IrSpeedReading sample(IrHealthMonitor& health,uint64_t now,bool paired,bool radio) {
    health.tick(now);
    IrSpeedReading r;
    const auto& odo=health.odometry();
    if(!paired || !radio || !odo.haveMeasurement()) {
      previous_={};
      r.reason=!paired?"UNPAIRED":!radio?"RADIO_UNAVAILABLE":irHealthName(health.health().fault);
      if(paired && radio && health.health().healthy)r.reason=irReadinessName(health.health().readiness);
      return r;
    }
    const auto current=odo.point();
    if(!odo.contains(previous_)) {
      r.reason=previous_.owner?"EPOCH_CHANGED":"WARMUP";
      previous_=current;return r;
    }
    if(current.capturedUs<=previous_.capturedUs) {
      r.reason="NO_NEW_SAMPLE";return r;
    }
    r.windowUs=current.capturedUs-previous_.capturedUs;
    r.deltaPulses=current.pulses-previous_.pulses;
    r.pitchUm=current.pitchUm;
    // Subtract integers before conversion; avoid precision loss on long runs.
    r.mmps=double(r.deltaPulses)*double(r.pitchUm)*1000.0/double(r.windowUs);
    r.valid=true;r.reason=r.deltaPulses?"MEASURED":"NO_PULSES";
    previous_=current;
    return r;
  }
 private:
  IrOdometryPoint previous_{};
};

// Coupling is operator knowledge, not a property of the optical instrument.
// Power is only a reason to doubt a prolonged zero, never proof of motion.
class IrSpeedQualification {
 public:
  IrSpeedReading assess(IrSpeedReading r,uint64_t now,bool coupled,bool powered,
                        bool hallAdvanced) {
    if(!r.valid || !powered || r.deltaPulses)zeroSince_=0;
    if(r.valid && powered && !r.deltaPulses && !zeroSince_)zeroSince_=now;
    if(!r.valid)return r;
    if(!coupled){r.valid=false;r.reason="COUPLING_UNCONFIRMED";}
    else if(!r.deltaPulses && hallAdvanced){r.valid=false;r.reason="HALL_WITHOUT_IR_PULSES";}
    else if(!r.deltaPulses && powered && now>=zeroSince_ && now-zeroSince_>=3000000) {
      r.valid=false;r.reason="NO_PULSES_WHILE_POWERED";
    }
    return r;
  }
 private:
  uint64_t zeroSince_=0;
};

inline int formatIrSpeed(char* out,size_t size,const IrSpeedReading& r,bool coupled) {
  char mmps[40]="null",pkph[40]="null";
  if(r.valid) {
    snprintf(mmps,sizeof(mmps),"%.3f",r.mmps);
    snprintf(pkph,sizeof(pkph),"%.3f",r.mmps/PKPH_MM_PER_SEC);
  }
  return snprintf(out,size,
    "\"ir_valid\":%u,\"ir_mmps\":%s,\"ir_pkph\":%s,\"ir_speed_reason\":\"%s\","
    "\"ir_coupled\":%u,\"ir_window_us\":%llu,\"ir_delta_pulses\":%llu,\"ir_pitch_um\":%lu,"
    "\"ir_speed_authority\":\"OBSERVE_ONLY\"",
    r.valid?1u:0u,mmps,pkph,r.reason,coupled?1u:0u,
    (unsigned long long)r.windowUs,(unsigned long long)r.deltaPulses,(unsigned long)r.pitchUm);
}
} // namespace ngr_nav
