# Otto X20 — a lap and a half, six clean station stops, and a pole read 297 ms late at MM136

**2026-09-15, 20:10–20:21.** Otto 9950011, `NAVI_ONE_1_0X20_DWELL_FIELDTEST`.
Declared CW. Six station stops. Struck at MM136 → 137 and stopped between
stations. `agree 261, disagree 1`.

Provenance: `~/NGR/telemetry/runs/9950011_20260915_201020.log` on
192.168.68.142, plus the idle logs either side.

---

## 1. The dwell work did what it was built to do, and was never tested

Six stops — Grillers, Arches, Bamboo, Patio, Grillers, Arches. Every one:

```
hall=REST   depart −8..+11 counts   suppressed=0   hold=0   armed=1 on departure
```

**Zero candidates during any dwell.** Both Arches stops came to rest at MM109
with the Hall 2 and 3 counts off its measured rest — clean magnetic space, not
the fringe field of 18:58. No `OLD_FIELD_CLEAR` was ever needed.

State this honestly: **the condition X20 exists to handle did not recur, so the
fix is un-contradicted rather than proven.** The `IN_OLD_FIELD` branch has
never executed on a locomotive. What this run does establish is that the dwell
guard costs nothing when the locomotive stops where it is supposed to — six
stops, no suppression, armed immediately every time.

## 2. The strike is an X19 defect in `finish()`, and X20 was not involved

MM136 is between Arches (108) and Bamboo (157). `measured_ms 400`,
`stopped_short 0` — an ordinary completed window at cruise, 34 s after leaving
Arches, nowhere near a station and with `stopped` false throughout.

The strike record contradicts itself:

```
t=689984  depart −74  →  polarity N
```

The departure that *declared* the candidate is negative — South. The polarity
assigned to it is North. In 266 records this happened twice, and the first one
is the strike.

## 3. What the waveform shows

The dump triggered by the strike carries the record whole (913 samples).
L-relative, milliseconds from the detection sample:

```
  ms    −20   +20   +60  +100  +140  +180  +220  +260  +300  +340  +380
  val   −25  −106   −57   +17   +33   +43   +42   +83   +99   +98   +75
```

There is a real S magnet at +20 ms, reaching −106. Then the signal recovers,
crosses zero, and settles onto a **positive shelf of +75..+99 that holds for
the rest of the 400 ms window**. The shelf reached 117 counts — more than the
magnet's own 106.

`finish()` searches for the peak across the **entire** window and takes the
excursion, and therefore the pole, from wherever the largest magnitude is. Here
that was the shelf, 297 ms after the magnet. `exc_first` = detection + 163 ms.

Every healthy S record on the same stretch has the same shape with a smaller
shelf — `+29 −102 −51 +29 +45 +32 +43 +41 …` — and the magnet still wins the
argmax, so the pole comes out right. MM136's shelf simply grew past it.

**X19's header already names half of this defect** and fixed only half:

> "A fixed window does not end, so it accumulates a long quiet tail, and every
> count of error in the excursion's zero is multiplied by the window length."

The *sum* was restricted to the excursion for exactly this reason. The *peak
search that chooses where the excursion is* was left ranging over the whole
window.

## 4. How near the edge everything else was

How long after the detection sample the excursion begins, all 266 records:

```
  0–20 ms     263
  21–50         0
  51–100        0
  101–160       0
  >160 ms       3      +163 (the strike), +226, +240
```

Bimodal with nothing in between — the same shape of evidence as the Arches
`exc_n / w_caliper` separation. 263 records take the pole from the magnet that
declared them. Three take it from something 163–240 ms later. Two of those
three disagree with their own departure sign; the third (`t=672103`,
`dep +182`) happened to agree and passed.

## 5. The proposed fix, checked against the actual samples

Anchor the peak search to the sign of `departAtDetect` — skip samples of the
opposite sign when finding the argmax. Nothing else changes: still a signed sum
over a contiguous run (decision 0064), still a median-of-three judgement copy
(0065), no new constant, no morphology, no PWM.

Re-run on the five decoded records from the strike dump:

| slot | | shipped: peakAt / pole | anchored: peakAt / pole |
| --- | --- | --- | --- |
| 0 | **the strike** | **+297 ms → N** | **+25 ms → S** |
| 1 | healthy | +52 ms → N | +52 ms → N |
| 2 | healthy | +54 ms → N | +54 ms → N |
| 3 | healthy | +24 ms → S | +24 ms → S |
| 4 | healthy | +25 ms → S | +25 ms → S |

The map expects **S** at MM137. The four healthy records are unchanged, sample
for sample.

**Not yet implemented.** This is a change to the detector's judgement path and
the operator has not asked for it.

## 6. Secondary, non-blocking

After the strike Otto sat at MM136 for forty minutes with detection live and
produced four more candidates while stationary (`NO_POSITION`, peaks 93–209),
and `suppressed` climbed from 25 to 558. Harmless — he is `STRUCK`, so nothing
can advance — but worth noting that **X20's dwell guard does not cover a
post-strike stop**: `withdraw()` clears `autoRunning`, which drops
`stationHolding`, which leaves the detector armed on a parked locomotive. That
is the Arches condition with the navigation consequence already removed.
