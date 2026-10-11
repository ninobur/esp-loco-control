# NAVI_EWO_0_3 — restoration status

Baseline: EWO 0.1 commit 85a2cf2. This directory contains isolated, host-testable primitives, **not a complete or deployable sketch**.

## Implemented in isolated modules
- `NaviEvidenceCore.h`: 70-count, two-sample Hall qualifier with separate opening/detection timestamps, continuous-field latch, 650 ms detection-to-detection fallback exclusion, and a bounded 12-entry DNA observation history. `verifyIdentified` checks retained, assigned observations against the caller's map on every invocation; it does not decide or correct geographic identity.
- `NaviIrEvidence.h`: evidence-only per-pulse timestamp monitor. It distinguishes timing anomalies from gaps in received pulse records and PWM baseline changes. Its 1.5x diagnostic is **not** an IR-veto or missing-pulse authority threshold. It never inserts pulses or changes measured distance.

## Verification
- Host test `test_evidence.cpp`: compiled and executed successfully with `g++ -std=c++17 -Wall -Wextra -Werror`.
- Host test `test_ir.cpp`: compiled and executed successfully with the same flags.
- These are unit tests, not full-firmware or replay validation.

## Outstanding
- Integrate with the actual EWO 0.1 navigation core, map, and Type-6 ingress.
- Implement geographic identity judgment and continuous DNA history consumption, including direction/reversal.
- Validate IR credibility against healthy and failing recorded pulse streams before it can withdraw IR's magnet-identification veto.
- Use credible IR displacement at station speeds for duplicate exclusion; 650 ms is the no-IR fallback, not proof of a new magnet.
- Restore first-target-after-declaration behavior and geographic re-anchoring.
- Run complete host regression suite, compile full ESP32 sketch, and independently review.
- Keep STOP, actuator code, IR calibration, and governing decisions unchanged unless separately authorized.

No firmware flashed or deployed.

## Latest addition — sequential identity judgment

- `NaviSequentialJudgment.h` implements a pure next-MM judgment from a **qualified** Hall encounter, expected mapped polarity and direction. Credible IR-window disagreement is reported but does not veto the qualified expected magnet.
- `tests/test_judgment.cpp` adds expected-target, IR-disagreement, opposite-polarity, and wrong-direction cases.
- This is **not yet wired into `NaviIntegratedCore`**. The new tests have not been executed in this session, and neither integration nor field readiness is claimed.
- Outstanding before integration: reliable qualification of the IR coordinate at the Hall opening, continuous DNA history checks in the operational path, real Type-6 pulse provenance, reversal/declaration tests, and full sketch build.

## First operational core integration draft (commit 439c9c2)
- Created `NaviIntegratedCore.h` from the exact EWO 0.1 core. Retained its public API, mapped target, `confirm()` geographic anchoring, and operational STOP interfaces.
- Replaced the 0.1 Hall target-only/IR-veto acceptance path with the evidence bridge. Spatial-reference collection no longer suppresses physical Hall encounter observation.
- Qualified expected-polarity encounters can now call the existing `confirm()` without requiring the IR ±15% window. A bounded DNA history records confirmed identities and runs read-only polarity checks.
- The original 0.1 `evaluateMissing()` is **still present** and can still infer geographic progression from IR. Type-6 credibility is **not wired into the core**, and the provisional `irWindowQuestionable_` flag is not yet set. The full operational sketch has **not** been copied or compiled. The integrated core is a work-in-progress, **not approved for flashing**.
- Critical remaining validation: first-target declaration behavior, Hall baseline behavior during spatial collection, station duplicate protection with IR, sequence continuity beyond individually confirmed identities, reversals, unknown/contradictory encounters, and stale STOP instruction after a correction. No automatic DNA correction implemented.
- Integration has **not passed a full build or host regression suite**. Do not represent this commit as field-ready.
