# IR TX 1.6 Stop Retention: Independent Review

Date: 2026-09-23
Reviewed: `agent/toby-1-13-flash` at `1de06ae` (handoff:
`docs/IR_TX_1_6_STOP_RETENTION_REVIEW_20260923.md`).
Reviewer: Claude (independent review requested by the operator). No hardware was
flashed, opened or captured.

## Verdict

**Not ready for a bench flash as written.** The idea behind it is sound: a stationary
wheel should read as a valid zero, and a missing measurement must not. The
implementation, though, fixes the stop case by freezing thresholds for **all**
retained operation, including while the wheel is turning. As a result, a real optical change
during motion can be reported as a measurement-ready zero. Three changes (below)
must land first. A scratch prototype of those changes passes every existing test
and every new adversarial case. With them in place, a supervised stationary or
hand-roll bench test is reasonable.

Bench approval is a separate question from navigation or AUTO acceptance. Neither this review nor
any bench result authorizes AUTO or NAV field use of 1.6.

## Findings

### H1 (High): frozen thresholds report a turning wheel as a valid zero

`firmware/common/IrMovementDetector.h:50-56, 69`. Once `proven_` is set,
`holding` is true whether or not the live window shows contrast. Thresholds stop
following the live envelope (`contrast && !holding`), and they refresh only
on a completed cycle. The only amplitude guards are:

- the **outer** envelope check (`:42`): raw inside `[provenLow-m, provenHigh+m]`
- the flat mid-band check (`:46`), which applies only when live span < 120.

Neither check asks whether the signal still crosses the frozen thresholds. With a
learned 1000/2000 reference (thresholds 1333/1667, margin 250):

| Case (wheel turning 5 cycles/s, 5 s) | 1.6 candidate | Default 1.5 detector |
|---|---|---|
| Dark level rises to 1400 (ambient/sun), bright 2100 | **0 of 25 counted**, TRACKING for 2.5 s, then SIGNAL_STALE (ready) indefinitely | 23 of 25, TRACKING |
| Contrast compresses to 1400↔1600 (span 200) | **0 of 25**, SIGNAL_STALE (ready) | 23 of 25, INADEQUATE_CONTRAST (unavailable) |
| 110-count ripple at the dark level | 0, SIGNAL_STALE | n/a (depends on noise floor) |

The second row is the exact failure the operator ruled out: an unavailable
measurement shown as zero. The 1.5 detector reported it as unavailable; 1.6
reports it as a healthy stop. Downstream effects, all reproduced:

- **Epoch/speed** (`IrHealthMonitor` → `IrOdometryEpoch` → `IrSpeedTelemetry`):
  SIGNAL_STALE is `Ready` (`IrInstrument.h:64-66`). No epoch-checked counter
  changes, so the epoch continues and the dashboard shows **valid 0 pKPH for 10 of
  10 s** while the wheel turns 5 cycles/s. `IrSpeedQualification`
  (`IrSpeedTelemetry.h:55-65`) catches this only when `actualPwm>0` for 3 s, or
  on a NAV advance. That leaves unpowered rolling unprotected: coasting, and
  the hand-roll bench test itself.
- **Navigation** (see H3): the first 2.5 s are TRACKING→TRACKING with no pulses.

### H2 (High): brief revocations are invisible to epoch continuity

`IrMovementDetector.h:42-48, 80`; `IrOdometryEpoch.h:155-169`. The new revocation
paths (amplitude, mid-band) call `invalidate()` but increment no counter that
`IrOdometryEpoch::discontinuity` checks. `unreliableSamples` does increment, but
the epoch deliberately ignores it, because SIGNAL_STALE increments it too. Saturation and
sample gaps are fine: their cumulative counters break the epoch whatever the
snapshot timing or radio loss.

A single non-saturating sample above `provenHigh+margin` while the wheel is turning
drops the open pulse. The detector reacquires within one or two cycles. The
epoch notices only if a 10 Hz snapshot happens to catch REACQUIRING, and only
if that packet survives the radio (`IR_SCOPE_ESPNOW_TX.ino:172-177, 198-199`).
Result: **34 of 34 trials lost a pulse; in 19 of them every snapshot was ready
and no epoch-checked counter changed.** The 1.5 detector never revokes on
amplitude: 0 of 34 lost.

This is a regression while the wheel is turning, not only at stops. Single-conversion ADC
transients are a known ESP32 behaviour: Toby's Hall ADC showed about 1.1 per 1000
(see the `transients-are-not-electrical` record). The IR car's rate has not been
measured. The same glitch at a stop revokes retention until the next motion. That
outcome is truthful, but it means one bad conversion defeats the feature.

### H3 (High): NAV behaviour can change; it is not observation-only

