# How much does the ordinary-track Hall level change between adjacent magnet intervals?

**Log:** `all_20260916.log`, Otto 9950011, 2026-09-16 11:14–12:11.
**Tool:** `tools/hall_interval_levels.py all_20260916.log --csv docs/data/20260916_hall_interval_levels`
**Tables:** `docs/data/20260916_hall_interval_levels/{levels,pairs,spans}.csv`

No firmware was changed and no detector is proposed here.

---

## 0. The short answer

In **ordinary running the level moves by 0–5 counts from one interval to the
next**, against a 70-count opening threshold and a weakest genuine ordinary
magnet of 107 counts. On that evidence, carrying the last trusted baseline
through one more interval is **supported**.

But the evidence is **15 pairs out of 678 intervals — 3.2 % of the
adjacencies**, and **fourteen of the fifteen are the weaker kind of
measurement**. The session also contains one **+87-count move in 50 s of
continuous running** that the log samples only five times, so it cannot say
whether that move was 39 small steps or one large one. The answer is
*supported and thin*, not *established*.

---

## 1. Segmentation

Four boots. Only the fourth carries any magnet traffic.

| boot | line | time | sketch |
|---|---|---|---|
| 1 | 2 | 11:14:57 | `NAVI_ONE_1_0X20_DWELL_FIELDTEST` |
| 2 | 2711 | 11:20:47 | `NAVI_ONE_1_0X20_DWELL_FIELDTEST` |
| 3 | 2976 | 11:22:27 | `NAVI_ONE_1_0X21_HALL_ONLY_FIELDTEST` |
| **4** | **3276** | **11:25:22** | **`NAVI_ONE_1_0X21_HALL_ONLY_FIELDTEST`** — all 690 detections |

Boot 4 is **one direction only**: `session_direction CCW` at 11:25:33, and all
690 `diag/excursion` records carry `"dir":"CCW"`. **There is no reversal in this
log, so directions cannot be separated.**

690 detections, every one accepted (`outcome MAGNET`, `is_magnet 1`); 679
`ADVANCED`, 6 `WRONG_MAGNET`, 5 `NO_POSITION`. Seven `start_interval` +
`auto 1` declarations, each ended by a strike.

Continuity is broken at a boot, a `session_direction`, a `start_interval`, an
`auto 0`, or a `WRONG MAGNET` warning. **678 of the 689 adjacencies survive**
that rule and are the population for everything below.

---

## 2. Where raw Hall actually exists

Two sources carry raw counts. Neither is continuous.

### `diag/waveform` — the only source for ordinary track

90 records, **all complete**, 913 samples each at 1 kHz: 512 ms of pre-roll
before the detection sample plus the 400 ms acquisition window. Raw counts come
back exactly, because `WaveformDump.h` stores `buf[i] = raw[i] − localRef`
negated when the pole is S:

```
raw[i] = localRef + (polarity ? s[i] : −s[i])
```

`localRef`, `reference` and `shadowRef` are carried in the header and are used
below **only as cross-reference**. Every level in this document is a median of
recovered raw counts.

**Total continuous raw coverage: 90 × 913 ms = 82.2 s of a 2728 s run — 3.01 %.**

The sampling rule (`NAVI_ONE_X20.ino:1209`) dumps *every refused* candidate plus
*one accepted* candidate per `X19_WAVEFORM_SAMPLE_MS = 15000`. **Nothing was
refused all session**, so the 90 records are the 15-second samples plus the
five-slot trailing windows dumped at strikes. That is why consecutive records
are rare and clustered.

### `diag/departure` — station ramps only

649 records at 10 Hz. `departureDiagActive = stationMachine.phase() ==
StPhase::Depart` (`NAVI_ONE_X20.ino:1736`), so it fires **only inside a station
DEPART phase**: 15 bursts of 5–6 s, confined to MM12–14, MM61–62, MM106–107 and
MM155–157. It is a record of the departure ramp and contains **no ordinary
track at all**. It is reported here and used for nothing else.

