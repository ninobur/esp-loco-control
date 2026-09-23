#pragma once
#include <stdint.h>

// One-sample-at-a-time Hall detector. No map, station, or IR dependency.
// sample() is called at 1 kHz with the median-of-five ADC reading.
namespace navi_one {

enum class HallEvent : uint8_t { None, Open, Close, BaselineAccepted, BaselineRejected };
enum class BaselineReject : uint8_t { None, SlowCadence, LowPwm, Spread, Interrupted };

struct HallDecision {
  HallEvent event = HallEvent::None;
  BaselineReject reject = BaselineReject::None;
  uint32_t openedMs = 0;
  uint32_t closedMs = 0;
  uint32_t priorGapMs = 0;
  int16_t baseline = 0;
  int16_t candidate = 0;
  int16_t peak = 0;
  uint16_t spread = 0;
  uint8_t polarity = 0;
  uint8_t pwm = 0;
};

class SimpleHall {
 public:
  bool ready() const { return primed_; }
  bool open() const { return state_ == State::Open; }
  int16_t baseline() const { return baseline_; }
  uint32_t accepted() const { return accepted_; }
  uint32_t rejected() const { return rejected_; }

  // A declaration changes the navigation frame, not the physical Hall line.
  // Keep the baseline, but abandon an in-progress collection and cadence.
  void resetFrame() {
    if (!primed_) return;
    if (state_ != State::Open) state_ = State::Idle;
    overCount_ = count_ = 0;
    haveLastOpen_ = false;
  }

  HallDecision sample(uint32_t now, int16_t raw, uint8_t pwm) {
    HallDecision out;
    out.baseline = baseline_;
    out.pwm = pwm;
    if (!primed_) {
      values_[count_++] = raw;
      if (count_ == 2000) {
        baseline_ = median(values_, count_);
        count_ = 0;
        primed_ = true;
      }
      out.baseline = baseline_;
      return out;
    }

    const int32_t delta = (int32_t)raw - baseline_;
    const bool above = delta >= 70 || delta <= -70;
    if (state_ != State::Open) {
      if (above) {
        if (++overCount_ == 1) firstOverMs_ = now;
        if (overCount_ >= 5) {
          if (state_ == State::Collect) {
            ++rejected_;
            // The Open event takes priority; interruption is counted.
          }
          state_ = State::Open;
          openedMs_ = firstOverMs_;
          priorGapMs_ = haveLastOpen_ ? openedMs_ - lastOpenMs_ : UINT32_MAX;
          lastOpenMs_ = openedMs_;
          haveLastOpen_ = true;
          peak_ = raw;
          polarity_ = delta > 0 ? 1 : 0;
          underCount_ = 0;
          count_ = 0;
          overCount_ = 0;
          out.event = HallEvent::Open;
          out.openedMs = openedMs_;
          out.priorGapMs = priorGapMs_;
          out.peak = peak_;
          out.polarity = polarity_;
          return out;
        }
      } else overCount_ = 0;
    }

    if (state_ == State::Open) {
      if ((polarity_ && raw > peak_) || (!polarity_ && raw < peak_)) peak_ = raw;
      if (above) {
        underCount_ = 0;
        lastOverMs_ = now;
      } else if (++underCount_ >= 30) {
        closedMs_ = lastOverMs_;
        guardStartMs_ = now; // 30 ms below threshold has been confirmed
        underCount_ = 0;
        state_ = priorGapMs_ <= 3000 ? State::Guard : State::Idle;
        out.event = HallEvent::Close;
        out.openedMs = openedMs_;
        out.closedMs = closedMs_;
        out.priorGapMs = priorGapMs_;
        out.peak = peak_;
        out.polarity = polarity_;
        if (state_ == State::Idle) out.reject = BaselineReject::SlowCadence;
        return out;
      }
    } else if (state_ == State::Guard) {
      if (now - guardStartMs_ >= 80) {
        if (pwm <= 30) {
          state_ = State::Idle;
          ++rejected_;
          out.event = HallEvent::BaselineRejected;
          out.reject = BaselineReject::LowPwm;
          out.openedMs = openedMs_;
          out.closedMs = closedMs_;
          return out;
        }
        state_ = State::Collect;
        count_ = 0;
        lowPwm_ = false;
      }
    }
    if (state_ == State::Collect) {
      if (pwm <= 30) lowPwm_ = true;
      values_[count_++] = raw;
      if (count_ == 200) {
        int16_t lo = values_[0], hi = values_[0];
        for (uint16_t i = 1; i < count_; ++i) {
          if (values_[i] < lo) lo = values_[i];
          if (values_[i] > hi) hi = values_[i];
        }
        out.spread = (uint16_t)((int32_t)hi - lo);
        out.candidate = median(values_, count_);
        out.openedMs = openedMs_;
        out.closedMs = closedMs_;
        out.priorGapMs = priorGapMs_;
        if (lowPwm_ || out.spread > 32) {
          ++rejected_;
          out.event = HallEvent::BaselineRejected;
          out.reject = lowPwm_ ? BaselineReject::LowPwm : BaselineReject::Spread;
        } else {
          baseline_ = out.candidate;
          ++accepted_;
          out.event = HallEvent::BaselineAccepted;
        }
        out.baseline = baseline_;
        count_ = 0;
        state_ = State::Idle;
      }
    }
    return out;
  }

 private:
  enum class State : uint8_t { Idle, Open, Guard, Collect };
  // In-place quickselect. The array belongs to this object and is not shared.
  static int16_t median(int16_t* a, uint16_t n) {
    uint16_t left = 0, right = n - 1, target = n / 2;
    while (left < right) {
      const int16_t pivot = a[(left + right) / 2];
      uint16_t i = left, j = right;
      while (i <= j) {
        while (a[i] < pivot) ++i;
        while (a[j] > pivot) { if (j == 0) break; --j; }
        if (i <= j) {
          int16_t tmp = a[i]; a[i] = a[j]; a[j] = tmp;
          ++i;
          if (j == 0) break;
          --j;
        }
      }
      if (target <= j) right = j;
      else if (target >= i) left = i;
      else return a[target];
    }
    return a[target];
  }

  int16_t values_[2000]{};
  int16_t baseline_ = 0, peak_ = 0;
  uint16_t count_ = 0;
  uint8_t overCount_ = 0, underCount_ = 0, polarity_ = 0;
  bool primed_ = false, haveLastOpen_ = false, lowPwm_ = false;
  uint32_t firstOverMs_ = 0, openedMs_ = 0, closedMs_ = 0, guardStartMs_ = 0;
  uint32_t lastOpenMs_ = 0, lastOverMs_ = 0, priorGapMs_ = UINT32_MAX;
  uint32_t accepted_ = 0, rejected_ = 0;
  State state_ = State::Idle;
};
}
