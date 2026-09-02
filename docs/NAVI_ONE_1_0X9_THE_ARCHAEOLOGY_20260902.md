# NAVI_ONE 1.0X9 — the archaeology

**Date:** 2026-09-02, evening
**Locomotive:** Toby (9950012)
**Commit:** X9 on `agent/toby-1-13-flash`
**Status:** BUILT AND GATED. Not flashed. Every number below is a replay or a
reconstruction; the field has not seen it.

---

## What it is

A passage split by a stop is no longer judged as one Gaussian with one width.
It is two fragments of one arc, and the recognizer restores the arc from them —
or says it cannot.

The operator's brief, in two sentences:

> The sketch should take an archaeological approach. It is restoring a total
> picture from parts. It needs however to make enough of it fit the model to be
> confident in what it has found.

> The magnet fragment must demonstrate a pattern more like a magnet than the
> patterns that a fragment can display, to be trusted.

## The rules, and the measurement each rests on

All measurements are on the 312 real, accepted, uninterrupted magnet passages in
the field records (187 survey at cruise, 125 telemetry at station speeds), and on
gate 12's four adversarial non-magnets through the real capture.

| rule | rests on |
|---|---|
| A fragment is judged by **pattern**, not by fit. | A locomotive crawling into a station decelerates *within* the fragment; no fixed-width Gaussian fits that in time. Finding 11's arrival, a magnet, fits at 135 against a 176 maximum. |
| **Regularity** about the fragment's own maximum, at the arc's own scale; a reversal is not a magnet. | Real flanks ≥ 0.86; shoulder 0.58, double lobe 0.69–0.76. Per-sample increments on a slow crawl are below the noise, so the question is asked over a window a twentieth of the fragment wide, and windows that do not move are not asked. |
| A fragment may **set the scale** (be the trunk) only if it **decelerates into a top**. | Through-apex fragments 0.32 median, 0.56 p95; 97% coverage p95 0.68; ramps 0.93–0.97. Threshold 0.60; 0.75 when the other fragment corroborates it holds the top (its maximum is 10% higher). |
| A fragment that decelerated by **noise** gives itself away: its free fit extrapolates above what it observed. | Through-apex fits land within 5.5% of the maximum (p95). Demoted at 10% above; one-sided, so a crawl whose fit lands *below* is left alone. |
| The scale is what the trunk **observed**. | A fragment reaching 45% of peak has its amplitude wrong by 45% when fitted freely; only at 97–100% coverage does that drop to 2–4%. A tail cannot size the elephant. |
| The **tail** must fit that scale: amplitude fixed, width and centre free, under the unchanged **0.13** ceiling; its apex beyond itself on the trunk's side. | Real tails against their trunk: 0.008 median, 0.018 max, identical at 1×, 2×, 4× speed ratios. Real tails' centres land ≥ 1.23 σ beyond the join, never inside. |
| **Two trunks** must agree on the size and hold **one apex** between them. | Two halves through the apex agree to 2.1% median, 8.1% p99. Two maxima each well inside its fragment, with the join-side edge below 85% of it, is two lobes. |
| A fragment **below the fit floor** (20% of peak) or with **less than a band-span of range** is not evidence: it can neither establish nor veto. | The fringe behind a fully crossed magnet, and finding 13's 12-count arrival wobble. |
| When the stop was **on the top**, the resting level *is* the apex. | `Passage.restLevel`: 400 ms of the field at a known-stationary point. The ±10-count band round it excises the last counts of both flanks, so neither fragment carries the top — this does. Requires at least one flank decelerating (0.80): stops 3 mm from an apex run 0.74–0.77, the electrical step 0.94. |
| A competing reconstruction is tried before a contradiction is declared. | Two trunks that disagree: the lower becomes a tail and must fit the higher. |

## Three outcomes

- **Magnet** — enough of one arc was seen to set its scale, and every fragment fits it.
- **Insufficient** — the fragments may well be a magnet; not enough survived to say, typically no fragment reached the apex. Routed to the path the firmware already has for a pause that never resumed: advance zero, stop, *a marker may have gone uncounted*. Published as `INSUFFICIENT_EVIDENCE`. **Not `WRONG_SHAPE`.**
- **WrongShape** — the fragments contradict one arc: a reversal, two apexes, a tail that does not fit the trunk's scale.

## The capture, so that the fragments exist

- **Provisional retention into the passage** from the first loss of plateau; committed by the unchanged resumption test, rewound if the field returns to the resting level. The 512-sample ring was too short for a slow departure over the top.
- The **resting level is frozen** while a departure is being retained; "flat" re-triggers at a magnet's apex and had been rewinding the rise.
- A **second pause folds a resume back** into the stop only if the field came home; a pause at the top is a second excision inside the departure.
- The **pre-stop cut is the first sustained entry into the band** round the resting level — not the flat window's start (50–75 ms early on an abrupt stop), not the last excursion out (a spike would drag the tail in behind it).
- **Closing out of a pause** commits what was retained as the join, or places the join at the end so the judgement says *insufficient* instead of the single fit calling half an arc the wrong shape.

## What the gates show

| | |
|---|---|
| 312 real magnets: rising tail (45%) + whole departure, at 1× and 3× | **311 of 311** |
| whole arrival + falling tail from 45% | 306 of 312 |
| rising tail (75%) + whole departure | **312 of 312** |
| both halves over the top | 309 of 312 |
| top cut away at 97% / 85% / 60% | 271 / 52 / 11 accepted, the rest *insufficient*, ≤1 wrong |
| **A1967_CW** — refused 0.1967 this morning | **MAGNET, one arc restored** |
| **A1818_CW** — refused 0.1818 | **MAGNET** |
| **A1810_CW** — refused 0.1810, a 35 s dwell in the field | **MAGNET** |
| A2695_CCW, A5586_CCW — arrival only | INSUFFICIENT |
| electrical step, DC ramp | INSUFFICIENT — no fragment reached an apex |
| shoulder | WRONG — turns back on itself |
| double lobe | WRONG — two apexes |
| gate 12 J: approach-ramp stalls across the arc | 15 of 15 whole (X8: 12; X7: 0) |
| gate 13: 187 survey magnets, every real construction | **100%**; 0% with the top cut away |
| thirteen gates | **0 failures** |

Every non-magnet is refused on structure — no apex, a reversal, two apexes — never on a residual that more retained evidence could flatter. **No threshold was moved.**

## What it does not do

- **It has not run on the railway.** Every number is a replay or a reconstruction. The accepted stitched arc of 11:03 (0.1223) was never published and is in no set. Flash and watch — and publish every stitched passage so the next one is.
- **Known risk 3 is untouched:** a passage that opens with no progression in a field at or above `entryMargin` still spans its dwell. It now ends in *insufficient* rather than *wrong shape*, which is the honest word, but it still stops.
- The three "both halves over the top" refusals and the eleven 60%-cut acceptances in the 312 are the model's noise floor; they are not understood individually.
- Flash is 982,899 with the real libraries; RAM 64,508, up 4 KB for the signature buffers on the Hall task.
- The console still prints "STITCHED WAVEFORM REFUSED" for any stop-episode refusal, and "UNKNOWN — STALE" during a dwell the navigator is perfectly sure of. Neither is the locomotive's doing.
