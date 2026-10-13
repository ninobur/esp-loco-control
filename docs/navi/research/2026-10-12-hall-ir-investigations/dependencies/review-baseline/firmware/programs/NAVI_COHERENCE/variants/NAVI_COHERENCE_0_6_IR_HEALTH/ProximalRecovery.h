#pragma once
#include <stdint.h>
#include <stdio.h>
#include "RouteMap.h"
#include "../../../../reference/NAVI_COHERENCE/IR_ARCHITECTURE_0_4/IrOdometryEpoch.h"

namespace navi_one {
// NAVI interprets measurements. The IR point contains no map or candidate ID.
class ProximalRecovery {
 public:
  static constexpr uint8_t WINDOW=10;
  static constexpr int8_t LIMIT=10;
  static constexpr double FRACTION=0.15;
  struct Entry {
    uint8_t mm=0,polarity=0;
    bool observed=false;
    int8_t dir=0;
    ngr_nav::IrOdometryPoint point{};
    uint64_t ordinal=0;
  };
  struct Decision {
    const char* reason="WARMUP";
    uint8_t positions=0,observations=0,incumbent=0,best=0,ties=0;
    int8_t offset=0;
    bool startup=true;
    int16_t referenceMm=-1;
    double travelMm=-1;
    uint32_t possibleMask=0,travelRejectedMask=0;
  };
  void reset() {*this=ProximalRecovery{};}
  void reverse() {
    // Preserve observations, but unsigned wheel travel cannot bridge reversal.
    reference_={};settled_=true;directionPositions_=0;
  }
  const Decision& decision() const {return decision_;}
  uint8_t count() const {return count_;}
  const Entry& entry(uint8_t i) const {return entries_[i];}
  void append(uint8_t mm,int8_t dir,bool observed,uint8_t polarity,
              const ngr_nav::IrOdometryPoint& point={}) {
    if(count_==WINDOW) {
      for(uint8_t i=1;i<count_;++i)entries_[i-1]=entries_[i];
      --count_;
    }
    ++progress_;
    entries_[count_++]={mm,polarity,observed,dir,point,progress_};
    if(directionPositions_<WINDOW)++directionPositions_;
  }
  // A reversal's repeated landmark is evidence, not an extra travelled MM.
  void repeated(uint8_t mm,int8_t dir,uint8_t polarity,
                const ngr_nav::IrOdometryPoint& point) {
    if(count_ && entries_[count_-1].mm==mm)
      entries_[count_-1]={mm,polarity,true,dir,point,progress_};
  }
  static bool travel(const ngr_nav::IrOdometryPoint& a,
                     const ngr_nav::IrOdometryPoint& b,double& mm) {
    if(!a.owner || a.owner!=b.owner || !a.epoch || a.epoch!=b.epoch ||
       a.boot!=b.boot || a.calibration!=b.calibration || !a.pitchUm ||
       a.pitchUm!=b.pitchUm || b.capturedUs<=a.capturedUs || b.pulses<a.pulses)return false;
    mm=double(b.pulses-a.pulses)*a.pitchUm/1000.;return true;
  }
  static double mapped(uint8_t start,int8_t dir,uint64_t steps) {
    uint32_t lap=0;for(uint16_t i=0;i<ROUTE_N;++i)lap+=ROUTE_SPACING_MM[i];
    double mm=double(steps/ROUTE_N)*lap;
    for(uint16_t i=0;i<steps%ROUTE_N;++i){mm+=spanMm(start,dir);start=nextMarker(start,dir);}
    return mm;
  }
  static bool compatible(double measured,double expected) {
    return measured>=expected*(1.-FRACTION) && measured<=expected*(1.+FRACTION);
  }
  Decision evaluate(uint8_t incumbent,int8_t dir) {
    decision_={};auto& d=decision_;d.startup=!settled_;d.positions=count_;
    if(settled_ && reference_.point.owner)d.referenceMm=reference_.mm;
    for(uint8_t i=0;i<count_;++i)if(entries_[i].observed)++d.observations;
    d.incumbent=score(0);d.best=d.incumbent;
    if(count_<WINDOW || directionPositions_<WINDOW)return d;
    const auto& current=entries_[count_-1].point;
    // One wrong observation is not a recovery episode. This is not a score
    // margin: once multiple contradictions exist, a one-vote improvement wins.
    if(d.observations-d.incumbent<2) {
      d.reason=d.observations==d.incumbent?"INCUMBENT_BEST":"ISOLATED_DISAGREEMENT";
      if(d.observations==d.incumbent && d.observations>=2)establish(incumbent,dir,current);
      return d;
    }
    double distance=0;
    if(settled_ && (!reference_.point.owner || reference_.dir!=dir ||
                   !travel(reference_.point,current,distance))) {
      d.reason="TRAVEL_UNAVAILABLE_HOLD";return d;
    }
    if(settled_)d.travelMm=distance;
    // Build the physical set BEFORE consulting polarity scores.
    for(int8_t k=-LIMIT;k<=LIMIT;++k) {
      const uint32_t bit=uint32_t(1)<<(k+LIMIT);
      bool possible=true;
      if(settled_) {
        const int64_t steps=int64_t(progress_-referenceProgress_)+int64_t(k)*dir;
        possible=steps>=0 && compatible(distance,mapped(reference_.mm,dir,uint64_t(steps)));
      }
      // A provisional declaration may be translated locally, but each measured
      // segment must still fit that candidate's spacing, including UNKNOWNs.
      for(uint8_t i=0;!settled_ && possible && i<count_;++i)for(uint8_t j=i+1;possible && j<count_;++j) {
        if(entries_[i].dir!=dir || entries_[j].dir!=dir)continue;
        double measured=0;
        if(travel(entries_[i].point,entries_[j].point,measured))
          possible=compatible(measured,mapped(routeMod(int(entries_[i].mm)+k),dir,j-i));
      }
      if(possible)d.possibleMask|=bit;else d.travelRejectedMask|=bit;
    }
    // The incumbent remains the default even if contradicted by the physical
    // filter. Only a unique strictly better feasible explanation can replace it.
    for(int8_t k=-LIMIT;k<=LIMIT;++k) {
      if(!k || !(d.possibleMask&(uint32_t(1)<<(k+LIMIT))))continue;
      const uint8_t s=score(k);
      if(s>d.best){d.best=s;d.offset=k;d.ties=1;}
      else if(s==d.best && s>d.incumbent)++d.ties;
    }
    if(d.best<=d.incumbent){d.reason="NO_BETTER_CANDIDATE";d.offset=0;return d;}
    if(d.ties!=1){d.reason="TIED_BEST_HOLD";d.offset=0;return d;}
    d.reason="UNIQUE_BETTER";
    // Reinterpret MM assignments; keep actual poles, UNKNOWNs, and measurements.
    for(uint8_t i=0;i<count_;++i)
      if(!settled_ || entries_[i].ordinal>referenceProgress_)
        entries_[i].mm=routeMod(int(entries_[i].mm)+d.offset);
    establish(routeMod(int(incumbent)+d.offset),dir,current);
    return d;
  }
  int format(char* out,size_t size,uint32_t event) const {
    const auto& d=decision_;
    char distance[40]="null";
    if(d.travelMm>=0)snprintf(distance,sizeof(distance),"%.3f",d.travelMm);
    return snprintf(out,size,"{\"event_serial\":%lu,\"policy\":\"PROXIMAL_R1\",\"reason\":\"%s\","
      "\"positions\":%u,\"observations\":%u,\"incumbent_score\":%u,\"best_score\":%u,"
      "\"ties\":%u,\"offset\":%d,\"startup\":%u,\"outer_mm\":10,"
      "\"reference_mm\":%d,\"travel_mm\":%s,\"possible_mask\":%lu,\"travel_rejected_mask\":%lu,\"distance_bounds\":\"UNVALIDATED\"}",
      (unsigned long)event,d.reason,d.positions,d.observations,d.incumbent,d.best,d.ties,int(d.offset),
      d.startup?1:0,int(d.referenceMm),distance,(unsigned long)d.possibleMask,(unsigned long)d.travelRejectedMask);
  }
 private:
  uint8_t score(int8_t offset) const {
    uint8_t score=0;for(uint8_t i=0;i<count_;++i)
      if(entries_[i].observed && entries_[i].polarity==polarityAt(routeMod(int(entries_[i].mm)+
          (settled_ && entries_[i].ordinal<=referenceProgress_?0:offset))))++score;
    return score;
  }
  void establish(uint8_t mm,int8_t dir,const ngr_nav::IrOdometryPoint& point) {
    settled_=true;reference_={mm,0,true,dir,point};referenceProgress_=progress_;
  }
  Entry entries_[WINDOW]{},reference_{};
  Decision decision_{};
  uint64_t progress_=0,referenceProgress_=0;
  uint8_t count_=0,directionPositions_=0;
  bool settled_=false;
};
} // namespace navi_one
