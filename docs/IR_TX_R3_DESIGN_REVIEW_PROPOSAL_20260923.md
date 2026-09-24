# IR Stop Retention: Codex Diagnosis and Proposed R3 Boundary

Date: 2026-09-23. Status: **proposed design, not implemented firmware**.
Operator authorized parallel investigation while Claude reviews. Installed R2
and all production detector/NAVI sources remain unchanged. No hardware access,
flash, live-service changes or motor operations in this investigation.

## Findings

### High: live-quality failure destroys the reference needed to assess a later stop

R2 `IrMovementDetector.h` clears `haveCompleted_`, `proven_` and `candidate_`
when live span falls below 300. It subsequently permits quiet retention only
below 120 AND with `proven_` still true. A smooth decay through the intervening
span therefore prevents retention even if the final level is compatible with
the earlier optical reference. Breaking distance continuity is appropriate;
automatically deleting the optical reference is a separate decision.

New `tools/test_ir_stop_decay.cpp` executes the actual Measurement, wire/CRC,
health/epoch and speed implementation, not a Python detector transcription:

| Input, then 2 s quiet hold | R2 outcome |
|---|---|
| 1000/2000 square wave -> 1200 | valid zero, no abort, old MM reference still valid |
| Smooth 1000-2000 waveform -> 1200 | unavailable, two aborts, old MM invalid |
| Smooth waveform -> 1000 | valid zero |
| Smooth waveform -> 2000 | valid zero |
| Smooth waveform -> 1500 | unavailable, old MM invalid (expected) |

The smooth wave is a deterministic 200 ms cosine cycle over 4 seconds. This
minimal counterexample uses a transition to 1200, not a physical deceleration
simulation. At the loss, the scratch state trace gives span 143 and reference
1008/1999; 1200 is within the existing quarter-span dark-level tolerance.
Thus waveform shape changes the outcome even for an admitted resting level.
This is a regression target, not a claim that arbitrary quiet levels imply
physical standstill.

Default execution of the new test deliberately exits 1 with ONE desired-
behavior failure on R2. `--expect-r2-failure` verifies exactly this baseline
and exits 0; that mode is explicitly NOT an acceptance pass. A revised build
must pass normal mode and the independent adversarial suite.

### Important qualification: the three physical pauses are not all proven valid stops

The previous bench report correctly records three unavailable pauses, but
those observations alone do not establish that all three should be READY.
Diagnostic replay of their recorded ADC samples yields these states just
before loss of the optical reference:

| Replay sample | Raw | Live envelope | Prior reference | Interpretation under current tolerance |
|---|---|---|---|---|
| 2226422 | 1948 | 1856/2095 | 800/2047 | Near learned bright level |
| 2242082 | 1589 | 1584/1711 | 848/2079 | Between admitted plateau bands |
| 2263746 | 1620 | 1616/1743 | 816/2047 | Between admitted plateau bands |

The first supports the reference-loss diagnosis near a legitimate plateau.
The latter two may properly remain unavailable under the existing optical
admission rule. Their physical spoke alignment is still unknown. Do NOT make
"all three must show zero" an acceptance test or use PWM0 to admit them.

### Medium: reference refresh can follow a collapsing envelope

The first short-roll replay had already narrowed its reference to 864/1231
before loss, versus the preceding much wider rolling modulation. Each completed
cycle currently may refresh against the prior reference OR prior candidate;
incrementally compatible changes can ratchet the reference. Its tolerance then
shrinks, making single ADC excursions more consequential. This needs review
alongside the observed outliers, not an unmeasured expansion of tolerances.

## Replay Integrity

Added reproducible tools:

- `tools/ir_raw_replay_input.py`: reads plain/gzip Pi logs, selects SID, checks
  both packet CRC layers and structure, emits recorded ADC/flag samples with
  their indices. Rejects duplicate/out-of-order input; reports missing samples.
  No interpolation, smoothing or invented movement truth.
- `tools/replay_ir_detector_trace.cpp`: compiles the actual detector, exposes
  its private state in a host-only translation unit, prints reference/fault
  transitions and disagreement with recorded flags. No production changes.
