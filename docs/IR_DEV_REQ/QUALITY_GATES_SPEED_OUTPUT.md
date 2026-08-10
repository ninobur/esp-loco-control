# Signal quality must gate the speed output, not label it

**Date:** August 6, 2026
**Status:** Requirement — accepted; implemented in the IR_SPEED_LOCAL 1.2
prototype, pending independent review, field gate, and production integration
**Applies to:** any IR speed output consumed by a governor, fusion layer, or
mission logic. Not a constraint on the diagnostic sketches' human-readable text.
**Source observation:** `ir_daylight_20260806_124221.log`, early windows, car
stationary.

## The requirement

`MARGINAL` and `UNAVAILABLE` must **suppress** the speed output. The payload
publishes `speed_valid: false` together with the state name, and no speed value.

The sensor declines to answer rather than answering wrongly. A downstream
consumer must not be able to ignore the qualifier by accident: it must be
structurally impossible to read a speed number out of an untrustworthy reading,
rather than merely inadvisable.

Publishing a speed value accompanied by a qualifier is not sufficient, because
it places the burden of correctness on every consumer, forever, and fails
silently the first time one of them does not carry it.

## The observation

Early in the 2026-08-06 shade baseline run, with the car standing still:

```
STATS 10s  n=340 rate=34.0/s | int med=20 min=15 max=39 | peak med=63
           | sat=0 miss=21 latch=0 closs=1 | ~624mm/s ~116.1pkph
PULSE ...  base=79 span=159  MARGINAL
```

Every pulse in that period was correctly labelled `MARGINAL`. `span` was 159
counts of 4095 and `peak med` was 63 — essentially no signal. The percentile
envelope had collapsed onto the noise floor exactly as designed, and the
detector was triggering on ADC noise.

The same STATS line reported **~624 mm/s / ~116.1 pkph, with the car standing
still**.

The label was right and the number was fiction, two fields apart on one line.

### The noise signature

`int min=15` in that window is exactly `DEBOUNCE_US` (15000 µs). An interval
distribution pressed against the debounce floor means edges are firing as fast
as the guard permits — the signature of noise, not of spokes. Real spoke
intervals in the same run were 63–78 ms.

This is worth keeping as a detection heuristic in its own right: a minimum
interval equal to the debounce constant is not a measurement, it is the guard
reporting how often it was asked.

### Why the detector was running at all

Verified against source, not inferred:

| Constant | Value | Effect on `span=159` |
|---|---:|---|
| `MIN_USABLE_SPAN` | 120 | 159 > 120 — detector does **not** halt, edges are emitted |
| `MARGINAL_SPAN` | 300 | 159 < 300 — quality is set to `MARGINAL` |

Both sketches carry the same pair (`IR_TEST.ino:289-290`,
`IR_DIAG.ino:194-195`). The band `120 <= span < 300` is therefore the region
where the firmware emits edges that it simultaneously declares untrustworthy.
That band is where this defect lives. The observed `span=159` sits squarely
inside it.

## Current firmware state — the precise gap

In `IR_TEST.ino`, speed validity and signal quality are computed on two
**independent** axes, and only the first one reaches the output:

```c
if (intervalsFilled >= SPEED_MEDIAN_N) speedState = SS_VALID;
bool haveEstimate = (intervalsFilled >= SPEED_PROVISIONAL_N);
bool speedValid   = (speedState == SS_VALID);
```

`speedValid` depends solely on **how many intervals have been collected**
(`SPEED_MEDIAN_N = 5`, `SPEED_PROVISIONAL_N = 3`). It does not read
`e.quality` at all. Quality is carried into the payload as a descriptive
string beside the number:

```c
"\"state\":\"%s\",\"speed_valid\":%d,%s,\"raw\":%d,\"span\":%d,\"quality\":\"%s\", ...
```

The consequence is direct: at the observed 34 edges/s, five noise intervals
accumulate in roughly **150 ms**. A stationary wheel in collapsed contrast
therefore reaches `state: "VALID"`, `speed_valid: 1` and a plausible non-zero
`speed_mmps` within a fifth of a second, with `quality: "MARGINAL"` sitting
alongside carrying no authority whatsoever.

The state machine answers "do I have enough intervals to compute a median?"
The quality field answers "is any of this real?" Only the first currently
gates the output. The requirement is that both must.

## What must not change

The requirement is about the **speed output**, not the detector.

`IR_TEST.ino` deliberately halts edge detection on the span floor only, and
explicitly not on quality:

> NOTE this halt is on the SPAN FLOOR only, deliberately NOT on quality — the
> blind-while-moving case also reads quality 0, but halting there would stop
> edge detection during exactly the recovery the decay is driving.

That reasoning is correct and this requirement does not disturb it. Detection
must keep running through poor quality so the envelope can adapt and recover
(decision 0006). What must stop is the *publication of a speed number* derived
from it. Suppressing the answer and suppressing the measurement are different
actions; conflating them would re-create the reacquisition deadlock.

## Why this is a requirement and not a bug report

