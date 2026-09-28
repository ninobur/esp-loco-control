#pragma once

#include <stddef.h>
#include <stdint.h>

namespace navi_eyes {

// Measurement-only spatial Hall reference. It does not decide whether a
// sample is ordinary track, a magnet, an RTB, or a navigation event.
template <size_t MaxBins = 128, size_t MaxSamplesPerBin = 31>
class SpatialHallReference {
 public:
  explicit SpatialHallReference(uint16_t binMm = 25) : binMm_(binMm ? binMm : 1) { reset(); }

  void reset() {
    for (size_t i = 0; i < MaxBins; ++i) counts_[i] = 0;
    occupied_ = 0;
    minDistance_ = 0;
    maxDistance_ = 0;
  }

  // Adds an observation using physical distance, never elapsed time. Samples
  // outside the configured finite workspace are reported as not retained;
  // they are still available to NAVI through the original observation.
  bool add(uint32_t distanceMm, int16_t hall) {
    const size_t index = distanceMm / binMm_;
    if (index >= MaxBins) return false;
    if (counts_[index] == 0) {
      ++occupied_;
      if (occupied_ == 1) minDistance_ = maxDistance_ = distanceMm;
      else {
        if (distanceMm < minDistance_) minDistance_ = distanceMm;
        if (distanceMm > maxDistance_) maxDistance_ = distanceMm;
      }
    }
    if (counts_[index] < MaxSamplesPerBin) values_[index][counts_[index]++] = hall;
    return true;
  }

  bool ready() const { return occupied_ != 0; }
  size_t occupiedBins() const { return occupied_; }
  uint16_t binMm() const { return binMm_; }
  uint32_t minDistanceMm() const { return minDistance_; }
  uint32_t maxDistanceMm() const { return maxDistance_; }

  int16_t candidate() const {
    if (!ready()) return 0;
    int16_t representatives[MaxBins];
    size_t n = 0;
    for (size_t i = 0; i < MaxBins; ++i) {
      if (!counts_[i]) continue;
      int16_t copy[MaxSamplesPerBin];
      for (size_t j = 0; j < counts_[i]; ++j) copy[j] = values_[i][j];
      representatives[n++] = median(copy, counts_[i]);
    }
    return median(representatives, n);
  }

 private:
  template <size_t N>
  static int16_t median(int16_t (&input)[N], size_t n) {
    for (size_t i = 1; i < n; ++i) {
      const int16_t v = input[i];
      size_t j = i;
      while (j && input[j - 1] > v) { input[j] = input[j - 1]; --j; }
      input[j] = v;
    }
    return input[n / 2];
  }

  uint16_t binMm_;
  uint16_t counts_[MaxBins]{};
  int16_t values_[MaxBins][MaxSamplesPerBin]{};
  size_t occupied_ = 0;
  uint32_t minDistance_ = 0, maxDistance_ = 0;
};

}  // namespace navi_eyes
