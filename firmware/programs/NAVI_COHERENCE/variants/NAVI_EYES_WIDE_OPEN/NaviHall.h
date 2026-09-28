#pragma once
// ---------------------------------------------------------------------------
// NaviHall.h -- NAVI_EYES_WIDE_OPEN. NAVI's own Hall observation path.
//
// There is no upstream Hall detector. X22/X22R is prior art (decision 0102)
// and nothing from its authority structure is here: no baseline lock cycle,
// no acquisition window, no CLEAR/SETTLE/QUIET/CADENCE/MOVING/STOPPED gates,
// no leash, no dwell or old-field hold, no lost-lock recovery, no window
// morphology. The Hall task on core 0 only acquires: one ADC reading per
// millisecond (the median of five conversions, decision 0073) is written,
// with its timestamp and the applied PWM, into HallSampleRing. NAVI reads
// every sample on the loop thread.
//
// Three pieces, all NAVI-owned:
//
//   HallSampleRing   single-producer ring of native samples (acquisition
//                    transport only; a reader detects overwritten samples).
//   OpeningObserver  the established MM opening observation: |raw - NAVI's
//                    Hall reference| >= 70 counts on two consecutive
//                    same-sign samples; the second sample is the opening and
//                    fixes its polarity. An opening is a transition, so one
//                    excursion yields one opening until the signal has
//                    returned inside 70 counts (a signal fact, not a timer).
//                    Nothing is suppressed; every opening goes to NAVI.
//   SpatialReference NAVI's Hall reference. A stationary boot reference
//                    (decision 0107), then after every NAVI-accepted opening
//                    that has an IR point: 0-100 mm IR route travel is
//                    clearance, 100-200 mm is collection, and at 200 mm the
//                    median of the collected samples becomes the reference
//                    for the next MM (operator specification, 2026-09-28).
//                    Samples are collected only across measured route
//                    displacement: a stationary span contributes nothing.
//
// No waveform shape, closure, lobe, tail, duration or Gaussian property has
// any authority here or anywhere downstream.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <string.h>
#include "RouteDisplacement.h"