This is the **third** instance of one failure shape. It is recorded here as a
design principle rather than three entries on a defect list because the
individual fixes have not generalised.

1. **2026-08-05, 14:00 run.** 102 status beats reported `quality:"OK"` through
   479 seconds during which the sensor produced no edges at all while the car
   was under power. Cross-referenced against the towing locomotive's PWM and
   against the pulse counter, which did not advance.
2. **Post-dwell recovery.** 24% of published pulses reported
   `speed_mmps: 0.00` while under power — a value indistinguishable from a
   real stop.
3. **This observation.** A plausible non-zero speed published from pure noise,
   with the correct quality label sitting beside it.

In all three the firmware knew, or could have known, that its output was
untrustworthy, and published a number anyway. Instances 1 and 2 have been
addressed individually — the honest-quality work and the `null`-instead-of-zero
change. Instance 3 shows the shape surviving both fixes, because each addressed
a particular wrong value rather than the structural question of whether a value
should be emitted at all.

The generalisation, stated as the principle: **an output the sensor cannot
vouch for must be absent, not annotated.**

## The same log produced the best result on record

The 2026-08-06 shade baseline is also the first clean result the sensor has
produced: `miss=0`, `latch=0`, seven detections on every one of seven phases,
`rh` p10 **+557**, `fh` p10 **+823**.

The clean data and the fictional data came from the same log, minutes apart.

That is precisely the argument for a structural gate. There is no operating
condition flag, no weather note, and no session-level judgement that separates
the two — the sensor passed through both states within one run, and the only
in-band evidence distinguishing them is the quality field that currently has no
power to act. A reviewer reading the good window would have no reason to
distrust the bad one, and a governor reading either would have no way to tell
them apart.

## Acceptance criteria

1. A stationary car in collapsed contrast (`span < MARGINAL_SPAN`) publishes
   `speed_valid: false` and no speed value, for the full duration.
2. `speed_mmps` and `pkph` are `null` — never `0.00`, never a stale carry-over
   — whenever `speed_valid` is false.
3. The published state name accompanies every suppression, so a consumer can
   distinguish "no contrast" from "not enough intervals yet" from "stopped".
4. Edge detection and envelope decay continue to run unchanged through the
   suppressed period; recovery timing does not regress.
5. Replay of `ir_daylight_20260806_124221.log` against the changed logic yields
   no `speed_valid: true` in the stationary noise windows, and no loss of valid
   output in the clean shade windows of the same log.

Criterion 5 is the one that matters most: the fix must be demonstrated against
the log that contains both behaviours, not against either one alone.

## Open item for a future pass

The field record
[`IR_DEV_REC/2026-08-06_DAYLIGHT_FOAM_BLACK_SPOKES.md`](../IR_DEV_REC/2026-08-06_DAYLIGHT_FOAM_BLACK_SPOKES.md)
reports the no-foam column of this same log as 1,121 pulses analysed with 183
(16.3%) marginal and `rh` p10 +63. Those are whole-run aggregates.

**Unverified question, flagged rather than assumed:** it is not currently known
how many of those 183 marginal pulses are the stationary-noise pulses described
above rather than genuine daylight optical weakness. If a material share are
noise, the 16.3% figure partly describes a stationary car and overstates the
daylight problem the foam shield was credited with solving. This does not
challenge the foam result — the `fh` and `rh` improvements are large and
consistent — but the marginal-percentage line specifically should be
recomputed with stationary windows excluded before it is cited again.

## Verification limits of this document

The firmware constants, the `speed_valid` code path, the span-floor comment,
and the `DEBOUNCE_US` value were each read from source in
`firmware/test-programs/` and are stated as verified.

The STATS and PULSE lines above are reproduced from the task record; the log
`ir_daylight_20260806_124221.log` is not committed to this repository and was
not re-parsed here. The arithmetic that depends on it — the 150 ms
noise-to-VALID figure — follows from the quoted rate and the constants in
source, and would change if the quoted rate is later found inaccurate.

## Cross-references

- [`docs/IR_TEST_STATE_AND_REACQUISITION.md`](../IR_TEST_STATE_AND_REACQUISITION.md)
  — the `VALID` / `REACQUIRING` / `UNAVAILABLE` state model this requirement
  depends on, and the payload schema (§4) that must carry `speed_valid`.
- [`docs/IR_SENSOR_NOTES.md`](../IR_SENSOR_NOTES.md) — narrative history;
  "The failure that mattered most: quality was dishonest" records instances 1
  and 2.
- [`docs/IR_DEV_REC/2026-08-06_DAYLIGHT_FOAM_BLACK_SPOKES.md`](../IR_DEV_REC/2026-08-06_DAYLIGHT_FOAM_BLACK_SPOKES.md)
  — field record for the run this observation came from.
- Decision 0005 — a timeout means the sensor stopped seeing, never that the
  wheel stopped turning; speed is null, never 0.00, when untrustworthy.
  This requirement extends the same rule from silence to noise.
- Decision 0006 — envelope decay gates on signal activity; the reason
  detection must keep running while output is suppressed.
