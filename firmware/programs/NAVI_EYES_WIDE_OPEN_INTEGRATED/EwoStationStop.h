#pragma once

#include <math.h>
#include <stdint.h>

#include "../NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/RouteMap.h"

namespace navi_eyes {

// This is the existing NAVI house-unit conversion used by the EWO IR speed
// telemetry.  The station controller consumes IR speed; it never derives speed
// from Hall timing or from a PWM lookup table.
static constexpr double EWO_PKPH_MM_PER_SEC = 5.37325;
static constexpr double EWO_STATION_FINAL_TRAVEL_MM = 150.0;

static constexpr int8_t EWO_STATION_PROFILE_FIRST_OFFSET = -5;
static constexpr int8_t EWO_STATION_PROFILE_FINAL_OFFSET = 2;
static constexpr uint8_t EWO_STATION_PROFILE_COUNT = 8;
static constexpr double EWO_STATION_PROFILE_PKPH[EWO_STATION_PROFILE_COUNT] = {
  40.0, 35.0, 30.0, 25.0, 20.0, 15.0, 10.0, 5.0
};

inline int16_t ewoStationOffsetToCentre(uint8_t mm, int8_t direction,
                                         uint8_t centre) {
  int32_t distance = direction > 0 ? int32_t(mm) - int32_t(centre)
                                   : int32_t(centre) - int32_t(mm);
  distance = navi_one::routeMod(distance);
  return distance > navi_one::ROUTE_N / 2
      ? int16_t(distance - navi_one::ROUTE_N)
      : int16_t(distance);
}

inline double ewoStationTargetAtOffset(int16_t offset) {
  if (offset <= EWO_STATION_PROFILE_FIRST_OFFSET)
    return EWO_STATION_PROFILE_PKPH[0];
  if (offset >= EWO_STATION_PROFILE_FINAL_OFFSET)
    return EWO_STATION_PROFILE_PKPH[EWO_STATION_PROFILE_COUNT - 1];
  return EWO_STATION_PROFILE_PKPH[
      static_cast<uint8_t>(offset - EWO_STATION_PROFILE_FIRST_OFFSET)];
}

// The target changes continuously over the current marker interval. The
// accepted Hall marker selects the two endpoint targets; IR distance supplies
// the interpolation coordinate.
inline double ewoStationTargetBetween(int16_t offset,
                                      uint64_t distanceFromMarkerUm,
                                      uint16_t intervalMm) {
  const double start = ewoStationTargetAtOffset(offset);
  const double finish = ewoStationTargetAtOffset(offset + 1);
  if (intervalMm == 0) return finish;
  double fraction = double(distanceFromMarkerUm) /
                    (double(intervalMm) * 1000.0);
  if (fraction < 0.0) fraction = 0.0;
  if (fraction > 1.0) fraction = 1.0;
  return start + (finish - start) * fraction;
}

inline double ewoStationFinalTargetPkph(double travelMm) {
  if (travelMm <= 0.0) return EWO_STATION_PROFILE_PKPH[
      EWO_STATION_PROFILE_COUNT - 1];
  if (travelMm >= EWO_STATION_FINAL_TRAVEL_MM) return 0.0;
  return EWO_STATION_PROFILE_PKPH[EWO_STATION_PROFILE_COUNT - 1] *
         (EWO_STATION_FINAL_TRAVEL_MM - travelMm) /
         EWO_STATION_FINAL_TRAVEL_MM;
}

// PWM remains the actuator. This is a measured-speed correction, not a
// station PWM profile: every command depends on the current measured speed
// error and the actuator's current value. requestPwm() applies the existing
// generic actuator ramp after this calculation.
inline int ewoStationPwmTarget(int actualPwm, double targetPkph,
                               double measuredPkph) {
  if (targetPkph <= 0.0) return 0;
  const double correction = (targetPkph - measuredPkph) * 2.0;
  int target = int(lround(double(actualPwm) + correction));
  if (target < 0) target = 0;
  if (target > 255) target = 255;
  return target;
}

struct EwoStationStopDemand {
  bool available = false;
  bool finalRamp = false;
  bool stopReached = false;
  bool referenceRequired = false;
  double targetPkph = 0.0;
  double measuredPkph = 0.0;
  double travelMm = 0.0;
  int pwmTarget = 0;
  const char* reason = "INACTIVE";
};

// Scoped controller state for one active station visit. It records only the
// accepted Hall coordinates needed to interpolate the current profile and to
// establish the +2 final-stop reference. It is not station-entry authority:
// the existing station machine still decides when a station visit exists.
class EwoStationStopProfile {
 public:
  void reset() {
    active_ = false;
    centre_ = 0;
    direction_ = 0;
    anchorMm_ = 0;
    anchorIrUm_ = 0;
    anchorValid_ = false;
    finalReferenceValid_ = false;
    finalReferenceUm_ = 0;
    finalStarted_ = false;
    lastConfirmedMm_ = 0;
    lastConfirmedIrUm_ = 0;
    lastConfirmedValid_ = false;
  }

