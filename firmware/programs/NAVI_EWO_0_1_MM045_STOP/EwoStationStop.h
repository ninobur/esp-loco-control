#pragma once

#include <math.h>
#include <stdint.h>

#include "../NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/RouteMap.h"

namespace navi_eyes {

// Type-5 IR movement reports this surveyed wheel pitch.  Distance is never
// inferred from PWM or Hall timing: it is completedPulses * pitchUm.
static constexpr double EWO_PKPH_MM_PER_SEC = 5.37325;
static constexpr uint32_t EWO_IR_PITCH_UM = 9652;
static constexpr uint16_t EWO_FINAL_TARGET_PULSES = 35;
static constexpr double EWO_STOP_DEADBAND_MM = double(EWO_IR_PITCH_UM) / 1000.0;
static constexpr double EWO_APPROACH_TARGET_PKPH = 25.0;
static constexpr double EWO_APPROACH_DEADBAND_PKPH = 1.0;
static constexpr double EWO_RATE_STEP = 0.05;
static constexpr double EWO_RATE_MIN = 0.80;
static constexpr double EWO_RATE_MAX = 1.20;
static constexpr uint64_t EWO_SPEED_WINDOW_MIN_US = 900000;

inline int16_t ewoStationOffsetToCentre(uint8_t mm, int8_t direction,
                                         uint8_t centre) {
  int32_t distance = direction > 0 ? int32_t(mm) - int32_t(centre)
                                   : int32_t(centre) - int32_t(mm);
  distance = navi_one::routeMod(distance);
  return distance > navi_one::ROUTE_N / 2
      ? int16_t(distance - navi_one::ROUTE_N)
      : int16_t(distance);
}

inline const char* brakeJudgmentName(uint8_t judgment) {
  switch (judgment) {
    case 1: return "LONG";
    case 2: return "ON_TARGET";
    case 3: return "SHORT";
    default: return "UNKNOWN";
  }
}

enum class EwoBrakePhase : uint8_t {
  Waiting = 0,
  Approach,
  StationHold,
  Final,
  Stopped,
};

inline const char* brakePhaseName(EwoBrakePhase phase) {
  switch (phase) {
    case EwoBrakePhase::Waiting:     return "WAITING";
    case EwoBrakePhase::Approach:    return "APPROACH";
    case EwoBrakePhase::StationHold: return "STATION_HOLD";
    case EwoBrakePhase::Final:       return "FINAL";
    case EwoBrakePhase::Stopped:     return "STOPPED";
  }
  return "?";
}

struct AdaptiveBrakeState {
  bool active = false;
  uint64_t referenceIrUm = 0;
  uint64_t previousIrUm = 0;
  uint64_t targetDistanceUm = 0;
  uint64_t previousCapturedUs = 0;
  uint64_t lastMovementCapturedUs = 0;
  uint64_t responseReferenceCapturedUs = 0;
  uint64_t previousPulses = 0;
  uint64_t responseReferencePulses = 0;
  int referencePwm = 0;
  int previousPwm = 0;
  double referenceSpeedPkph = 0.0;
  double previousSpeedPkph = 0.0;
  double speedDropPerPwm = 0.0;
  double nominalDownMs = 0.0;
  double rateFactor = 1.0;
  double projectedStopDistanceMm = 0.0;
  double targetStopDistanceMm = 0.0;
  uint8_t judgment = 0;  // 0 UNKNOWN, 1 LONG, 2 ON_TARGET, 3 SHORT
};

struct EwoStationStopDemand {
  bool available = false;
  bool irUnavailable = false;
  bool approachRamp = false;
  bool stationHold = false;
  bool finalRamp = false;
  bool stopReached = false;
  bool referenceRequired = false;
  bool newIrObservation = false;
  double targetPkph = 0.0;
  double measuredPkph = 0.0;
  double distanceMm = 0.0;
  double targetStopMm = 0.0;
  double nominalDownMs = 0.0;
  double appliedDownMs = 0.0;
  double rateFactor = 1.0;
  double speedDropPerPwm = 0.0;
  double projectedStopMm = 0.0;
  int pwmTarget = 0;
  uint16_t pwmUpMs = 0;
  uint16_t pwmDownMs = 0;
  uint64_t referenceIrUm = 0;
  uint64_t irPulses = 0;
  uint8_t judgment = 0;
  EwoBrakePhase phase = EwoBrakePhase::Waiting;
  const char* reason = "INACTIVE";
};

// This controller owns the station approach and final brake command only. The
// existing StationMachine still owns visit discovery, dwell timing, and
// departure bookkeeping. It does not supply PWM authority while this state is
// Approach, StationHold, or Final.
class EwoStationStopProfile {
 public:
  void reset() {
    active_ = false;
    centre_ = 0;
    direction_ = 0;
    phase_ = EwoBrakePhase::Waiting;
    stationPwm_ = 0;
    holdStepMs_ = 0;
    currentPitchUm_ = EWO_IR_PITCH_UM;
    approachReferenceUm_ = 0;
    approachReferencePulses_ = 0;
    approachDistanceUm_ = 0;
    startPkph_ = 0.0;
    finalReferenceUm_ = 0;
    finalReferencePulses_ = 0;
    finalTargetDistanceUm_ = 0;
    lastConfirmedMm_ = 0;
    lastConfirmedIrUm_ = 0;
    lastConfirmedPulses_ = 0;
    lastConfirmedPitchUm_ = EWO_IR_PITCH_UM;
    hallRevision_ = 0;
    processedHallRevision_ = 0;
    finalStartReported_ = false;
    stoppedReported_ = false;
    brake_ = AdaptiveBrakeState{};
  }

