#pragma once
#include "IrOdometryEpoch.h"

namespace ngr_nav {
// NAVI-owned association, not IR's claim to a landmark identity. The adapter
// must align point to Hall detection time; this class has no clock estimator.
class MmDistanceReference {
 public:
  void invalidate() { valid_ = false; }
  bool synchronize(uint8_t mm, uint32_t hallMs, const IrOdometryPoint& point,
                   const IrOdometryEpoch& current) {
    valid_ = current.contains(point);
    if (!valid_) return false;
    mm_ = mm;
    hallMs_ = hallMs;
    point_ = point;
    return true;
  }
  bool validFor(const IrOdometryEpoch& current) const {
    return valid_ && current.contains(point_);
  }
  IrDistance distanceFromMm(const IrOdometryEpoch& current) const {
    if (!validFor(current)) return {};
    return {true, double(current.pulses() - point_.pulses) * double(point_.pitchUm) / 1000.0};
  }
  uint8_t mm() const { return mm_; }
  uint32_t hallDetectedAtMs() const { return hallMs_; }
  uint64_t epochId() const { return point_.epoch; }
  uint64_t capturedUs() const { return point_.capturedUs; }
 private:
  bool valid_ = false;
  uint8_t mm_ = 0;
  uint32_t hallMs_ = 0;
  IrOdometryPoint point_{};
};
} // namespace ngr_nav