`diag/dwell` (54 records) and `diag/excursion`/`mm/marker` (`raw_detect`) give
single raw samples, at a dwell transition or inside a magnet. Neither can
establish a level.

---

## 3. What counts as ordinary track

**Ordinary track is flat.** A magnet is steep; a station ramp and the climb out
of a displaced field are steady drifts. One test rejects all three:

> A sample is usable only if it sits at the centre of a **101 ms window whose
> 21 ms moving mean spans ≤ 10 counts**, and only if the **40 ms between it and
> the detection instant** are also quiet. Erosion is one-sided, toward the
> magnet: a pre-roll sample needs quiet *after* it, a tail sample quiet *before*
> it.

Grillers' departure ramp drifts ≈220 counts/s = 22 counts per 100 ms and fails.
1 kHz Hall noise on straight track spans 4–8 counts and passes — the resulting
levels have **median MAD of 1 count** over 120–339 samples.

A level is accepted from a pre-roll with ≥120 usable ms and from a tail with
≥40 usable ms. In practice the last usable pre-roll sample is **−100 to −447 ms**
before detection (median −171) and the first usable tail sample is **+151 to
+350 ms** after it (median +267).

### Flatness is not sufficient — the Grillers exclusion

Grillers parks Otto **on** a magnet. Every `DWELL_BEGIN` there reads 2141–2163
against ~1950 everywhere else, and the shelf is perfectly flat. At 11:46:54 the
detector's own `local_ref` had migrated to **2121** while the last measured
ordinary level two intervals earlier was **1953**.

**Intervals touching MM57–67 are therefore reported as their own class and are
never folded into "ordinary".** Station APPROACH / ZONE / ZERO_RAMP / DEPART,
`pwm < 45`, and a PWM drop of more than 8 between the bounding detections are
separated the same way.

Interval classes across the 678: ORDINARY 455, APPROACH 132, GRILLERS 46,
ZERO_RAMP 22, DEPART 22, SLOW 1.

### Intervals whose level cannot be established

**607 of 678 (89.5 %).** The log does not sample them.

Separately, **24 of the 90 waveform records yield no level at either end**, and
**8 of those contain no flat region anywhere in their 913 ms** — they are a
single monotonic climb, one of them running from −56 to +147 counts across the
whole record. All 8 are ramps: MM61 on the Grillers zero ramp (×4), MM106 (×3)
and MM156 (×1) on station departures. They are flagged, never estimated.

---

## 4. Coverage, stated plainly

| | count | of 678 |
|---|---|---|
| intervals with an **end** level (a pre-roll) | 55 | 8.1 % |
| intervals with a **start** level (a tail) | 20 | 2.9 % |
| intervals with **any** raw level | **71** | **10.5 %** |
| …of those, ORDINARY class | 42 | |
| **adjacencies measured as a pair** | **22** | **3.2 %** |
| pair measurements (some adjacencies have two) | 26 | |

Two kinds of pair exist, and they are not equally good:

* **Type A (18 of 26).** One record straddles both intervals: its **pre-roll**
  ends interval *k*, its **tail** opens interval *k+1*. The two levels are only
  366–566 ms apart and sit either side of a single magnet.
* **Type B (8 of 26).** Two records at consecutive detections, **pre-roll to
  pre-roll**, 1.1–2.0 s apart — a genuine full-interval separation. These exist
  only inside the five-slot windows dumped at strikes.

---

## 5. The measured change

| regime | n | median &#124;Δ&#124; | p90 | p99 | max | signed range |
|---|---|---|---|---|---|---|
| **ORDINARY** | **15** | **3** | **5** | **5** | **5** | **−5 … +4** |
| ORDINARY/APPROACH | 3 | 1 | 2 | 2 | 2 | −2 … +1 |
| APPROACH | 2 | 0 | 4 | 4 | 4 | 0 … +4 |
| GRILLERS | 6 | 5 | 6 | 12 | 12 | −5 … +12 |
| DECEL, SLOW, DEPART, ZERO_RAMP | **0** | — | — | — | — | — |

