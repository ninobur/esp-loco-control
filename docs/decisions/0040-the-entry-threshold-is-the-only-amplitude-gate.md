# 0040 — The entry threshold is the only amplitude gate, and Otto's was too low

Status: Accepted (operator direction 2026-08-20; flashed to Otto the same
afternoon and confirmed from his boot telemetry: `entry_margin: 65`).
Field verdict pending.

## Decision

Otto's `HALL_ENTRY_MARGIN_COUNTS` rises from **13 to 65**, putting his detection
threshold at **±90 counts** from baseline instead of ±38. Toby is unchanged.

## Why

The operator realigned Otto's Hall sensor on 2026-08-20. His mean peak rose
from 146.7 to 176.4 (Toby: ~193), closing a 25% deficit that had been flat for
eight days. That fix is what made this decision possible: it separated two
populations that had previously overlapped.

Measured across 596 markers after the realignment:

| | count | peak | duration |
|---|---|---|---|
| genuine | 585 | **104 minimum** | 116 ms minimum |
| spurious | 11 | **91 maximum** | 40–58 ms (9 of them) |

A clean gap from 91 to 104 with nothing in between.

The spurious events are admitted as markers. They advance `navMm` ahead of the
physical locomotive, the polarity sequence desynchronises, and a quorum opens
that cannot resolve. At 14:30:51 reads of peak 40, 51, 49 and 43 at mm 16, 19,
20 and 21 produced exactly that, ending in `NO_QUORUM` at mm 27 — the third of
the day, all in the same stretch.

## What actually gates a marker

Established by reading the firmware, and recorded because two plausible-looking
constants do **nothing**:

- **`HALL_MIN_PEAK_DELTA` gates nothing.** It appears only in the boot
  telemetry string. A change to it is a no-op that looks like a fix, and this
  decision nearly shipped one.
- **`HALL_DOMINANCE_PERCENT` gates nothing**, and is defined only in Toby's
  profile. Its absence from Otto's profile explains nothing.
- The real amplitude gate is `recomputeThresholds()`:
  `entry = baseline ± (HALL_DEADBAND_COUNTS + HALL_ENTRY_MARGIN_COUNTS)`,
  which was 25 + 13 = **38 counts**.
- The only other gate is `EVENT_FLOOR_MS` = **40 ms**, which is not per-loco.

The phantoms cleared both by a hair — peaks from 40, durations from 40 ms.

## Risk, stated rather than buried

A genuine marker whose peak falls below 90 counts is now **missed**, and missed
markers drift position. That is the failure that bit Otto in July when he ran
on the 50/80/80 fallbacks: 4% of markers lost, with nothing showing it.

The 14-count margin between the gate and the weakest observed genuine read
rests on **one session, at 92 °F, after a fresh realignment**. It is not a
proven margin.

**The signal to watch is the marker step distribution.** Consecutive `mm`
values should always differ by 1. A rise in steps of 2 means markers are being
missed, and the margin should drop to about 45 (entry 70) before anything else
is considered.

## A measurement caveat that outlived its usefulness

Polarity "error rate" — comparing observed pole against the map at the reported
`mm` — **conflates sensing errors with position offset**. Once a phantom has
advanced `navMm`, every subsequent correct read is scored against the wrong map
position. The hour run after the realignment reported 3.31% "errors" with a
mean peak of 126 on the wrong reads: those were correct reads at wrong labels,
not misreads.

Earlier Otto-versus-Toby error comparisons in this investigation are affected
by the same confound and should not be treated as clean sensing measurements.
Peak distribution and phantom count are the sound metrics.

## Not done

- `EVENT_FLOOR_MS` (40 ms) is shared and untouched. Nine of the eleven
  phantoms had durations of 40–58 ms, so a per-loco duration floor is a
  plausible second gate — but it is a shared-code change and this decision
  does not make it.
- Toby is unchanged. His peaks are higher and his phantom population has not
  been characterised.
