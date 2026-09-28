#pragma once
#include <stdint.h>
#include "../../../../reference/NAVI_COHERENCE/IR_ARCHITECTURE_0_4/IrOdometryEpoch.h"

namespace navi_one {
// NAVI_EYES_WIDE_OPEN: route displacement is IR wheel travel measured while
// NAVI is moving the locomotive (decision 0106: "PWM = 0 means no movement.
// Period.").
//
// Every accepted IR point is observed here, in order. The pulses that
// accumulated since the previous point in the same Epoch are route travel only
// if motive PWM was above zero throughout that span, as seen by the loop.
// Pulses from a span that touched motive PWM 0 are real wheel rotation
// (handling, IR-car tilt, flicker), retained and reported as `excluded`, but
// they are not signed route displacement and cannot advance, retreat or
// correct position, or move the Hall-reference collection interval.
//
// It is measurement bookkeeping, not a judgment: it never decides what an
// observation means. NAVI (Navigator, SpatialReference) consumes it.
class RouteDisplacement {
 public:
  static constexpr uint8_t HISTORY=64; // same depth as IrHealthMonitor's point history
  // p: the newest accepted IR point. pwmZeroDuringSpan: motive PWM was 0 at
  // any loop pass since the previous observed point.
  void observe(const ngr_nav::IrOdometryPoint& p,bool pwmZeroDuringSpan){
    if(!p.owner||!p.epoch||!p.pitchUm)return;
    if(used_){
      const Entry& last=history_[(head_+HISTORY-1)%HISTORY];
      if(same(last.p,p)){
        if(p.capturedUs<=last.p.capturedUs||p.pulses<last.p.pulses)return; // ordering: not new
        const uint64_t delta=p.pulses-last.p.pulses;
        excluded_=last.excluded+(pwmZeroDuringSpan?delta:0);
        if(pwmZeroDuringSpan)excludedTotal_+=delta;
      } else excluded_=0; // a new Epoch starts its own account
    } else excluded_=0;
    history_[head_]={p,excluded_};head_=uint8_t((head_+1)%HISTORY);if(used_<HISTORY)++used_;
  }
  // Excluded (PWM-0) pulses accumulated in p's Epoch up to p. False when p was
  // never observed or has left the history: nothing is manufactured.
  bool excludedAt(const ngr_nav::IrOdometryPoint& p,uint64_t& excluded)const{
    for(uint8_t i=0;i<used_;++i){
      const Entry& e=history_[(head_+HISTORY-1-i)%HISTORY];
      if(same(e.p,p)&&e.p.capturedUs==p.capturedUs&&e.p.pulses==p.pulses){excluded=e.excluded;return true;}
    }
    return false;
  }
  // Route micrometres from a (whose excluded count is aExcluded) to b.
  bool routeUm(const ngr_nav::IrOdometryPoint& a,uint64_t aExcluded,
               const ngr_nav::IrOdometryPoint& b,uint64_t& um)const{
    uint64_t bExcluded=0;
    if(!same(a,b)||b.pulses<a.pulses||b.capturedUs<a.capturedUs||!excludedAt(b,bExcluded)||bExcluded<aExcluded)return false;
    const uint64_t raw=b.pulses-a.pulses,excl=bExcluded-aExcluded;
    um=(raw>excl?raw-excl:0)*uint64_t(a.pitchUm);return true;
  }
  uint64_t excludedTotal()const{return excludedTotal_;}
  static bool same(const ngr_nav::IrOdometryPoint& a,const ngr_nav::IrOdometryPoint& b){
    return a.owner&&a.owner==b.owner&&a.epoch&&a.epoch==b.epoch&&a.boot==b.boot&&
           a.calibration==b.calibration&&a.pitchUm&&a.pitchUm==b.pitchUm;
  }
 private:
  struct Entry{ngr_nav::IrOdometryPoint p{};uint64_t excluded=0;};
  Entry history_[HISTORY]{};uint8_t head_=0,used_=0;
  uint64_t excluded_=0,excludedTotal_=0;
};
} // namespace navi_one
