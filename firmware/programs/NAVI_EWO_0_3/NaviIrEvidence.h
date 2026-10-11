#pragma once
#include <stdint.h>

namespace ngr_evidence {
// Evidence only: never inserts pulses, changes calibration, or advances NAVI.
enum class PulseFinding : uint8_t { Baseline, Normal, TimingAnomaly, ObservationGap, ClockError };
struct PulseAssessment {
  PulseFinding finding=PulseFinding::Baseline;
  uint64_t intervalUs=0;
  uint64_t referenceUs=0;
};
class IrPulseEvidence {
 public:
  // Individually timestamped completed pulses; NOT packet arrival timestamps.
  PulseAssessment observe(uint64_t serial,uint64_t completedUs,uint8_t pwm) {
    if (!have_) { start(serial,completedUs,pwm); return {}; }
    if (completedUs<=lastUs_) { reset(); return {PulseFinding::ClockError,0,0}; }
    if (serial!=lastSerial_+1) {
      start(serial,completedUs,pwm);
      return {PulseFinding::ObservationGap,0,0};
    }
    const uint64_t dt=completedUs-lastUs_;
    lastSerial_=serial;lastUs_=completedUs;
    if (pwm!=lastPwm_) {
      lastPwm_=pwm;baselineUs_=dt;
      return {PulseFinding::Baseline,dt,0};
    }
    if (!baselineUs_) { baselineUs_=dt;return {PulseFinding::Baseline,dt,0}; }
    const uint64_t reference=baselineUs_;
    // 1.5x is an exploratory timing diagnostic, NOT a veto/authority threshold.
    const bool anomalous=dt>reference+reference/2;
    if (!anomalous) baselineUs_=(baselineUs_*7+dt)/8;
    return {anomalous?PulseFinding::TimingAnomaly:PulseFinding::Normal,dt,reference};
  }
  void reset(){have_=false;baselineUs_=0;}
 private:
  void start(uint64_t serial,uint64_t t,uint8_t pwm){
    have_=true;lastSerial_=serial;lastUs_=t;lastPwm_=pwm;baselineUs_=0;
  }
  bool have_=false;
  uint64_t lastSerial_=0,lastUs_=0,baselineUs_=0;
  uint8_t lastPwm_=0;
};
}
