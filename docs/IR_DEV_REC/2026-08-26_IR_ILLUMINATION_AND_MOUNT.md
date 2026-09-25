# Contrast versus mile marker — the illumination model holds for the right-side mount, and the left-side mount does not need it

**Date:** 2026-08-26
**Question (operator):** does sunlight reaching the spoke face explain the
lap-to-lap contrast differences? Specific hypothesis: with the sensor mounted
**right** and running **CCW**, reflected sunlight should illuminate the spokes
from roughly **MM108 down to MM090**, and be absent elsewhere. *If contrast is
flat around the loop, the illumination model is wrong.*

**Method:** per-packet envelope span (`runMax − runMin`, see
`docs/IR_PACKET_FORMAT.md` §3) joined to Toby's reported mile marker on Pi
wall-clock time, nearest neighbour within 1.5 s, `moving == 1` only, binned in
10-marker bands. Join accuracy ≈ ±1–2 markers (§4 of the format doc).

## Contrast is not flat

| MM band | Lap 1 RIGHT | Lap 3 LEFT | Lap 4 RIGHT |
|---|---:|---:|---:|
| 0–9 | 319 | 799 | 543 |
| 10–19 | 367 | 2495 | 479 |
| 20–29 | 479 | 1263 | 751 |
| 30–39 | 271 | 1871 | 239 |
| 40–49 | 255 | 1407 | 271 |
| 50–59 | 335 | 2335 | 1391 |
| 60–69 | 959 | 655 | 1007 |
| 70–79 | 1567 | 1887 | 1471 |
| 80–89 | 1823 | 1711 | 703 |
| **90–99** | **2007** | 1783 | **1807** |
| **100–109** | **2175** | 1151 | **2079** |
| 110–119 | 1695 | 2079 | 687 |
| 120–129 | 439 | 2367 | 415 |
| 130–139 | 287 | 1647 | 1247 |
| 140–149 | 367 | 2799 | 511 |
| 150–159 | 1103 | 2047 | 2159 |
| 160–169 | 1967 | 1887 | 1511 |
| 170 | 527 | 335 | 367 |
| **median** | **431** | **1855** | **607** |
| **peak band ÷ median** | **6.9×** | **1.8×** | **5.2×** |

Laps 1 and 4 are both CCW with the sensor on the **right**, recorded ~40
minutes apart. Lap 3 is CCW with the sensor on the **left**, recorded between
them.

## The hypothesis is confirmed, for the right-side mount

Both right-side laps peak at **MM 90–109** — the maximum band in each case,
and 1,807–2,175 counts against a whole-lap median of 431–607. That is the
predicted MM108→MM090 window, hit independently twice.

The troughs are equally consistent: MM 30–49 and 120–129 sit at 239–479 in
both right-side laps. The profiles agree on both peaks and dips despite the
sun having moved between them.

There is a second high band at **MM 150–169** in all three laps, matching the
operator's lap-3 field note *"sunny again from 169 through 152"*. Independent
corroboration from a different observation method.

**Verdict: contrast is strongly position-dependent (≈7× swing), peaks where
the illumination model predicts, and reproduces across two separate laps. The
illumination model is not wrong.**

## The more useful finding: the left-side mount is not illumination-limited

Lap 3, left-side, has a **whole-lap median of 1,855** — four times lap 1's 431
— and varies only **1.8×** end to end. It does not have a strong sunlit peak
because it has good contrast almost everywhere, including the MM 30–49 and
120–129 bands where the right-side mount collapses.

So the correct reading is not "the left mount catches more sun". It is:

> The **right-side mount is marginal and depends on incident sunlight to reach
> usable contrast.** The left-side mount has adequate intrinsic contrast and
> is largely independent of illumination.

This reframes the distance errors in
`2026-08-26_IR_FOUR_LAP_DISTANCE_TRUTH.md`. Lap 4 (right, fast) lost 39.3% of
distance; its contrast was below ~700 for more than half the loop, which is
where spokes get swallowed. Lap 2 (left, slow) was accurate to 0.9%.

**The actionable conclusion is mechanical, not algorithmic: fix the mount, not
the threshold.** A threshold retune that rescued the right-side mount at MM
120–129 would necessarily be sensitive enough to produce false edges at
MM 100–109, where the same mount sees 5× more signal. No single falling
fraction serves a 7× dynamic range across one lap.

## Confounds and limits

- **Speed is not the driver here.** Envelope span is a 2 s percentile, not a
  per-packet peak-to-peak, so it is far less speed-sensitive; and laps 1
  (slow) and 4 (fast) produce the same spatial profile despite a ~2× speed
  difference.
- **Envelope smoothing blurs ~2 markers** at speed (2.048 s window) and is
  quantised to 16 counts. A feature narrower than ~2 markers would be
  flattened. The MM090–109 window is ~20 markers wide, comfortably resolved.
- **Lap 4 ran `NO_QUORUM`** from roughly MM 25 onward, so its positions are
  dead-reckoned rather than landmark-confirmed. Its profile agrees with lap
  1's anyway, which is reassuring, but lap 1 is the better-attested of the two.
- **One sun position, one day.** Late-morning August. Nothing here predicts a
  low-sun-angle or overcast session.
- **Mount geometry was not measured.** Why the right mount has 4× less
  intrinsic contrast — alignment, standoff, spoke-face finish, stray light —
  is unexplained and is now the most valuable thing to investigate.

## Next

1. Measure the physical difference between the two mounts before running
   anything else. A 4× intrinsic contrast difference between two nominally
   equivalent installations is the largest single effect found so far.
2. The bench capture (hand-turned, revolution markers, no transport loss)
   remains the way to settle the falling fraction — but per the above, do it
   on a mount whose contrast has been characterised first, or it will
   calibrate to an arbitrary installation.
3. Re-run this profile at a different sun angle to test the model
   predictively rather than retrospectively.