  bool activeFor(uint8_t centre, int8_t direction) const {
    return active_ && centre_ == centre && direction_ == direction;
  }

  void begin(uint8_t centre, int8_t direction, uint8_t currentMm,
             uint64_t currentIrUm, uint64_t currentPulses, int actualPwm,
             double measuredPkph, double nominalDownMs, uint8_t stationPwm,
             uint16_t holdStepMs, uint64_t capturedUs) {
    active_ = true;
    centre_ = centre;
    direction_ = direction;
    stationPwm_ = stationPwm;
    holdStepMs_ = holdStepMs;
    nominalDownMs_ = nominalDownMs > 1.0 ? nominalDownMs : 1.0;
    currentMm_ = currentMm;
    currentIrUm_ = currentIrUm;
    currentPulses_ = currentPulses;
    currentCapturedUs_ = capturedUs;
    activatePendingHall(actualPwm, measuredPkph, capturedUs);
  }

  // This is called only for an accepted TargetConfirmed event. A late or
  // missed-magnet event cannot establish a Hall anchor. A valid NAVI/IR
  // position may separately start the approach at -10, never the final brake.
  void noteAcceptedHall(uint8_t mm, uint64_t irUm, uint32_t pitchUm = 0) {
    lastConfirmedMm_ = mm;
    lastConfirmedIrUm_ = irUm;
    lastConfirmedPitchUm_ = pitchUm ? pitchUm : EWO_IR_PITCH_UM;
    lastConfirmedPulses_ = irUm / lastConfirmedPitchUm_;
    ++hallRevision_;
  }

  bool takeFinalStart() {
    if (phase_ != EwoBrakePhase::Final || finalStartReported_) return false;
    finalStartReported_ = true;
    return true;
  }

  bool takeStopped() {
    if (phase_ != EwoBrakePhase::Stopped || stoppedReported_) return false;
    stoppedReported_ = true;
    return true;
  }

  bool finalReferenceValid() const {
    return phase_ == EwoBrakePhase::Final || phase_ == EwoBrakePhase::Stopped;
  }
  uint64_t finalReferenceUm() const { return finalReferenceUm_; }
  EwoBrakePhase phase() const { return phase_; }
  const AdaptiveBrakeState& brakeState() const { return brake_; }

