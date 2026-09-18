# Hall selectivity and gate timing, from Otto's 2026-06-30 calibration

Date: 2026-09-18
Source: `Calibration Data OTTO with two 2 axle passenger cars/`
(operator-supplied: `cal_9950011_20260630_113040`, `_131620`, `_135528`;
`_135528_full` is byte-identical to `_135528` and was not counted twice)

Read-only analysis. No firmware changed.

**5,021 confirmed marker passages, all 171 markers, PWM 32–240, speeds to
1,124 mm/s.** This is a physical calibration of the locomotive and the magnets,
not a navigation-architecture record, which is why it survives the rule against
fitting the new design to old mixed telemetry: nothing here is about how a
navigator behaved.

Vintage matters and is treated explicitly in §4 below.

---

## 1. Gate timing by PWM — the direct answer

For each PWM band, the fastest credible passage observed, and the minimum
marker-to-marker interval that speed makes physically possible over the shortest
mapped spacing (280 mm) and the typical one (300 mm).

| PWM | n | v median | v p99 | v max | min dt @280 mm | @300 mm |
|---:|---:|---:|---:|---:|---:|---:|
| 40–49 | 486 | 69.5 | 134.2 | 156.0 | 1795 ms | 1923 ms |
| 50–59 | 480 | 102.2 | 172.4 | 193.1 | 1450 ms | 1554 ms |
| 60–69 | 554 | 147.2 | 212.8 | 238.9 | 1172 ms | 1256 ms |
| 70–79 | 312 | 182.1 | 251.3 | 271.7 | 1031 ms | 1104 ms |
| 80–89 | 420 | 224.6 | 281.7 | 303.3 | 923 ms | 989 ms |
| **90–99** | 277 | 239.8 | 303.6 | **322.6** | **868 ms** | **930 ms** |
| 100–109 | 296 | 298.9 | 362.8 | 372.2 | 752 ms | 806 ms |
| 110–119 | 126 | 330.6 | 411.5 | 414.4 | 676 ms | 724 ms |
| 120–129 | 273 | 386.1 | 454.5 | 487.8 | 574 ms | 615 ms |
| 130–139 | 283 | 421.9 | 533.8 | 548.4 | 511 ms | 547 ms |
| 140–149 | 219 | 465.1 | 532.9 | 541.5 | 517 ms | 554 ms |
| 150–159 | 118 | 485.0 | 657.9 | 709.7 | 395 ms | 423 ms |
| 160–169 | 196 | 554.5 | 621.1 | 626.3 | 447 ms | 479 ms |
| 180–189 | 223 | 622.4 | 721.2 | 738.9 | 379 ms | 406 ms |
| 200–209 | 278 | 697.7 | 855.1 | 1124.6 | 249 ms | 267 ms |
| 220–229 | 202 | 779.4 | 864.6 | 885.0 | 316 ms | 339 ms |
| 240–249 | 167 | 842.7 | 996.7 | 1123.6 | 249 ms | 267 ms |

**The design consequence.** A *fixed* guard that is valid at every PWM must be no
longer than about **250 ms** (280 mm ÷ 1,124 mm/s), or **291 ms** if the p99.9
figure of 961.5 mm/s is preferred over the two outlying maxima. Both are far
shorter than the 645 ms the X21 formulation carried, and short enough that a
fixed guard stops being a useful instrument.

A guard long enough to be useful — 868 ms at PWM 90 — can only be justified by
conditioning on PWM. **Prospectus §8.4 explicitly puts that in judgment, not in
hard protection:** "Hard protection must not claim impossibility merely because a
recent speed estimator or PWM model predicts slow motion."

So this calibration does not rescue a long fixed guard. It says the opposite: the
hard-reachability layer of §6 is worth about 250 ms, and everything the old
645 ms guard was doing above that belongs in §11 judgment, where PWM is admissible
evidence.

## 2. §6.1's Vmax figure is low by more than a factor of two

The prospectus cites "approximately 417 mm/s" as the observed ceiling. Otto,
**pulling two 2-axle passenger cars**, reached:

- **842.7 mm/s** median at PWM 240–249
- **996.7 mm/s** at p99
- **1,123.6 mm/s** fastest credible single passage

417 mm/s is roughly what this locomotive does at PWM 110. As a lower bound on the
physical ceiling it is not merely conservative, it is wrong by more than 2×, and
any hard-reachability arithmetic built on it would declare reachable observations
impossible.

§6.1 asks for a kinematic bound from motor no-load speed, gearing and wheel
circumference. Until that exists, the defensible interim lower bound from
measurement is **1,124 mm/s**, not 417.

## 3. Making the Hall read magnets and nothing else

### The separation was never subtle

Peak amplitude of genuine reads, 5,021 passages, every marker, every speed:

| | counts |
|---|---:|
| weakest read in the entire corpus | **132** (MM162) |
| p1 | 169 |
| p5 | 191 |
| median | 250 |

Genuine reads rejected by a candidate entry threshold:

| threshold | genuine reads rejected |
|---:|---|
| 70 counts | 0 of 5,021 |
| 90 | 0 |
| 100 | 0 |
| 110 | 0 |
| 120 | 0 |
| **130** | **0** |
| 150 | 9 (0.18%) |

Every phantom quoted anywhere in the record sits at **38–66 counts**. Genuine
reads start at 132. **In June there was a clear band from 66 to 132 with nothing
in it** — better than 2:1 separation, and a 130-count entry threshold would have
cost nothing.

### Amplitude does not decay with speed

