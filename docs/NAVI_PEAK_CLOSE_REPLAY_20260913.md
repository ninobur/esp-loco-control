# NAVI_ONE — replay of the operator's peak-relative close rule

**Date:** 2026-09-13
**Status:** Replay evidence only. Test-only code. **Nothing in the flashed
image, in `HallCapture.h`, or in any decision record is changed by this work.**
**Corpora:** Otto 2026-09-09 CW+CCW survey; Toby 2026-08-28 survey; the six
exact Northpoint waveforms of 2026-09-13.

## The proposal

Operator, 2026-09-13:

> When to close. 10 beats after a peak (downslope). … If I am on a bicycle and
> I stop on a hill, I know that if I stop on the upslope and start again, it is
> the same hill.

Close a passage when the signal has spent N consecutive milliseconds below its
**own running peak**, rather than when it falls within `exitMargin` of the live
baseline.

This matters because the current close test is the one mechanism in acquisition
that can be defeated by a contaminated reference. At Northpoint the reference
migrated while a passage was open; the signal then could not get within 25
counts of it; the passage stayed open for 1,200 and 2,446 ms and swallowed the
real magnets behind it. A peak-relative rule measures the passage against
`entryBaseline_`, frozen at open, so **no live reference participates in the
decision to close**.

## What the replay had to establish

Section G of `gate_cruise_ceiling.cpp` already shows the rule closes the latch.
That was never the question. The question is whether it damages the measurement
of ordinary magnets:

1. Does polarity survive?
2. Does peak amplitude survive?
3. **Does it ever split one physical magnet into two passages?** A split would
   double-advance position, which is worse than the fault being fixed.

`tests/replay_peak_close.cpp` runs the production capture and the proposed
capture side by side over every admitted, untruncated, PWM ≥ 70 record in both
surveys, with the duration floor disabled in both so that "no close" means no
close.

## Finding 1 — the naive rule shreds magnets

The rule as first written closes on the downslope while the signal is still far
above the entry margin, so the very next sample opens another passage:

```
passages per magnet, 3,132 Otto cruise magnets, no re-arm condition
  N=10 H=0    mean 4.80   max 7
  N=20 H=3    mean 2.57   max 4
  N=40 H=8    mean 1.52   max 2
```

The fixture test did not catch this because it computed only the first close.
The first replay did not catch it because the 82 ms floor was still enabled and
the fragments were being floor-rejected, which the program reported as "no
close". Both were my errors; both are corrected in the committed program.

## Finding 2 — with a re-arm condition, measurement is untouched

One added condition: after a peak-relative close, do not open again until the
signal has actually fallen below the entry margin once (`peakCloseLockout`).

```
Otto, 2026-09-09 survey, entry margin 70, floor disabled
rule           n     SPLITS  noclose  pol dif  peak dif | duration: min p5 med p95  under-82
N=10 H=0     3132      247        1        0        29  |  13   53   65   78   3073 (98.1%)
N=20 H=3     3132      247        1        0         0  |  23   68   80   93   1893 (60.5%)
N=20 H=8     3132      247        1        0         0  |  30   72   83   97   1339 (42.8%)
N=40 H=8     3132      124        1        0         0  |  50   92  103  117      5 ( 0.2%)
PRODUCTION   3131        -        -        -         -  |  67  115  133  156      1 ( 0.0%)

Toby, 2026-08-28 survey, entry margin 38, floor disabled
N=40 H=8      187        5        0        0         0  |  89  108  122  142      0 ( 0.0%)
PRODUCTION    187        -        -        -         -  | 117  126  153  188      0 ( 0.0%)

passages per magnet, N=40 H=8:  Otto mean 1.04 max 2;  Toby mean 1.03 max 2
```

**Zero polarity differences and zero peak differences at N ≥ 20**, across 3,132
Otto magnets and 187 Toby magnets. Closing after the apex still captures the
apex, and the signed sum still gives the right pole. The rule does not change
what a passage says about the magnet it crossed.

