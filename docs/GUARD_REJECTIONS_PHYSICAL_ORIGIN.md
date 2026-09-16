# The physical origin of the 24 guarded / false openings

**Analysis. No firmware changed.** The 22 ≥70/2 openings rejected by the 500 ms
guard, plus the 2 demonstrably false Stage-4 survivors, from the QUORUM TRACE
replay (`docs/SIMPLE_DETECTOR_REPLAY_QUORUM.md`).

Morphology is **not** used as an acceptance criterion. The test is physical
continuity: between the preceding accepted magnet's opening and this opening,
**did the signal return to the resting reference at all?** — measured as the
longest continuous run inside QUORUM's own 25-count deadband, plus the number
of times the signal crossed between poles.

Speed is measured, not assumed: `v measured` is the surveyed spacing divided by
the bracketing accepted interval. The PWM-curve figure is shown alongside for
comparison; where they disagree the measured one is used.

---

## Headline

**Polarity: 11 SAME, 13 OPPOSITE.** But opposite polarity does **not** mean a
different magnet — see category A.

**Not one of the 24 is a new track magnet.** Every one is 6–191 mm from the
previous marker, against a surveyed spacing of 280–355 mm. All are sub-marker.

## The table

```
t       stg  pol   gap ms  v PWM-fit  v measured  dist mm  origin
214354  gd   OPP   151     272        113          17      A
216996  gd   OPP   131     272        n/a         ~36      A
238224  gd   OPP    96     272         67           6      A
252197  gd   OPP   113     272        125          14      A
252469  gd   OPP   385     272        125          48      A
258026  gd   OPP    91     127        n/a         ~12      A
316376  gd   OPP    83     360        448          37      A
354551  gd   OPP   154     272        291          45      A

243211  S4   SAME  532     272        359         191      B*
243305  gd   SAME   94     272        264          25      B
265878  gd   SAME  152     121        n/a         ~18      B

199915  gd   OPP   280     300        205          57      C
244634  gd   SAME  286     272        n/a         ~78      C
250874  gd   SAME  362     272        194          70      C
263774  S4   OPP   781     121        220         171      C
266100  gd   OPP   374     121        n/a         ~45      C
296758  gd   SAME  487     300        211         103      C
302629  gd   OPP   195     360        437          85      C
310983  gd   SAME  452     360        102          46      C
323698  gd   SAME  424     266        119          50      C
340055  gd   SAME  274     127         95          26      C
340195  gd   SAME  414     127         95          39      C
347659  gd   OPP   122     272        275          34      C
347985  gd   SAME  448     272        275         123      C
```

## A — the second lobe of the magnet just counted (8, all OPPOSITE)

The trace sweeps monotonically from the counted pole, through rest, into the
opposite pole — **exactly one pole crossing**, and no sustained quiet interval
(longest time inside the deadband 12–26 ms). Distance travelled 6–48 mm, i.e.
still inside or barely past the magnet, whose ≥70 region is 27.6 mm wide.

```
  214354   -74  -87  -93  -92  -83  -66  -46  -23   -1  +18  +37  +44  +54  +62  +68
  238224   -70  -85  -92  -93  -98  -98  -86  -69  -49  -30  -19   -4  +16  +32  +52  +64
  354551   -70 -101 -128 -145 -153 -138 -134 -110  -74  -44  -16   +1  +10  +48  +52  +69
```

This is one magnet's bipolar signature, not two magnets. **These are genuine
encounters with the magnet already counted.**

## B — never left the previous field (3, all SAME)

No pole crossing, and the signal never got inside the deadband for more than
8–11 ms; minimum |deviation| 13–16 counts.

- **243305** (25 mm) and **265878** (18 mm) are inside the magnet's own width.
  Genuine re-triggers on the magnet just counted.
- **243211** is marked **B\*** because it does not fit. It is 145–191 mm away by
  either speed estimate — far too distant for a 27.6 mm magnet. The trace sits
  on a **+22…+32 count shelf** for most of the interval and then rises:
  ```
   +72 +170 +205 +111  +43  +29  +26  +30  +32  +23  +22  +25  +25  +56  +62
  ```
  The better reading is a **displaced resting reference** of ~25 counts, not a
  persisting field. On that reading it belongs in category C. I cannot settle it
  from this corpus and am not going to pretend otherwise.

## C — a separate field after a clear return to rest (13)

The signal returned to the deadband and stayed there — 36 to 316 ms — and then
a distinct field arrived. Measured distance from the previous marker:
**26–171 mm, median 57 mm.**

```
  296758   +83  +60   -3   +3   -4    0   -4   -6  -11   -6   -8  -12  -16  -13  +76
  323698   +77 +160 +141  +60  +17   -1   -3   -2   +1   -2   +3   -1   -1   +1  +77
  263774   -70 -122 -131  -76  -27   +1  +17  +14  +20  +20  +20  +18  +14  +62  +69
```

These are real, separate magnetic features **between** track magnets. Not
rebounds, not the preceding magnet, and not at a marker position — the nearest
marker is 280 mm away at minimum. 7 are SAME polarity and 6 OPPOSITE, so
polarity carries no information about which they are.

## Summary of origin

| | n | associated with the preceding magnet? |
| --- | --- | --- |
| A second lobe of the same magnet | 8 | **yes** |
| B re-trigger inside the same magnet | 2 | **yes** |
| B\* displaced reference (243211) | 1 | unresolved |
| C separate sub-marker field | 13 | **no** — but not a track magnet either |

**10 of 24 are the preceding magnet. 13 are distinct fields between markers.
None is the next track magnet.**

## Correction to the previous report

My earlier answer classified 8 of the 22 guard rejections as re-reads using
**distance alone** (< 35 mm travelled). The continuous trace revises that:

- only **2** are re-triggers on the same magnet in the sense I meant;
- **8** are the magnet's opposite lobe — still the same magnet, but a different
  mechanism, and I had counted most of them as "a separate field";
- several I had called separate fields at 36–163 mm are confirmed as such, but
  the pole-matching heuristic that produced the split was wrong.

The total "associated with the preceding magnet" moves from 8 to 10, and the
reasoning behind it changes completely. **Opposite polarity does not imply a
different magnet**, which is the substantive finding here.

## Limits

- One locomotive, the 2026-08-25 session, where all 22 guard rejections occurred.
  The three other sessions produced none.
- Category C's "between markers" conclusion rests on the measured bracketing
  speed; four events have no bracketing interval and use the PWM fit, marked
  `n/a` above.
- The rest test uses QUORUM's own 25-count deadband and its own per-sample
  baseline. A baseline that was itself displaced — the likely story for 243211 —
  makes "returned to rest" unmeasurable, and that is a limitation of the record,
  not a finding.