  bool activeFor(uint8_t centre, int8_t direction) const {
    return active_ && centre_ == centre && direction_ == direction;
  }

  void begin(uint8_t centre, int8_t direction, uint8_t currentMm,
             uint64_t currentIrUm) {
    active_ = true;
    centre_ = centre;
    direction_ = direction;
    anchorMm_ = currentMm;
    anchorIrUm_ = currentIrUm;
    anchorValid_ = true;
    finalReferenceValid_ = false;
    finalReferenceUm_ = 0;
    finalStarted_ = false;
    // A late AUTO entry may begin at a marker already confirmed before the
    // station machine saw it. Preserve that accepted Hall coordinate.
    if (lastConfirmedValid_ && lastConfirmedMm_ == currentMm) {
      anchorIrUm_ = lastConfirmedIrUm_;
    }
    if (lastConfirmedValid_ &&
        ewoStationOffsetToCentre(lastConfirmedMm_, direction_, centre_) ==
            EWO_STATION_PROFILE_FINAL_OFFSET) {
      finalReferenceValid_ = true;
      finalReferenceUm_ = lastConfirmedIrUm_;
    }
  }

  // Called only for NAVI's accepted TargetConfirmed event, never for a
  // missed-magnet progression. +2 is therefore a physical Hall reference.
  void noteAcceptedHall(uint8_t mm, uint64_t irUm) {
    lastConfirmedMm_ = mm;
    lastConfirmedIrUm_ = irUm;
    lastConfirmedValid_ = true;
    if (!active_) return;
    if (ewoStationOffsetToCentre(mm, direction_, centre_) ==
        EWO_STATION_PROFILE_FINAL_OFFSET) {
      finalReferenceValid_ = true;
      finalReferenceUm_ = irUm;
    }
    anchorMm_ = mm;
    anchorIrUm_ = irUm;
    anchorValid_ = true;
  }

  bool takeFinalStart() {
    if (!finalReferenceValid_ || finalStarted_) return false;
    finalStarted_ = true;
    return true;
  }

  bool finalReferenceValid() const { return finalReferenceValid_; }
  uint64_t finalReferenceUm() const { return finalReferenceUm_; }

  EwoStationStopDemand demand(uint8_t currentMm, uint64_t currentIrUm,
                              bool irDistanceValid, bool irSpeedValid,
                              double measuredMmS, int actualPwm) const {
    EwoStationStopDemand out;
    if (!active_) return out;
    out.measuredPkph = measuredMmS / EWO_PKPH_MM_PER_SEC;
    if (!irDistanceValid || !irSpeedValid) {
      out.reason = "IR_REQUIRED_UNAVAILABLE";
      return out;
    }

    const int16_t offset = ewoStationOffsetToCentre(currentMm, direction_, centre_);
    if (finalReferenceValid_) {
      if (currentIrUm < finalReferenceUm_) {
        out.reason = "IR_DISTANCE_REVERSED";
        return out;
      }
      out.available = true;
      out.finalRamp = true;
      out.travelMm = double(currentIrUm - finalReferenceUm_) / 1000.0;
      out.targetPkph = ewoStationFinalTargetPkph(out.travelMm);
      out.pwmTarget = ewoStationPwmTarget(actualPwm, out.targetPkph,
                                          out.measuredPkph);
      out.stopReached = out.travelMm >= EWO_STATION_FINAL_TRAVEL_MM &&
                        measuredMmS <= 0.0 && actualPwm == 0;
      out.reason = out.stopReached ? "STOPPED" : "FINAL_IR_DISTANCE";
      return out;
    }

    if (offset >= EWO_STATION_PROFILE_FINAL_OFFSET) {
      out.referenceRequired = true;
      out.reason = "STATION_PLUS_TWO_HALL_REQUIRED";
      return out;
    }
    if (offset < EWO_STATION_PROFILE_FIRST_OFFSET) {
      out.available = true;
      out.targetPkph = ewoStationTargetAtOffset(offset);
    } else {
      if (!anchorValid_ || anchorMm_ != currentMm || currentIrUm < anchorIrUm_) {
        out.reason = "IR_PROFILE_REFERENCE_UNAVAILABLE";
        return out;
      }
      out.available = true;
      out.targetPkph = ewoStationTargetBetween(
          offset, currentIrUm - anchorIrUm_,
          navi_one::spanMm(currentMm, direction_));
    }
    out.pwmTarget = ewoStationPwmTarget(actualPwm, out.targetPkph,
                                        out.measuredPkph);
    out.reason = "MEASURED_SPEED_PROFILE";
    return out;
  }

 private:
  bool active_ = false;
  uint8_t centre_ = 0;
  int8_t direction_ = 0;
  uint8_t anchorMm_ = 0;
  uint64_t anchorIrUm_ = 0;
  bool anchorValid_ = false;
  bool finalReferenceValid_ = false;
  uint64_t finalReferenceUm_ = 0;
  bool finalStarted_ = false;
  uint8_t lastConfirmedMm_ = 0;
  uint64_t lastConfirmedIrUm_ = 0;
  bool lastConfirmedValid_ = false;
};

}  // namespace navi_eyes
