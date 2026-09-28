#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include "../../../../common/IrMovementWire.h"

namespace ngr_nav {
using ir_movement::WireSnapshot;
enum class MotionIssue : uint8_t { None, NoSource, Stale, Unaligned, Reset, Order, Optical, Interrupted, Direction };
inline const char* issueName(MotionIssue r) {
  switch(r) {
    case MotionIssue::None:return "NOMINAL_ONLY";
    case MotionIssue::NoSource:return "NO_SOURCE";
    case MotionIssue::Stale:return "STALE";
    case MotionIssue::Unaligned:return "CLOCK_UNCERTAIN";
    case MotionIssue::Reset:return "RESET";
    case MotionIssue::Order:return "COUNTER_ORDER";
    case MotionIssue::Optical:return "OPTICAL_INVALID";
    case MotionIssue::Interrupted:return "INTERRUPTED";
    default:return "DIRECTION_CHANGE";
  }
}
struct MotionPoint {
  WireSnapshot wire{};
  uint64_t arrivalUs=0;
  int64_t alignmentUs=0;
  uint32_t frame=0;
  MotionIssue issue=MotionIssue::NoSource;
};
struct MotionInterval {
  uint64_t pulses=0;
  double nominalMm=0;
  MotionIssue issue=MotionIssue::NoSource;
  bool usable() const {return issue==MotionIssue::None;}
  // Today's calibration has no validated physical-error bound.
  bool bounded() const {return false;}
};
inline MotionInterval between(const MotionPoint& a,const MotionPoint& b) {
  MotionInterval r;
  if(a.issue!=MotionIssue::None){r.issue=a.issue;return r;}
  if(b.issue!=MotionIssue::None){r.issue=b.issue;return r;}
  const auto& x=a.wire;const auto& y=b.wire;
  if(a.frame!=b.frame){r.issue=MotionIssue::Direction;return r;}
  if(!x.bootId || x.bootId!=y.bootId){r.issue=MotionIssue::Reset;return r;}
  if(y.capturedUs<=x.capturedUs || y.completedPulses<x.completedPulses || x.pitchUm!=y.pitchUm){r.issue=MotionIssue::Order;return r;}
  if(x.opticalReason!=ir_movement::TRACKING || y.opticalReason!=ir_movement::TRACKING){r.issue=MotionIssue::Optical;return r;}
  if(x.unreliableSamples!=y.unreliableSamples || x.saturatedSamples!=y.saturatedSamples ||
     x.sampleGaps!=y.sampleGaps || x.openAborts!=y.openAborts ||
     x.inferredAdded || y.inferredAdded || x.inferredRemoved || y.inferredRemoved){r.issue=MotionIssue::Interrupted;return r;}
  r.pulses=y.completedPulses-x.completedPulses;
  r.nominalMm=double(r.pulses)*x.pitchUm/1000.0;r.issue=MotionIssue::None;return r;
}
inline uint16_t movementCrc(const uint8_t* p,size_t n) {
  uint16_t c=0xffff;
  while(n--){c^=uint16_t(*p++)<<8;for(unsigned i=0;i<8;++i)c=c&0x8000 ? uint16_t((c<<1)^0x1021):uint16_t(c<<1);}
  return c;
}
inline bool validMac(const uint8_t* p) {
  uint8_t v=0;for(unsigned i=0;i<6;++i)v|=p[i];return v && !(p[0]&1);
}
enum class RxResult : uint8_t { Accepted, Unpaired, WrongSource, Invalid, Duplicate, Old, Reset, Exhausted };

// Transport-independent history: a future GPIO adapter can submit the same
// snapshots with capture and arrival in the local clock domain.
class MovementSource {
 public:
  void pair(const uint8_t* mac){memcpy(mac_,mac,6);paired_=validMac(mac);used_=head_=0;retiredCount_=0;have_=false;exhausted_=false;++frame_;}
  const uint8_t* mac() const{return mac_;}
  bool paired() const{return paired_;}
  void newFrame(){++frame_;used_=head_=0;}
  void transportGap(){newFrame();}
  uint32_t frame() const{return frame_;}
  const WireSnapshot& latest() const{return last_;}
  uint64_t lastArrival() const{return arrival_;}
  bool have() const{return have_;}
  uint32_t accepted=0,rejected=0,duplicates=0,reboots=0;
  RxResult receive(const uint8_t* mac,const uint8_t* bytes,size_t length,uint64_t now) {
    if(!paired_){++rejected;return RxResult::Unpaired;}
    if(memcmp(mac_,mac,6)){++rejected;return RxResult::WrongSource;}
    if(length!=sizeof(WireSnapshot)){++rejected;return RxResult::Invalid;}
    WireSnapshot w;memcpy(&w,bytes,sizeof(w));
    if(now>uint64_t(INT64_MAX/4) || w.capturedUs>uint64_t(INT64_MAX/4) ||
       w.magic!=0x4952 || w.version!=1 || w.type!=5 || !w.bootId ||
       w.opticalReason>ir_movement::TRACKING || w.distanceValidated || !w.pitchUm ||
       w.completedPulses>w.observedRises || w.completedPulses>UINT64_MAX/w.pitchUm ||
       w.nominalUm!=w.completedPulses*w.pitchUm ||
       movementCrc(bytes,offsetof(WireSnapshot,crc))!=w.crc){++rejected;return RxResult::Invalid;}
    if(exhausted_){++rejected;return RxResult::Exhausted;}
    bool reset=false;
    if(have_ && w.bootId!=last_.bootId){
      for(unsigned i=0;i<retiredCount_;++i)if(retired_[i]==w.bootId){++rejected;return RxResult::Old;}
      if(retiredCount_==8){exhausted_=true;newFrame();++rejected;return RxResult::Exhausted;}
      retired_[retiredCount_++]=last_.bootId;newFrame();reset=true;++reboots;have_=false;
    }
    if(have_){
      if(w.sequence==last_.sequence){++duplicates;return RxResult::Duplicate;}
      if(int32_t(w.sequence-last_.sequence)<=0 || w.capturedUs<=last_.capturedUs || now<arrival_ ||
         w.completedPulses<last_.completedPulses || w.observedRises<last_.observedRises ||
         w.unreliableSamples<last_.unreliableSamples || w.saturatedSamples<last_.saturatedSamples ||
         w.sampleGaps<last_.sampleGaps || w.openAborts<last_.openAborts ||
         w.inferredAdded<last_.inferredAdded || w.inferredRemoved<last_.inferredRemoved ||
         w.pitchUm!=last_.pitchUm || w.calibrationId!=last_.calibrationId){++rejected;return RxResult::Old;}
    }
    const int64_t offset=int64_t(now)-int64_t(w.capturedUs);
    if(!have_ || offset<offset_)offset_=offset;
    last_=w;arrival_=now;have_=true;++accepted;
    history_[head_]={w,now,0,frame_,MotionIssue::None};head_=(head_+1)%64;if(used_<64)++used_;
    return reset?RxResult::Reset:RxResult::Accepted;
  }
  MotionPoint at(uint64_t eventUs,uint64_t now) const {
    MotionPoint out;
    if(eventUs>uint64_t(INT64_MAX/4) || now>uint64_t(INT64_MAX/4))return out;
    if(!have_ || !used_ || exhausted_)return out;
    if(now<arrival_ || now-arrival_>1000000){out.issue=MotionIssue::Stale;return out;}
    uint64_t best=UINT64_MAX;
    for(unsigned i=0;i<used_;++i){
      const auto& p=history_[i];
      const int64_t delta=int64_t(p.wire.capturedUs)+offset_-int64_t(eventUs);
      uint64_t d=delta<0?uint64_t(-delta):uint64_t(delta);
      if(d<best){out=p;out.alignmentUs=delta;best=d;}
    }
    // This is a diagnostic alignment gate, NOT a measured delay/error bound.
    out.issue=best<=150000?MotionIssue::None:MotionIssue::Unaligned;
    return out;
  }
 private:
  MotionPoint history_[64]{};
  WireSnapshot last_{};
  uint64_t retired_[8]{},arrival_=0;
  uint8_t mac_[6]{};
  uint8_t used_=0,head_=0,retiredCount_=0;
  uint32_t frame_=1;
  int64_t offset_=0;
  bool paired_=false,have_=false,exhausted_=false;
};
} // namespace ngr_nav
