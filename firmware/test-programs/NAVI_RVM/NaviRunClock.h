#pragma once
#include <stdint.h>
#include <stddef.h>

namespace navi_eyes {
// Wall-clock evidence is never rewritten. This clock removes only manual
// STOP duration. Scheduled station braking/dwell/departure continues to count.
// Recent transitions allow queued Hall samples to retain their acquisition
// time coordinate even when STOP/GO commands are consumed before that queue.
class NaviRunClock {
 public:
  void stopped(bool stop,uint64_t nowUs) {
    if(stop==paused_) return;
    if(stop) { paused_=true;start_=nowUs;return; }
    if(nowUs<start_) return;
    if(count_==kCapacity) {
      const auto& old=stops_[head_];
      base_+=old.end-old.start;coveredFrom_=old.end;
      head_=(head_+1)%kCapacity;--count_;
    }
    stops_[(head_+count_)%kCapacity]={start_,nowUs};++count_;
    paused_=false;
  }
  bool removedBefore(uint64_t wallUs,uint64_t& removedUs) const {
    if(wallUs<coveredFrom_) return false;
    removedUs=base_;
    for(size_t i=0;i<count_;++i) {
      const auto& s=stops_[(head_+i)%kCapacity];
      if(wallUs<=s.start) return true;
      removedUs+=(wallUs<s.end?wallUs:s.end)-s.start;
      if(wallUs<s.end) return true;
    }
    if(paused_ && wallUs>start_) removedUs+=wallUs-start_;
    return true;
  }
  uint64_t now(uint64_t wallUs) const {
    uint64_t removed=0;
    return removedBefore(wallUs,removed) && wallUs>=removed?wallUs-removed:0;
  }
  bool paused() const { return paused_; }
  bool interruptions(uint64_t from,uint64_t to,uint32_t& count) const {
    count=0;
    if(from<coveredFrom_ || to<from) return false;
    for(size_t i=0;i<count_;++i) {
      const auto& s=stops_[(head_+i)%kCapacity];
      if(s.start<to && s.end>from) ++count;
    }
    if(paused_ && start_<to) ++count;
    return true;
  }
 private:
  struct Stop { uint64_t start=0,end=0; };
  // Matches the command queue's bounded depth. Older selected/candidate
  // coordinates are already frozen; never guess a lost timestamp association.
  static constexpr size_t kCapacity=16;
  Stop stops_[kCapacity]{};
  size_t head_=0,count_=0;
  uint64_t start_=0,base_=0,coveredFrom_=0;
  bool paused_=false;
};
}