  EwoStationStopDemand demand(uint8_t currentMm, uint64_t currentIrUm,
                              uint64_t currentPulses, uint64_t capturedUs,
                              bool irDistanceValid, bool irSpeedValid,
                              double measuredMmS, int actualPwm) {
    currentMm_ = currentMm;
    currentIrUm_ = currentIrUm;
    currentPulses_ = currentPulses;
    currentCapturedUs_ = capturedUs;

    EwoStationStopDemand out;
    out.phase = phase_;
    out.measuredPkph = measuredMmS / EWO_PKPH_MM_PER_SEC;
    out.nominalDownMs = nominalDownMs_;
    out.referenceIrUm = phase_ == EwoBrakePhase::Final ||
                        phase_ == EwoBrakePhase::Stopped
                            ? finalReferenceUm_ : approachReferenceUm_;
    out.irPulses = currentPulses;
    activatePendingHall(actualPwm, out.measuredPkph, capturedUs);
    const int16_t offset = ewoStationOffsetToCentre(currentMm, direction_, centre_);
    if (phase_ == EwoBrakePhase::Waiting && offset == -10 &&
        irDistanceValid && irSpeedValid) {
      // Station discovery can be IR-positioned when the -10 magnet is missed.
      // Do not promote that observation into an accepted Hall reference: the
      // final brake still starts only at an accepted Station 0 Hall event.
      startApproach(currentIrUm, currentPulses, actualPwm, out.measuredPkph,
                    capturedUs);
    }
    out.phase = phase_;
    out.referenceIrUm = phase_ == EwoBrakePhase::Final ||
                        phase_ == EwoBrakePhase::Stopped
                            ? finalReferenceUm_ : approachReferenceUm_;

    if (phase_ == EwoBrakePhase::Waiting) {
      out.referenceRequired = true;
      out.reason = offset == -10 ? "STATION_APPROACH_IR_REQUIRED"
                   : offset < 0 ? "STATION_APPROACH_ENTRY_MISSED"
                                : "STATION_ZERO_HALL_REQUIRED";
      return out;
    }
    if (offset >= 0 && phase_ != EwoBrakePhase::Final &&
        phase_ != EwoBrakePhase::Stopped) {
      // An IR-positioned approach does not authorize final braking. Do not
      // keep station-speed hold through Station 0 when its Hall was missed.
      out.referenceRequired = true;
      out.reason = "STATION_ZERO_HALL_REQUIRED";
      return out;
    }
    if (phase_ == EwoBrakePhase::Stopped) {
      out.available = true;
      out.stopReached = true;
      out.pwmTarget = 0;
      out.reason = "STOPPED_PHYSICAL_IR";
      fillBrakeTelemetry(out);
      return out;
    }
    if (!irDistanceValid || !irSpeedValid) {
      // Required evidence is exposed, but the nominal monotonic PWM ramp is
      // allowed to continue. There is no Hall-speed, PWM, or other fallback.
      out.available = true;
      out.irUnavailable = true;
      out.pwmTarget = phase_ == EwoBrakePhase::Final ? 0 : stationPwm_;
      out.pwmUpMs = 0;
      out.pwmDownMs = static_cast<uint16_t>(appliedDownMs());
      out.reason = "IR_EVIDENCE_UNAVAILABLE_NOMINAL_RAMP";
      fillBrakeTelemetry(out);
      return out;
    }

    if (currentIrUm < out.referenceIrUm) {
      out.reason = "IR_DISTANCE_REVERSED";
      return out;
    }

    out.available = true;
    out.distanceMm = double(currentIrUm - out.referenceIrUm) / 1000.0;
    out.appliedDownMs = appliedDownMs();

    if (phase_ == EwoBrakePhase::Approach) {
      out.approachRamp = true;
      const double fraction = approachDistanceUm_ == 0
                                  ? 1.0
                                  : double(currentIrUm - approachReferenceUm_) /
                                        double(approachDistanceUm_);
      const double bounded = fraction < 0.0 ? 0.0 : (fraction > 1.0 ? 1.0 : fraction);
      out.targetPkph = startPkph_ +
                       (EWO_APPROACH_TARGET_PKPH - startPkph_) * bounded;
      out.pwmTarget = stationPwm_;
      out.pwmUpMs = 0;
      out.pwmDownMs = static_cast<uint16_t>(out.appliedDownMs);
      out.newIrObservation = processNewObservation(out, actualPwm);
      out.reason = "ADAPTIVE_APPROACH_RAMP";
      fillBrakeTelemetry(out);
      return out;
    }

    out.stationHold = true;
    if (phase_ == EwoBrakePhase::StationHold) {
      out.targetPkph = EWO_APPROACH_TARGET_PKPH;
      out.pwmTarget = stationPwm_;
      out.pwmUpMs = holdStepMs_;
      out.pwmDownMs = holdStepMs_;
      out.reason = "STATION_SPEED_HOLD";
      fillBrakeTelemetry(out);
      return out;
    }

    out.stationHold = false;
    out.finalRamp = true;
    out.targetStopMm = double(finalTargetDistanceUm_) / 1000.0;
    const double finalFraction = out.targetStopMm <= 0.0
                                     ? 1.0
                                     : out.distanceMm / out.targetStopMm;
    const double finalBounded = finalFraction < 0.0
                                    ? 0.0
                                    : (finalFraction > 1.0 ? 1.0 : finalFraction);
    out.targetPkph = 5.0 * (1.0 - finalBounded);
    out.pwmTarget = 0;
    out.pwmUpMs = 0;
    out.pwmDownMs = static_cast<uint16_t>(out.appliedDownMs);
    out.newIrObservation = processNewObservation(out, actualPwm);
    out.stopReached = physicalStop(irSpeedValid, measuredMmS, currentPulses,
                                   capturedUs);
    if (out.stopReached) {
      phase_ = EwoBrakePhase::Stopped;
      brake_.active = false;
      out.phase = phase_;
      out.reason = "STOPPED_PHYSICAL_IR";
    } else {
      out.reason = "ADAPTIVE_FINAL_RAMP";
    }
    fillBrakeTelemetry(out);
    return out;
  }