namespace navi_hall {

// MM opening (Twenty Questions handoff §11; 20Q3): departure >= 70 counts on
// two consecutive same-sign samples, opening on the second.
static constexpr int16_t OPENING_DEPARTURE_COUNTS=70;
static constexpr uint8_t OPENING_SAMPLES=2;
// Spatial Hall reference, operator specification for this build (2026-09-28).
// IR-measured route travel after the accepted opening, never time.
static constexpr uint64_t REF_CLEARANCE_END_UM=100000; // 0..100 mm: clearance, no samples
static constexpr uint64_t REF_COLLECT_END_UM=200000;   // 100..200 mm: collect; close at 200 mm
// Boot reference (decision 0107: stationary, operator's clear-of-magnets
// condition). 2000 native samples = the 2 s the operator is already told to
// keep clear of magnets at boot (inherited X22 prime duration; instrument
// initialization only, no navigation authority).
static constexpr uint32_t BOOT_REFERENCE_SAMPLES=2000;
static constexpr uint16_t ADC_LEVELS=4096; // 12-bit ESP32 ADC

struct HallSample{uint64_t tUs=0;int16_t raw=0;uint8_t pwm=0;};

// Single producer (Hall task, core 0), readers on the loop thread (core 1).
template<uint16_t N=1024> class HallSampleRing{
  static_assert((N&(N-1))==0,"power of two");
 public:
  static constexpr uint16_t SIZE=N;
  void push(const HallSample& s){
    const uint32_t h=__atomic_load_n(&head_,__ATOMIC_RELAXED);
    buf_[h&(N-1)]=s;
    __atomic_store_n(&head_,h+1,__ATOMIC_RELEASE);
  }
  uint32_t head()const{return __atomic_load_n(&head_,__ATOMIC_ACQUIRE);}
  // Copies sample i. False if it is not written yet or has been (or may have
  // been, during the copy) overwritten by the producer.
  bool at(uint32_t i,HallSample& out)const{
    uint32_t h=head();
    if(int32_t(h-i)<=0||h-i>=N)return false;
    if(h<N&&i>=h)return false; // before the first write

    out=buf_[i&(N-1)];
    h=head();
    return h-i<N;
  }
 private:
  HallSample buf_[N]{};
  uint32_t head_=0;
};

struct Opening{
  uint64_t tUs=0;uint32_t tMs=0;
  int16_t raw=0,reference=0,departure=0;
  uint8_t polarity=0,pwm=0; // polarity: 1 = N (positive departure), 0 = S
};

class OpeningObserver{
 public:
  // One native sample measured against NAVI's current Hall reference.
  bool sample(const HallSample& s,int16_t reference,Opening& out){
    const int32_t dep=int32_t(s.raw)-reference;
    const int32_t mag=dep<0?-dep:dep;
    if(mag<OPENING_DEPARTURE_COUNTS){armed_=true;run_=0;sign_=0;return false;}
    if(!armed_){++continuing_;return false;} // same excursion as the last opening
    const int8_t sign=dep>0?1:-1;
    if(run_&&sign!=sign_)run_=0;              // two samples on opposite sides are not one excursion
    sign_=sign;
    if(++run_<OPENING_SAMPLES)return false;
    run_=0;sign_=0;armed_=false;++openings_;
    out.tUs=s.tUs;out.tMs=uint32_t(s.tUs/1000);out.raw=s.raw;out.reference=reference;
    out.departure=int16_t(dep);out.polarity=dep>=0?1:0;out.pwm=s.pwm;
    return true;
  }
  uint32_t openings()const{return openings_;}
  uint32_t continuingSamples()const{return continuing_;}
  bool armed()const{return armed_;}
 private:
  bool armed_=true;uint8_t run_=0;int8_t sign_=0;
  uint32_t openings_=0,continuing_=0;
};

enum class RefPhase:uint8_t{Boot,Held,Clearance,Collecting};
inline const char* refPhaseName(RefPhase p){switch(p){case RefPhase::Held:return "HELD";case RefPhase::Clearance:return "CLEARANCE";case RefPhase::Collecting:return "COLLECTING";default:return "BOOT";}}
enum class RefEnd:uint8_t{None,Closed,NoAnchor,Superseded,FrameChanged,EpochBreak,RingGap,NoSamples,Unmeasurable};
inline const char* refEndName(RefEnd e){switch(e){case RefEnd::Closed:return "CLOSED_AT_200MM";case RefEnd::NoAnchor:return "NO_IR_POINT_AT_OPENING";case RefEnd::Superseded:return "SUPERSEDED_BY_ACCEPTED_OPENING";case RefEnd::FrameChanged:return "DECLARATION_OR_DIRECTION";case RefEnd::EpochBreak:return "EPOCH_BREAK";case RefEnd::RingGap:return "HALL_SAMPLE_GAP";case RefEnd::NoSamples:return "NO_SAMPLES_COLLECTED";case RefEnd::Unmeasurable:return "ROUTE_DISTANCE_UNAVAILABLE";default:return "NONE";}}

// What happened on this call, for telemetry.
enum class RefEvent:uint8_t{None,BootReady,Began,CollectionOpened,Closed,Ended};

class SpatialReference{
 public:
  struct Status{
    RefPhase phase=RefPhase::Boot;
    int16_t value=0,previous=0;
    uint32_t anchorSerial=0,closes=0,ends=0;
    uint32_t collected=0,stationarySkipped=0,lastCount=0;
    uint64_t routeUm=0;
    RefEnd lastEnd=RefEnd::None;
    bool saturated=false;
    uint32_t setAtMs=0;
  };
  const Status& status()const{return s_;}
  bool ready()const{return s_.phase!=RefPhase::Boot;}
  int16_t value()const{return s_.value;}
  bool collecting()const{return s_.phase==RefPhase::Clearance||s_.phase==RefPhase::Collecting;}

  // Boot: every native sample until the stationary boot reference exists.
  RefEvent bootSample(int16_t raw,uint32_t nowMs){
    if(ready())return RefEvent::None;
    add(raw);
    if(total_<BOOT_REFERENCE_SAMPLES)return RefEvent::None;
    s_.value=median();s_.previous=s_.value;s_.lastCount=total_;s_.phase=RefPhase::Held;s_.setAtMs=nowMs;
    clear();return RefEvent::BootReady;
  }

  // NAVI accepted an MM opening (event serial). anchor is the IR point NAVI
  // synchronized to that opening, or has no owner when there was none.
  // Any collection in progress belongs to the previous interval and ends.
  template<uint16_t N>
  RefEvent acceptedOpening(uint32_t serial,const ngr_nav::IrOdometryPoint& anchor,uint64_t anchorExcluded,
                           uint64_t anchorLocalUs,const HallSampleRing<N>& ring){
    if(!ready())return RefEvent::None;
    if(collecting())end(RefEnd::Superseded);
    s_.anchorSerial=serial;
    if(!anchor.owner){end(RefEnd::NoAnchor);return RefEvent::Ended;}
    // First native sample at or after the anchor's local time.
    const uint32_t h=ring.head();uint32_t i=h;HallSample x;
    while(h-i<N-1&&ring.at(i-1,x)&&x.tUs>=anchorLocalUs)--i;
    if(h-i>=N-1){end(RefEnd::RingGap);return RefEvent::Ended;}
    anchor_=anchor;anchorExcluded_=anchorExcluded;lastLocalUs_=anchorLocalUs;lastUm_=0;cursor_=i;
    clear();s_.collected=s_.stationarySkipped=0;s_.routeUm=0;s_.phase=RefPhase::Clearance;
    return RefEvent::Began;
  }
  // A declaration, redeclaration or reversal: the route from the accepted
  // opening is no longer continuous. The incumbent reference is retained.
  void frameChanged(){if(collecting())end(RefEnd::FrameChanged);}

