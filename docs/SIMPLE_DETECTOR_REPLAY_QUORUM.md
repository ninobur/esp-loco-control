# Replaying a deliberately simple detector on the continuous Hall corpus

**Analysis. No firmware changed, no threshold proposed.**

Opening rule: **|raw − baseline| ≥ 70 counts for 2 consecutive 1-kHz samples.**
No morphology, peak shape, width, rise, fall, duration or closure is used to
validate an opening. The two contextual protections are then applied in stages.

Corpus: QUORUM TRACE, Otto 9950011, 2026-08-24/25 — 4 capture files,
6,446,880 samples. The reference is QUORUM's own per-sample `baseline`, carried
in the stream. **Moving** was established from marker traversal, not from PWM:
a run of ≥3 navigation advances each ≤4 s apart. 25 such segments.

---

## The stages

| | | n |
| --- | --- | --- |
| 1 | raw ≥70/2 openings (all) | 1,690 |
| | **raw ≥70/2 openings while moving** | **1,577** |
| 2 | eliminated **solely** by the 500 ms rule | **22** |
| 3 | of the remaining 1,555, eliminated **solely** on opening polarity | **154** |
| 4 | surviving (accepted) | **1,401** |
| | **of those, demonstrably not the expected magnet** | **2** |

### **2 / 1,401.**

## Stage 4 — every surviving false event, individually

Ground truth is physical: the implied speed from the previous accepted marker,
against **Otto's own measured speed-vs-PWM curve**, derived from this same
accepted chain:

```
   PWM 40-49  median 121 mm/s      PWM 80-89  median 266
   PWM 50-59         127           PWM 90-99         272
   PWM 60-69         165           PWM 120-129       360  (p90 374)
   PWM 70-79         204
```

**1. `qt_20260825_140423.qtcap`, t = 243211 ms, pole N, PWM 90.**
532 ms after the previous accepted opening → **564 mm/s**, 2.07× Otto's measured
PWM-90 speed. QUORUM's own contemporaneous record logs **DISAGREE** at that
instant. The preceding interval was 4,551 ms (67 mm/s), so markers had been
missed going in.

**2. `qt_20260825_140423.qtcap`, t = 263774 ms, pole N, PWM 47.**
781 ms after the previous accept → **384 mm/s**, 3.0× Otto's measured PWM-47
speed and above his maximum at any throttle below 120. Removing it makes the
local sequence 262993 → 265726 a single 2,733 ms interval at 110 mm/s, normal
for PWM 47. QUORUM logged nothing either way here.

I **cannot** determine from this corpus whether either was a stray field or a
re-read that fell outside the 500 ms guard. Both are demonstrably not a
traverse of the next expected magnet; that is all the evidence supports.

**A fleet figure would have got this wrong.** Using the 400 mm/s fleet maximum
(a Toby-era number) flags 7 events. Six of them are a consecutive run at
PWM 108–120 implying 417–456 mm/s, smoothly decelerating, every one logged
AGREE by QUORUM. They are Otto's genuine high-speed range, not impostors.

## Re-reads, reported separately

Of the 22 openings cut by the 500 ms rule, classified by the distance the
sensor had actually travelled since the accepted magnet (the ≥70 region of a
magnet is 27.6 mm wide, p90 31.1):

- **8 are genuine re-reads** — 12, 18, 26, 26, 30, 31, 33 and 35 mm travelled,
  so the sensor had never left the magnet it had just counted. **These are real
  magnet encounters and are not stray-field false positives.**
- **14 had travelled 36–163 mm** — clear of the magnet, but far short of the
  ~300 mm to the next marker. Fields encountered between markers.

All 22 are in a single session, `qt_20260825_140423`. The other three sessions
produced **no** 500 ms rejections at all.

## The stage-3 number is not 154 impostors

Measured by distance from the previous accepted marker, the 154 polarity
rejections sit:

```
    0-60 mm  (just past the last marker)      0    (0%)
   60-200 mm (between markers)                8    (9%)
  200-260 mm                                  6    (7%)
  260-340 mm (WHERE THE NEXT MARKER IS)      35   (40%)
    >340 mm                                  38   (44%)
```

**84% sit at or beyond a real marker position.** They are overwhelmingly real
magnets rejected because the replayed pointer was out of step — not stray
fields. Corroborated from the other side: 72 of 1,379 accepted intervals imply
under 0.65× Otto's speed for the throttle, which is the pointer catching up
after a skip.

Whether that desynchronisation is a property of the protection or an artefact
of this replay — its seeding, and openings the ≥70/2 rule missed — **this
corpus cannot separate.** Read stage 3 as "154 openings the polarity rule
refused", not as "154 impostors caught".

## It concentrates in one session

```
  file                          accept   polarity   500ms
  qt_20260824_201746.qtcap         351         58       0
  qt_20260824_205508.qtcap         939         16       0
  qt_20260825_140423.qtcap         111         80      22
```

The 2026-08-24 20:55 session is the clean one: **939 accepted, 16 polarity
rejections (1.7%), zero guard rejections, zero demonstrable false accepts.**
Both stage-4 events and every guard rejection are in the 2026-08-25 session.

## Limits

- **One locomotive, two days, one detector lineage.** 2026-08-24/25, before the
  September track work and the entire X13–X20 lineage. Do not generalise beyond
  this corpus.
- The `qt_20260824_195000` file contributed no moving segments and is excluded.
- Ground truth for stage 4 is physical plausibility plus QUORUM's contemporaneous
  verdict. There are no operator anchors inside the moving segments.
- The HWT corpus was not used here: it carries no navigation pointer, so the
  polarity protection cannot be replayed against it.