- `tools/test_ir_raw_replay_input.py`: three passing offline tests covering
  plain/gzip input, missing data, inner-CRC corruption and duplicate rejection.

Three-stop capture: 1766 valid raw packets, zero rejected corrupt packets,
18 missing ranges / 1920 missing samples. Replay uses idealized index * 1 ms;
missing radio samples become synthetic sampling gaps, unlike the live detector.
In sample range 2225000-2270000, 272 of 44713 flag words differ. Private state
values in the table are REPLAY INFERENCES, not transmitted firmware state.
The separate recorded type-5 quality/counter transitions remain primary evidence.
For example the replay gap at 2240832 is NOT a physical sampler fault.

## Proposed State Separation

1. **Optical reference:** levels learned from compatible complete modulation,
   with provenance. Quiet input may validate compatibility, never teach a new
   reference or turn a flat cold boot into a measurement. Decide how to prevent
   refresh from chasing the final partial excursion of a stop.
2. **Pulse phase:** whether low/high/open state is actually known. An ambiguous
   interval cannot silently preserve an open pulse and later credit its tail
   as a complete wheel pitch. Re-establish phase without inventing distance.
3. **Measurement continuity:** monotonic fault evidence ends the old epoch.
   Keeping an optical reference in memory must NOT preserve or reopen that
   epoch or its MM association.

Candidate recovery policy for independent review: after a quality loss, retain
the prior optical reference as quarantined calibration, but mark measurement
unavailable and discard uncertain phase. A later fresh, settled quiet window
near a trusted plateau may support a NEW measurement epoch, with a new origin
and no inferred distance over the blind interval. First new speed endpoint
is WARMUP, not zero; only a later same-epoch endpoint can yield zero. NAVI must
establish a new MM reference independently. This is not restoration of the old
route association or proof that motion did not occur during the outage.

The exact qualification window and spike policy are undecided. Existing
512-sample and quarter-span values are engineering parameters, not measured
error bounds. Sample gaps, saturation, electrical/optical changes and a new boot
need explicit reference-discard/quarantine rules. A plateau-like obstruction
remains indistinguishable from standstill with one channel. Midband ambiguity
and insufficient slow-crawl contrast must remain unavailable, never forced zero.

## Required Tests Before a Candidate Flash

- Smooth-wave stop counterexample; clean dark/bright holds and restart.
- Captured near-bright stop replay, with missing-data and timing uncertainty
  stated. Do not force the two intermediate-level physical pauses to pass.
- Moving compressed contrast, lighting steps, outliers during movement and
  silence. No return of R1's sustained false zero or invisible pulse loss.
- Every continuity break invalidates old MM reference, including when bad
  reports are lost; quiet recovery never reuses an old epoch.
- If phase was lost, restart cannot credit a partial high-to-low tail as a
  complete pitch in the new epoch. No count repair inferred from PWM/Hall.
- Cold flat/noise, mid-edge stops, frozen captures, radio staleness, reboot,
  saturation and ADC sample gaps remain distinguishable from measured zero.
- Default detector users remain unchanged; old/new NAV consumer semantics are
  a separate architecture issue. Inherited 23/25 illumination result stays open.

## Reproduction

From repository root:

```sh
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Ifirmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH tools/test_ir_stop_decay.cpp -o /tmp/ir_stop_decay
/tmp/ir_stop_decay
/tmp/ir_stop_decay --expect-r2-failure
python3 -B tools/test_ir_raw_replay_input.py
python3 -B tools/ir_raw_replay_input.py field-records/logs/20260923_ir_r2_three_stops_radio.log.gz --sid e9f8b7c4 > /tmp/ir_samples.txt
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Ifirmware/common tools/replay_ir_detector_trace.cpp -o /tmp/ir_trace
/tmp/ir_trace 2225000 2270000 < /tmp/ir_samples.txt
```

The second command fails by design on R2; do not suppress that in a future
release gate. Host ASan/UBSan runs completed without sanitizer errors.

## Next Decision

Review this separation with Claude/SAM before changing production detector
code. The operator has supplied useful physical evidence; no additional rolling
is necessary to establish this regression. No R3 binary or field approval is
claimed. Physical validation remains necessary after any reviewed implementation.
