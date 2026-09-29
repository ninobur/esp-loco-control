#pragma once
#include <algorithm>
#include <stdint.h>
#include <limits.h>
#include <memory>
#include <new>

namespace navi_eyes {
// NAVI-owned spatial population. No acquisition filtering. One exact histogram
// median per distinct observed pulse position, then the median of five medians.
// A position closes on the next distinct pulse report, not after a timer or an
// arbitrary number of Hall readings. Missing populations are never invented.
class BootReference {
 public:
  BootReference() : histogram_(new (std::nothrow) uint32_t[4096]{}) {
    fault_ = !histogram_;
  }
  bool storageReady() const { return ready_ || bool(histogram_); }
  void origin(uint64_t pulses) { if (!haveOrigin_) { prior_ = pulses; haveOrigin_ = true; } }
  void newFrameBeforeCollection(uint64_t pulses) {
    if (!started()) { prior_ = pulses; haveOrigin_ = true; }
  }
  void progress(uint64_t pulses) {
    if (ready_ || fault_ || !haveOrigin_ || pulses <= prior_) return;
    prior_ = pulses;
    if (collecting_) {
      if (!count_) { fault_ = true; return; }
      medians_[positions_++] = populationMedian();
      if (positions_ == 5) {
        std::sort(medians_, medians_ + 5);
        reference_ = medians_[2]; ready_ = true; collecting_ = false;
        histogram_.reset();
        return;
      }
    }
    collecting_ = true;
    std::fill(histogram_.get(), histogram_.get() + 4096, 0);
    count_ = 0;
  }
  void observe(int16_t raw) {
    if (!collecting_ || ready_ || fault_) return;
    if (raw < 0 || raw > 4095 || histogram_[raw] == UINT32_MAX) {
      fault_ = true; return;
    }
    ++histogram_[raw]; ++count_;
  }
  void invalidate() { if (!ready_) { fault_ = true; collecting_ = false; } }
  bool started() const { return collecting_ || positions_ != 0 || ready_; }
  bool ready() const { return ready_; }
  bool fault() const { return fault_; }
  uint8_t positions() const { return positions_; }
  int16_t reference() const { return reference_; }
 private:
  int16_t populationMedian() const {
    const uint64_t lower = (count_ - 1) / 2, upper = count_ / 2;
    uint64_t seen = 0;
    int lo = -1;
    for (int value = 0; value < 4096; ++value) {
      seen += histogram_[value];
      if (lo < 0 && seen > lower) lo = value;
      if (seen > upper) return int16_t((lo + value) / 2);
    }
    return 0; // unreachable for a nonempty population
  }
  std::unique_ptr<uint32_t[]> histogram_;
  uint64_t count_ = 0, prior_ = 0;
  int16_t medians_[5]{}, reference_ = 0;
  uint8_t positions_ = 0;
  bool haveOrigin_ = false, collecting_ = false, ready_ = false, fault_ = false;
};
} // namespace navi_eyes
