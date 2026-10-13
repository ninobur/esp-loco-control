#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// Observation-only state. No navigator, movement, health or motor reference.
// All methods run on the loop task, after transport copies the wire records.
namespace ir_sync {
struct __attribute__((packed)) PulseWire {
  uint16_t magic; uint8_t version,type; uint32_t sid,sequence;
  uint64_t bootId,completedPulses,completedUs,intervalUs,nominalUm;
  uint32_t pitchUm; uint8_t opticalReason; uint16_t span,crc;
};
static_assert(sizeof(PulseWire)==61,"Type-6 contract mismatch");
static_assert(offsetof(PulseWire,crc)==59,"Type-6 CRC offset mismatch");
inline uint16_t crc(const uint8_t* data,size_t n) {
  uint16_t c=0xffff;
  while(n--){c^=uint16_t(*data++)<<8;
    for(unsigned i=0;i<8;++i)c=(c&0x8000)?uint16_t((c<<1)^0x1021):uint16_t(c<<1);}
  return c;
}
struct Point {
  uint64_t receivedUs=0,pulses=0,boot=0;
  uint32_t sequence=0;
  bool valid=false,continuous=false;
};
struct Selection { Point point{}; bool historyLost=false; };
template<size_t N> class History {
 public:
  void clear(){head_=used_=0;overwritten_=false;}
  void add(const Point& p){
    if(used_==N)overwritten_=true;else ++used_;
    points_[head_]=p;head_=(head_+1)%N;
  }
  Selection at(uint64_t hallUs) const {
    for(size_t i=0;i<used_;++i){
      const auto& p=points_[(head_+N-1-i)%N];
      if(p.receivedUs<=hallUs)return {p,false};
    }
    return {{},overwritten_};
  }
 private:
  Point points_[N]{};
  size_t head_=0,used_=0;
  bool overwritten_=false;
};
struct Hall { uint64_t capturedUs=0; uint32_t serial=0; };
class Diagnostics {
 public:
  void pair(const uint8_t* mac,uint64_t now) {
    memcpy(mac_,mac,6);uint8_t any=0;
    for(unsigned i=0;i<6;++i)any|=mac_[i];
    paired_=any && !(mac_[0]&1);acceptAfter_=now;
    type5_.clear();type6_.clear();last6_={};forceBreak_=false;
    hallDrops+=hallUsed_;hallHead_=hallUsed_=0;
  }
  void type5(uint64_t arrived,uint64_t pulses,uint64_t boot,uint32_t sequence) {
    if(paired_ && arrived>acceptAfter_)type5_.add({arrived,pulses,boot,sequence,true,true});
  }
  void transportGap(){forceBreak_=true;}
  bool type6(const uint8_t* mac,const uint8_t* bytes,size_t length,uint64_t arrived) {
    if(!paired_ || memcmp(mac_,mac,6)){++wrongSource;return false;}
    if(arrived<=acceptAfter_){++old;return false;}
    if(length!=sizeof(PulseWire)){++bad;return false;}
    PulseWire e{};memcpy(&e,bytes,sizeof(e));
    if(e.magic!=0x4952 || e.version!=1 || e.type!=6 || !e.bootId ||
       !e.completedPulses || !e.completedUs || !e.pitchUm ||
       e.completedPulses>UINT64_MAX/e.pitchUm ||
       e.nominalUm!=e.completedPulses*uint64_t(e.pitchUm) ||
       crc(bytes,offsetof(PulseWire,crc))!=e.crc){++bad;return false;}
    const bool same=last6_.valid && last6_.boot==e.bootId;
    const uint32_t step=e.sequence-last6_.sequence;
    if(last6_.valid && arrived<last6_.receivedUs){++old;return false;}
    if(same && (step==0 || step>=0x80000000u || e.completedPulses<=last6_.pulses)){
      ++old;return false;
    }
    const bool adjacent=same && step==1 && e.completedPulses-last6_.pulses==1;
    if(same && !adjacent)++gaps;
    last6_={arrived,e.completedPulses,e.bootId,e.sequence,true,adjacent && !forceBreak_};
    forceBreak_=false;type6_.add(last6_);++accepted;
    return true;
  }
  void hall(uint32_t serial,uint64_t capturedUs) {
    if(hallUsed_==16){++hallDrops;return;}
    halls_[(hallHead_+hallUsed_)%16]={capturedUs,serial};++hallUsed_;
  }
  bool nextHall(Hall& out) const {if(!hallUsed_)return false;out=halls_[hallHead_];return true;}
  void popHall(){if(hallUsed_){hallHead_=(hallHead_+1)%16;--hallUsed_;}}
  Selection type5At(uint64_t us) const{return type5_.at(us);}
  Selection type6At(uint64_t us) const{return type6_.at(us);}
  int format(char* out,size_t size,const Hall& hall,uint32_t drop5,uint32_t drop6) const {
    const auto a=type5_.at(hall.capturedUs),b=type6_.at(hall.capturedUs);
    const bool match=a.point.valid && b.point.valid && a.point.boot==b.point.boot;
    const bool eligible=match && b.point.continuous;
    const bool negative=match && b.point.pulses<a.point.pulses;
    const uint64_t delta=!match?0:(negative?a.point.pulses-b.point.pulses:b.point.pulses-a.point.pulses);
    return snprintf(out,size,
      "[IR_SYNC] hall=%lu t_us=%llu src=%02x:%02x:%02x:%02x:%02x:%02x "
      "t5=%llu t5_age_ms=%lld t5_valid=%u t5_boot=%016llx t5_seq=%lu "
      "t6=%llu t6_age_ms=%lld t6_valid=%u t6_boot=%016llx t6_seq=%lu "
      "t6_cont=%u same_boot=%u compare_ok=%u t6_minus_t5=%s%llu "
      "gaps=%lu bad=%lu old=%lu foreign=%lu rx5drop=%lu rx6drop=%lu "
      "hist5lost=%u hist6lost=%u halldrop=%lu authority=OBSERVE_ONLY\n",
      (unsigned long)hall.serial,(unsigned long long)hall.capturedUs,
      unsigned(mac_[0]),unsigned(mac_[1]),unsigned(mac_[2]),unsigned(mac_[3]),unsigned(mac_[4]),unsigned(mac_[5]),
      (unsigned long long)a.point.pulses,a.point.valid?(long long)((hall.capturedUs-a.point.receivedUs)/1000):-1LL,
      unsigned(a.point.valid),(unsigned long long)a.point.boot,(unsigned long)a.point.sequence,
      (unsigned long long)b.point.pulses,b.point.valid?(long long)((hall.capturedUs-b.point.receivedUs)/1000):-1LL,
      unsigned(b.point.valid),(unsigned long long)b.point.boot,(unsigned long)b.point.sequence,
      unsigned(b.point.continuous),unsigned(match),unsigned(eligible),negative?"-":"",(unsigned long long)delta,
      (unsigned long)gaps,(unsigned long)bad,(unsigned long)old,(unsigned long)wrongSource,
      (unsigned long)drop5,(unsigned long)drop6,unsigned(a.historyLost),unsigned(b.historyLost),(unsigned long)hallDrops);
  }
  uint32_t gaps=0,bad=0,old=0,wrongSource=0,accepted=0,hallDrops=0;
 private:
  History<64> type5_;
  History<64> type6_;
  Point last6_{};
  Hall halls_[16]{};
  size_t hallHead_=0,hallUsed_=0;
  uint8_t mac_[6]{};
  uint64_t acceptAfter_=0;
  bool paired_=false,forceBreak_=false;
};
} // namespace ir_sync
