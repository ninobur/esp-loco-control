#pragma once
#include <stdint.h>
#include <stddef.h>
namespace navi_eyes {
enum class DartMetric : uint8_t { Distance, Time, None };
enum class DartStop : uint8_t {
  None, NoApplicableReference, NoTimingHistory, IncompleteEvidence,
  MissedTarget, NoHallBySuccessor, TimingContextChanged
};
inline const char* dartStopName(DartStop reason) {
  switch(reason) {
    case DartStop::None:return "NONE";
    case DartStop::NoApplicableReference:return "NO_APPLICABLE_REFERENCE";
    case DartStop::NoTimingHistory:return "NO_PREVIOUS_LAP_TIMING";
    case DartStop::IncompleteEvidence:return "INCOMPLETE_ACQUISITION";
    case DartStop::MissedTarget:return "MISSED_TARGET_SUCCESSOR_OBSERVED";
    case DartStop::NoHallBySuccessor:return "NO_HALL_BY_MM_PLUS_2_CHECK_INFRASTRUCTURE_HARDWARE";
    case DartStop::TimingContextChanged:return "TIMING_CONTEXT_INTERRUPTED";
  }
  return "UNKNOWN";
}
struct DartTravel {
  uint64_t intervalUs=0, departureOffsetUs=0;
  uint32_t fromId=0, toId=0, lap=0;
  uint8_t context=0;
  bool valid=false;
};
// One completed lap supplies the next. The collecting bank never changes the
// reference bank; no averaging, older-lap substitution, or minimum sample count.
class DartTravelHistory {
 public:
  void begin(uint8_t mm,int8_t direction) {
    collecting_=mm<171 && (direction==1 || direction==-1);
    start_=expectedFrom_=mm; direction_=direction; count_=0;
  }
  void interrupt() { collecting_=false; }
  const DartTravel& get(uint8_t mm,int8_t direction) const {
    if(!ready_ || mm>=171 || direction!=referenceDirection_) return unavailable_;
    return banks_[1-writeBank_][mm];
  }
  void record(uint8_t from,uint8_t to,int8_t direction,const DartTravel& record) {
    if(!collecting_) begin(from,direction);
    const uint8_t next=uint8_t((int(from)+direction+171)%171);
    if(!collecting_ || direction!=direction_ || from!=expectedFrom_ || to!=next) {
      begin(to,direction); return;
    }
    banks_[writeBank_][from]=record;
    banks_[writeBank_][from].lap=completedLaps_+1;
    expectedFrom_=to; ++count_;
    if(count_==171 && to==start_) {
      ++completedLaps_; referenceDirection_=direction_; ready_=true;
      writeBank_=1-writeBank_; count_=0; start_=expectedFrom_=to;
    }
  }
  uint32_t completedLaps() const { return completedLaps_; }
  uint16_t collectedIntervals() const { return count_; }
 private:
  DartTravel banks_[2][171]{}, unavailable_{};
  uint32_t completedLaps_=0;
  uint16_t count_=0;
  uint8_t writeBank_=0,start_=0,expectedFrom_=0;
  int8_t direction_=0,referenceDirection_=0;
  bool collecting_=false,ready_=false;
};
}