 private:
  void fillBrakeTelemetry(EwoStationStopDemand& out) const {
    out.rateFactor = brake_.rateFactor;
    out.speedDropPerPwm = brake_.speedDropPerPwm;
    out.projectedStopMm = brake_.projectedStopDistanceMm;
    out.judgment = brake_.judgment;
    if (out.appliedDownMs == 0.0 && brake_.active)
      out.appliedDownMs = appliedDownMs();
  }

  double appliedDownMs() const {
    const double rate = brake_.rateFactor < EWO_RATE_MIN
                            ? EWO_RATE_MIN
                            : (brake_.rateFactor > EWO_RATE_MAX
                                   ? EWO_RATE_MAX : brake_.rateFactor);
    return nominalDownMs_ / rate;
  }

  void initializeBrake(uint64_t referenceIrUm, uint64_t referencePulses,
                       int actualPwm, double measuredPkph,
                       uint64_t capturedUs, uint64_t targetDistanceUm) {
    brake_ = AdaptiveBrakeState{};
    brake_.active = true;
    brake_.referenceIrUm = referenceIrUm;
    brake_.previousIrUm = referenceIrUm;
    brake_.targetDistanceUm = targetDistanceUm;
    brake_.targetStopDistanceMm = double(targetDistanceUm) / 1000.0;
    brake_.previousCapturedUs = capturedUs;
    brake_.lastMovementCapturedUs = capturedUs;
    brake_.responseReferenceCapturedUs = capturedUs;
    brake_.previousPulses = referencePulses;
    brake_.responseReferencePulses = referencePulses;
    brake_.referencePwm = actualPwm;
    brake_.previousPwm = actualPwm;
    brake_.referenceSpeedPkph = measuredPkph;
    brake_.previousSpeedPkph = measuredPkph;
    brake_.nominalDownMs = nominalDownMs_;
    brake_.rateFactor = 1.0;
    brake_.judgment = 0;
  }

