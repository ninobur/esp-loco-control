#pragma once
#include "NaviStopOverlay.h"

namespace navi_eyes {
// RouteMap::landmarkAt and the established station definitions agree on these
// centers. Operator, 2026-10-15: stop at the centers; delete the MM045 trial.
struct StationTarget { const char* name; uint8_t mm; };
static constexpr StationTarget kStationTargets[] = {
  {"PATIO",15}, {"GRILLERS",63}, {"ARCHES",108}, {"BAMBOO",157}
};
class NaviStations {
 public:
  void reset() { active_=0xff; direction_=0; }
  uint8_t index() const { return active_; }
  const char* name() const { return active_<4?kStationTargets[active_].name:"NONE"; }
  int32_t targetUm() const { return active_<4?stopMarkerUm(kStationTargets[active_].mm):0; }
  bool update(const NaviIntegratedCore& navi,uint64_t nowUs,StopExecution execution) {
    if(!navi.declared()) return false;
    const int8_t direction=navi.direction();
    const bool reversed=direction!=direction_;
    if(!reversed && active_<4) {
      if(execution==StopExecution::Braking || execution==StopExecution::Dwell) return false;
      if(execution==StopExecution::Released) {
        const auto view=stopView(navi,targetUm(),nowUs);
        const auto geo=view.available?view.geography:stopGeography(
          int32_t(navi.mm())*kStopMm,stopCoordinate(targetUm()),direction);
        if(geo.applicable()) return false;
      }
    }
    int64_t offset=0;
    const int32_t position=navi.physicalOffsetUm(nowUs,offset)?
      stopPositionUm(navi.mm(),direction,offset):stopMarkerUm(navi.mm());
    int32_t nearest=stopCircuitUm(); uint8_t selected=0;
    for(uint8_t i=0;i<4;++i) {
      const int32_t distance=stopRouteMod(int64_t(direction)*
        (stopMarkerUm(kStationTargets[i].mm)-position),stopCircuitUm());
      if(distance<nearest) { nearest=distance; selected=i; }
    }
    const bool changed=reversed || selected!=active_;
    active_=selected;direction_=direction;return changed;
  }
 private:
  uint8_t active_=0xff;
  int8_t direction_=0;
};
}
