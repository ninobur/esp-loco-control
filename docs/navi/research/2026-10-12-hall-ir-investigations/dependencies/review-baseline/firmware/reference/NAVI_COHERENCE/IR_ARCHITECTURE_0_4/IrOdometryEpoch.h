#pragma once
#include <stdint.h>
#include <limits.h>
#include "IrInstrument.h"

namespace ngr_nav {
enum class IrEpochBreak : uint8_t {
  None, FirstObservation, InstrumentUnavailable, NotReady, BootChanged,
  CalibrationChanged, PitchChanged, TimeOrder, CounterOrder, SampleGapOccurred,
  SaturationOccurred, PulseAbortOccurred, InferenceOccurred, Reset,
  SourceChanged, TransportGap, LinkStale, IdentityExhausted
};
inline const char* irEpochBreakName(IrEpochBreak value) {
  switch (value) {
    case IrEpochBreak::None: return "NONE";
    case IrEpochBreak::FirstObservation: return "FIRST_OBSERVATION";
    case IrEpochBreak::InstrumentUnavailable: return "INSTRUMENT_UNAVAILABLE";
    case IrEpochBreak::NotReady: return "NOT_READY";
    case IrEpochBreak::BootChanged: return "BOOT_CHANGED";
    case IrEpochBreak::CalibrationChanged: return "CALIBRATION_CHANGED";
    case IrEpochBreak::PitchChanged: return "PITCH_CHANGED";
    case IrEpochBreak::TimeOrder: return "TIME_ORDER";
    case IrEpochBreak::CounterOrder: return "COUNTER_ORDER";
    case IrEpochBreak::SampleGapOccurred: return "SAMPLE_GAP_OCCURRED";
    case IrEpochBreak::SaturationOccurred: return "SATURATION_OCCURRED";
    case IrEpochBreak::PulseAbortOccurred: return "PULSE_ABORT_OCCURRED";
    case IrEpochBreak::InferenceOccurred: return "INFERENCE_OCCURRED";
    case IrEpochBreak::Reset: return "RESET";
    case IrEpochBreak::SourceChanged: return "SOURCE_CHANGED";
    case IrEpochBreak::TransportGap: return "TRANSPORT_GAP";
    case IrEpochBreak::LinkStale: return "LINK_STALE";
    default: return "IDENTITY_EXHAUSTED";
  }
}

class IrOdometryEpoch;
// A copyable measurement for timestamp-aligned history. No route information.
struct IrOdometryPoint {
  const IrOdometryEpoch* owner = nullptr;
  uint64_t epoch = 0, boot = 0, capturedUs = 0, pulses = 0;
  uint32_t calibration = 0, pitchUm = 0;
};
struct IrDistance {
  bool available = false;
  double mm = 0;
};

class IrOdometryEpoch {
 public:
  struct Update {
    bool snapshotAccepted = false, measurementReady = false;
    bool epochStarted = false, epochEnded = false;
    uint64_t epochId = 0;
    IrEpochBreak reason = IrEpochBreak::None;
  };
  IrOdometryEpoch() = default;
  IrOdometryEpoch(const IrOdometryEpoch&) = delete;
  IrOdometryEpoch& operator=(const IrOdometryEpoch&) = delete;