  void startApproach(uint64_t referenceIrUm, uint64_t referencePulses,
                     int actualPwm, double measuredPkph,
                     uint64_t capturedUs) {
    phase_ = EwoBrakePhase::Approach;
    approachReferenceUm_ = referenceIrUm;
    approachReferencePulses_ = referencePulses;
    approachDistanceUm_ = routeDistanceUm(-10, -5);
    startPkph_ = measuredPkph > EWO_APPROACH_TARGET_PKPH
                     ? measuredPkph : EWO_APPROACH_TARGET_PKPH;
    initializeBrake(approachReferenceUm_, approachReferencePulses_, actualPwm,
                    measuredPkph, capturedUs, approachDistanceUm_);
  }

  void activatePendingHall(int actualPwm, double measuredPkph,
                           uint64_t capturedUs) {
    if (!active_ || hallRevision_ == processedHallRevision_) return;
    processedHallRevision_ = hallRevision_;
    currentPitchUm_ = lastConfirmedPitchUm_ ? lastConfirmedPitchUm_ : EWO_IR_PITCH_UM;
    const int16_t offset = ewoStationOffsetToCentre(
        lastConfirmedMm_, direction_, centre_);
    if (offset == -10 && phase_ == EwoBrakePhase::Waiting) {
      startApproach(lastConfirmedIrUm_, lastConfirmedPulses_, actualPwm,
                    measuredPkph, capturedUs);
      return;
    }
    if (offset >= -5 && offset < 0 &&
        (phase_ == EwoBrakePhase::Waiting || phase_ == EwoBrakePhase::Approach)) {
      phase_ = EwoBrakePhase::StationHold;
      approachReferenceUm_ = lastConfirmedIrUm_;
      approachReferencePulses_ = lastConfirmedPulses_;
      brake_.active = false;
      return;
    }
    if (offset == 0 && phase_ != EwoBrakePhase::Final &&
        phase_ != EwoBrakePhase::Stopped) {
      phase_ = EwoBrakePhase::Final;
      finalReferenceUm_ = lastConfirmedIrUm_;
      finalReferencePulses_ = lastConfirmedPulses_;
      finalTargetDistanceUm_ = uint64_t(EWO_FINAL_TARGET_PULSES) * currentPitchUm_;
      initializeBrake(finalReferenceUm_, finalReferencePulses_, actualPwm,
                      measuredPkph, capturedUs, finalTargetDistanceUm_);
      return;
    }
  }

  uint64_t routeDistanceUm(int16_t fromOffset, int16_t toOffset) const {
    uint64_t total = 0;
    for (int16_t offset = fromOffset; offset < toOffset; ++offset) {
      const uint8_t marker = navi_one::routeMod(
          int32_t(centre_) + int32_t(offset) * direction_);
      total += uint64_t(navi_one::spanMm(marker, direction_)) * 1000;
    }
    return total;
  }

  void adjustRate(uint8_t judgment) {
    brake_.judgment = judgment;
    if (judgment == 1) brake_.rateFactor += EWO_RATE_STEP;
    if (judgment == 3) brake_.rateFactor -= EWO_RATE_STEP;
    if (brake_.rateFactor < EWO_RATE_MIN) brake_.rateFactor = EWO_RATE_MIN;
    if (brake_.rateFactor > EWO_RATE_MAX) brake_.rateFactor = EWO_RATE_MAX;
  }

