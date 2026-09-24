# IR TX 1.6 R2: Response to Independent Review

Date: 2026-09-23. Author: Codex. Status: revised candidate, compiled and
host-tested, NOT FLASHED. Independent re-review and physical bench acceptance
pending. No USB, Pi, motor, dashboard deployment or AUTO operations performed
for this revision. The operator's decision to wait for review remains in force.

## Evidence and Disposition

Starting point: candidate `1de06ae`, independent review/test commit `4de40df`,
branch `agent/toby-1-13-flash`. Reproduced all five hard failures before editing.
The additional operator-supplied review also identified continuity visibility
and older/newer consumer disagreement. Claude's reproducible high-severity
findings control the no-flash disposition, regardless of a more permissive
bench recommendation in the other review.

Canonical sketch remains
`firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX/IR_SCOPE_ESPNOW_TX.ino`.
Both READY and MOVE now identify 1.6 R2. No RX reflash or topic/packet changes.
The older NGR-Files Arduino TX folder is unchanged; reconcile it with a backup
before a later approved IDE flash. No second sketch path was created.

## Changes and Reasons

- H1: retention applies only when the live percentile span is below 120.
  Otherwise thresholds follow the live envelope. A quiet signal must be near
  either learned plateau (quarter-span margin), not just inside the outer
  envelope. Live span below 300 remains inadequate, not a claimed stop.
- H2: in retention mode, invalidation of armed/open/proven state increments
  `openAborts`, which both continuity consumers already check. Also record
  loss of completed-measurement readiness on inadequate live contrast, even
  if the bad report never reaches the receiver. Avoid double-counting the
  existing open-pulse timeout. Clear candidate/reference on invalidation.
- M1: first retention requires two consecutive compatible completed-cycle
  envelopes, each with span at least 300. Later refresh requires agreement
  with the proven reference or the previous completed-cycle envelope, as in
  the review prototype. A single completion cannot establish cold retention.
- H3: no NAV logic was changed. The pathological usable 0 mm interval under
  the reviewed lighting case now measures 48.3 mm. This is NOT a claim that
  TX changes are observation-only: NAVI consumes this evidence and may react.
- L1/L2: both serial version labels corrected; documentation below records
  diagnostic counter meaning. Default detector mode remains unchanged.

The constants 120, 300 and quarter-span tolerance remain engineering choices,
not measured optical error bounds. No PWM or command state is used to infer
stationarity in the detector. No pulses are synthesized or repaired here.

## Verification

All listed C++ host tests passed with AddressSanitizer and UndefinedBehaviorSanitizer:

- Existing movement, phase-retention, movement-contract, wire, stationary
  and stationary-pipeline tests. Both 120-second plateau holds keep counts
  and epoch stable; restart resumes measured motion.
- Independent `tools/test_ir_stationary_adversarial.cpp`: all hard cases
  pass, versus five failures reproduced on the original candidate. Moving
  glitch cases lose zero pulses in 34 trials; compressed contrast gives zero
  valid-zero speed windows in 10 seconds; NAV interval is 48.3/48.3 mm.
- New `tools/test_ir_stationary_revision.cpp`: one cycle cannot establish
  retention, two agreeing cycles can; amplitude, midband, compressed-contrast
  and saturation faults withheld from radio still break the epoch and old MM
  reference on recovery. Continued flat invalid input does not repeatedly
  count the same discarded state. A full stopped sampling interval gives zero;
  a new radio sequence carrying a frozen capture timestamp gives NO_NEW_SAMPLE,
  then LINK_STALE, never an indefinitely held valid speed.
- New `tools/test_ir_default_equivalence.cpp`: 4,000,000 differential samples
  against `1de06ae^:firmware/common/IrMovementDetector.h`, both retainPhase
  modes, retention disabled. No mismatch in counts, flags, phase, reason or
  envelope. This is a separate check from Claude's larger original run.
- NAVI 0.6 health, speed, coherence (2,052 corrections / 6,840 holds), station
  correction (32 approaches), proximal recovery tests; 13 actual health JSON
  payloads parsed (638 bytes maximum). Dashboard's three offline tests and
  14 JavaScript validity/conversion assertions pass. No dashboard edits.

