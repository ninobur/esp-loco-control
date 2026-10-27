#pragma once
// PWM90 manual experiment: previous median is used ONLY to recognize return
// to ordinary track. After 200 ms confirmed return, a fresh median is collected
// until the directed timing gate opens, when it is frozen for 70x2 detection.
// No samples from the confirmation period enter the new median.
#include <stdint.h>
#include <algorithm>
#include <cmath>
namespace navi_power {
static constexpr uint32_t BASELINE_CAPACITY = 2048;
static constexpr float BASELINE_ENTRY_MARGIN_ADC = 20.0f;
static constexpr uint16_t BASELINE_CONFIRM_BLOCKS = 10;
class BaselineTracker {
 public:
  enum class Phase : uint8_t { Boot, AwaitReturn, ConfirmReturn, Collect, Frozen };
  void beginBootCollection() { phase_ = Phase::Boot; count_=0; ready_=false; }
  void onBlock(float hallAdc, bool pwmOn) {
    if (!std::isfinite(hallAdc)) { if (phase_ == Phase::ConfirmReturn) confirm_=0; return; }
    if (phase_ == Phase::Boot) { if (!pwmOn) append_(hallAdc); return; }
    if (phase_ == Phase::AwaitReturn || phase_ == Phase::ConfirmReturn) {
      if (std::fabs(hallAdc - level_) <= BASELINE_ENTRY_MARGIN_ADC) {
        if (++confirm_ >= BASELINE_CONFIRM_BLOCKS) { phase_ = Phase::Collect; count_=0; confirm_=0; }
        else phase_ = Phase::ConfirmReturn;
      } else { confirm_=0; phase_ = Phase::AwaitReturn; }
      return;
    }
    if (phase_ == Phase::Collect && pwmOn) append_(hallAdc);
  }
  // Called at an accepted opening. The frozen median becomes the retained
  // return reference for the following interval.
  void onConfirmedOpening() {
    if (phase_ == Phase::Boot || phase_ == Phase::Collect) recompute_();
    phase_ = Phase::AwaitReturn; confirm_=0; count_=0;
  }
  // Called at the segment timing boundary, NOT at a Hall departure.
  // Zero fresh samples explicitly retains the previous median.
  bool freezeAtGate() {
    const bool fresh = phase_ == Phase::Collect && count_ != 0;
    if (fresh) recompute_();
    if (phase_ != Phase::Boot) { phase_ = Phase::Frozen; confirm_=0; count_=0; }
    return fresh;
  }
  float level() const { return level_; }
  bool haveLevel() const { return ready_; }
  Phase phase() const { return phase_; }
  uint32_t samples() const { return count_; }
 private:
  void append_(float v) {
    buf_[count_ % BASELINE_CAPACITY]=v; ++count_;
    // A live median of this interval, including the first sample.
    recompute_();
  }
  void recompute_() {
    const uint32_t n=std::min(count_, BASELINE_CAPACITY);
    if (!n) return;
    std::nth_element(buf_,buf_+n/2,buf_+n);
    level_=buf_[n/2]; ready_=true;
  }
  Phase phase_=Phase::Boot;
  float level_=0;
  float buf_[BASELINE_CAPACITY]{};
  uint32_t count_=0;
  uint16_t confirm_=0;
  bool ready_=false;
};
} // namespace navi_power
