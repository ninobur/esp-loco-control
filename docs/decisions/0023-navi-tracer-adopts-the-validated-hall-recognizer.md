# 0023 — NAVI_TRACER_0_1 ports the independently validated Hall recognizer and replaces NAVI_RVM_SHAPE_MANUAL's statistical shape classifier

Status: **Accepted (2026-10-22).** Implemented on branch
`feature/navi-tracer-0-1`, not flashed, not field accepted.

## Decision

`NAVI_TRACER_0_1` (`firmware/test-programs/NAVI_TRACER_0_1/`) uses
`NAVI_RVM_SHAPE_MANUAL` (`dev/navi-rvm-shape-manual-20261018`) as its
hardware/manual-control foundation, but replaces its recognition engine
entirely with a direct C++ port (`NaviTracerCore.h`) of the independently
validated Hall recognizer at commit `6efc952`
(`research/navi_target_acquisition_20261021/target_acquisition.py`,
`research/navi-target-acquisition-pwm90-20261021`). `RvmShape.h` /
`RvmShapeModel.h` (the pre-trained Gaussian-mixture LLR shape classifier,
"Model A") and `RvmNavigator.h` are deleted from this sketch, not adapted.
`RvmPause.h` (STOP/GO waveform hold-back buffering, built to keep that
classifier's window-indexed blocks in exact sequence) is also not carried
forward; the validated recognizer's own literal `pwm == 0` gate is used
instead, with no new stopped-PWM threshold.

Default acquisition duration is the research's own evaluated
**duration-floor** configuration (`0.02 s`), not the original 110 ms figure;
the 110 ms value remains available as a `NAVI_TRACER_USE_LEGACY_110MS_DURATION`
build-time alternative. The ΔBIC shape diagnostic is not ported (diagnostic
only, never a recognition gate).

## Context

`NAVI_RVM_SHAPE_MANUAL`'s own recognizer scores 20 ms Hall blocks against a
pre-trained statistical shape model with a continuously re-fit baseline.
Independently of this work, `research/navi_target_acquisition_20261021`
(commit `6efc952`) validated a materially different, much simpler
recognizer against the real Otto PWM-90 recording: a deterministic
CLOSE → TARGET → BASELINE → ACQUIRE → CLOSE-AND-ADVANCE state machine with a
**frozen** pre-encounter baseline (continuous extrapolation was shown,
directly, to risk a false return-to-baseline during a genuine 284.58 s
departure) and a 70-ADC/2-bin departure criterion. The field-test assignment
this decision implements required porting that validated recognizer onto
Otto's hardware for an observational first field trial — not redesigning or
re-validating navigation.

## Alternatives considered

- **Keep Model A, tune it further.** Rejected: it is a different, unrelated
  algorithm from what was independently validated; continuing it would not
  be testing the validated research at all.
- **Port the validated recognizer but keep `RvmPause.h`'s STOP/GO buffering
  for safety margin.** Rejected: that machinery exists to solve a problem
  (window-indexed block-sequence fidelity) the timestamp-driven validated
  recognizer does not have, and importing unneeded state/complexity for a
  hypothetical is what `AGENTS.md` §8 and the assignment's §9 both warn
  against. The field test itself is the intended way to find out whether
  Otto's real PWM ramp-down (field-documented: Otto physically stops near
  PWM ≈ 26, confirmed in `RvmPause.h`'s own comments) actually perturbs this
  recognizer — telemetry distinctly reports every `0 < PWM < 15` block so
  that evidence will exist either way.
- **Ship the original 110 ms acquisition duration as the default.** Rejected:
  `REPORT.md` evidences it rejects the median real encounter on this
  locomotive; the duration-floor configuration is the research's own
  evaluated recommendation, preserved exactly.

## Consequences

- NAVI_TRACER_0_1 carries no geographic-recovery, no CTO, and no automatic
  motor control; the recognizer is purely observational (verified: no
  identifier from `NaviTracerCore.h` or the recognition-event path is
  reachable from `writePwm`/`requestPwm`/`ledcWrite`).
- The stop/resume behavior under Otto's real PWM ramp-down is an
  acknowledged, open field-test question, not resolved by this decision.
- A faithful, evidence-based host-side equivalence test
  (`tests/navi_tracer_0_1/`) exists comparing the ported C++ core against
  the Python reference event-for-event; see
  `docs/NAVI_TRACER_0_1_BEHAVIORAL_EQUIVALENCE.md` for what it actually
  covers and what it does not (no real recorded data was available in the
  implementing environment).

## References

- `research/navi_target_acquisition_20261021/REPORT.md`, `target_acquisition.py` (commit `6efc952`)
- `docs/NAVI_TRACER_0_1_BASE_SKETCH_ASSESSMENT.md`
- `docs/NAVI_TRACER_0_1_BEHAVIORAL_EQUIVALENCE.md`
- Decision 0018 (firmware role and evidence status tracked independently)
- NAVI_TRACER_0_1 implementation authorization, 2026-10-22 (this branch's commit)
