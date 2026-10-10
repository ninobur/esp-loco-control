#pragma once

#include <stdint.h>
#include "NaviIntegratedCore.h"

namespace navi_eyes {

// A fractional MM is a fraction of the surveyed interval, not a fixed number
// of millimetres. Geometry is read-only: it never advances NAVI or its sensors.
static constexpr int32_t kStopMm = 1000000;
static constexpr int32_t kStopCircuit = int32_t(navi_one::ROUTE_N) * kStopMm;
static constexpr uint32_t kStopDwellMs = 5000;

inline int32_t stopRouteMod(int64_t value, int32_t length) {
  value %= length;
  return int32_t(value < 0 ? value + length : value);
}

inline int32_t stopMarkerUm(uint8_t mm) {
  int32_t distance = 0;
  for (uint8_t i = 0; i < mm; ++i)
    distance += int32_t(navi_one::spanMm(i, 1)) * 1000;
  return distance;
}

inline int32_t stopCircuitUm() { return stopMarkerUm(navi_one::ROUTE_N); }

inline int32_t stopCoordinate(int32_t routeUm) {
  routeUm = stopRouteMod(routeUm, stopCircuitUm());
  for (uint8_t mm = 0; mm < navi_one::ROUTE_N; ++mm) {
    const int32_t span = int32_t(navi_one::spanMm(mm, 1)) * 1000;
    if (routeUm < span)
      return int32_t(mm) * kStopMm + int32_t(int64_t(routeUm) * kStopMm / span);
    routeUm -= span;
  }
  return 0;  // All normalized coordinates return inside the loop.
}

inline int32_t stopPositionUm(uint8_t mm, int8_t direction, int64_t offsetUm) {
  return stopRouteMod(int64_t(stopMarkerUm(mm)) + direction * offsetUm,
                      stopCircuitUm());
}

enum class StopSection : uint8_t { Outside, Approach, Fifty, Penultimate, Final };

struct StopGeography {
  StopSection section = StopSection::Outside;
  uint8_t pwm = 90;
  int32_t remainingMm = 0;
  bool applicable() const { return section != StopSection::Outside; }
};

// Pure lookup. Any point in the eleven-MM footprint is a valid entry point.
// The final geographic 30/20/0 values are calibration aims; the operator
// authorized a continuous time-based final ramp, independent of more pulses.
inline StopGeography stopGeography(int32_t position, int32_t target,
                                   int8_t direction) {
  StopGeography out;
  if (direction != 1 && direction != -1) return out;
  const int32_t d = stopRouteMod(int64_t(direction) * (target - position), kStopCircuit);
  out.remainingMm = d;
  if (d > 11 * kStopMm) return out;
  if (d > 7 * kStopMm) {
    out.section = StopSection::Approach;
    out.pwm = uint8_t(50 + (10 * (d - 7 * kStopMm) + kStopMm - 1) / kStopMm);
  } else if (d > 2 * kStopMm) {
    out.section = StopSection::Fifty;
    out.pwm = 50;
  } else if (d > kStopMm) {
    out.section = StopSection::Penultimate;
    out.pwm = uint8_t(30 + (20 * (d - kStopMm) + kStopMm - 1) / kStopMm);
  } else {
    out.section = StopSection::Final;
    out.pwm = d > kStopMm / 2
        ? uint8_t(20 + (20 * (d - kStopMm / 2) + kStopMm - 1) / kStopMm)
        : uint8_t((40 * d + kStopMm - 1) / kStopMm);
  }
  return out;
}

enum class StopExecution : uint8_t { Geographic, Braking, Dwell, Released };

struct StopView {
  StopGeography geography;
  bool available = false, fine = false;
  int32_t positionUm = 0, errorUm = 0;
};

inline StopView stopView(const NaviIntegratedCore& navi, int32_t targetUm,
                         uint64_t nowUs) {
  StopView out;
  if (!navi.declared() || !navi.positionReliable() ||
      navi.pwmZeroMovementRequiresDeclaration()) return out;
  int64_t offset = 0;
  if (navi.physicalOffsetUm(nowUs, offset)) {
    out.available = out.fine = true;
    out.positionUm = stopPositionUm(navi.mm(), navi.direction(), offset);
    out.geography = stopGeography(stopCoordinate(out.positionUm),
                                  stopCoordinate(targetUm), navi.direction());
    const int32_t length = stopCircuitUm();
    out.errorUm = stopRouteMod(int64_t(navi.direction()) *
        (out.positionUm - targetUm) + length / 2, length) - length / 2;
  } else if (!navi.physicalPositionAnchored()) {
    // New start/reposition only: the operator declaration is location truth.
    // IR supplies movement/velocity until the first MM contact, not an exact
    // physical offset inferred from the time or place of declaration.
    // Once anchored, IR gaps preserve the last command rather than resetting
    // the fine instruction to an interval boundary. Entering this overlay or
    // resuming STOP/GO never resets an established IR location reference.
    out.available = true;
    out.geography = stopGeography(int32_t(navi.mm()) * kStopMm,
                                  stopCoordinate(targetUm), navi.direction());
  }
  return out;
}

struct StopCommand {
  uint8_t pwm = 90;
  bool overlay = false;
  bool releasedNow = false;
  StopExecution execution = StopExecution::Geographic;
};

// Only execution progress is retained: unfinished final braking, zero-PWM
// dwell, and consumption of this visit until departure leaves its footprint.
// There is no admission history, speed controller, or position integration.
class NaviStopOverlay {
 public:
  void reset() {
    execution_ = StopExecution::Geographic;
    zeroTiming_ = wasEnabled_ = geographicOverlay_ = false;
    direction_ = 0;
  }