  bool processNewObservation(EwoStationStopDemand& out, int actualPwm) {
    if (currentPulses_ <= brake_.previousPulses) return false;
    const bool hasResponseWindow =
        currentCapturedUs_ >= brake_.responseReferenceCapturedUs &&
        currentCapturedUs_ - brake_.responseReferenceCapturedUs >=
            EWO_SPEED_WINDOW_MIN_US;
    if (hasResponseWindow) {
      const int pwmRemoved = brake_.referencePwm - actualPwm;
      const double speedDrop = brake_.referenceSpeedPkph - out.measuredPkph;
      if (pwmRemoved > 0 && speedDrop > 0.0) {
        const double observed = speedDrop / double(pwmRemoved);
        brake_.speedDropPerPwm = brake_.speedDropPerPwm == 0.0
                                     ? observed
                                     : 0.7 * observed +
                                           0.3 * brake_.speedDropPerPwm;
      }
      brake_.referencePwm = actualPwm;
      brake_.referenceSpeedPkph = out.measuredPkph;
      brake_.responseReferenceCapturedUs = currentCapturedUs_;
      brake_.responseReferencePulses = currentPulses_;
    }

    brake_.lastMovementCapturedUs = currentCapturedUs_;
    brake_.previousCapturedUs = currentCapturedUs_;
    brake_.previousIrUm = currentIrUm_;
    brake_.previousPulses = currentPulses_;
    brake_.previousPwm = actualPwm;
    brake_.previousSpeedPkph = out.measuredPkph;

    if (phase_ == EwoBrakePhase::Approach) {
      const double expected = out.targetPkph;
      if (out.measuredPkph > expected + EWO_APPROACH_DEADBAND_PKPH)
        adjustRate(1);
      else if (out.measuredPkph < expected - EWO_APPROACH_DEADBAND_PKPH)
        adjustRate(3);
      else
        adjustRate(2);
      return true;
    }

    if (phase_ == EwoBrakePhase::Final && brake_.speedDropPerPwm > 0.0) {
      const double measuredMmS = out.measuredPkph * EWO_PKPH_MM_PER_SEC;
      const double pwmToPhysicalStop = out.measuredPkph / brake_.speedDropPerPwm;
      const double secondsToStop = pwmToPhysicalStop * appliedDownMs() / 1000.0;
      const double remainingMm = measuredMmS * 0.5 * secondsToStop;
      brake_.projectedStopDistanceMm = out.distanceMm + remainingMm;
      if (brake_.projectedStopDistanceMm >
          brake_.targetStopDistanceMm + EWO_STOP_DEADBAND_MM)
        adjustRate(1);
      else if (brake_.projectedStopDistanceMm <
               brake_.targetStopDistanceMm - EWO_STOP_DEADBAND_MM)
        adjustRate(3);
      else
        adjustRate(2);
    }
    return true;
  }

  bool physicalStop(bool irSpeedValid, double measuredMmS,
                    uint64_t currentPulses, uint64_t capturedUs) const {
    if (!irSpeedValid || measuredMmS > 0.0 ||
        currentPulses != brake_.previousPulses ||
        capturedUs < brake_.lastMovementCapturedUs)
      return false;
    return capturedUs - brake_.lastMovementCapturedUs >=
           EWO_SPEED_WINDOW_MIN_US;
  }

  bool active_ = false;
  uint8_t centre_ = 0;
  int8_t direction_ = 0;
  uint8_t currentMm_ = 0;
  uint64_t currentIrUm_ = 0;
  uint64_t currentPulses_ = 0;
  uint64_t currentCapturedUs_ = 0;
  EwoBrakePhase phase_ = EwoBrakePhase::Waiting;
  uint8_t stationPwm_ = 0;
  uint16_t holdStepMs_ = 0;
  double nominalDownMs_ = 400.0;
  uint32_t currentPitchUm_ = EWO_IR_PITCH_UM;
  uint64_t approachReferenceUm_ = 0;
  uint64_t approachReferencePulses_ = 0;
  uint64_t approachDistanceUm_ = 0;
  double startPkph_ = EWO_APPROACH_TARGET_PKPH;
  uint64_t finalReferenceUm_ = 0;
  uint64_t finalReferencePulses_ = 0;
  uint64_t finalTargetDistanceUm_ = 0;
  uint8_t lastConfirmedMm_ = 0;
  uint64_t lastConfirmedIrUm_ = 0;
  uint64_t lastConfirmedPulses_ = 0;
  uint32_t lastConfirmedPitchUm_ = EWO_IR_PITCH_UM;
  uint64_t hallRevision_ = 0;
  uint64_t processedHallRevision_ = 0;
  bool finalStartReported_ = false;
  bool stoppedReported_ = false;
  AdaptiveBrakeState brake_{};
};

}  // namespace navi_eyes