  // Reset/source replacement cannot reuse an epoch identity while references
  // survive. Keep this object alive as long as its points/references exist.
  Update reset() {
    auto u = endEpoch(IrEpochBreak::Reset);
    haveSnapshot_ = false;
    return u;
  }
  Update sourceChanged() {
    auto u = endEpoch(IrEpochBreak::SourceChanged);
    haveSnapshot_ = false;
    return u;
  }
  // Called by the adapter even when NO packets arrive. This class does not
  // invent a freshness timeout or turn communications loss into zero travel.
  Update endEpoch(IrEpochBreak reason) {
    Update u;
    u.epochId = epochId_;
    if (reason == IrEpochBreak::None) return u;
    u.reason = reason;
    u.epochEnded = epochActive_;
    if (epochActive_ || (epochId_ && pendingBreak_ == IrEpochBreak::None)) pendingBreak_ = reason;
    epochActive_ = false;
    return u;
  }
  Update ingest(const ir_movement::WireSnapshot& w) {
    const auto instrument = classifyIrInstrument(w);
    if (!instrument.measurementReady()) {
      return endEpoch(instrument.healthy ? IrEpochBreak::NotReady :
                                           IrEpochBreak::InstrumentUnavailable);
    }
    // Defense in depth. Never adopt an old sample as a new origin. Source
    // admission and retired-boot rejection remain the transport's job.
    if (haveSnapshot_ && w.bootId == last_.bootId && w.capturedUs <= last_.capturedUs)
      return endEpoch(IrEpochBreak::TimeOrder);

    Update u;
    u.epochId = epochId_;
    if (!epochActive_) {
      const auto reason = pendingBreak_ != IrEpochBreak::None ? pendingBreak_ :
                          epochId_ ? IrEpochBreak::Reset : IrEpochBreak::FirstObservation;
      return beginEpoch(w, reason, false);
    }
    const auto reason = discontinuity(w);
    if (reason != IrEpochBreak::None) {
      endEpoch(reason);
      return beginEpoch(w, reason, true);
    }
    last_ = w;
    u.snapshotAccepted = u.measurementReady = true;
    return u;
  }
  bool epochActive() const { return epochActive_; }
  bool haveMeasurement() const { return haveSnapshot_ && epochActive_; }
  uint64_t epochId() const { return epochId_; }
  uint64_t pulses() const { return last_.completedPulses; }
  uint64_t capturedUs() const { return last_.capturedUs; }
  uint64_t bootId() const { return last_.bootId; }
  uint32_t calibrationId() const { return last_.calibrationId; }
  uint32_t pitchUm() const { return last_.pitchUm; }
  IrOdometryPoint point() const {
    if (!haveMeasurement()) return {};
    return {this, epochId_, bootId(), capturedUs(), pulses(), calibrationId(), pitchUm()};
  }
  bool contains(const IrOdometryPoint& p) const {
    return haveMeasurement() && p.owner == this && p.epoch == epochId_ &&
           p.boot == bootId() && p.calibration == calibrationId() && p.pitchUm == pitchUm() &&
           p.capturedUs >= originUs_ && p.capturedUs <= capturedUs() &&
           p.pulses >= originPulses_ && p.pulses <= pulses();
  }
  IrDistance epochDistance() const {
    if (!haveMeasurement()) return {};
    return {true, double(pulses() - originPulses_) * double(pitchUm()) / 1000.0};
  }

 private:
  Update beginEpoch(const ir_movement::WireSnapshot& w, IrEpochBreak reason, bool ended) {
    Update u;
    u.epochEnded = ended;
    u.epochId = epochId_;
    if (epochId_ == UINT64_MAX) {
      epochActive_ = false;
      u.reason = IrEpochBreak::IdentityExhausted;
      return u;
    }
    ++epochId_;
    last_ = w;
    originPulses_ = pulses();
    originUs_ = capturedUs();
    haveSnapshot_ = epochActive_ = true;
    pendingBreak_ = IrEpochBreak::None;
    u.snapshotAccepted = u.measurementReady = u.epochStarted = true;
    u.epochId = epochId_;
    u.reason = reason;
    return u;
  }
  IrEpochBreak discontinuity(const ir_movement::WireSnapshot& w) const {
    if (w.bootId != last_.bootId) return IrEpochBreak::BootChanged;
    if (w.calibrationId != last_.calibrationId) return IrEpochBreak::CalibrationChanged;
    if (w.pitchUm != last_.pitchUm) return IrEpochBreak::PitchChanged;
    if (w.completedPulses < last_.completedPulses || w.observedRises < last_.observedRises ||
        w.sampleGaps < last_.sampleGaps || w.saturatedSamples < last_.saturatedSamples ||
        w.openAborts < last_.openAborts || w.inferredAdded < last_.inferredAdded ||
        w.inferredRemoved < last_.inferredRemoved) return IrEpochBreak::CounterOrder;
    if (w.sampleGaps != last_.sampleGaps) return IrEpochBreak::SampleGapOccurred;
    if (w.saturatedSamples != last_.saturatedSamples) return IrEpochBreak::SaturationOccurred;
    if (w.openAborts != last_.openAborts) return IrEpochBreak::PulseAbortOccurred;
    if (w.inferredAdded != last_.inferredAdded || w.inferredRemoved != last_.inferredRemoved)
      return IrEpochBreak::InferenceOccurred;
    return IrEpochBreak::None;
  }
  ir_movement::WireSnapshot last_{};
  bool haveSnapshot_ = false, epochActive_ = false;
  uint64_t epochId_ = 0, originPulses_ = 0, originUs_ = 0;
  IrEpochBreak pendingBreak_ = IrEpochBreak::None;
};
} // namespace ngr_nav
