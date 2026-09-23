#pragma once
#include <stdint.h>
#include "../../common/IrMovementWire.h"

namespace ngr_nav {

// IR health answers one question only:
// Is the optical wheel instrument presently capable of supplying meaningful
// movement measurements?  It does NOT decide whether the locomotive is moving
// correctly, whether a Hall event is a landmark, or where NAVI is.
enum class IrHealth : uint8_t {
  Available = 0,
  NoSource,
  LinkStale,
  Priming,
  Reacquiring,
  InadequateContrast,
  Saturated,
  SampleGap,
  TransportInvalid
};

inline const char* irHealthName(IrHealth h) {
  switch (h) {
    case IrHealth::Available:           return "AVAILABLE";
    case IrHealth::NoSource:            return "NO_SOURCE";
    case IrHealth::LinkStale:           return "LINK_STALE";
    case IrHealth::Priming:             return "PRIMING";
    case IrHealth::Reacquiring:         return "REACQUIRING";
    case IrHealth::InadequateContrast:  return "INADEQUATE_CONTRAST";
    case IrHealth::Saturated:           return "SATURATED";
    case IrHealth::SampleGap:            return "SAMPLE_GAP";
    default:                             return "TRANSPORT_INVALID";
  }
}

// SIGNAL_STALE is intentionally AVAILABLE.  In the IR detector it means that
// no wheel pulse has completed for 2.5 s.  A healthy locomotive at a station
// dwell is expected to do exactly that.  Zero movement is a measurement, not
// an instrument failure.
inline IrHealth opticalHealth(uint8_t reason) {
  switch (reason) {
    case ir_movement::TRACKING:
    case ir_movement::SIGNAL_STALE:
      return IrHealth::Available;
    case ir_movement::PRIMING:
      return IrHealth::Priming;
    case ir_movement::REACQUIRING:
      return IrHealth::Reacquiring;
    case ir_movement::INADEQUATE_CONTRAST:
      return IrHealth::InadequateContrast;
    case ir_movement::SATURATION:
      return IrHealth::Saturated;
    case ir_movement::SAMPLE_GAP:
      return IrHealth::SampleGap;
    default:
      return IrHealth::TransportInvalid;
  }
}

inline bool irAvailable(IrHealth h) { return h == IrHealth::Available; }

} // namespace ngr_nav
