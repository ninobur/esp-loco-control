#pragma once
#include <stdint.h>

namespace navi_eyes {

// Operator-approved provisional criteria, 2026-10-14. These assess IR only;
// neither is a Hall candidate admission or geographic commitment threshold.
static constexpr uint64_t kIrConsistentDiscrepancyUm = 10000;
static constexpr uint32_t kIrWithdrawalDiscrepancyPulses = 5;

enum class IrGeographicAuthority : uint8_t { Retained, Withdrawn };
enum class IrIntervalClassification : uint8_t {
  Unavailable, Consistent, InconsistentPossible, ProvisionalImpossible
};
enum class IrCredibilityTransition : uint8_t { None, Withdraw, Restore, OperatorReassessment };

struct IrCredibilityInput {
  uint64_t measuredUm = 0, expectedUm = 0;
  uint32_t installedPitchUm = 0;
  bool measurementApplicable = false;
  // These are NAVI's conclusions about identified endpoints, not conclusions
  // this evaluator can draw from the distance residual or packet health.
  bool identifiedHallInterval = false;
  bool freshIndependentReferenceAfterWithdrawal = false;
  bool followingIdentityIndependentOfDisputedIr = false;
  // Operator-authorized, single interval after a fresh declaration/reference
  // when no previous-lap prediction exists. This does not claim independence.
  bool operatorProvisionalInterval = false;
  IrGeographicAuthority authority = IrGeographicAuthority::Retained;
};

struct IrCredibilityResult {
  IrIntervalClassification classification = IrIntervalClassification::Unavailable;
  IrGeographicAuthority authority = IrGeographicAuthority::Retained;
  IrCredibilityTransition transition = IrCredibilityTransition::None;
  uint64_t absoluteDiscrepancyUm = 0, withdrawalThresholdUm = 0;
  int64_t signedDiscrepancyUm = 0;
  bool discrepancyAvailable = false;
};

// Pure, allocation-free policy evaluator. The caller supplies a completed
// identified Hall-to-Hall interval and validated measurement applicability.
// It does not select or discard Hall observations, manage timing history,
// command motors, or infer an independent geographic identity from IR.
inline IrCredibilityResult evaluateIrCredibility(const IrCredibilityInput& in) {
  IrCredibilityResult out;
  out.authority = in.authority;
  out.withdrawalThresholdUm = uint64_t(in.installedPitchUm) *
                             kIrWithdrawalDiscrepancyPulses;
  if (!in.measurementApplicable || !in.identifiedHallInterval ||
      !in.installedPitchUm) return out;

  const bool shortInterval = in.measuredUm < in.expectedUm;
  const uint64_t magnitude = shortInterval ? in.expectedUm - in.measuredUm :
                                            in.measuredUm - in.expectedUm;
  // Unrepresentable arithmetic is unavailable, never an invented residual.
  if (magnitude > uint64_t(INT64_MAX)) return out;
  out.absoluteDiscrepancyUm = magnitude;
  out.signedDiscrepancyUm = shortInterval ? -int64_t(magnitude) : int64_t(magnitude);
  out.discrepancyAvailable = true;
  if (magnitude >= out.withdrawalThresholdUm) {
    out.classification = IrIntervalClassification::ProvisionalImpossible;
    out.authority = IrGeographicAuthority::Withdrawn;
    if (in.authority == IrGeographicAuthority::Retained)
      out.transition = IrCredibilityTransition::Withdraw;
  } else {
    out.classification = magnitude <= kIrConsistentDiscrepancyUm ?
        IrIntervalClassification::Consistent : IrIntervalClassification::InconsistentPossible;
    if (in.authority == IrGeographicAuthority::Withdrawn &&
        in.freshIndependentReferenceAfterWithdrawal &&
        (in.followingIdentityIndependentOfDisputedIr || in.operatorProvisionalInterval)) {
      out.authority = IrGeographicAuthority::Retained;
      out.transition = IrCredibilityTransition::Restore;
    }
  }
  return out;
}
} // namespace navi_eyes
