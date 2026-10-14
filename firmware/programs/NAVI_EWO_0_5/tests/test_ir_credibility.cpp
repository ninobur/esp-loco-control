#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "../NaviIrCredibility.h"
#include "../../../common/IrMovementWire.h"
using namespace navi_eyes;
static unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

static IrCredibilityInput interval(int64_t residual) {
  IrCredibilityInput in;
  in.expectedUm = 300000;
  in.measuredUm = uint64_t(int64_t(in.expectedUm) + residual);
  in.installedPitchUm = ir_movement::kInstalledPitchUm;
  in.measurementApplicable = in.identifiedHallInterval = true;
  return in;
}
int main() {
  CHECK(ir_movement::kInstalledPitchUm == 9652);
  for (int sign : {-1, 1}) {
    for (int64_t residual : {0LL, 9999LL, 10000LL, 10001LL, 48259LL, 48260LL, 48261LL, 60000LL}) {
      auto in = interval(sign * residual);
      auto out = evaluateIrCredibility(in);
      CHECK(out.discrepancyAvailable);
      CHECK(out.signedDiscrepancyUm == sign * residual);
      CHECK(out.absoluteDiscrepancyUm == uint64_t(residual));
      CHECK(out.withdrawalThresholdUm == 48260);
      const auto expected = residual <= 10000 ? IrIntervalClassification::Consistent :
          residual < 48260 ? IrIntervalClassification::InconsistentPossible :
                            IrIntervalClassification::ProvisionalImpossible;
      CHECK(out.classification == expected);
      CHECK(out.transition == (residual >= 48260 ? IrCredibilityTransition::Withdraw :
                                                 IrCredibilityTransition::None));
      // Classification cannot mutate the supplied geographic interpretation.
      CHECK(in.identifiedHallInterval && in.measuredUm == uint64_t(300000 + sign * residual));
    }
  }
  // Neither repetition nor packet quality changes the approved numerical rule.
  auto in = interval(-30000);
  for (int i = 0; i < 8; ++i) {
    auto out = evaluateIrCredibility(in);
    CHECK(out.authority == IrGeographicAuthority::Retained);
    CHECK(out.transition == IrCredibilityTransition::None);
    in.authority = out.authority;
  }
  // One exactly five-pulse discrepancy withdraws; repeated evidence is not required.
  in = interval(-48260);
  auto withdrawn = evaluateIrCredibility(in);
  CHECK(withdrawn.transition == IrCredibilityTransition::Withdraw);
  in.authority = withdrawn.authority;
  CHECK(evaluateIrCredibility(in).transition == IrCredibilityTransition::None);

  // Restoration requires both a NEW independent reference and the following
  // independent identification. A good number alone is insufficient.
  for (int flags = 0; flags < 4; ++flags) {
    for (int64_t residual : {0LL, 10000LL, 10001LL, 48259LL, 48260LL, -48259LL, -48260LL}) {
      in = interval(residual);
      in.authority = IrGeographicAuthority::Withdrawn;
      in.freshIndependentReferenceAfterWithdrawal = flags & 1;
      in.followingIdentityIndependentOfDisputedIr = flags & 2;
      auto out = evaluateIrCredibility(in);
      const bool restores = flags == 3 && residual > -48260 && residual < 48260;
      CHECK((out.transition == IrCredibilityTransition::Restore) == restores);
      CHECK((out.authority == IrGeographicAuthority::Retained) == restores);
    }
  }
  // Missing/stale or otherwise inapplicable measurements are not negative
  // evidence and cannot withdraw or restore geographic measurement authority.
  for (auto authority : {IrGeographicAuthority::Retained, IrGeographicAuthority::Withdrawn}) {
    for (int invalid = 0; invalid < 4; ++invalid) {
      in = interval(60000);
      in.authority = authority;
      in.freshIndependentReferenceAfterWithdrawal = true;
      in.followingIdentityIndependentOfDisputedIr = true;
      if (invalid == 0) in.measurementApplicable = false;
      if (invalid == 1) in.identifiedHallInterval = false;
      if (invalid == 2) in.installedPitchUm = 0;
      if (invalid == 3) in.measuredUm = UINT64_MAX;
      auto out = evaluateIrCredibility(in);
      CHECK(!out.discrepancyAvailable);
      CHECK(out.classification == IrIntervalClassification::Unavailable);
      CHECK(out.authority == authority);
      CHECK(out.transition == IrCredibilityTransition::None);
    }
  }
  // Approved restoration may still be inconsistent (>10 mm). It does not
  // rewrite the earlier withdrawal measurement or require zero residual.
  in = interval(30000);
  in.authority = IrGeographicAuthority::Withdrawn;
  in.freshIndependentReferenceAfterWithdrawal = true;
  in.followingIdentityIndependentOfDisputedIr = true;
  const auto restored = evaluateIrCredibility(in);
  CHECK(restored.classification == IrIntervalClassification::InconsistentPossible);
  CHECK(restored.transition == IrCredibilityTransition::Restore);
  CHECK(withdrawn.signedDiscrepancyUm == -48260);
  CHECK(withdrawn.authority == IrGeographicAuthority::Withdrawn);
  std::printf("PASS %u IR credibility assertions (policy evaluator only)\n", checks);
}
