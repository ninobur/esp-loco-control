# NAVI_ONE — the discard branch amputated flanks; one condition fixes it

**Date:** 2026-09-03, evening
**Builds:** NAVI_ONE 1.0X12 "Whole flank" and NAVI_ONE_STATION_CURVES 0.2.
Both compiled; **neither flashed, neither run**. The operator flashes.
**Changes only:** the discard condition in `HallCapture.h` (identical in both
sketches) and the build name. No threshold, no archaeology rule, no station
exception, no quorum, no IR.
**Decision records:** 0075 (this fix, proposed), 0074 (shape authority,
deferred behind this fix at the operator's direction).

---

## The evidence that forced it

The operator's `NAVI_ONE_STATION_CURVES_0_1` run of 2026-09-03 published every
passage. Toby shut down on the fourth Arches CW departure of the evening,
passage 1805, MM110 targeting MM111, expected pole N observed N, amplitude
ratio 1.005, an ordinary passage (`paused_ms` 0, `stitch_at` 0), residual
0.1372 against the 0.13 ceiling, WRONG_SHAPE, routed to a shutdown by the
stop-episode rule.

All six Arches CW departures the recorder captured carry the same defect.
Pulled from the telemetry mirror by `open_ms`; the pre-roll is the twelve
samples before the passage opened, the jump is from the last of them to the
first sample the passage kept.

| seq | pre-roll → first sample | jump | flank slope | residual | verdict |
|---:|---|---:|---:|---:|---|
| 76 | 32 36 37 37 → 72 76 80 82 | 35 | 3.25/ms | 0.1009 | accepted |
| 247 | 29 31 33 35 → 97 100 101 104 | 62 | 2.50/ms | 0.1180 | accepted |
| 1292 | 33 33 34 35 → 109 112 114 118 | 74 | 2.50/ms | 0.1269 | accepted, 0.003 under |
| 1463 | 25 27 29 31 → 97 101 106 109 | 66 | 3.75/ms | 0.1148 | accepted |
| 1634 | 31 33 33 36 → 82 85 88 91 | 46 | 3.25/ms | 0.1016 | accepted |
| 1805 | 32 34 36 36 → 139 143 143 146 | 103 | 2.75/ms | 0.1372 | **WRONG_SHAPE, shutdown** |

A flank of 2.5 to 3.75 counts per millisecond cannot rise 35 to 103 counts in
one millisecond. Between 10 and 36 ms of every flank is missing, and the
residual tracks the size of the hole.

## The mechanism, now verified in the harness

`HallCapture::sample()` discards a just-opened passage as a false start when a
stop is armed, the field reads as a plateau, and the passage has not yet shown
progress. The plateau verdict is refreshed every 25 ms from a sixteen-sample
window with its single highest and lowest samples trimmed. On a genuine flank
the one rising sample in that window is the trimmed one, so the verdict stays
"flat" until the next refresh, or the one after it. Every sample in that
interval is discarded; the next sample reopens the passage; the pre-roll ring,
which fills only while no passage is open, freezes. The record that finally
survives begins wherever the verdict caught up, glued to pre-roll from before
the flank began.

Gate 14, run against the **unfixed** capture, reproduces it exactly:

| synthetic flank | discards | opened late by | pre-roll → flank step |
|---:|---:|---:|---:|
| 1.00/ms | 14 | 14 ms | 15 |
| 2.50/ms | 37 | 37 ms | 95 |
| 3.25/ms | 40 | 40 ms | 133 |
| 5.00/ms | 44 | 44 ms | 166 |

The loss is the stale interval times the slope. Its length depends on where
the flank lands in the 25 ms refresh cycle and how many rising samples the
trim absorbs, which is why the six field records lose different amounts.

## The fix

`HallCapture.h`, the discard guard, one added condition:

```cpp
const int32_t rest = plateauLevel_ - entryBaseline_;
if (mag < cfg_.entryMargin &&
    (rest < 0 ? -rest : rest) < (int32_t)cfg_.entryMargin) {
  discard();
  return false;
}
```

A reading at or above the opening threshold now was not opened by an excursion
that has gone. It is left alone and the verdict is allowed to catch up. The
existing guard (a rest level at or above `entryMargin` is a field, not a false
start) is unchanged. The comment in the code records the six departures.

## Proof, steps 1 to 4 of the operator's list

**1. A gate that opens a genuine flank while `plateau_` is stale.**
`tests/gate_flank_discard.cpp`, gate 14. Section A drives four synthetic
flanks into a capture that has sat armed and flat for 1.2 s. With the fix:

| flank | discards | opened | pre-roll → flank step | max step in first 20 | residual |
|---:|---:|---|---:|---:|---:|
| 1.00/ms | 0 | on the crossing | 1 | 1 | 0.1605 |
| 2.50/ms | 0 | on the crossing | 3 | 3 | 0.0645 |
| 3.25/ms | 0 | on the crossing | 3 | 4 | 0.0505 |
| 5.00/ms | 0 | on the crossing | 5 | 5 | 0.0737 |

(The 1.00/ms residual is high because a straight 210 ms ramp followed by a
Gaussian fall is not a Gaussian. It is a capture test, not a shape test.)

**2. All six departure records replayed.** Section B feeds each record twice.
First as published, after the same 1.2 s armed rest at its own pre-roll level:
the harness's recognizer returns the field residual to four places on all six,
so the harness and the locomotive agree. Then with the missing flank restored
as a straight line at the slope of the record's first four retained samples:

| seq | published | control (harness) | samples restored | discards | step | residual restored | verdict |
|---:|---:|---:|---:|---:|---:|---:|---|
| 76 | 0.1009 | 0.1009 | 10 | 0 | 3 | 0.0828 | accepted |
| 247 | 0.1180 | 0.1180 | 24 | 0 | 3 | 0.0779 | accepted |
| 1292 | 0.1269 | 0.1269 | 29 | 0 | 3 | 0.0789 | accepted |
| 1463 | 0.1148 | 0.1148 | 17 | 0 | 4 | 0.0854 | accepted |
| 1634 | 0.1016 | 0.1016 | 13 | 0 | 3 | 0.0772 | accepted |
| 1805 | 0.1372 | 0.1372 | 36 | 0 | 3 | 0.0728 | accepted |

Restored, the six sit at 0.073 to 0.085, inside the 0.072 to 0.079 band the
earlier Arches CW departures at PWM 90 were judged at. No discard-created
discontinuity in any of them. The restored flank is a straight line and the
real one curves; the gate is about whether the capture keeps what it is
given, and the header of the gate says so.

Section C checks that the branch still does what it is for. Bamboo's
one-sample artifact in a 30-count fringe is still discarded and nothing stays
open. An excursion that rose to 70 and settled at 30 is discarded once the
reading is back below the threshold. Resting inside a real field at 88 stays
open through the dwell, the known fault, unchanged.

**3. Every existing gate.** Gates 1 to 13 plus 14, all green, before and after
the fix (the before run is the baseline; gate 14 alone fails on the unfixed
code, 53 of 79 checks). The adversarial non-magnets in gates 2, 8, 12 and 13
are unchanged.

**4. Field-test images, changing only the discard condition.**

| sketch | flash | RAM (globals) |
|---|---:|---:|
| NAVI_ONE_1_0X12_FIELDTEST "Whole flank" | 983,187 B (75%) | 64,508 B |
| NAVI_ONE_STATION_CURVES_0_2 | 984,815 B (75%) | 64,588 B |

The recorder 0.1 binary the operator built was 983,312 B; 0.2 is 1.5 KB
larger. One comparison and a name change do not account for that; the
operator's 0.1 build and this one may differ in core options, and the figure
is reported, not explained. Both compiled with
`arduino-cli`, ESP32 core 3.3.11, into scratchpad build paths; the operator's
own build directory was not touched, and the operator's own build precedes any
flash.

## Steps 5 and 6 are the operator's

Repeat multiple Arches CW departures. Success is continuous captured flanks
with no discard-created jump in the published records, not a lap without a
shutdown. The recorder build is the one that can show it, because it publishes
every passage; X12 publishes only refusals and would prove only the weaker
thing. The `discards` counter on the 1 Hz alert line should read zero across
each departure; today it read about 25 per amputated passage.

## What this fix does not do

- It does not touch the shape ceiling, the stop-episode rule, or the
  archaeology. Decision 0074 asks whether shape should keep operational
  authority; the operator has ruled that question waits until clean
  acquisition is demonstrated, because shape-advisory would have prevented
  this shutdown while masking the corrupted acquisition underneath it.
- It does not model the baseline adapting during a departure. In the harness
  the baseline is held through the armed section, as it is below the
  adaptation PWM. A real departure crosses that PWM within the flank.
- It does not address the stationary-inside-a-field passage (Arches CCW
  14:14:40 class). That is the fault the comment already names as understood
  and bounded.

If the same amputation recurs after this fix, the operator's instruction
stands: stop adding rescue layers and reconsider the capture architecture.

## Files

- `firmware/test-programs/NAVI_ONE/HallCapture.h` and
  `firmware/test-programs/NAVI_ONE_STATION_CURVES/HallCapture.h`: the condition.
- `firmware/test-programs/NAVI_ONE/tests/gate_flank_discard.cpp`: gate 14.
- `firmware/test-programs/NAVI_ONE/tests/fixtures_arches_departures.h`: the six
  records, verbatim from the mirror.
- `firmware/test-programs/NAVI_ONE/tests/run_tests.sh`: gate 14 added.
- Build names bumped in both `.ino` files.
