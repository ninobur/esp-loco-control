#pragma once
#include <stdint.h>
#include <stddef.h>

// NAVI_EWO_0_3 isolated restoration primitive. No motor or geographic authority.
// Geographic identity is assigned by the NAVI integration, never by this detector.
namespace ngr_evidence {
enum class Polarity : int8_t { None=0, North=1, South=-1 };
struct Encounter { uint64_t openedUs; uint64_t detectedUs; uint64_t irUm; uint32_t serial; Polarity polarity; };

class HallQualifier {
 public:
  static constexpr int kThreshold = 70;
  static constexpr uint64_t kGuardUs = 650000;
  // Samples must be contiguous at the acquisition cadence, not two ADC conversions in one burst.
  bool observe(int raw, int reference, uint64_t timestampUs, uint32_t serial,
               uint64_t irUm, Encounter& out) {
    const int delta=raw-reference;
    const Polarity p=delta>=kThreshold?Polarity::North:
                     delta<=-kThreshold?Polarity::South:Polarity::None;
    if (p==Polarity::None) { pending_=Polarity::None; count_=0; return false; }
    if (p!=pending_ || (haveSample_ && timestampUs-lastSampleUs_>2000)) {
      pending_=p; count_=1; openingUs_=timestampUs; openingIrUm_=irUm; openingSerial_=serial;
    } else if (count_<2) ++count_;
    lastSampleUs_=timestampUs; haveSample_=true;
    if (count_<2) return false;
    // Exclude repeat excursions from the same encounter starting at detection.
    if (haveAccepted_ && timestampUs-lastAcceptedDetectionUs_<kGuardUs) return false;
    haveAccepted_=true; lastAcceptedDetectionUs_=timestampUs;
    out={openingUs_,timestampUs,openingIrUm_,openingSerial_,p};
    return true;
  }
  void reset() { *this=HallQualifier{}; }
 private:
  Polarity pending_=Polarity::None;
  uint8_t count_=0;
  bool haveSample_=false,haveAccepted_=false;
  uint64_t openingUs_=0,openingIrUm_=0,lastSampleUs_=0,lastAcceptedDetectionUs_=0;
  uint32_t openingSerial_=0;
};

// Retains evidence across normal confirmations. Caller decides whether a
// field is geographically identified; UNKNOWN identity is never invented.
class DnaHistory {
 public:
  static constexpr size_t kCapacity=12;
  struct Entry { Encounter encounter; bool identified; uint8_t mm; int8_t direction; };
  void append(const Encounter& e,int8_t direction) {
    if (size_==kCapacity) { for(size_t i=1;i<size_;++i) data_[i-1]=data_[i]; --size_; }
    data_[size_++]={e,false,0,direction};
  }
  bool identify(size_t index,uint8_t mm) {
    if(index>=size_) return false;
    data_[index].identified=true; data_[index].mm=mm; return true;
  }
  size_t size() const { return size_; }
  const Entry& at(size_t index) const { return data_[index]; }
  void clearForDeclaration() { size_=0; }
 private:
  Entry data_[kCapacity]{};
  size_t size_=0;
};
}
