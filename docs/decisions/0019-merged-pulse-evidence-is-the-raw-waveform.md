# 0019 — The merged-pulse question is answered by a raw-waveform scope, not by tuning thresholds on event telemetry

Status: Accepted  (2026-08-09)

## Decision
A dedicated diagnostic instrument, `IR_SCOPE` (firmware + live plotter +
offline replay), streams the raw 1 kHz IR waveform together with the exact
production-rule thresholds and detector state that interpreted it. The
falling-threshold question (would 0.40/0.50/0.60 of span recover seven
pulses without false edges?) is answered by replaying the real detector
semantics over recorded waveforms — never by adjusting the production
threshold on event statistics alone.

## Context
Event telemetry shows normal and ~doubled pulse widths/intervals mixed on
the seven-spoke wheel. The suspected mechanism — an inter-spoke trough that
never crosses `thrLow`, so the detector never rearms — is invisible in
event lines by construction: a missing fall edge emits nothing. IR_DIAG's
headrooms (decision 0010) bound the problem but are computed per surviving
event; the waveform between events is exactly what is missing. The
2026-08-05 lesson stands: capture data before accepting a story.

## Alternatives considered
- Tuning `thrLow` directly from event-telemetry headrooms — rejected:
  survivor-biased, and it changes production behaviour on inference.
- Enlarging IR_TEST's freeze-and-dump raw ring — rejected: trigger-gated
  snapshots cannot show the routine merged pulse, only declared failures.
- Streaming raw samples one MQTT message each (HallProbe pattern at 1 kHz)
  — rejected: 40× the message rate that already caused dropouts, and MQTT
  timing would become the sample timing.

## Consequences
- The detector inside IR_SCOPE is IR_DIAG's verbatim (decision 0009 chain:
  diagnostic rules match production); it must be updated in step if the
  production expressions ever change.
- Batched transport with session/sequence/discontinuity accounting is the
  house pattern for any future raw-stream diagnostic.
- Any threshold change proposal must cite a recorded capture and its replay
  table/overlay; decision 0010's headroom rule then judges it.
- No speed is computed or trusted until a hand-turn shows exactly seven
  pulses per revolution.

## References
`firmware/test-programs/IR_SCOPE/` (sketch, plotter, replay, README);
`docs/IR_DEV_REC/2026-08-09_IR_SCOPE_BUILD.md`; decisions 0009, 0010.
