#pragma once
#include <stdint.h>
#include "../../../common/IrMovementWire.h"

namespace ngr_nav {
enum class IrHealthFault : uint8_t {
  None, NoSource, LinkStale, PacketInvalid, OrderFault, CalibrationFault,
  InadequateContrast, Saturation, SampleGap, PulseFault
};
enum class IrReadiness : uint8_t { Ready, Priming, Reacquiring, Unavailable };

inline const char* irHealthName(IrHealthFault value) {
  switch (value) {
    case IrHealthFault::None: return "HEALTHY";
    case IrHealthFault::NoSource: return "NO_SOURCE";
    case IrHealthFault::LinkStale: return "LINK_STALE";
    case IrHealthFault::PacketInvalid: return "PACKET_INVALID";
    case IrHealthFault::OrderFault: return "ORDER_FAULT";
    case IrHealthFault::CalibrationFault: return "CALIBRATION_FAULT";
    case IrHealthFault::InadequateContrast: return "INADEQUATE_CONTRAST";
    case IrHealthFault::Saturation: return "SATURATION";
    case IrHealthFault::SampleGap: return "SAMPLE_GAP";
    default: return "PULSE_FAULT";
  }
}
inline const char* irReadinessName(IrReadiness value) {
  switch (value) {
    case IrReadiness::Ready: return "READY";
    case IrReadiness::Priming: return "PRIMING";
    case IrReadiness::Reacquiring: return "REACQUIRING";
    default: return "UNAVAILABLE";
  }
}
struct IrInstrumentState {
  bool healthy = false;
  IrHealthFault fault = IrHealthFault::NoSource;
  IrReadiness readiness = IrReadiness::Unavailable;
  uint8_t detectorReason = ir_movement::PRIMING;
  bool measurementReady() const { return healthy && readiness == IrReadiness::Ready; }
};

// Classifies a decoded snapshot AFTER transport has verified source, size, CRC
// and ordering. Raw reason stays uint8_t so unknown wire values remain visible.
inline IrInstrumentState classifyIrInstrument(const ir_movement::WireSnapshot& w) {
  IrInstrumentState s;
  s.detectorReason = w.opticalReason;
  // Zero calibration ID is the deployed TX's nominal, unvalidated scale.
  // It does not determine optical health or measurement continuity.
  if (!w.bootId || !w.pitchUm) {
    s.fault = IrHealthFault::CalibrationFault;
    return s;
  }
  switch (w.opticalReason) {
    case ir_movement::INADEQUATE_CONTRAST:
      s.fault = IrHealthFault::InadequateContrast; return s;
    case ir_movement::SATURATION:
      s.fault = IrHealthFault::Saturation; return s;
    case ir_movement::SAMPLE_GAP:
      s.fault = IrHealthFault::SampleGap; return s;
    case ir_movement::PRIMING:
      s.readiness = IrReadiness::Priming; break;
    case ir_movement::REACQUIRING:
      s.readiness = IrReadiness::Reacquiring; break;
    case ir_movement::SIGNAL_STALE:
    case ir_movement::TRACKING:
      s.readiness = IrReadiness::Ready; break;
    default:
      s.fault = IrHealthFault::PacketInvalid; return s;
  }
  s.healthy = true;
  s.fault = IrHealthFault::None;
  return s;
}
} // namespace ngr_nav
