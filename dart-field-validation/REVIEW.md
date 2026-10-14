# NAVI EWO 0 4 and 0 5 review package

October 14, 2026. Published for source review at the operator's request.
Neither version is field accepted. Publishing this package does not authorize
flashing, operation or merging into an operating branch.

## Start here

- [`NAVI_EWO_0_4`](../firmware/programs/NAVI_EWO_0_4/README.md): the tested first
  DART increment, preserved from its verified 31-file rollback snapshot and
  renamed under the operator's EWO_0_x scheme. Navigation code is unchanged by
  the rename. The active seven-suite host runner passes, including 85 DART
  assertions and 643 invariant assertions. Its predecessor under the original
  DART name compiled with ESP32 core 3.3.12; the naming change has not received
  a new ESP32 compilation.
- [`NAVI_EWO_0_5`](../firmware/programs/NAVI_EWO_0_5/README.md): unfinished
  continuation, retaining the paused working draft and the subsequently
  approved IR credibility evaluator. Its focused evaluator test passes 223
  assertions. The evaluator is not connected to navigation. The complete 0_5
  navigator is not validated, compiled or suitable for operation.
- [First-increment validation report](../dart-validation/REPORT.md): baseline
  comparison, historical replays, known inherited failures and limits.
- [Approved IR rules and integration status](ir-credibility/STATUS.md): exact
  five-pulse withdrawal, independent-reference restoration and remaining work.

## Provenance and review boundaries

Exact operational foundation: `85a2cf2a16d262abdf7258c60f60508b823913ee`,
`NAVI_EWO_0_1_MM045_STOP`. Original EWO and COHERENCE source, maps, calibration,
shared dependencies and operating systems are unchanged.

The first-increment rollback is retained in `rollback-increment-1/` and checked
against `rollback-manifest.json`. Version mapping and verification are recorded
in `version-renaming.json`. Old names in original requests, logs, hashes and
historical results are preserved deliberately. Executable first-increment
verification scripts now target `NAVI_EWO_0_4`.

The 0_5 draft includes timing and stopping logic begun before its operational
parameters were settled. Its presence is evidence of unfinished work, not an
approved operating policy. Outstanding choices are historical timing
sufficiency/prediction/context, the stop condition when no Hall observation
arrives, and OVERLAY action without fine IR. They must not be inferred from
the draft implementation.

Independent integration work still includes actual selected-endpoint tracking,
credibility transition records, measurement provenance, and production-code
tests. The draft metric-change replay and inferred/unavailable-coordinate
handling also require completion and validation before activation.

The five inherited suites that fail under the changed DART detector/window
contract remain recorded as failures in the original validation report. No
host test or replay establishes physical track reliability. Complete 0_5
regressions, ESP32 3.3.12 compilation, resource measurements and actual Pi
recorder receipt remain outstanding.

Test/build logs are included as review evidence despite the repository's
general log-ignore rule. No credentials or deployment binaries are included.