**Deceleration, slow movement and station departures produced no usable pair at
all.** The question is unanswered for those regimes.

### The Type A bias is real and it is instrumental

Fourteen of the fifteen ORDINARY pairs are Type A, and their signed values skew
negative (median **−3**): `−5 −5 −5 −4 −4 −3 −3 −3 −1 −1 0 +1 +3 +4`.

Four intervals have **both** a tail (from record *k*) and a later pre-roll (from
record *k+1*) — the same interval measured twice:

| interval | tail | later pre-roll | difference |
|---|---|---|---|
| MM120→119 | 1983 | 1984 | +1 |
| MM119→118 | 1983 | 1985 | +2 |
| MM118→117 | 1985 | 1989 | +4 |
| MM64→63 (Grillers) | 1994 | 1985 | −9 |

The tail reads **1–4 counts low** in ordinary running. Loosening the flatness
test to 16 counts admits tails from +229 ms and the largest ordinary changes
grow to −13, −13, −11, −10 — **all negative, all Type A, all with early tails**.
That is residual field on the trailing edge, not a change in the line.

**Corrected for it, the ordinary interval-to-interval change is ≤ ~3 counts, and
the single unbiased Type B ordinary pair reads 0.**

### The eight Type B pairs, in full

| k | regime | MM | level a → b | Δ | separation |
|---|---|---|---|---|---|
| 90 | ORDINARY | 121→120 / 120→119 | 1984 → 1984 | **0** | 1090 ms |
| 91 | ORDINARY/APPROACH | 120→119 / 119→118 | 1984 → 1985 | +1 | 1302 ms |
| 92 | APPROACH | 119→118 / 118→117 | 1985 → 1989 | +4 | 1137 ms |
| 147 | GRILLERS | 65→64 / 64→63 | 1990 → 1985 | −5 | 1871 ms |
| 148 | GRILLERS | 64→63 / 63→62 | 1985 → 1997 | **+12** | 1977 ms |
| 323 | GRILLERS | 64→63 / 63→62 | 1948 → 1953 | +5 | 2005 ms |
| 495 | GRILLERS | 64→63 / 63→62 | 1953 → 1957 | +4 | 2013 ms |
| 667 | GRILLERS | 64→63 / 63→62 | 1953 → 1959 | +6 | 1960 ms |

**Exactly one clean ordinary full-interval comparison exists in this log**, and
it reads zero.

### Sensitivity

| flatness span | guard | usable pre-rolls | usable tails | ordinary pairs | median | max |
|---|---|---|---|---|---|---|
| 5 | 40 | 25 | 2 | 1 | 0 | 0 |
| 8 | 40 | 51 | 16 | 9 | 3 | 5 |
| **10** | **40** | **63** | **22** | **15** | **3** | **5** |
| 16 | 40 | 84 | 54 | 31 | 4 | 13 |
| 25 | 40 | 89 | 57 | 31 | 4 | 14 |
| 10 | 80 | 50 | 13 | 8 | 2 | 7 |
| 10 | 120 | 38 | 3 | 2 | 3.5 | 3.5 |

The maximum is **3.5–14 counts** across the whole band. The upper end is bought
by admitting magnet fringe into the tail, as §5 shows; the lower end is bought
by measuring almost nothing.

---

## 6. The apparent large jumps

### The one that matters: +87 counts in 50 s of continuous running

| clock | MM | level | MAD | n |
|---|---|---|---|---|
| 11:28:46 | 156 | 1903 | 2 | 305 |
| 11:29:01 | 144 | **1919** | 1 | 336 |
| 11:29:16 | 132 | **1973** | 1 | 297 |
| 11:29:31 | 120 | 1984 | 0 | 339 |
| 11:29:33 | 119 | 1984 | 0 | 177 |
| 11:29:34 | 118 | 1985 | 1 | 248 |
| 11:29:35 | 117 | 1989 | 1 | 259 |
| 11:29:36 | 117 | 1990 | 1 | 249 |

