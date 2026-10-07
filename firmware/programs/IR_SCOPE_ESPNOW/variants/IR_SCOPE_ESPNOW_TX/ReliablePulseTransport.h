#pragma once
#include <cstring>
#include "../../../../common/IrPulseInput.h"

// Observation-only transport. Native PulseEventPacket bytes are never edited.
// All classes are task-owned; callers supply zero-wait inter-task handoffs.
namespace ir_link {
constexpr unsigned Capacity=256, AckBatch=8;
constexpr uint64_t DiagnosticUs=1000000, RetrySpacingUs=20000;
constexpr uint8_t Channel=11;
constexpr uint32_t Otto=9950011;
struct __attribute__((packed)) Header { uint16_t magic=0x4952; uint8_t version=2,type=0; };
struct __attribute__((packed)) Hello {
  Header h{0x4952,2,8}; uint32_t loco=Otto,sequence=0;
  uint64_t session=0,txBoot=0; uint8_t pairedMac[6]{},channel=0; uint16_t crc=0;
};
struct __attribute__((packed)) Join {
  Header h{0x4952,2,10}; uint64_t session=0,txBoot=0; uint32_t challenge=0; uint16_t crc=0;
};
struct __attribute__((packed)) Event {
  Header h{0x4952,2,6}; uint64_t session=0,generation=0,ageUs=0;
  uint16_t slot=0; uint32_t attempt=0;
  PulseEventPacket pulse{}; uint16_t crc=0;
};
struct __attribute__((packed)) AckItem {
  uint64_t generation=0; uint32_t sequence=0; uint16_t slot=0;
};
struct __attribute__((packed)) Ack {
  Header h{0x4952,2,9}; uint64_t session=0,txBoot=0;
  uint8_t count=0; AckItem items[AckBatch]{}; uint16_t crc=0;
};
struct __attribute__((packed)) Status {
  Header h{0x4952,2,7}; uint64_t session=0,txBoot=0; uint32_t sequence=0;
  uint32_t generated=0,overflow=0,attempts=0,retries=0,macOK=0,macFail=0;
  uint32_t acknowledged=0,ackTimely=0,ackUncertain=0,unresolved=0,high=0;
  uint32_t timeouts=0,sessionChanges=0,priorUnresolved=0;
  uint64_t oldestUs=0,ackAgeMaxUs=0,confirmedCount=0,confirmedUpperUs=0;
  uint8_t channel=Channel; uint16_t crc=0;
};
static_assert(sizeof(Event)<=250 && sizeof(Ack)<=250 && sizeof(Status)<=250,"ESP-NOW v1 capacity");
static_assert(sizeof(Event)==97 && sizeof(Ack)==135 && sizeof(Status)==115 && sizeof(Hello)==37 && sizeof(Join)==26,"v2 wire contract");
inline uint16_t crc16(const uint8_t* p,size_t n) {
  uint16_t c=0xffff;
  while(n--){c^=uint16_t(*p++)<<8;for(unsigned i=0;i<8;++i)c=c&0x8000?uint16_t((c<<1)^0x1021):uint16_t(c<<1);}
  return c;
}
template<class T> inline void seal(T& p) { p.crc=crc16(reinterpret_cast<const uint8_t*>(&p),sizeof(T)-2); }
template<class T> inline bool valid(const T& p,uint8_t type) {
  return p.h.magic==0x4952 && p.h.version==2 && p.h.type==type &&
    p.crc==crc16(reinterpret_cast<const uint8_t*>(&p),sizeof(T)-2);
}
inline bool forward(uint32_t a,uint32_t b){const uint32_t d=a-b;return d && d<0x80000000u;}
inline bool sameMac(const uint8_t* a,const uint8_t* b){return std::memcmp(a,b,6)==0;}
inline bool configured(const uint8_t* mac){uint8_t any=0;for(unsigned i=0;i<6;++i)any|=mac[i];return any && !(mac[0]&1);}
inline bool validPulse(const PulseEventPacket& e) {
  return e.magic==0x4952 && e.version==1 && e.type==6 && e.bootId && e.completedPulses && e.completedUs &&
    e.sequence==uint32_t(e.completedPulses) && e.pitchUm==ir_movement::kInstalledPitchUm &&
    e.completedPulses<=UINT64_MAX/e.pitchUm && e.nominalUm==e.completedPulses*uint64_t(e.pitchUm) &&
    e.opticalReason<=ir_movement::TRACKING && e.intervalUs<=e.completedUs &&
    e.crc==crc16(reinterpret_cast<const uint8_t*>(&e),sizeof(e)-2);
}
enum class Timing : uint8_t { Uncertain, Timely, Late };
inline Timing timing(uint64_t lower,uint64_t upper=0) {
  if(lower>DiagnosticUs)return Timing::Late;
  if(upper && upper<=DiagnosticUs)return Timing::Timely;
  return Timing::Uncertain;
}
inline const char* name(Timing t){return t==Timing::Timely?"timely":t==Timing::Late?"late":"uncertain";}

class Transmitter {
 public:
  struct Slot { PulseEventPacket pulse{}; uint64_t generation=0,nextUs=0; uint32_t attempts=0; bool used=false; };
  Slot slots[Capacity]{};
  uint64_t session=0,boot=0,lastRetryUs=0,ackAgeMaxUs=0,confirmedCount=0,confirmedUpperUs=0;
  uint32_t count=0,attempts=0,retries=0,acknowledged=0,ackTimely=0,ackUncertain=0,sessionChanges=0,priorUnresolved=0;
  unsigned retryCursor=0;
  void connect(uint64_t s) {
    if(session==s)return;
    if(session){++sessionChanges;priorUnresolved=count;}
    session=s;
    confirmedCount=confirmedUpperUs=0;
    // Replays remain retries; a receiver reboot cannot promote old backlog over new pulses.
    for(auto& slot:slots)if(slot.used)slot.nextUs=0;
  }
  bool enqueue(const PulseEventPacket& e) {
    for(auto& s:slots)if(!s.used){s.used=true;s.pulse=e;++s.generation;s.attempts=0;s.nextUs=0;++count;boot=e.bootId;return true;}
    return false;
  }
  int choose(uint64_t now) const {
    int first=-1;
    for(unsigned i=0;i<Capacity;++i)if(slots[i].used && !slots[i].attempts &&
      (first<0 || slots[i].pulse.completedPulses<slots[first].pulse.completedPulses))first=int(i);
    if(first>=0)return first;
    if(now-lastRetryUs<RetrySpacingUs)return -1;
    for(unsigned n=0;n<Capacity;++n){unsigned i=(retryCursor+n)%Capacity;
      if(slots[i].used && now>=slots[i].nextUs)return int(i);}
    return -1;
  }
  Event submit(unsigned i,uint64_t now) {
    auto& s=slots[i];
    if(s.attempts){++retries;lastRetryUs=now;retryCursor=(i+1)%Capacity;}
    ++s.attempts;++attempts;
    s.nextUs=now+(uint64_t(100000)<<(s.attempts<4?s.attempts-1:3));
    Event e{};e.session=session;e.slot=i;e.generation=s.generation;e.attempt=s.attempts;
    e.ageUs=now>=s.pulse.completedUs?now-s.pulse.completedUs:0;e.pulse=s.pulse;seal(e);return e;
  }
  unsigned acknowledge(const Ack& a,uint64_t now) {
    if(!valid(a,9) || a.session!=session || a.txBoot!=boot || a.count>AckBatch)return 0;
    unsigned released=0;
    for(unsigned n=0;n<a.count;++n){const auto& k=a.items[n];if(k.slot>=Capacity)continue;auto& s=slots[k.slot];
      if(!s.used || !s.attempts || k.generation!=s.generation || k.sequence!=s.pulse.sequence)continue;
      const uint64_t age=now>=s.pulse.completedUs?now-s.pulse.completedUs:UINT64_MAX;
      if(age>ackAgeMaxUs)ackAgeMaxUs=age;
      if(age<=DiagnosticUs)++ackTimely;else ++ackUncertain;
      if(s.pulse.completedPulses>=confirmedCount){confirmedCount=s.pulse.completedPulses;confirmedUpperUs=age;}
      s.used=false;--count;++released;++acknowledged;
    }
    return released;
  }
  uint64_t oldest(uint64_t now) const {uint64_t age=0;for(const auto& s:slots)if(s.used && now>=s.pulse.completedUs && now-s.pulse.completedUs>age)age=now-s.pulse.completedUs;return age;}
};

// Each slot is retained until its next generation proves transmitter release.
// Unlike a sliding sequence bitmap, one ancient hole cannot pin the window.
class Receiver {
 public:
  struct Entry { PulseEventPacket pulse{}; uint64_t generation=0; bool have=false; };
  Entry entries[Capacity]{};
  uint64_t session=0,boot=0,highest=0,first=0,received=0,missing=0,priorMissing=0;
  uint64_t currentReceivedUs=0,currentLowerUs=0,currentUpperUs=0,acceptanceLowerUs=0,lastStatusUs=0;
  uint64_t previousBoot=0;
  uint32_t previousUnresolved=0;bool previousStatusKnown=false;
  uint32_t duplicates=0,historical=0,invalid=0,stale=0,bootChanges=0,statusGaps=0,statusOrder=0;
  uint32_t lastStatus=0; bool haveStatus=false;
  PulseEventPacket current{}; Status status{};
  void start(uint64_t s) {
    // No Receiver-sized temporary on the ESP32 task stack.
    for(auto& e:entries)e={};session=s;boot=highest=first=received=missing=priorMissing=0;
    currentReceivedUs=currentLowerUs=currentUpperUs=acceptanceLowerUs=lastStatusUs=0;
    previousBoot=0;previousUnresolved=0;previousStatusKnown=false;
    duplicates=historical=invalid=stale=bootChanges=statusGaps=statusOrder=lastStatus=0;
    haveStatus=false;current={};status={};
  }
  void bind(uint64_t b) {
    if(boot==b)return;
    if(boot){++bootChanges;priorMissing=missing;previousBoot=boot;
      previousStatusKnown=haveStatus;previousUnresolved=status.unresolved;}
    boot=b;highest=first=received=missing=0;current={};currentReceivedUs=currentLowerUs=currentUpperUs=acceptanceLowerUs=0;
    haveStatus=false;lastStatusUs=0;status={};for(auto& e:entries)e={};
  }
  // 0 rejected; 1 unique current; 2 unique historical; 3 identical duplicate.
  int accept(const Event& e,uint64_t arrival,uint64_t now,AckItem& ack) {
    if(!valid(e,6) || e.session!=session || !boot || e.pulse.bootId!=boot ||
       e.slot>=Capacity || !e.generation || !e.attempt || !validPulse(e.pulse)) {++invalid;return 0;}
    auto& slot=entries[e.slot];
    if(slot.have && e.generation<slot.generation){++stale;return 0;}
    if(slot.have && e.generation==slot.generation){
      if(std::memcmp(&e.pulse,&slot.pulse,sizeof(e.pulse))){++invalid;return 0;}
      ++duplicates;ack={e.generation,e.pulse.sequence,e.slot};return 3;
    }
    // Validate source consistency and cross-event chronology without rejecting gaps.
    for(const auto& prior:entries)if(prior.have){
      const auto& p=prior.pulse;const auto& q=e.pulse;
      if(p.sid!=q.sid || p.completedPulses==q.completedPulses ||
         (p.completedPulses<q.completedPulses && p.completedUs>=q.completedUs) ||
         (p.completedPulses>q.completedPulses && p.completedUs<=q.completedUs)) {++invalid;return 0;}
    }
    const bool latest=e.pulse.completedPulses>highest;
    if(!first)first=e.pulse.completedPulses;
    if(latest)highest=e.pulse.completedPulses;
    ++received;missing=highest-received; // Since transmitter boot, includes initial holes.
    slot.pulse=e.pulse;slot.generation=e.generation;slot.have=true;
    if(latest){current=e.pulse;currentReceivedUs=arrival;currentUpperUs=0;
      currentLowerUs=e.ageUs;
      const uint64_t queued=now>=arrival?now-arrival:0;
      acceptanceLowerUs=e.ageUs>UINT64_MAX-queued?UINT64_MAX:e.ageUs+queued;
    } else ++historical;
    ack={e.generation,e.pulse.sequence,e.slot};return latest?1:2;
  }
  bool observeStatus(const Status& s,uint64_t now) {
    if(!valid(s,7) || s.session!=session || !boot || s.txBoot!=boot){++invalid;return false;}
    if(haveStatus){if(!forward(s.sequence,lastStatus)){++statusOrder;return false;}statusGaps+=s.sequence-lastStatus-1;}
    else if(s.sequence)statusGaps+=s.sequence-1;
    lastStatus=s.sequence;haveStatus=true;lastStatusUs=now;status=s;
    if(current.completedPulses && s.confirmedCount==current.completedPulses)currentUpperUs=s.confirmedUpperUs;
    return true;
  }
};
} // namespace ir_link
