# NAVI EWO 0 5 IR credibility implementation status

October 14, 2026. The operator-approved rules are preserved verbatim in
[APPROVED_RULES.txt](APPROVED_RULES.txt). The operator subsequently named the
prior DART increment `NAVI_EWO_0_4` and this continuation `NAVI_EWO_0_5`; the
supplied rules retain their original title as historical evidence. They replace the earlier proposed
requirement for repeated discrepancies: **one interval at five pulses or more
withdraws IR geographic authority.** These provisional criteria require field
reassessment and must not change automatically.

The numeric policy evaluator and its focused test now live in
[`NAVI_EWO_0_5`](../../firmware/programs/NAVI_EWO_0_5/README.md).
The earlier staged files here remain a record of the original isolated test.
The evaluator is not yet called by the navigator. The working sketch retains
the paused, untested draft edits; timing, missing-target and OVERLAY decisions
remain outstanding. This is not a complete operational build.

## Implemented approved rules

- Absolute discrepancy at or below 10,000 µm is consistent.
- Above 10,000 µm but below five installed pulse pitches is inconsistent but
  possible; IR authority remains retained.
- At or above five pulse pitches, withdraw immediately. Installed pitch remains
  9,652 µm; the withdrawal boundary is exactly 48,260 µm.
- Restoration requires a possible interval below five pulses, a fresh
  independently identified reference after withdrawal, and an independently
  supported following identification. A discrepancy above 10 mm may restore
  authority while remaining classified inconsistent.
- Missing or inapplicable measurements are unavailable evidence. They cannot
  alone withdraw or restore geographic credibility. Transport applicability
  remains a separate concern.

The evaluator has no candidate filtering, geographic selection, calibration,
motor, timing-history or telemetry side effects. This follows the governing
principles of NAVI-owned judgment and preserving measurement disagreement.
It consumes NAVI's endpoint identification conclusions; it cannot establish
independence from the residual itself.

## Verification

The host test includes the staged evaluator directly and the existing installed
pitch constant. All **223 assertions pass** with address and undefined-behavior
sanitizers and warnings treated as errors. Cases include both signs of error,
10 mm exactly, 10 mm plus 1 µm, five pulses minus 1 µm, exactly five pulses,
larger discrepancies, immediate withdrawal, repeated subthreshold errors,
restoration with a still-inconsistent possible interval, missing independent
endpoint evidence, and unavailable or unrepresentable measurements.

The 31-file first-increment rollback snapshot matches its saved SHA-256
manifest. ESP32 core 3.3.12 is installed. This pass has not compiled an integrated
EWO_0_5 sketch, exercised actual production candidate selection with this policy,
measured ESP32 runtime, or verified MQTT delivery. No flash, operation, commit,
push or live-system change occurred.

## Integration still required

NAVI must derive the independence inputs from actual geographic decisions:
after withdrawal at one selected opening, the next independently identified
70×2 opening starts the restoration measurement, and its following identified
opening supplies the second endpoint. Merely receiving coherent IR packets or
passing the residual threshold cannot supply those identity conclusions.

Integration must retain both endpoint measurements and provenance, selected
opening IDs, MM identities, expected/measured distances, signed/absolute errors,
pulse equivalents, comparison method, and authority transition reasons. It must
switch the next target comparison without consuming the same Hall observation
twice or altering the selected coordinate. The staged evaluator does not yet
implement this endpoint tracking or serialization.

Required clarifications remain:

1. Historical timing sufficiency, predictor and stop/restart applicability.
2. The missing-target stop condition when there is no Hall observation to
   compare with MM+2.
3. The permitted STOP OVERLAY action without fine IR position.

No default was chosen for those operating decisions. The approved IR rules
settle the credibility thresholds; they do not specify the remaining policies.