| speed band | n | peak p50 | peak p5 | min | below 70 |
|---|---:|---:|---:|---:|---:|
| 0–149 | 1272 | 255 | 196 | 150 | 0 |
| 300–449 | 858 | 250 | 188 | 134 | 0 |
| 600–749 | 492 | 245 | 184 | 151 | 0 |
| 750–899 | 351 | 243 | 184 | 147 | 0 |
| 900–1049 | 19 | 236 | 154 | 154 | 0 |

At 1,000 mm/s a 34 mm field is still under the sensor for ~34 ms — 34 samples at
1 kHz. Amplitude is not speed-limited anywhere on this railway. A high entry
threshold costs nothing at speed.

### Two discriminators that do NOT work, tested and rejected

**Apparent magnet width is not constant.** Fitting `hallms × v = a·v + b` over
4,982 passages:

```
  fixed detector overhead  a = 19.1 ms
  true magnet field width  b = 34.4 mm
  residual sd 7.3 mm  (raw width sd 8.5 mm)
```

The model is physically sensible — a 34 mm magnet plus 19 ms of detector
overhead — but the residual is 21% of the width, so width discriminates poorly.
Naive `hallms × speed ≈ constant` is falsified: apparent width rises from 35.3 mm
at low speed to 48.5 mm at 800–900 mm/s. It is also morphology, which §5.3 bars
from navigation authority. Recorded here so the idea is not proposed again.

**Polarity purity is not a discriminator.** 951 of 4,982 genuine passages
(**19.1%**) register on both poles. The minor pole reaches p50 6, p90 34, max 76
counts. A real magnet routinely produces a small opposite-pole excursion, so
"only one pole may respond" would reject one passage in five.

### What actually works: amplitude, with the margin restored

The June corpus says the detector has no discrimination problem when the signal
is healthy. The problem is that the signal stopped being healthy.

| | Otto peak, correct reads |
|---|---:|
| 2026-06-30 (this corpus) | **250** median, 132 minimum |
| 2026-08-20 (`field-records/20260820_OTTO_HALL_SENSOR_WEAK.md`) | **146** median; 102 on wrong reads |

Otto was red-tagged that day for a weak Hall sensor. His median signal had fallen
**42%**. That record independently exonerates the map and the magnets: 7,625
reads, not one marker consistently disagreeing, and Toby on the same track in the
same session reading a median of 189 with a 0.44% error rate against Otto's 2.69%.

This is what the mm100–125 failure was. Otto's profile records that a 90-count
gate cost him four markers around mm 100–125 on 2026-08-20. **In June that
stretch was nowhere near 90 counts:**

| MM | 100 | 103 | 107 | 110 | 112 | 116 | 118 | 122 | 125 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| June minimum | 218 | 209 | 209 | 206 | **144** | 169 | 163 | 180 | 192 |

Every marker in mm100–125 had a June floor of 144 or better. A 90-count gate
there was not aggressive in June; it had 54 counts of margin at the worst marker.
By August the same gate was rejecting real markers. **The gate did not change.
The signal did.**

### The recommendation

Restore the signal margin; do not get cleverer in software.

1. **Re-measure first.** Nothing here sets a threshold for today's hardware. Otto
   has had a new board since (`20260910_OTTO_NEW_BOARD_PIN_AND_NAVI_IR_LINEAGE.md`).
   The measurement needed is the one this corpus is: every marker, both
   directions, a range of PWM, recording peak amplitude per passage.
2. **Set the entry threshold from the measured per-marker floor, with margin.**
   If the repaired hardware returns June-like values, the floor is ~132 and a
   threshold near 100–110 has 20–30 counts of headroom while putting every
   phantom ever observed (38–66) far outside. That is roughly 1.5× today's 70.
3. **Fix the weak markers physically rather than lowering the gate for them.**
   The June floors identify them: MM162 (132), MM56 (141), MM28 (144), MM112
   (144), MM51 (147), MM91 (149), MM42 (151), MM152 (153), MM157 (153).
   `field-records/20260828_MM128_PROGRESSIVE_MAGNET_FAILURE.md` and
   `20260828_MM152_STABLE_WEAK_NOT_A_FAULT.md` already track two of these.
4. **Treat a falling floor as an instrument fault, not a tuning problem.** A
   per-marker peak floor published in telemetry makes sensor decay visible before
   it becomes a navigation failure. Otto's decay was only diagnosed after it
   stopped the railway twice.

## 4. Vintage, stated plainly

This is 2026-06-30 data. Otto's Hall sensor subsequently degraded and the board
has been replaced. Nothing in §3 should be used to set a threshold on current
hardware.

What survives the vintage:

- **§1 and §2 are locomotive physics** — PWM against speed, and the minimum
  interval that speed permits. A replaced Hall board does not change how fast the
  locomotive goes at PWM 240. These are usable now, and the consist (two 2-axle
  passenger cars) makes the speeds conservative rather than optimistic.
- **§3's method and its two negative results** — width and polarity purity
  fail as discriminators — are properties of magnets and sensors in general.
- **§3's numbers are a target, not a setting.** They show what "healthy" looked
  like on this railway, which is what the new measurement should be compared
  against.

## Method

Every line of all three files is a marker record; the format is fixed and was
parsed with one regex. Records were kept only where `pos=EXACT_LOCKED`,
`obs==expected`, `timing_mode=MOVING`, `seg_mm` in 250–360, and `dt>0`. For the
speed tables an additional artefact filter required the passage speed to lie
within 0.6–1.6× the median of its four neighbours, which removed 572 records
(double counts appear as an isolated short `dt` between normal ones — the same
failure that produced a false 801.6 mm/s in `NAVI_VMAX_PWM90_20260918.md`).
Peak is `max(peakN, peakS)`. `hallms` is the locomotive's own measure of how long
the line was open.