`Navigator.h:57-62`, `MovementEvidence.h:39-53`. When the older `between()`
interval is usable, a Hall event is admitted only if IR distance falls inside
±15% of a mapped span. Otherwise it is **refused** (`reject()`). Under H1,
two TRACKING points 1 s apart give a usable interval of **0.0 mm against 48.3 mm of
real travel**. That refuses a genuine landmark. The next Hall event, whose
interval is by then unusable, falls back to `s_.target`: the marker that
was just refused. NAV then runs one marker behind. The 1.5 detector counts
through the same lighting step.

What does not change: stops. Before, a stop reported INADEQUATE_CONTRAST;
now it reports SIGNAL_STALE. Both are non-TRACKING and both increment
`unreliableSamples`, so NAV intervals that span a stop remain unusable, as before.
After a restart, 1.6 returns to TRACKING one completion earlier than 1.5. That is benign.

### M1 (Medium): one cycle is enough to earn retention

`IrMovementDetector.h:64-67`. One completion with live span ≥ 300 sets
`proven_`. A single shadow, hand pass or glint therefore teaches the plateaus that
later stops are judged against. Retention should require at least two consecutive
completions whose envelopes agree within the margin, or agreement with the
existing reference. The same rule stops a completion caused by a partial transition from
re-teaching the reference once thresholds follow the live envelope again (see
Fix 1). The `midband_stop` case below needed this rule in the prototype.

### L1 (Low): legacy diagnostic counters change meaning

`IR_SCOPE_ESPNOW_TX.ino:163-165`. `latch` (aborts) and `contrastLoss` fall at
stops, because high stops no longer abort and stops no longer enter
INADEQUATE_CONTRAST. The only consumers are the diagnostic analyzers
(`tools/ir_diag_analyze.py`, `tools/ir_scope_espnow_analyze.py`). Compare
1.5 and 1.6 captures with that in mind.

### L2 (Low): serial label still says 1.5

`IR_SCOPE_ESPNOW_TX.ino:237` prints `MOVE TX_1_5`, while READY says 1.6. A bench
log would mislabel its own build. Fix it before flashing.

## Answers to the Specific Questions

1. **Is one cycle sufficient?** No. See M1. Require two agreeing completions.
2. **Can the refresh or margin conceal an optical change?** Yes: H1. The margin bounds only
   the outer envelope, and frozen thresholds drop pulses without revoking.
   Refresh-on-completion cannot help once completions stop. On idealized square
   waves, no pulses were invented.
3. **Are long stops and restart counted correctly?** Yes, for clean plateaus. Both 120 s holds and restarts
   are exact in the supplied tests, and I reproduced them. A high stop completes its pulse on the
   first fall after restart, which is physically correct.
4. **Do faults reliably end continuity?** Saturation and sampling gaps do, because they are
   cumulative counters. Amplitude and mid-band revocations do not (H2).
5. **Slow startup, mid-transition stops, sunlight:**
   - Slow startup stays unavailable and never reads zero. The slowest cold
     acquisition is a 480 ms half-period, about 10 mm/s or 1.9 pKPH.
   - A mid-transition stop is truthful. It reports ready for about 0.5 s while
     the window collapses, then INADEQUATE_CONTRAST, and the epoch breaks. The
     straddling pulse is lost.
   - Sunlight changes fail (H1).
6. **Could NAVI behaviour change?** Yes (H3). TX-side does not mean observation-only.
7. **Is default behaviour unchanged?** Yes. A differential run over 72,278,210 randomized samples compared
   the pre-change detector (`1de06ae^`) with the new detector with
   `retainStationary=false`, for both `retainPhase` values: 0 mismatches in counts,
   reason, envelope, flags or phase. `Measurement`'s new argument defaults to false.

## Required Changes Before Bench

1. **Retain only while the window is quiet.** Use
   `holding = retainStationary && proven && !contrast`. With live contrast,
   use live thresholds and the 1.5 reason logic. While holding, require raw to be
   within the margin of **one learned plateau**, not merely inside the outer envelope.
2. **Make every loss of continuity epoch-visible.** In retain mode, any
   `invalidate()` that destroys armed, open or proven state must increment
   a counter that `IrOdometryEpoch` checks. The prototype uses `openAborts`, so the
   wire format is unchanged. Brief faults, and faults whose snapshot is lost over
   the radio, then still end the epoch.
3. **Earn and refresh the reference with two agreeing completions** (M1).
4. Fix L2. Keep `tools/test_ir_stationary_adversarial.cpp` in the gate: it fails on
   `1de06ae` (5 hard failures) and must pass on the revised build.

A 70-line scratch prototype of changes 1–3 was checked; it was **not applied to
the branch**, because the author owns the patch. Results:

- All existing tests pass: detector, phase retention, contract, stationary,
  pipeline, and NAVI 0.6 health, speed and coherence.
- All adversarial hard cases pass.
- Default mode is still bit-identical.