ESP32 compile: `arduino-cli compile --fqbn esp32:esp32:esp32 --build-path
/private/tmp/ir-tx-1-6-r2-build firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX`
(run as one command), installed core 3.3.11. Flash 901,412 bytes, static RAM
64,288 bytes. This incremental compile emitted no warnings; no claim that
the entire project is warning-free. No upload performed.

## Remaining Limits and Review Questions

1. The dark-level rise test STILL counts 23/25 real cycles during adaptation,
   exactly like default 1.5. One overlapping one-second window conceals at
   least two missed cycles with ready endpoints and unchanged fault counters.
   Passing the review's no-regression gate does not solve this inherited
   weakness or establish trustworthy distance across arbitrary lighting steps.
   Review whether field use needs an additional envelope-change continuity
   fence; this revision does not silently introduce one.
2. Very slow cold acquisition remains unavailable (review sweep: slowest
   acquiring half-period 480 ms, about 10.1 mm/s). Gradual live edges can also
   cross inadequate span and break the epoch. No false readiness workaround.
3. Flat electrical failure or obstruction at a learned level is indistinguishable
   from a stopped wheel. Stationary noise and ADC-glitch rate are unmeasured.
   A single quiet-plateau outlier can revoke retention until reacquisition.
4. Older MovementEvidence requires TRACKING and unchanged unreliableSamples;
   new health/epoch accepts quiet SIGNAL_STALE and excludes unreliableSamples.
   Stop-spanning NAV intervals therefore remain unavailable. No consumer
   unification is claimed. `unreliableSamples` semantics are unchanged.
5. `openAborts` now includes discarded continuity and live-quality loss in
   opt-in retention mode, not only timed-out high pulses. Legacy `latch`
   mirrors aborts; `contrastLoss` changes because compatible stops no longer
   count as inadequate contrast. Compare 1.5/1.6 diagnostics accordingly.
6. A one-second pulse-delta speed can quantize to zero before the next pulse
   at sufficiently slow travel. Distinguish that resolution limit from the
   sustained false zero demonstrated at five cycles/second in the review.

## Additional Replay Wildcard

Operator supplied Claude's replay-transcription review during this revision.
Fetched branch `claude/replay-noise-transcription-sync-h62oob`, inspected commit
`6e7ccc2`, but did not merge it: its file paths predate repository organization
and its test concerns IR_SPEED_LOCAL, not the current IR_SCOPE_ESPNOW_TX.
Ran the updated Python test in scratch against the current IR_SPEED_LOCAL
source: fingerprint/constants check and all four scenarios pass. Noise: 1,167
counted pulses but zero valid samples; flat: zero pulses/valid samples; real
spoke train: 115 pulses / 6,909 valid samples; recovery: 745 ms after transition.
This confirms the stated verdicts, not universal physical accuracy.

The warning is important: a VALID classification can outlive the measurement
it describes. IR_SPEED_LOCAL uses a 2,048 ms envelope and retains a median
interval speed until a new publication or invalidity. Toby's current display
instead uses the 512-sample common detector, type-5 captures, and timestamped
cumulative-count differences. Measurement updates capturedUs per ADC sample;
the receiver rejects non-advancing captures, applies a one-second freshness
watchdog, and speed refuses duplicate endpoints. It does not reuse the older
median-speed value. A partially moving one-second window can still show a
nonzero average briefly after a stop; dashboard/network cadence adds latency.

Added a compiled C++ frozen-capture regression rather than another Python
transcription. The current detector/pipeline tests include actual firmware
headers, so Python source-copy drift does not affect those tests. They remain
synthetic and do not cover ESP32 scheduling, physical noise or all optical
waveforms. The inherited lighting-step undercount above remains open.

## Next Gate

Ask Claude/SAM to review the actual R2 diff, particularly the added live-quality
counter path, two-cycle learning, and remaining lighting-step undercount.
Only after approval and operator authorization: supervised IR-car-only bench,
following the ten checks in the independent review. Keep Toby motor power off;
no NAV/AUTO field authorization follows from a compile or bench pass.

Record counts, reason, epoch, reference, raw waveform/noise and speed during
low/high stops, mid-edge stop, restart, slow roll, light changes and reboot.
Separate measured zero, unavailable data and pulse-resolution effects.