  // Every accepted IR point, after RouteDisplacement::observe(). localUs is
  // the point's capture time on this ESP32's clock.
  template<uint16_t N>
  RefEvent irPoint(const ngr_nav::IrOdometryPoint& p,uint64_t localUs,
                   const navi_one::RouteDisplacement& route,const HallSampleRing<N>& ring,uint32_t nowMs){
    if(!collecting())return RefEvent::None;
    if(!navi_one::RouteDisplacement::same(anchor_,p)){end(RefEnd::EpochBreak);return RefEvent::Ended;}
    if(localUs<=lastLocalUs_)return RefEvent::None;
    uint64_t um=0;
    if(!route.routeUm(anchor_,anchorExcluded_,p,um)){
      if(p.capturedUs<anchor_.capturedUs)return RefEvent::None; // a point from before the anchor
      end(RefEnd::Unmeasurable);return RefEvent::Ended;
    }
    const uint64_t t0=lastLocalUs_,t1=localUs,d0=lastUm_,d1=um>lastUm_?um:lastUm_;
    const bool moved=d1>d0;
    // Walk the native samples captured in [t0,t1). A span with no measured
    // route displacement is stationary: its samples are not track traversed.
    HallSample x;
    for(;;){
      if(cursor_==ring.head())break;
      if(!ring.at(cursor_,x)){end(RefEnd::RingGap);return RefEvent::Ended;}
      if(x.tUs>=t1)break;
      ++cursor_;
      if(x.tUs<t0)continue;
      if(!moved){++s_.stationarySkipped;continue;}
      const uint64_t d=d0+(d1-d0)*(x.tUs-t0)/(t1-t0);
      if(d>=REF_CLEARANCE_END_UM&&d<REF_COLLECT_END_UM){add(x.raw);++s_.collected;}
    }
    lastLocalUs_=t1;lastUm_=d1;s_.routeUm=d1;
    RefEvent ev=RefEvent::None;
    if(s_.phase==RefPhase::Clearance&&d1>=REF_CLEARANCE_END_UM){s_.phase=RefPhase::Collecting;ev=RefEvent::CollectionOpened;}
    if(d1>=REF_COLLECT_END_UM){
      if(!total_){end(RefEnd::NoSamples);return RefEvent::Ended;}
      s_.previous=s_.value;s_.value=median();s_.lastCount=total_;s_.setAtMs=nowMs;++s_.closes;
      s_.lastEnd=RefEnd::Closed;s_.phase=RefPhase::Held;clear();return RefEvent::Closed;
    }
    return ev;
  }
 private:
  void end(RefEnd why){s_.lastEnd=why;++s_.ends;s_.phase=RefPhase::Held;clear();}
  void clear(){memset(hist_,0,sizeof(hist_));total_=0;}
  void add(int16_t raw){
    const uint16_t v=uint16_t(raw<0?0:raw>=ADC_LEVELS?ADC_LEVELS-1:raw);
    if(hist_[v]==UINT16_MAX){s_.saturated=true;return;}
    ++hist_[v];++total_;
  }
  // Median of the collected samples. Even count: mean of the two middle
  // values, rounded down.
  int16_t median()const{
    const uint32_t lo=(total_-1)/2,hi=total_/2;uint32_t seen=0;int32_t a=-1,b=-1;
    for(uint16_t v=0;v<ADC_LEVELS&&b<0;++v){
      seen+=hist_[v];
      if(a<0&&seen>lo)a=v;
      if(seen>hi)b=v;
    }
    return int16_t((a+b)/2);
  }
  Status s_{};
  uint16_t hist_[ADC_LEVELS]{};uint32_t total_=0;
  ngr_nav::IrOdometryPoint anchor_{};uint64_t anchorExcluded_=0,lastLocalUs_=0,lastUm_=0;
  uint32_t cursor_=0;
};

} // namespace navi_hall