This is **a real change in the ordinary-track level**, and it is none of the
alternatives:

* **Not magnet fringe.** Every trace is flat to MAD ≤ 2 counts over 250–340 ms.
* **Not a stop.** PWM 90 throughout, `telem/speed` 229–268 mm/s, `station IDLE`.
* **Not a navigation error.** All eight `ADVANCED`.
* **Not missing samples.** All records complete.
* **Not a change of boot or reference.** One boot, one epoch; the fixed
  `reference` is 1903 for the entire session.
* **Not supply.** `telem/voltage` is 16.37–16.42 across the window and
  16.30–16.46 across the session. Motor current rises 0.13 → 0.31 A at 11:28:46
  as Otto leaves the Southpoint ramp, and stays there for the rest of the run —
  including the later laps where the level does *not* move like this.
* **Not spatial.** The same places read differently on later laps:
  MM144 = 1919 / 1945 / 1950 / 1963 and MM132 = 1973 / 1943 / 1951 / 1963 on the
  four laps. The 11:29 lap is the odd one.

`shadowRef` — the firmware's own rolling median, computed independently of this
analysis — tracks it: 1901, 1919, 1974, 1984, 1984, 1984, 1984, 1989.

**The span MM144→MM132 is +54 counts over 12 intervals in 15.4 s: an average of
4.50 counts per interval.** The log samples it twice. **It cannot distinguish
twelve steps of 4.5 counts from one step of 54.** This is the single largest
limit on the whole answer and it is left visible rather than folded into a
percentile.

### Average per-interval change over every sampled span

Consecutive pre-roll levels within one epoch, |Δlevel| ÷ intervals crossed:

| spans | n | median | p90 | max |
|---|---|---|---|---|
| uninterrupted (no station, no PWM < 20) | 15 | 0.17 | 1.38 | **4.50** |
| all spans | 53 | 0.23 | 4.50 | 94.00 |

**This bounds an average, never a single step inside a span.**

### The other outliers, each accounted for

| where | apparent Δ | verdict |
|---|---|---|
| MM62→60, 11:46:11→11:46:54 (`k` 325→327, lines 15295–15300) | **+188 over 2 intervals** | **Displaced field, not track.** `local_ref` migrated to 2121 during the 12-second zero ramp; raw at rest 2141. Excluded as GRILLERS. |
| MM64→63→62, four laps (Type B `k` 147/148/323/495/667) | +4 … +12 | Grillers ramp. The level is genuinely rising as Otto descends into the field. Real, but not ordinary track. |
| MM19→13, 11:41:03→11:41:58 | −14 over 6 intervals | Station approach and dwell at Southpoint inside the span. Interrupted; excluded from the clean statistic. |
| MM110→94, 12:05:59→12:07:04 | −13 over 16 intervals | Arches stop inside the span. Interrupted. |
| MM90→77, 11:36:37→11:36:52 | +18 over 13 intervals | Same 11:29–11:38 high-level episode. Uninterrupted; 1.38 counts/interval average. |
| MM60, 11:46:54 = 2141 | — | Otto standing in the Grillers field. Flagged, never used as a level. |

### A finding that falls out of this

`baseline_mode` is `fixed_no_lap`; the primed `reference` was **1903** and never
moved. The measured ordinary level ranged **1903 → 1997** in the same session.
**The fixed baseline was wrong by up to 94 counts — more than the 70-count
opening threshold — within four minutes of the prime.** The two
`diag/baseline_lap` estimates (1948 at 11:58, 1954 at 12:08) agree with the raw
measurement and were not applied.

---

## 7. Margin against the 70-count threshold

The opening rule is a departure of **≥70 counts held for two consecutive 1 kHz
samples while moving** (`NAVI_ONE_X20.ino:199`).

| population | n | weakest peak | p1 | p5 | median |
|---|---|---|---|---|---|
| all accepted detections (firmware `peak`, vs its own `local_ref`) | 690 | **79** | 105 | 135 | 171 |
| ORDINARY-regime detections only | 473 | **107** | 120 | 138 | 172 |
| the 63 sampled records, peak vs the **measured ordinary level** | 63 | **134** | 134 | — | 179 |

