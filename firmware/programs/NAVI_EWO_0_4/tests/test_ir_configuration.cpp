#include "../NaviIntegratedCore.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace navi_eyes;
using namespace ngr_nav;

static ir_movement::WireSnapshot packet(uint32_t seq, uint64_t pulses = 10) {
  ir_movement::WireSnapshot w;
  w.bootId = 42; w.sequence = seq; w.capturedUs = uint64_t(seq) * 100000;
  w.completedPulses = w.observedRises = pulses;
  w.nominalUm = pulses * w.pitchUm;
  w.opticalReason = ir_movement::TRACKING;
  return w;
}

static void calibrationIsRawEvidence() {
  NaviIntegratedCore n;
  n.observeIr(packet(1), 100000, 40);
  n.declare(0, 1, 100001);
  const auto epoch = n.irMeasurementEpochId();
  for (uint32_t seq = 2; seq <= 12; ++seq) {
    auto w = packet(seq, 10 + seq);
    w.calibrationId = seq % 2 ? 0 : 1234;
    n.observeIr(w, w.capturedUs, 40);
    assert(n.latestIr().calibrationId == w.calibrationId);
    assert(n.irMeasurementEpochId() == epoch && n.irApplicable(w.capturedUs));
    assert(n.relationshipReliable() && !n.distanceHolding());
  }
  assert(n.irSpeedAvailable(1200000)); // metadata did not clear speed history
  assert(n.irSpeedMmS() > 96.51 && n.irSpeedMmS() < 96.53);
}

static void malformedScaleIsNotReconfiguration() {
  for (unsigned fault = 0; fault < 5; ++fault) {
    NaviIntegratedCore n;
    n.observeIr(packet(1), 100000, 40);
    n.declare(0, 1, 100001);
    const auto epoch = n.irMeasurementEpochId();
    auto w = packet(2, 11);
    switch (fault) {
      case 0: w.pitchUm = 0; w.nominalUm = 0; break;
      case 1: ++w.pitchUm; w.nominalUm = w.completedPulses * w.pitchUm; break;
      case 2: --w.nominalUm; break;
      case 3:
        w.completedPulses = UINT64_MAX / w.pitchUm + 1;
        w.nominalUm = w.completedPulses * w.pitchUm;
        break;
      case 4: w.bootId = 0; break;
    }
    assert(!w.bootId || !ir_movement::validConfiguredDistance(w));
    // The core also protects direct callers. Ingress rejects these frames
    // before delivery; neither path accepts a new pitch or negotiates one.
    n.observeIr(w, 200000, 40);
    assert(!n.irApplicable(200000) && !n.irMeasurementEpochActive());
    assert(n.irMeasurementEpochId() == epoch);
    assert(n.irHealthFault() == uint8_t(IrHealthFault::PacketInvalid));
    assert(!n.relationshipReliable() && n.confirmedCount() == 0);
  }
  const auto w = packet(1, UINT64_MAX / ir_movement::kInstalledPitchUm);
  assert(ir_movement::validConfiguredDistance(w));
}

static void realContinuityBoundaries() {
  for (unsigned fault = 0; fault < 6; ++fault) {
    NaviIntegratedCore n;
    uint8_t mac[6] = {1, 2, 3, 4, 5, 6};
    n.observeIr(packet(2, 20), 200000, 40, mac);
    n.declare(0, 1, 200001);
    auto w = packet(3, 21);
    uint64_t receivedUs = 300000;
    // Metadata must not bypass any same-source/boot ordering check.
    w.calibrationId = 1234;
    switch (fault) {
      case 0: ++w.bootId; w.sequence = 1; w.capturedUs = 1; break;
      case 1: ++mac[5]; w.sequence = 1; w.capturedUs = 1; break;
      case 2: w.sequence = 2; break;
      case 3: w.capturedUs = 200000; break;
      case 4: w.completedPulses = 19; w.nominalUm = 19 * w.pitchUm; break;
      case 5: receivedUs = 199999; break;
    }
    n.observeIr(w, receivedUs, 40, mac);
    assert(!n.relationshipReliable());
    assert(!std::strcmp(n.irDistanceState(receivedUs), "FRAME_LOST_REDECLARE"));
    if (fault < 2) {
      assert(n.irMeasurementEpochId() == 2 && n.irMeasurementEpochActive());
    } else {
      assert(n.irMeasurementEpochId() == 1 && !n.irMeasurementEpochActive());
      assert(n.irHealthFault() == uint8_t(IrHealthFault::OrderFault));
      n.observeIr(packet(4, 22), 400000, 40, mac);
      assert(n.irMeasurementEpochId() == 2 && n.irMeasurementEpochActive());
      assert(!n.relationshipReliable()); // no automatic coordinate recovery
    }
  }
}

static void diagnosticsAndTransportAreNotEpochs() {
  NaviIntegratedCore n;
  n.observeIr(packet(1), 100000, 40);
  n.declare(0, 1, 100001);
  for (uint8_t reason = ir_movement::PRIMING; reason <= ir_movement::TRACKING; ++reason) {
    auto w = packet(reason + 2);
    w.opticalReason = reason;
    w.sampleGaps = w.saturatedSamples = w.openAborts = reason + 1;
    n.observeIr(w, w.capturedUs, 0);
    const auto health = classifyIrInstrument(w);
    assert(n.irHealthFault() == uint8_t(health.fault));
    assert(n.irReadiness() == uint8_t(health.readiness));
    assert(health.detectorReason == reason);
    assert(n.irApplicable(w.capturedUs) && n.irMeasurementEpochId() == 1);
    assert(n.relationshipReliable());
  }
  const auto last = n.latestIr();
  assert(!n.irApplicable(last.capturedUs + NaviIntegratedCore::kIrFreshUs + 1));
  n.noteObservationLoss(1, 1, last.capturedUs + 2000000);
  auto w = packet(last.sequence + 20);
  n.observeIr(w, w.capturedUs, 0);
  assert(n.irApplicable(w.capturedUs) && n.irMeasurementEpochId() == 1);
  assert(n.relationshipReliable());
  w.opticalReason = 255;
  assert(classifyIrInstrument(w).fault == IrHealthFault::PacketInvalid);
  assert(uint8_t(IrHealthFault::InadequateContrast) == 6); // NSR1 IDs unchanged
}

int main() {
  calibrationIsRawEvidence(); malformedScaleIsNotReconfiguration();
  realContinuityBoundaries(); diagnosticsAndTransportAreNotEpochs();
  std::puts("PASS: fixed Type-5 configuration, raw calibration metadata, real epochs, validity and diagnostic boundaries");
}
