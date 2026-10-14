#pragma once
#include "NaviStopOverlay.h"

namespace navi_eyes {
enum class TileTimingMode : uint8_t { Geographic, HoldUntilOpening, Ramp };
struct TileTimingCommand {
  StopGeography geography;
  TileTimingMode mode=TileTimingMode::Geographic;
  bool available=false, entryNow=false, missingHistory=false;
  uint8_t entryPwm=0;
  uint64_t elapsedUs=0, durationUs=0;
};

// Throttle execution only. This class cannot observe sensors, select magnets,
// change geographic identity, or advance a tile when its timer expires.
class NaviTileTiming {
 public:
  void reset() { mode_=TileTimingMode::Geographic; lastSelection_=0; durationUs_=0; }
  void suspend(uint32_t selection) {
    lastSelection_=selection; mode_=TileTimingMode::HoldUntilOpening;
  }
  TileTimingCommand command(const StopView& view,uint32_t selection,
      uint64_t selectedDecisionUs,uint64_t nowUs,int32_t targetUm,
      const DartTravel& previousLap,uint8_t cruise,uint8_t nextCruise,
      uint8_t currentCommand) {
    TileTimingCommand out;
    const bool newOpening=selection && selection!=lastSelection_;
    lastSelection_=selection;
    // Preserve the inherited coarse declaration instruction before the first
    // physical anchor, as well as normal fine-IR geography.
    if(view.available) {
      mode_=TileTimingMode::Geographic;
      out.available=view.available;out.geography=view.geography;return out;
    }
    if(!view.identityAvailable) {
      mode_=TileTimingMode::HoldUntilOpening;out.mode=mode_;return out;
    }
    const auto entry=stopGeography(int32_t(view.identifiedMm)*kStopMm,
        stopCoordinate(targetUm),view.identifiedDirection);
    const uint8_t next=navi_one::nextMarker(view.identifiedMm,view.identifiedDirection);
    const auto exit=stopGeography(int32_t(next)*kStopMm,
        stopCoordinate(targetUm),view.identifiedDirection);
    if(newOpening) {
      entry_=entry;
      entryPwm_=entry.applicable()?entry.pwm:cruise;
      exitPwm_=exit.applicable()?exit.pwm:nextCruise;
      startedUs_=selectedDecisionUs;durationUs_=previousLap.intervalUs;
      mode_=TileTimingMode::Ramp;
      out.entryNow=true;out.entryPwm=entryPwm_;
      // Final braking and departure have their existing executor. They do not
      // require an invented within-tile position or a timing-history ramp.
      if(entry.section!=StopSection::Final && !previousLap.valid &&
         (entry.applicable() || exit.applicable())) out.missingHistory=true;
    } else if(mode_==TileTimingMode::Geographic) {
      mode_=TileTimingMode::HoldUntilOpening;
    }
    out.mode=mode_;out.durationUs=durationUs_;out.entryPwm=entryPwm_;
    if(mode_==TileTimingMode::HoldUntilOpening) {
      out.geography.pwm=currentCommand;return out;
    }
    out.available=true;out.geography=entry_;
    if(nowUs>=startedUs_) out.elapsedUs=nowUs-startedUs_;
    if(entry_.section==StopSection::Final) return out;
    if(!entry_.applicable() && !exit.applicable()) {
      out.geography.pwm=cruise;return out;
    }
    if(!durationUs_) { out.missingHistory=true;return out; }
    const uint64_t elapsed=out.elapsedUs<durationUs_?out.elapsedUs:durationUs_;
    const int delta=int(exitPwm_)-int(entryPwm_);
    // Quotient/remainder avoids multiplying an unrestricted elapsed time.
    const unsigned magnitude=unsigned(delta<0?-delta:delta);
    const uint64_t steps=elapsed<=UINT64_MAX/255 ? elapsed*magnitude/durationUs_ :
      uint64_t((static_cast<long double>(elapsed)*magnitude)/durationUs_);
    out.geography.pwm=uint8_t(int(entryPwm_)+(delta<0?-int(steps):int(steps)));
    // A tile intersecting the existing approach footprint executes its entry
    // and exit instructions; it does not claim a fractional geographic fix.
    if(!out.geography.applicable()) out.geography.section=StopSection::Approach;
    return out;
  }
  TileTimingMode mode() const { return mode_; }
 private:
  TileTimingMode mode_=TileTimingMode::Geographic;
  uint32_t lastSelection_=0;
  uint64_t startedUs_=0,durationUs_=0;
  uint8_t entryPwm_=0,exitPwm_=0;
  StopGeography entry_;
};
}