Only 6 of 690 have a peak below 100, and every one is at a station approach,
zero ramp or departure — never in ordinary running.

**Detection margin if the preceding interval's baseline were carried forward:**

* ordinary running: **107 − 70 = 37 counts** on the weakest magnet in the class.
  Measured interval-to-interval change is ≤ 5 counts, and ≤ 3 once the Type A
  tail bias is removed. The margin is **7× to 12×** the measured change, and
  still **8×** the 4.50 counts/interval worst-case *average* of §6.
* anywhere in the session: **79 − 70 = 9 counts**, at MM69 on a station approach
  (idx 143, 11:37:01). A carried error of 10 counts in the adverse direction
  would have missed that magnet. No ordinary pair came close to 10.

**Polarity margin.** Polarity is the sign of the departure at the detection
sample. Against a carried baseline the measured departure is
`true_departure − Δ`, so the sign inverts only when `|Δ| > |true_departure|`,
which is **≥70 by construction and ≥107 in ordinary running**. A 5-count carry
error cannot invert a pole — **a 21× margin on the weakest ordinary magnet.**

**Observed cases where carrying forward would miss a magnet or invert a pole:
none in ordinary running.** The only failures in the log are at Grillers, and
they are analysed in §8.

---

## 8. Conclusion

### Carrying the last trusted baseline through one additional interval

**Supported by this log, and thinly.** Nothing in it contradicts the rule in
ordinary running. The measured change is 0–5 counts (≤3 after bias correction)
against a 37-count margin on the weakest ordinary magnet and a ≥107-count
margin on polarity.

The support is thin in three specific ways, and none of them is closed here:

1. **15 pairs out of 678 intervals — 3.2 %.** Fourteen are Type A straddles of a
   single magnet, which carry a 1–4 count tail bias; **one** is a clean
   full-interval comparison, and it reads 0.
2. **The 11:29 episode moved the level 87 counts in 50 s of continuous running
   and was sampled five times.** If that move was stepwise rather than gradual,
   a single interval in this log carried a change larger than the whole margin,
   and the log cannot tell. **The small maximum in §5 must not be read as a
   property of the intervals the log did not sample.**
3. **Deceleration, slow movement and station departures have zero usable pairs.**
   The rule is untested there, and those are exactly the regimes where the six
   sub-100-count magnets live.

### Two problems this does *not* answer

* **Obtaining the first trusted baseline.** Separate and unsolved. The primed
  fixed `reference` of 1903 was up to 94 counts wrong within four minutes (§6),
  which is more than the opening threshold. Nothing about carrying a baseline
  forward helps acquire one.
* **Leaving a magnet after a stop.** At Grillers the last ordinary level before
  the stop was 1953 and Otto came to rest at 2141. Carrying 1953 through the
  dwell would have held a **standing +188-count departure for the entire
  30 s stop**, and at 11:46:54 would have read the fall back to the line as
  `2047 − 1953 = +94`, an **N** opening where the migrated reference produced an
  **S** one — a different wrong answer, not a right one. **Carry-forward does not
  fix Grillers and is not evidence about it.** That is the displaced-field
  problem of `20260916_OTTO_X21_GRILLERS_REST_MIGRATES_ON_THE_RAMP.md`.

### What would settle it

One field run with `X19_WAVEFORM_SAMPLE_MS` reduced so that consecutive accepted
candidates are both dumped. Fifty consecutive Type B pairs in ordinary running,
in both directions, would replace all fifteen measurements above.

---

## Reproducing

```bash
tools/hall_interval_levels.py all_20260916.log --csv docs/data/20260916_hall_interval_levels
```

Outputs `levels.csv` (85 accepted levels, with the usable window offsets and the
source log lines of each waveform chunk), `pairs.csv` (the 26 pair measurements),
`spans.csv` (the 53 consecutive-level spans of §6).
