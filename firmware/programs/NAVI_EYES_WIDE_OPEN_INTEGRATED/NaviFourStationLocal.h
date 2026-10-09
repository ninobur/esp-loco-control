#pragma once

// Pass 2 local operating program.  This is deliberately pure: it derives the
// current requirement from geographic position and turns fresh IR observations
// into bounded PWM requests.  It does not observe Hall, advance navigation, or
// write PWM.
#include <algorithm>
#include <cmath>
#include <stdint.h>
#include "../NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/RouteMap.h"

namespace navi_eyes {

enum class LocalInstruction : uint8_t { Cruise, Approach, StationSpeed, FinalStop, StopTile, Depart };
struct LocalRequirement { LocalInstruction instruction = LocalInstruction::Cruise; uint8_t station = 0xff; int8_t offset = 99; };

class FourStationGeography {
 public:
  // The four base STOP centres.  Boundaries are expressed in direction of travel.
  static constexpr uint8_t kCentres[4] = {15, 63, 108, 157};
  static int8_t offset(uint8_t mm, int8_t direction, uint8_t centre) {
    int16_t d = direction > 0 ? navi_one::routeMod(int(mm) - int(centre))
                              : navi_one::routeMod(int(centre) - int(mm));
    return d > navi_one::ROUTE_N / 2 ? int8_t(d - navi_one::ROUTE_N) : int8_t(d);
  }
  static LocalRequirement evaluate(uint8_t mm, int8_t direction) {
    LocalRequirement r;
    if (direction != 1 && direction != -1) return r;
    for (uint8_t i = 0; i < 4; ++i) {
      const int8_t o = offset(mm, direction, kCentres[i]);
      if (o < -10 || o > 2) continue;
      r.station = i; r.offset = o;
      if (o <= -6) r.instruction = LocalInstruction::Approach;
      else if (o < 0) r.instruction = LocalInstruction::StationSpeed;
      else if (o <= 1) r.instruction = LocalInstruction::FinalStop;
      else r.instruction = LocalInstruction::StopTile;
      return r;
    }
    return r;
  }
};

struct LocalControlInput {
  uint64_t nowUs = 0, irUm = 0; uint32_t irSequence = 0;
  uint8_t mm = 0; int8_t direction = 0; int actualPwm = 0;
  bool irSpeedValid = false; double speedPkph = 0;
};
struct LocalControlOutput { int pwmTarget = -1; uint16_t upMs = 150, downMs = 150; LocalRequirement requirement; bool decelerating = false; bool dwelling = false; };

class FourStationLocalController {
 public:
  static constexpr double kCruisePkph = 45, kStationPkph = 20;
  void reset() { *this = FourStationLocalController(); }
  LocalControlOutput tick(const LocalControlInput& in, int cruisePwm) {
    LocalControlOutput out; out.requirement = FourStationGeography::evaluate(in.mm, in.direction);
    const bool fresh = in.irSequence != 0 && in.irSequence != lastIrSequence_;
    if (fresh) {
      lastIrSequence_ = in.irSequence;
      // A fresh Type-5 report is not necessarily a movement pulse.  Dwell
      // completion depends on measured wheel displacement, not packet cadence.
      if (in.irUm != lastMotionUm_) { lastMotionUm_ = in.irUm; lastPulseUs_ = in.nowUs; }
    }
    if (in.direction != direction_) { direction_ = in.direction; clearExecution(); }
    if (out.requirement.station != activeStation_) { activeStation_ = out.requirement.station; clearExecution(); }
    if (departed_ && out.requirement.instruction == LocalInstruction::StopTile) out.requirement.instruction = LocalInstruction::Depart;
    if (out.requirement.instruction == LocalInstruction::StopTile && !departed_) {
      out.pwmTarget = 0; out.downMs = decelStepMs_; out.decelerating = true;
      if (in.actualPwm == 0 && lastPulseUs_ && in.nowUs - lastPulseUs_ >= 50000) {
        if (!dwellSinceUs_) dwellSinceUs_ = in.nowUs;
        if (in.nowUs - dwellSinceUs_ >= 5000000) departed_ = true;
      }
      out.dwelling = dwellSinceUs_ && !departed_;
      if (departed_) { out.requirement.instruction = LocalInstruction::Depart; out.pwmTarget = cruisePwm; out.upMs = 150; out.decelerating = false; }
      return out;
    }
    const double target = (out.requirement.instruction == LocalInstruction::StationSpeed) ? kStationPkph : kCruisePkph;
    if (out.requirement.instruction == LocalInstruction::Approach || out.requirement.instruction == LocalInstruction::FinalStop) {
      // Start each glide from current measured state/PWM.  The target trajectory is
      // v^2 = vend^2 + (vstart^2-vend^2) * remaining/total; three outliers move
      // only the existing downward ramp pace, never PWM upward.
      const double terminal = out.requirement.instruction == LocalInstruction::Approach ? kStationPkph : 0.0;
      beginGlideIfNeeded(out.requirement, in, terminal);
      // One count below the physical actuator state lets serviceRamp() pace the
      // established monotonic reduction.  A glide never asks it to increase PWM.
      out.decelerating = true; out.pwmTarget = std::max(0, in.actualPwm - 1); out.downMs = decelStepMs_;
      if (fresh && in.irSpeedValid && glideDistanceUm_) {
        const uint64_t travelled = in.irUm >= glideOriginUm_ ? in.irUm - glideOriginUm_ : 0;
        const uint64_t remaining = travelled >= glideDistanceUm_ ? 0 : glideDistanceUm_ - travelled;
        if (out.requirement.instruction == LocalInstruction::FinalStop && remaining == 0) out.pwmTarget = 0;
        const double expected = sqrt(std::max(0.0, glideTerminal_ * glideTerminal_ +
            (glideStartSpeed_ * glideStartSpeed_ - glideTerminal_ * glideTerminal_) * double(remaining) / double(glideDistanceUm_)));
        const double band = std::max(1.0, expected * .05);
        if (in.speedPkph > expected + band) ++outsideFast_; else if (in.speedPkph < expected - band) ++outsideSlow_; else outsideFast_ = outsideSlow_ = 0;
        if (outsideFast_ >= 3) { decelStepMs_ = std::max<uint16_t>(50, decelStepMs_ - 25); outsideFast_ = 0; }
        if (outsideSlow_ >= 3) { decelStepMs_ = std::min<uint16_t>(400, decelStepMs_ + 25); outsideSlow_ = 0; }
      }
      return out;
    }
    glideActive_ = false;
    if (!in.irSpeedValid || !fresh) { out.pwmTarget = -1; return out; } // IR loss: hold existing PWM.
    if (in.speedPkph < target * .95) { out.pwmTarget = cruisePwm; out.upMs = 150; return out; }
    if (moratorium_) { --moratorium_; return out; }
    samples_[sampleCount_++] = in.speedPkph;
    if (sampleCount_ < 5) return out;
    std::sort(samples_, samples_ + 5); const double median = samples_[2]; sampleCount_ = 0;
    if (median < target * .95) { out.pwmTarget = in.actualPwm + 1; moratorium_ = 5; }
    else if (median > target * 1.05) { out.pwmTarget = std::max(0, in.actualPwm - 1); moratorium_ = 5; }
    return out;
  }
 private:
  void clearExecution() { departed_ = false; dwellSinceUs_ = 0; glideActive_ = false; sampleCount_ = moratorium_ = 0; }
  void beginGlideIfNeeded(const LocalRequirement& r, const LocalControlInput& in, double terminal) {
    if (glideActive_ && glideStation_ == r.station && glideKind_ == r.instruction) return;
    glideActive_ = true; glideStation_ = r.station; glideKind_ = r.instruction; glideOriginUm_ = in.irUm;
    glideStartSpeed_ = in.irSpeedValid ? in.speedPkph : (terminal ? kCruisePkph : kStationPkph); glideTerminal_ = terminal;
    // Geographic distance is map-derived; final uses 1.5 marker intervals.
    const uint8_t centre = FourStationGeography::kCentres[r.station];
    glideDistanceUm_ = r.instruction == LocalInstruction::Approach ? 5ULL * navi_one::spanMm(centre, in.direction) * 1000ULL :
      (uint64_t(navi_one::spanMm(centre, in.direction)) * 3ULL / 2ULL) * 1000ULL;
    decelStepMs_ = 150; outsideFast_ = outsideSlow_ = 0;
  }
  uint32_t lastIrSequence_ = 0; uint64_t lastMotionUm_ = 0, lastPulseUs_ = 0, dwellSinceUs_ = 0, glideOriginUm_ = 0, glideDistanceUm_ = 0;
  uint8_t activeStation_ = 0xff, glideStation_ = 0xff, sampleCount_ = 0, moratorium_ = 0, outsideFast_ = 0, outsideSlow_ = 0;
  int8_t direction_ = 0; LocalInstruction glideKind_ = LocalInstruction::Cruise; bool departed_ = false, glideActive_ = false;
  double samples_[5]{}, glideStartSpeed_ = 0, glideTerminal_ = 0; uint16_t decelStepMs_ = 150;
};
}
