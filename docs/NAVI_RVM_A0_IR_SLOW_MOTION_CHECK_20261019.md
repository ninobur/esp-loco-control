# Step A0: does run 8's raw IR waveform resolve slow motion at the MM109 stall?

Claude (Opus 5.5), 2026-10-19. **Analysis only**, step A0 of
`docs/NAVI_RVM_SLOW_CRAWL_STALL_TEST_DESIGN_20261019.md`. Nothing on Otto, the
IR car or the Pi was changed.

## Sources (read-only copies)

| Source | Content | sha256 |
|---|---|---|
| Pi `/home/david/NGR/ir_espnow/ir_espnow_raw_20261019.log`, lines with epoch 1792430900–1792431900 | 36,398 lines; 8,427 type-1 raw-waveform packets (one car, sid 3888415790), 96 samples each at 1 kHz | `6acf3ee2f89c955926d86d380af9e63fb17eb512ba26f2681bbb77c55c5a8cbf` |
| Run 8 NSR tail (as in the run-8 replay) | Otto native Hall, applied PWM; status records for the clock | see the run-8 replay section |

**Scripts:** `analysis/navi_rvm_shape_20261018/field_20261019/a0_ir/`.
- `a0_decode.py`: type-1 decode with the repo's `tools/ir_scope_espnow_analyze.py`.
- `a0_clock.py`: Otto clock to Pi time.
- `a0_view.py`: per-interval IR and Hall.

## Clock alignment

- **Otto to Pi.** NSR status records: Pi = Otto + 1792430929.0725 s (lower
  envelope). Drift 3 ppm; delay spread p99 33 ms.
- **IR sample to Pi.**
  - The sample counter runs at 1.0000236 ms per sample.
  - **The minimum packet delay is an outlier.** Typical packets arrive
    0.665–0.755 s (p1–p99) above it. The alignment therefore uses the **1st
    percentile**. With the minimum, the IR appeared about 0.67 s early and the
    stall timing was wrong.
  - The remaining constant transport delay is unknown, but small enough that
    the next check holds.
- **Check on a physical event.** At the departure from MM109 the order must be
  PWM, then the wheel, then the Hall:
  - PWM first above 32 at **665.161 s**;
  - IR first leaves its stationary band at **about 666.45 s**;
  - the Hall leaves 2021 ± 6 at **666.567 s**.

  The order is consistent.
- **Packet loss:** 19 % of IR samples are missing (814 gaps). Every interval
  quoted below has enough samples to judge it.

## Observations

**Moving at PWM 32 (586–604 s):**
- the raw IR swings between 1873 and 2247 (p5–p95);
- edges (stripe transitions) come every 0.19 s (median);
- that is about **25 mm/s** at the nominal 9.652 mm per pulse (two edges per
  pulse).

**MM109 stall entry** (0.1 s intervals):

| Otto time (s) | IR mean (min–max) | IR edges | Hall | PWM |
|---|---|---|---|---|
| 605.4–606.1 | swinging 1860 → 2236 → 2110 | 2 | 1949 → 2015 | 32 |
| **606.2** | **2094.6 (2083–2111)** | 0 | 2018.9 | 32 |
| 606.3–607.3 | 2092.8–2093.7, flat | 0 | 2019.9 → 2024.9 | 32 |
| 607–612 (1 s means) | 2093.0 → 2091.3 | 0 | 2025.6 → 2033.7 | 32 |
| 612–664 | 2091 → 2083 | 0 | 2034 → about 2022 | 32 |

- **The wheel stops at about 606.2 s** and stays still until the departure
  (about 666.4 s): there are no edges.
- It stops with the IR at its mid-swing level, on a stripe edge, where the
  signal is most position-sensitive. The average sensitivity over the swing is
  at least 374 counts per 4.8 mm, about 78 counts/mm.
- Over 606.2–612 the IR changes by about 3 counts, **0.04 mm or less of wheel
  movement**.
- Over the same 6 s **the Hall rises 15 ADC** (2019 → 2034). At the Hall
  gradient just before the stop (about 5 ADC/mm at 25 mm/s), that corresponds to
  about 3 mm of travel. The IR would have shown that as hundreds of counts.
- Over the next 50 s the Hall falls back by about 12 ADC while the IR moves by
  8 counts (0.1 mm or less).

**MM023 stall:**
- The wheel moves until about 903.3 s (IR range 113 in 903.0–903.5), then is
  flat at 2163–2165 (on a slope of the swing) until 911.5 s.
- **The Hall stays at 2018 ± 1 for the whole 8 s**, including the PWM change
  from 29 to 35 at 905 s.
- The wheel moves again from 911.5 s, and the Hall follows at 912 s.

**Battery current** (INA219, 5 s samples): 0.19–0.29 A through both stalls,
with no visible step at either stall. At this sampling and noise it can neither
support nor exclude an electrical cause.

## Answers

1. **Does the raw IR resolve slow motion? Yes, for the cases in run 8.**
   - At 25 mm/s the stripe edges are clear, and the onboard detector still
     flags them; this is above its 2 Hz limit.
   - At standstill the raw level on a stripe edge resolves wheel movement to
     about 0.05 mm.
   - **Not tested:** motion between about 0.1 and 19 mm/s, where the onboard
     counter fails. Run 8 contains no such motion (both stalls stop within
     about 0.5 s of slowing). Whether the raw level tracks such a creep depends
     on where in the stripe the sensor sits: on a plateau, small motion is
     invisible.
2. **When did the wheel stop at MM109?** At about **606.2 s**, with the Hall at
   about 2,019.
3. **Was Otto creeping during 606–612 s, while the Hall rose 2017 → 2034?
   Not according to the IR car's wheel.** The 15 ADC Hall rise, and the later
   12 ADC fall, happened with the wheel still to within about 0.05 mm.
   - **This changes the reading in the continuity assessment.** The "creep" was
     not motion of the train as measured at the IR car.
   - Its cause is **not established**. Candidates, none tested:
     - movement of Otto relative to the IR car (coupling slack);
     - a non-motion change in Otto's Hall reading while stalled at PWM 32 (for
       example from the motor or its current);
     - drift.
   - **MM023's stall shows no such change** (± 1 ADC over 8 s, at PWM 29–35).

## Consequences for the track test design (for David)

- **Proceed is possible.** The raw IR gives motion truth down to standstill
  wherever the sensor rests on a stripe edge.
- **Add to preconditions:**
  - record the coupling between Otto and the IR car (which side, slack), so the
    "Otto moved, car did not" case can be judged;
  - use the 1st-percentile IR alignment, checked on a departure, as here.
- **Add to the analysis:** classify each stop by IR rest level (edge or
  plateau). Report creep sensitivity only for edge rests.
- **The 0.1–19 mm/s range remains the gap** the test exists to fill. T1 (slowest
  crawl) and T4 (creep) are the trials that can reach it.
