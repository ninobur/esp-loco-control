# Toby QUORUM 1.6 — cross-locomotive Hall phantom check

**Date:** 2026-08-11  
**Locomotive:** Toby 9950012  
**Flashed sketch:** `QUORUM_1_6` (unchanged for this test)  
**Capture:** `field-records/logs/20260811_TOBY_QUORUM_1_6_cross-loco-phantom.log`  
**Purpose:** Test whether the weak extra Hall events recently observed with Otto
follow the locomotive or recur for a second locomotive over the same railway.

## Result

The Grillers-area extra event recurred with Toby. This is strong evidence that
the source is associated with the physical railway/magnetic field at that
location rather than Otto's recently repaired Hall sensor or its mounting.

Toby also supplied a new counterexample to a timing-only containment rule: at
believed mm 84, a weak event arrived at a plausible ordinary interval and was
accepted; the strong event 115 ms later was rejected. Counting remained correct
(one of the pair advanced the odometer), but the accepted event carried the
wrong polarity. A rule using only the preceding interval cannot identify which
member of such a close pair is the real marker.

No firmware was changed, and this record does not approve a firmware change.

## Capture summary

- Capture duration: 17.0 minutes, including setup and shutdown.
- Navigation-accepted markers: 208 (`201 AGREE`, `7 DISAGREE`).
- Raw agreement among accepted markers: 96.6%.
- One full CW lap: declared at mm 40 and released near mm 38.
- One partial CCW leg: declared at mm 45 and ended at mm 10.
- CW: 166 agreements, 7 disagreements.
- CCW: 35 agreements, 0 disagreements.
- One QUORUM incident: adopted offset -1, validated, and closed.
- One `PHANTOM_REJECTED` event.
- No `NO_QUORUM` terminal event.

## Event A — Grillers recurrence on a second locomotive

The genuine Grillers marker was followed by a weak extra event:

```text
18:18:09.835  mm 62  S  peak 157  width 329 ms  dt 2718 ms
18:18:12.338  mm 63  N  peak 170  width 302 ms  dt 2532 ms  Grillers
18:18:12.385  mm 64  N  peak  41  width  42 ms  dt  303 ms  accepted DISAGREE
```

The weak event passed Toby's active conservation gate:

```text
dt_expected=1734 ms, dt_conserve_ratio=1.63
```

Because it advanced the odometer to mm 64, it also triggered the station
machine's `ZERO_RAMP` at offset +1. After the dwell, genuine readings were one
position behind the odometer. QUORUM opened at mm 71, adopted offset -1
(`74 -> 73`), and closed on the following agreement at mm 74.

This is the same broad signature observed with Otto at Grillers: a normal,
strong station read followed by an abnormally weak, short extra read. Repeating
it with a different locomotive and Hall installation is the requested
cross-locomotive discriminator.

## Event B — weak-first pair near believed mm 84

The second event pair exposes a limit in timing-only containment:

```text
18:18:54.964  mm 83  N  peak 197  width 189 ms  dt 1338 ms  AGREE
18:18:56.061  mm 84  N  peak  40  width  40 ms  dt 1166 ms  accepted DISAGREE
18:18:56.278  mm 84  S  peak 216  width 189 ms  dt  115 ms  PHANTOM_REJECTED
18:18:57.547  mm 85  S  peak 201  width 167 ms  dt 1298 ms  AGREE
```

The map expected `S` at mm 84. The weak `N` event was accepted because its
1166 ms arrival looked ordinary. The strong `S` event—the one consistent with
both the map and neighbouring pulse morphology—arrived 115 ms later, so the
gate rejected it. The count was conserved, but event identity was inverted.

For the proposed measured-predecessor rule (`reject when dt <= 0.30 * previous
accepted dt`), the same ordering remains:

- weak event: `1166 > 0.30 * 1338` -> accepted;
- strong event: `115 <= 0.30 * 1166` -> rejected.

Therefore the proposed timing change still has value for early second events
such as Grillers, but it is not a complete phantom discriminator. The stateful
harness must include this weak-first ordering explicitly and report the retained
polarity/disagreement outcome, not merely the number of accepted events.

## Signal separation in this capture

Across 212 published marker events:

- median peak: 183; fifth percentile: 148;
- median pulse width: 171.5 ms;
- only two events had peak below 80, and they were exactly the two weak events
  described above (peaks 41 and 40);
- only those same two events had pulse width below 80 ms (42 and 40 ms).

This is compelling diagnostic separation in this run, but it is not authority
for a fixed absolute peak or width threshold. Previous evidence shows detector
scale changes with mounting, speed, and installation. The physical source
should be inspected before adding another compensator.

## IR telemetry

There is no usable moving IR comparison in this capture. The IR node published
only 16 speed reports between 18:12:56 and 18:14:01, before Toby's run began;
all were `INVALID_CONTRAST` or `MARGINAL`. It then went offline at 18:14:24.

## Consequences

1. Inspect the physical railway near Grillers and believed mm 84 for a second
   magnetic source, magnet geometry, fastener, or double-lobed field response.
2. Add both Toby event orderings to the stateful host replay required before
   any timing-gate firmware change.
3. Preserve the approval boundary: harness-only experimentation first; no
   flashable acceptance-path change is approved by this record.
4. Do not interpret the absence of CCW faults as a directional pass: the CCW
   leg covered only 35 markers and did not traverse the full route.