The 4% that produce a second passage produce it inside the 500 ms guard, where
it is refused `TOO_SOON`; position advances exactly once. That is the same
mechanism already relied on for post-stop successors, not a new one.

## Finding 3 — durations shorten, and the 82 ms floor is only just clear

The proposed rule ends the passage at the shoulder instead of at the tail, so
durations run about 23% shorter (Otto median 103 ms against production 133 ms).
Five of 3,132 Otto records fall under the current 82 ms floor; none of Toby's.

**The floor and the close rule are coupled.** Decision 0085's 82 ms was derived
against the production definition of duration and against Otto's one known
genuine 82 ms passage measured that way. If the close rule changes, that
derivation must be redone. It cannot be carried across by assumption.

## Finding 4 — the Northpoint latch does not survive the rule

```
record        kind              production        proposed (N=40 H=8)
LATCH_1200    latched plateau    1200 ms  1 ev     115 ms, then 155 ms
LATCH_2446    latched plateau    2492 ms  1 ev     175 ms, 159 ms, 103 ms
MM90_160      genuine arc         160 ms  1 ev     118 ms, 1 event
STRIKE_156    genuine arc         156 ms  1 ev     114 ms, 1 event
```

Genuine arcs give exactly one passage. The plateaus are cut into chunks instead
of one unbreakable block, so **acquisition re-arms every 115–175 ms instead of
never**, and the real magnets behind the disturbance get passages of their own.
That is the Northpoint failure mode removed, with no reference involved.

With the floor disabled the plateaus fragment into 10 and 6 chunks; with the
live 82 ms floor active, 2 and 3 of those reach recognition, and the successors
fall inside the 500 ms guard. The disturbance is therefore noisier on
`diag/acquisition` than today's single long event — it is the Grillers shape,
fragment followed by guard-refused fragment — but under decision 0080 a
fragment has no navigation authority and cannot stop the locomotive.

## Limits of this evidence

- This is **stored-passage replay against a reconstructed quiet line**. It
  tests segmentation and measurement. It is not acquisition coverage and it
  cannot stand in for a field run.
- The corpora are cruise records, PWM ≥ 70. Slow and stationary crossings are
  represented only by the Northpoint fixtures and the station fixtures, not by
  a survey population.
- N and the hysteresis are not derived from anything; N=40 H=8 is the corner of
  the sweep that happens to hold splits down and durations up. A real value
  would need a derivation of its own.
- Nothing here addresses **why the reference drifted**. The peak-relative rule
  makes the close decision independent of the reference; it does not make the
  reference correct. Baseline adaptation still ingests a sustained shelf with
  no amplitude, duration, or shape screening.

## Files

- `firmware/programs/NAVI_ONE/variants/NAVI_ONE/tests/replay_peak_close.cpp` — the replay.
- `firmware/programs/NAVI_ONE/variants/NAVI_ONE/tests/HallCaptureCeiling.h` — `peakCloseN`,
  `peakCloseHyst`, `peakCloseLockout`, all defaulting to disabled. Test-only;
  not included by the sketch and not in `run_tests.sh`.

Reproduce:

```
g++ -O2 -std=c++17 -o /tmp/rpc tests/replay_peak_close.cpp
/tmp/rpc 70 otto_cw.log otto_ccw.log     # Otto, entry margin 70
/tmp/rpc 38 toby.log                     # Toby, entry margin 38
```

## References

- `docs/NAVI_CRUISE_CEILING_REPLAY_20260913.md`
- `docs/NAVI_OTTO_20260913_INVESTIGATION_SUMMARY.md`
- `field-records/20260913_NORTHPOINT_ACQUISITION_LATCH.md`
- Decision 0080 — morphology has no navigation authority
- Decision 0081 — unconditional 500 ms guard
- Decision 0085 — experimental 82 ms completed-passage floor
