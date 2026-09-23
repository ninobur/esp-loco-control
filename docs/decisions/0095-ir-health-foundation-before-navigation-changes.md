# 0095 - Establish IR measurement health before the remaining NAVI changes

Date: 2026-09-23
Status: Operator-directed sequence; first independent implementation tested,
not integrated or field accepted.
Decided by: David (priority and architectural criterion); Codex (implementation
details). Continues Sam's architecture 0.3 and Claude's review.

## Operator direction

David stated: "IR health measures are necessary precursors to other changes
mandated by the 20 questions," then requested: "Please take the first step."
He also reaffirmed the central test, recalling Sam's formulation: is this
something a cyclometer would know, or a rider would know?

Instrument code reports wheel measurement, its health/readiness and continuity.
NAVI interprets it using route, Hall, operating state and history. Zero measured
movement is not loss of communication and is not, by itself, a diagnosis.

## First implementation and its boundaries

Codex implemented a separate `IR_ARCHITECTURE_0_4` package, preserving the
original 0.3 ZIP and leaving COHERENCE 0.5 unchanged. Health, readiness, Epoch
and NAVI-owned MM reference remain separate. Ended epochs cannot reopen;
recovery immediately restores fresh odometry, not distance across the outage.

Engineering choices: retain ending reasons for recovery telemetry; monotonic
64-bit epoch IDs through reset/source replacement; owner-bound references;
explicit Hall-aligned measurement points; explicit distance availability;
and an adapter hook to terminate an epoch when no packet arrives. These are
Codex's mechanisms, not additional operator-imposed navigation policies.

Executable host tests and an ESP32 compile-only fixture pass. No transport
integration, live telemetry, Hall threshold, timing fallback, MM-window,
position-correction, station or AUTO change is authorized by this record alone.

## Risk exposed, not concealed

A test of the actual current detector reproduces stationary contrast collapse:
after a healthy synthetic waveform becomes constant, it reports inadequate
contrast. Thus healthy-zero semantics in the receiver do not yet establish
real stop/dwell continuity. Do not relabel genuine unavailable measurement as
zero or claim that excluding `unreliableSamples` solves acquisition.

Measurement readiness and adapter continuity remain the next work before the
later navigation changes rely on this layer. No new threshold or operational
policy was invented to bypass this gap. This record does not supersede the
twenty-question answers or decision 0094's instrument model.

## References

- `docs/NAVI_IR_HEALTH_STEP1_20260923.md`: changes, tests, limits and handoff.
- `docs/NAVI_COHERENCE_CURRENT_WORK_20260923.md`: lineage and implementation map.
- `docs/NAVI_COHERENCE_TWENTY_QUESTIONS_HANDOFF_20260923.txt`: supplied handoff.
- `firmware/NAVI_COHERENCE/IR_ARCHITECTURE_0_4/README.md`: API and test commands.
