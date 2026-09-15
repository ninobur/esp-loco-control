# 0085 — Test an 82 ms completed-passage floor

**Date:** 2026-09-12
**Status:** **Accepted for an experimental field test, 2026-09-12**
**Decided by:** operator — “Yes. I approve 82.”
**Supersedes:** 0084's blocked 90 ms field-test value

## Decision

Build an explicitly non-field-accepted NAVI_ONE image with a shared **82 ms**
completed-passage duration floor. Reject shorter events before recognition and
navigation, and publish one background acquisition record for each rejection.
Keep the unconditional 500 ms close-to-open guard unchanged.

This authorizes a field trial, not adoption of 82 ms as production policy.

## Context

The 47 ms Bamboo phantom cleared the 500 ms timing guard and stopped Otto at
cruise. The operator requested a 90 ms experiment, but audit exposed two
independent limits: gate 8 loses all marker visibility in its sustained-offset
case beginning at 88 ms, and Otto has one known genuine 82 ms passage.

Because the code rejects only `duration < floor`, a floor of 82 admits that
82 ms genuine record. It also remains six milliseconds below the observed
gate-8 failure boundary. The next shortest genuine Otto passage is 128 ms;
Toby's accepted primary minimum is 131 ms.

The two measurements bound the supported interval from opposite sides:
82–87 ms preserves the known 82 ms genuine passage and stays below gate 8's
88 ms failure. The operator selected 82, the conservative end of that interval.

## Alternatives considered

- **90 ms:** rejected before flash because the actual-value suite fails gate 8
  and it excludes a known genuine 82 ms Otto passage.
- **60 ms:** excludes the measured 40–58 ms phantom band with more margin, but
  the operator approved the highest value supported by current evidence.
- **Amplitude, smoothing, adaptive timing, and post-stop rules:** unchanged;
  they are different decisions.

## Consequences

- The 47 ms Bamboo event and documented 40–58 ms phantom band are rejected.
- The known 82 ms genuine passage remains admissible at equality.
- There is zero measured margin above that single 82 ms observation: the same
  crossing one millisecond shorter would be rejected. Accepted `dur_ms` values
  from 82–95 ms are therefore the field trial's principal warning band.
- Every acquisition gate uses the same shared value as the firmware.
- Each rejection is observable on `diag/acquisition` but cannot reach
  `MagnetRecognizer`, `judgedQ`, Navigator, or position.
- Field telemetry must test the six-millisecond clearance to the gate-8 failure
  boundary and the operator's possible ten-marker post-departure clustering.
- This does not solve the separate multi-second departure plateaus or validate
  the provisional post-stop resolver.

## References

- `docs/decisions/0084-test-a-90-ms-completed-passage-floor.md`
- `docs/NAVI_BAMBOO_TRANSIENT_ANALYSIS_20260912.md`
- `firmware/test-programs/NAVI_ONE/NAVIFieldConfig.h`
- `firmware/test-programs/NAVI_ONE/tests/gate_baseline_latch.cpp`
- Decision 0081 — unconditional 500 ms guard
