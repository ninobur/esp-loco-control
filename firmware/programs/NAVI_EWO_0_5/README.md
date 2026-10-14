# NAVI_EWO_0_5

October 14, 2026. Experimental integration candidate. **The startup-memory
failure has been repaired. The updated EWO 0_5 is installed on Otto and has
connected successfully on repeated stationary starts. Train-run clearance
still requires confirming INA219 under normal battery power.**

Foundation: `NAVI_EWO_0_1_MM045_STOP`,
`85a2cf2a16d262abdf7258c60f60508b823913ee`. The separate, tested first DART
increment is [NAVI_EWO_0_4](../NAVI_EWO_0_4/README.md), unchanged by this work.
The original EWO source, COHERENCE R2, shared dependencies, map and calibration
remain unchanged. The first-increment rollback and SHA-256 manifest remain in
`dart-field-validation/`.

## Implemented

- Median-five 70×2 observations, causal IR association, closest-to-target
  selection, midpoint ties and one advancement per selected target.
- Selected-coordinate anchoring; no historical percentage admission gate or
  650 ms fallback. Every observed opening enters the bounded diagnostic path.
- Approved provisional IR credibility: absolute discrepancy ≤10 mm consistent;
  >10 mm and <48.260 mm inconsistent but possible; ≥48.260 mm immediately
  withdraws IR authority. Geographic selection is preserved. Restoration needs
  a new independently selected opening followed by an independently selected
  opening with an applicable, possible interval. No automatic recalibration.
- The immediately preceding complete lap supplies directed interval timing.
  Two bounded banks keep the current partial lap separate. There is no average,
  older-lap substitution or PWM-derived position. Actual station departure is
  recorded separately from the preceding dwell.
- MM+2 missing-target stop with no invented advancement; unavailable timing
  evidence also stops navigation.
- Geographic identity reaches STOP OVERLAY without fabricated fine position.
  Mid-tile IR loss holds the command; the next selected Hall opening starts
  that tile's entry-to-entry PWM ramp over the previous lap's traversal time.
  Existing final braking, dwell and departure remain in their original executor.
- Bounded decision/provenance, authority-transition and resource diagnostics.
  Type-6 and morphology remain observation-only.

## Verification and review

[Current startup-repair report](../../../dart-field-validation/startup-repair/REPORT.md)
records the memory diagnosis, unchanged recorder capacities, verification and
remaining power-system check. The [initial deployment report](../../../dart-field-validation/STOP_GO_AND_OTTO_DEPLOYMENT_20261014.md)
preserves the original crash and R2 rollback.
[Operating decisions](../../../dart-field-validation/OPERATING_DECISIONS_20261014.md)
record the subsequent operator answers. [IR criteria and integration](../../../dart-field-validation/ir-credibility/STATUS.md)
retain the original criteria and their interpretation.

Run from the repository root:

```sh
sh firmware/programs/NAVI_EWO_0_5/run_tests.sh
python3 dart-field-validation/test_runtime.py
python3 dart-field-validation/test_transport.py
python3 dart-field-validation/run_replays.py
python3 dart-field-validation/check_regressions.py
```

Eight active host executables pass, including 223 policy, 85 DART and 410
integrated assertions. Extracted production motor/overlay and transport tests
pass 76 and 712 assertions. Six identical-input baseline/proposed replays pass
their assertions. Five inherited detector/window suites still fail, and one
first-increment invariant suite now fails its superseded no-withdrawal
expectation; those failures remain visible, not relabelled passes.

The additional word-storage test passes. The old/new production recorders emit
773 byte-identical records across four record types, including deliberate
overflow. Recorder capacities and all navigation/motor rules remain unchanged.

ESP32 core 3.3.12 compilation succeeds: 1,057,275 B program, 81,412 B static RAM.
The deployment build uses Otto’s existing private configuration; no credentials
are included in the repository.
Two explicitly labelled host-generated transition records reached the actual
Pi MQTT log. Subsequent hardware diagnosis identified ordinary-RAM exhaustion.
Relocating unchanged record rings to word-only internal memory and increasing
the loop stack resolved startup in the completed repair. See the current report
for actual board/recorder evidence and its stationary-test limitations.

## Manual STOP/GO

Both arrival-comparison and tile-ramp clocks pause during manual STOP and resume
on GO, retaining the selected MM, pending candidate and tile-ramp progress.
Raw Hall timestamps stay intact. Actual scheduled station dwell/departure still
counts; manual-stop duration is excluded from the next lap's timing reference.
The existing moved-while-stopped declaration latch remains authoritative.
David authorized the tested build to be flashed to USB-connected Otto and the
subsequent startup repair. The new clock rules are now installed. No motion
commands or field test were run by the agent.