Trade-off to review with the revision: with live thresholds, a slow gradual edge
passes through a 120–300 span. It reads as INADEQUATE_CONTRAST, which breaks the
epoch during a very slow crawl, as 1.5 already does. Choosing unavailability there is
deliberate. It must not be "fixed" by reporting it as ready.

The prototype's core change:

```diff
-    if (retainStationary_ && proven_) {
+    const bool contrast=high-low>=120;
+    const bool holding=retainStationary_ && proven_ && !contrast;
+    if (holding) {
       const unsigned margin=(provenHigh_-provenLow_)/4;
-      if (raw+margin<provenLow_ || raw>provenHigh_+margin) { ...revoke }
-      if (high-low<120 && raw>=thresholdLow_ && raw<=thresholdHigh_) { ...revoke }
+      const bool nearLow=raw+margin>=provenLow_ && raw<=provenLow_+margin;
+      const bool nearHigh=raw+margin>=provenHigh_ && raw<=provenHigh_+margin;
+      if (!nearLow && !nearHigh) { invalidate(); reason=INADEQUATE_CONTRAST; return; }
     }
-    if (contrast && !holding) { thresholds from live envelope }
+    if (contrast) { thresholds from live envelope }
     ...
-      if(retainStationary_ && high-low>=300) { adopt proven + thresholds }
+      if(retainStationary_ && high-low>=300) { adopt only if compatible with
+        proven or with the previous completion's envelope; remember candidate }
-    if (retainStationary_ && proven_ && high-low<300) reason=SIGNAL_STALE;
+    if (holding) reason=SIGNAL_STALE;
   void invalidate() {
+    if(retainStationary_ && (armed_||open_||proven_)) ++aborts;
```

## Physical Acceptance Checks (bench only; revised build)

These checks need operator supervision, the IR car only, a 115200 serial monitor
owned by the operator, and no AUTO. For each step, record the raw `MOVE` line,
health, epoch id/active/break reason, and dashboard IR speed.

1. **Boot:** READY and MOVE both say 1.6. Stationary from cold must read
   unavailable, never 0.
2. **Acquire:** roll by hand through at least 5 marks. Confirm that completed counts
   match the marks counted by eye.
3. **Stops:** stop on a dark mark and on a bright mark for 60 s each. Each must read
   valid 0 with the same epoch and no count growth. Restart: the counts continue
   exactly and the epoch does not change.
4. **Stop mid-edge:** park the wheel half-over a transition. It must read
   unavailable within about 1 s and the epoch must break. It must not read valid 0.
5. **Noise floor:** at each stop, log the stationary peak-to-peak. Retention is
   defensible only if it sits well below the 120-count gate. Also record any
   single-sample glitches per minute.
6. **Very slow crawl** (one mark every 2–4 s by hand). Unavailable or
   measured are both acceptable. Valid 0 while moving is a failure.
7. **Optical disturbances, stationary and rolling:** a hand shadow, a torch or
   sunlight on the sensor, and the sensor lifted or tilted. While rolling, each
   disturbance must either keep counting correctly or go unavailable with an epoch
   break. **A valid 0 while rolling is a failure.**
8. **Obstruction** (card) at a learned level: record the result. This
   is the known one-channel limit (it can read as stationary). It must not be
   described as detected.
9. **Power-cycle the IR car:** new boot id, epoch break, reacquire, never
   inherited retention.
10. **Toby paired, PWM 0, Manual:** confirm NAV Hall admission is unchanged at
    stops. Look at the `ir_interval`/refusal fields. Any refusal attributable to IR
    stops the bench.

Passing the bench makes the build a candidate for a reviewed field trial. It does not
make 1.6 acceptable for NAV or AUTO. Those need their own run under
the existing field-acceptance process, with Hall/IR agreement reviewed.

## Reproduction

```
c++ -std=c++17 -fsanitize=address,undefined -Ifirmware/common \
  -Ifirmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH \
  tools/test_ir_stationary_adversarial.cpp -o /tmp/ir_adv && /tmp/ir_adv
```

At `1de06ae` the run exits 1 with the following:

- FAIL: `dark_level_rise_vs_default` (40 concealed one-second windows against 1),
  `contrast_compression_moving`, `glitch_while_moving` (19/34),
  `nav_interval_undercount` (0.0 against 48.3 mm), `speed_valid_zero_while_turning` (10/10 s).
- PASS: `slow_cold_start_unavailable`, `midband_stop`.

I re-ran the existing tests at `1de06ae` under ASan/UBSan and all pass:
`test_ir_stationary`, `test_ir_stationary_pipeline`, `test_ir_movement`,
`test_ir_phase_retention`, `test_ir_movement_contract`, `test_ir_movement_wire`,
and NAVI 0.6 `test_ir_health_monitor`, `test_ir_speed` and `test_coherence`.
Those pass results are consistent with the findings above: none of the tests
exercises optical change while the wheel is turning. I did not repeat the ESP32 compile.
