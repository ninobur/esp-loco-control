#pragma once
#include <stdint.h>
#include "RouteMap.h"

namespace navi_one {

struct LapBaselineUpdate {
  bool complete = false;
  bool valid = false;
  uint8_t origin = 0;
  uint16_t advances = 0;
  uint16_t coverage = 0;
  int32_t estimate = 0;
  int32_t baselineBefore = 0;
  int16_t requested = 0;
  int8_t applied = 0;
};

// X18 field-test policy: SET LOCATION supplies a fixed lap origin. A reversal
// discards the partial circuit and waits to return to that same origin. Only
// accepted navigation advances enter the estimator, one shadow value per MM.
class LapBaselineController {
 public:
  void declare(uint8_t origin) {
    origin_ = origin % ROUTE_N;
    declared_ = true;
    seekingOrigin_ = false;
    clearLap();
  }

  void directionChanged() {
    if (!declared_) return;
    clearLap();
    seekingOrigin_ = true;
  }

  void invalidate() {
    clearLap();
    declared_ = false;
    seekingOrigin_ = false;
  }

  LapBaselineUpdate advance(uint8_t mm, int32_t shadow, int32_t baseline) {
    LapBaselineUpdate u;
    u.origin = origin_;
    if (!declared_) return u;
    mm %= ROUTE_N;
    if (seekingOrigin_) {
      if (mm == origin_) { seekingOrigin_ = false; clearLap(); }
      return u;
    }

    ++advances_;
    if (!seen_[mm]) { seen_[mm] = true; ++coverage_; }
    value_[mm] = (int16_t)shadow;
    if (advances_ < ROUTE_N) return u;

    u.complete = true;
    u.advances = advances_;
    u.coverage = coverage_;
    u.baselineBefore = baseline;
    u.valid = (advances_ == ROUTE_N && mm == origin_ && coverage_ == ROUTE_N);
    if (u.valid) {
      int16_t sorted[ROUTE_N];
      for (uint16_t i = 0; i < ROUTE_N; ++i) sorted[i] = value_[i];
      for (uint16_t i = 1; i < ROUTE_N; ++i) {
        const int16_t v = sorted[i]; int j = (int)i - 1;
        while (j >= 0 && sorted[j] > v) { sorted[j + 1] = sorted[j]; --j; }
        sorted[j + 1] = v;
      }
      u.estimate = sorted[ROUTE_N / 2];
      int32_t d = u.estimate - baseline;
      u.requested = (int16_t)(d < -32768 ? -32768 : (d > 32767 ? 32767 : d));
      u.applied = (int8_t)(d < -2 ? -2 : (d > 2 ? 2 : d));
    }
    clearLap();
    return u;
  }

  bool declared() const { return declared_; }
  bool seekingOrigin() const { return seekingOrigin_; }
  uint8_t origin() const { return origin_; }
  uint16_t advances() const { return advances_; }
  uint16_t coverage() const { return coverage_; }

 private:
  void clearLap() {
    advances_ = coverage_ = 0;
    for (uint16_t i = 0; i < ROUTE_N; ++i) seen_[i] = false;
  }
  int16_t value_[ROUTE_N] = {};
  bool seen_[ROUTE_N] = {};
  uint16_t advances_ = 0, coverage_ = 0;
  uint8_t origin_ = 0;
  bool declared_ = false, seekingOrigin_ = false;
};

}  // namespace navi_one
