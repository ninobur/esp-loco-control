#pragma once
#include <stdint.h>
#include <stddef.h>

// Isolated NAVI evidence primitives. No motor or geographic authority.
namespace ngr_evidence {
enum class Polarity : int8_t { None=0, North=1, South=-1 };
struct Encounter {
  uint64_t openedUs=0, detectedUs=0, irUm=0;
  uint32_t serial=0;
  Polarity polarity=Polarity::None;
  bool irValid=false;
};

class HallQualifier {
 public:
  static constexpr int kThreshold=70;
  static constexpr uint64_t kGuardUs=650000;
  // Call once per 1-kHz sample; a gap breaks two-sample persistence.
  bool observe(int raw,int reference,uint64_t timestampUs,uint32_t serial,
               uint64_t irUm,Encounter& out, bool irCredible=false,
               uint64_t separationUm=0, bool coordinateValid=false) {
    if (haveSample_ && timestampUs<=lastSampleUs_) { resetPending(); return false; }
    const bool contiguous=haveSample_ && serial==lastSampleSerial_+1 &&
        timestampUs-lastSampleUs_>=500 && timestampUs-lastSampleUs_<=1500;
    lastSampleUs_=timestampUs; lastSampleSerial_=serial; haveSample_=true;
    const int delta=raw-reference;
    const Polarity p=delta>=kThreshold?Polarity::North:
                     delta<=-kThreshold?Polarity::South:Polarity::None;
    if (p==Polarity::None) { resetPending(); fieldLatched_=false; return false; }
    if (fieldLatched_) return false; // one continuous field cannot retrigger
    if (p!=pending_ || !contiguous) {
      pending_=p; count_=1; openingUs_=timestampUs;
      openingIrUm_=irUm; openingSerial_=serial; openingIrValid_=coordinateValid; return false;
    }
    if (++count_<2) return false;
    fieldLatched_=true; // latch even if excluded: must leave field before rearming
    if (haveDetection_) {
      if (irCredible && openingIrValid_ && lastIrValid_ && separationUm && openingIrUm_>=lastIrUm_) {
        if (openingIrUm_-lastIrUm_<separationUm) return false;
      } else if (timestampUs-lastDetectionUs_<kGuardUs) return false;
    }
    haveDetection_=true; lastDetectionUs_=timestampUs;
    lastIrUm_=openingIrUm_; lastIrValid_=openingIrValid_;
    out={openingUs_,timestampUs,openingIrUm_,openingSerial_,p,openingIrValid_};
    return true;
  }
  void resetForDeclarationOrReversal(){ *this=HallQualifier{}; }
  // Reversal retains the encounter latch and exclusion origin. A direction
  // change cannot turn the field beneath the locomotive into a new magnet.
  void reverse(){resetPending();}
  void breakSamples(){resetPending();haveSample_=false;}
  bool fieldLatched()const{return fieldLatched_;}
 private:
  void resetPending(){ pending_=Polarity::None; count_=0; }
  Polarity pending_=Polarity::None;
  uint8_t count_=0;
  bool haveSample_=false,haveDetection_=false,fieldLatched_=false;
  uint64_t openingUs_=0,openingIrUm_=0,lastSampleUs_=0,lastDetectionUs_=0;
  uint32_t openingSerial_=0,lastSampleSerial_=0;
  bool openingIrValid_=false,lastIrValid_=false;
  uint64_t lastIrUm_=0;
};

// Observations persist across confirmation. Assignment is separate from sensing.
class DnaHistory {
 public:
  static constexpr size_t kCapacity=12;
  struct Entry {
    Encounter encounter{};
    bool identified=false;
    uint8_t mm=0;
    int8_t direction=0;
    bool contradictory=false;
  };
  void append(const Encounter& e,int8_t direction){
    if(size_==kCapacity){for(size_t i=1;i<size_;++i)data_[i-1]=data_[i];--size_;}
    data_[size_++]={e,false,0,direction};
  }
  bool identify(size_t index,uint8_t mm){
    if(index>=size_)return false;
    data_[index].identified=true;data_[index].mm=mm;return true;
  }
  size_t size()const{return size_;}
  const Entry& at(size_t index)const{return data_[index];}
  void clearForDeclaration(){size_=0;}
  void markContradiction(size_t index){if(index<size_)data_[index].contradictory=true;}
 private:
  Entry data_[kCapacity]{};
  size_t size_=0;
};

// Continuous, read-only sequence check against incumbent geographic assignments.
// Caller provides route polarity and must separately judge whether an encounter
// is real and which MM it represents. No map-wide search or automatic correction.
struct DnaCheck { size_t compared=0,matched=0,contradictions=0; };
template<class ExpectedPolarity>
DnaCheck verifyIdentified(const DnaHistory& history,ExpectedPolarity expected){
  DnaCheck result;
  for(size_t i=0;i<history.size();++i){
    const auto& e=history.at(i);
    if(!e.identified)continue;
    ++result.compared;
    if(expected(e.mm)==e.encounter.polarity)++result.matched;
    else ++result.contradictions;
  }
  return result;
}
}
