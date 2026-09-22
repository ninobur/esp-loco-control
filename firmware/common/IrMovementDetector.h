#pragma once
#include <stdint.h>

namespace ir_movement {
enum Reason : uint8_t { PRIMING, INADEQUATE_CONTRAST, SATURATION,
                       SAMPLE_GAP, SIGNAL_STALE, REACQUIRING, TRACKING };

// TRACKING describes observable optical activity, not validated distance accuracy.
class Detector {
 public:
  static constexpr unsigned Window = 512;
  explicit Detector(bool retainPhase=false) : retainPhase_(retainPhase) {}
  void sample(uint64_t us, uint16_t raw) {
    rise = fall = false;
    if (raw>4095) raw=4095;
    if (haveSample_ && (us <= lastSample_ || us-lastSample_ > 1500)) {
      ++gaps; invalidate(); fill_=0; index_=0;
      for (auto &v : hist_) v=0;
      reason=SAMPLE_GAP;
    }
    lastSample_=us; haveSample_=true;
    if (fill_==Window) --hist_[window_[index_]];
    else ++fill_;
    window_[index_]=raw>>4; ++hist_[raw>>4]; index_=(index_+1)%Window;
    if (!haveEnvelope_ || us-lastEnvelope_ >= 50000) {
      lastEnvelope_=us; haveEnvelope_=true;
      unsigned sum=0; int lo=-1, hi=-1;
      for (int b=0;b<256;++b) {
        sum+=hist_[b];
        if (lo<0 && sum>fill_*5/100) lo=b;
        if (hi<0 && sum>fill_*95/100) hi=b;
      }
      low=lo*16; high=hi*16+15;
    }
    if (raw==0 || raw>=4095) { ++saturated; invalidate(); reason=SATURATION; return; }
    if (fill_<128) { invalidate(); reason=PRIMING; return; }
    const bool contrast=high-low>=120;
    if (!contrast && (!retainPhase_ || !armed_)) { invalidate(); reason=INADEQUATE_CONTRAST; return; }
    if (contrast) {
      thresholdLow_=low+(high-low)/3;
      thresholdHigh_=low+2*(high-low)/3;
    }
    const unsigned tl=thresholdLow_, th=thresholdHigh_;
    if (open_ && us-openAt_>=2500000) { ++aborts; invalidate(); }
    // Rearming always requires an observed low, including after a timeout.
    if (!armed_) { if(raw<tl) armed_=true; reason=REACQUIRING; return; }
    if (!open_ && raw>th) { open_=true; openAt_=us; ++rises; rise=true; }
    else if (open_ && raw<tl) {
      open_=false; ++completed; lastCompleted_=us; haveCompleted_=true; fall=true;
    }
    if (high-low<300) reason=INADEQUATE_CONTRAST;
    else if (!haveCompleted_) reason=REACQUIRING;
    else reason=us-lastCompleted_>=2500000 ? SIGNAL_STALE : TRACKING;
  }
  uint64_t completed=0, rises=0, aborts=0, gaps=0, saturated=0;
  unsigned low=0, high=0;
  bool rise=false, fall=false;
  Reason reason=PRIMING;
  bool inPulse() const { return open_; }
 private:
  void invalidate() { if(open_) { open_=false; } armed_=false; haveCompleted_=false; }
  uint8_t window_[Window]{};
  uint16_t hist_[256]{};
  unsigned fill_=0,index_=0;
  uint64_t lastSample_=0,lastEnvelope_=0,openAt_=0,lastCompleted_=0;
  bool haveSample_=false,haveEnvelope_=false,armed_=false,open_=false,haveCompleted_=false;
  bool retainPhase_=false;
  unsigned thresholdLow_=0, thresholdHigh_=0;
};
} // namespace ir_movement