  StopCommand command(const StopGeography& geo, bool geographyAvailable,
                      int8_t direction, uint8_t cruise, uint8_t appliedPwm,
                      uint8_t currentCommand, uint32_t nowMs, bool enabled) {
    StopCommand out;
    out.pwm = cruise;
    if (direction != direction_) {
      reset();
      direction_ = direction;
    }
    // Actual nonzero PWM invalidates a continuous zero-PWM dwell even while
    // manual authority has suspended AUTO. No IR-rest test is introduced.
    if (appliedPwm != 0) zeroTiming_ = false;
    if (appliedPwm == 0 && (execution_ == StopExecution::Braking ||
                           execution_ == StopExecution::Dwell) && !zeroTiming_) {
      zeroTiming_ = true;
      zeroSinceMs_ = nowMs;
      execution_ = StopExecution::Dwell;
    }
    if (execution_ == StopExecution::Released && geographyAvailable && !geo.applicable()) {
      execution_ = StopExecution::Geographic;
      zeroTiming_ = false;
    }
    if (!enabled) {
      wasEnabled_ = false;
      out.pwm = currentCommand;
      out.execution = execution_;
      return out;
    }
    // A manual interruption may have moved the locomotive elsewhere. On GO,
    // use its current instruction rather than imposing an old final approach.
    if (!wasEnabled_ && geographyAvailable && geo.section != StopSection::Final &&
        execution_ != StopExecution::Released) {
      execution_ = StopExecution::Geographic;
      zeroTiming_ = false;
    }
    wasEnabled_ = true;
    if (execution_ == StopExecution::Released) {
      out.execution = execution_;
      return out;
    }
    if (execution_ == StopExecution::Geographic && geographyAvailable &&
        geo.section == StopSection::Final)
      execution_ = StopExecution::Braking;
    if (execution_ == StopExecution::Braking || execution_ == StopExecution::Dwell) {
      out.overlay = true;
      out.pwm = 0;
      if (appliedPwm == 0) {
        if (!zeroTiming_) { zeroTiming_ = true; zeroSinceMs_ = nowMs; }
        execution_ = StopExecution::Dwell;
        if (uint32_t(nowMs - zeroSinceMs_) >= kStopDwellMs) {
          execution_ = StopExecution::Released;
          out.overlay = false;
          out.releasedNow = true;
          out.pwm = cruise;
        }
      } else {
        execution_ = StopExecution::Braking;
      }
    } else if (geographyAvailable) {
      out.overlay = geo.applicable();
      out.pwm = out.overlay ? geo.pwm : cruise;
    } else {
      out.pwm = currentCommand;  // Preserve the established instruction, not invented travel.
      out.overlay = geographicOverlay_;
    }
    geographicOverlay_ = out.overlay;
    out.execution = execution_;
    return out;
  }

 private:
  StopExecution execution_ = StopExecution::Geographic;
  bool zeroTiming_ = false, wasEnabled_ = false, geographicOverlay_ = false;
  int8_t direction_ = 0;
  uint32_t zeroSinceMs_ = 0;
};

}  // namespace navi_eyes
