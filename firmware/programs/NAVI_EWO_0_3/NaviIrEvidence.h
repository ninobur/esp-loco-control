#pragma once
#include <algorithm>
#include <stdint.h>

namespace ngr_evidence {
// Timing judgments concern measurement credibility, never synthetic distance.
// A counted long interval cannot prove whether the wheel slowed or an optical
// cycle was lost. Keep that distinction even when withdrawing distance veto.
enum class PulseFinding : uint8_t {
  Baseline, Normal, TimingAnomaly, ObservationGap, ClockError,
  BracketedMissingCompatible
};
struct PulseAssessment {
  PulseFinding finding=PulseFinding::Baseline;
  uint64_t intervalUs=0, referenceUs=0;
  bool questionable=false;
};
class IrPulseEvidence {
 public:
  PulseAssessment observe(uint64_t serial,uint64_t completedUs,uint8_t pwm) {
    if (!have_) { start(serial,completedUs,pwm); return {}; }
    if (completedUs<=lastUs_) { reset(); return {PulseFinding::ClockError,0,0,false}; }
    if (serial!=lastSerial_+1) {
      start(serial,completedUs,pwm);
      return {PulseFinding::ObservationGap,0,0,false};
    }
    const uint64_t dt=completedUs-lastUs_;
    lastSerial_=serial; lastUs_=completedUs;
    if (!pwm || pwm!=lastPwm_) {
      lastPwm_=pwm; count_=0; candidateUs_=0;
      return {PulseFinding::Baseline,dt,0,false};
    }
    const uint64_t reference=median();
    PulseAssessment out{count_==5?PulseFinding::Normal:PulseFinding::Baseline,dt,reference,false};
    if (candidateUs_) {
      // One suspicious counted interval is sufficient once a following normal
      // interval agrees with the pre-event range at unchanged motive PWM.
      const auto range=limits();
      if (dt>=range.first && dt<=range.second &&
          candidateUs_/reference>=2 && candidateUs_/reference<=8) {
        const uint64_t n=(candidateUs_+reference/2)/reference;
        if (candidateUs_/n>=range.first && candidateUs_/n<=range.second)
          out={PulseFinding::BracketedMissingCompatible,candidateUs_,reference,true};
      }
      candidateUs_=0;
    }
    if (count_==5 && dt>reference+reference/2) {
      candidateUs_=dt;
      if (!out.questionable) out={PulseFinding::TimingAnomaly,dt,reference,false};
      return out; // anomaly cannot corrupt the pre-event timing population
    }
    add(dt);
    return out;
  }
  void reset(){*this=IrPulseEvidence{};}
 private:
  struct Range {uint64_t first,second;};
  Range limits() const {
    uint64_t lo=intervals_[0],hi=lo;
    for(unsigned i=1;i<count_;++i){lo=std::min(lo,intervals_[i]);hi=std::max(hi,intervals_[i]);}
    // TX samples at 1 kHz: one tick is measurement resolution, not a speed rule.
    return {lo>1000?lo-1000:0,hi+1000};
  }
  uint64_t median() const {
    if(!count_)return 0;
    uint64_t copy[5]{};
    for(unsigned i=0;i<count_;++i)copy[i]=intervals_[i];
    std::sort(copy,copy+count_);return copy[count_/2];
  }
  void add(uint64_t dt){if(count_==5){for(unsigned i=1;i<5;++i)intervals_[i-1]=intervals_[i];--count_;}intervals_[count_++]=dt;}
  void start(uint64_t serial,uint64_t t,uint8_t pwm){reset();have_=true;lastSerial_=serial;lastUs_=t;lastPwm_=pwm;}
  bool have_=false;
  uint64_t lastSerial_=0,lastUs_=0,candidateUs_=0,intervals_[5]{};
  unsigned count_=0;
  uint8_t lastPwm_=0;
};
}
