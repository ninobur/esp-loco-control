# Ramp-down travel templates, per station and direction

**Purpose.** Empirical templates of how far Otto actually travels after the
ramp-down signal, built separately for each station and each direction, and
applied to the candidate 200 ms Hall baseline windows in the continuous
September 16 recording.

**Source.** `xhr_20260916_191904.xhr`, session **`C3B93D0B`**, Otto (9950011),
md5 `6f78f5de8cb9116a26e62362baf1e09e`. 1 kHz raw Hall with actual PWM,
commanded PWM, `nav_mm`, `nav_dir` and `st_phase` on every sample. Surveyed
spacing from `RouteMap.h` (mean 304 mm, min 280, max 355). Station geometry from
`Stations.h`.

**Machine-readable output.**

- [`analysis/ramp_templates/ramp_templates_C3B93D0B.json`](../analysis/ramp_templates/ramp_templates_C3B93D0B.json) — 8 templates
- [`analysis/ramp_templates/baseline_windows_C3B93D0B.csv`](../analysis/ramp_templates/baseline_windows_C3B93D0B.csv) — 1,510 windows

**Reproduce.**

```bash
python3 tools/xhr_ramp_templates.py xhr_20260916_191904.xhr --session C3B93D0B --templates t.json --windows w.csv
```

---

## 0. Observed versus estimated

| | what it is | how it is known |
|---|---|---|
| **OBSERVED** | a marker crossing | a sustained 70-count departure in the raw trace, at a measured time; its distance from the previous marker is the **surveyed** spacing |
| **OBSERVED** | resting inside a marker's field | the dwell Hall level sits ≥70 counts off the line, which fixes the resting place to about the field's own width (≈30 mm) |
| **ESTIMATED** | position between two crossings | interpolated |
| **ESTIMATED** | position after the last crossing | bounded above by the next surveyed spacing, because that marker demonstrably was **not** reached |

The ramp-down signal itself is X18's `st_phase` entering `ZERO_RAMP`. It is used
only as **the event the runs are aligned on**, never as a position or a
distance. Its timing is corroborated by the actual PWM: every run steps down one
count per 200 ms, matching `STATION_STOP_STEP_MS` exactly.

### Sources searched and not used

`tools/baseline_laps/out/markers.csv` and `observations.csv` (2026-09-14) carry
marker events and a `moving` flag but **no ramp-down phase and no sub-second
timing**, so no run can be aligned on the ramp signal. The
`20260901_navi_one_*` waveform captures are per-passage slots without continuous
marker timing. The September 16 capture is the only source in the repo with a
recorded ramp-down command, marker timing and actual PWM on one clock.

---

## 1. Usable runs

15 ramp-down episodes, all with a complete PWM ramp and a following dwell.

| station | dir | runs | PWM at signal | ramp to zero | entry speed (mm/s) | sufficient? |
|---|---|---|---|---|---|---|
| Grillers | CW | **3** | 59 | 11.8 s | **91** (90–92) | yes |
| Patio | CW | **2** | 59 | 11.8 s | **113** (113–114) | yes |
| Arches | CW | **3** | 59 | 11.8 s | **131** (126–135) | yes |
| Bamboo | CW | **3** | 59 | 11.8 s | **159** (156–162) | yes |
| Patio | CCW | 1 | 59 | 11.8 s | 117 | **no — single run** |
| Arches | CCW | 1 | 59 | 11.8 s | 120 | **no — single run** |
| Bamboo | CCW | 1 | 59 | 11.8 s | 126 | **no — single run** |
| Grillers | CCW | 1 | 71 | 14.2 s | **170** | **no — single run** |

**Why no pair may stand in for another.** Entry speed spans 91 to 170 mm/s — a
factor of **1.9 between the two directions at one platform**. Grillers CW is the
climb out (91 mm/s); Grillers CCW is the same grade descending, held at 72 PWM
rather than 60 (170 mm/s). Substituting either for the other would misplace the
locomotive by hundreds of millimetres.

---

## 2. The templates

Cumulative distance against elapsed time from the ramp signal. Every row marked
OBSERVED is a marker crossing at surveyed spacing; the final leg is bounded.

### Grillers CW — 3 runs

| leg | elapsed | cumulative | basis |
|---|---|---|---|
| marker 1 | 3.94 s (1 of 3 runs) | 315 mm | OBSERVED |
| to rest | — | **315 – 665 mm** | MIXED: **3 of 3** runs rest inside a marker's field |

All three runs come to rest inside the field of the marker one spacing past the
ramp point (dwell level +203, +199, +130 counts above the line). Two never close
its span at all — they stop *on* it. This is the most tightly constrained
landing of the eight: travel ≈ **one surveyed spacing, 315 mm**, with the spread
being where in the field it settles.

### Patio CW — 2 runs

| leg | elapsed | cumulative | basis |
|---|---|---|---|
| marker 1 | 2.50 s (2.45–2.55) | 310 mm | OBSERVED |
| to rest | — | **310 – 610 mm** | ESTIMATED: next marker not reached; bounded by its 300 mm spacing |

### Arches CW — 3 runs

| leg | elapsed | cumulative | basis |
|---|---|---|---|
| marker 1 | 2.54 s (2.38–2.76) | 305 mm | OBSERVED |
| to rest | — | **305 – 605 mm** | ESTIMATED: bounded by the next 300 mm spacing |

### Bamboo CW — 3 runs

| leg | elapsed | cumulative | basis |
|---|---|---|---|
| marker 1 | 2.08 s (2.01–2.17) | 320 mm | OBSERVED |
| marker 2 | 5.60 s (5.46–5.73) | 620 mm | OBSERVED (2 of 3 runs) |
| to rest | — | **620 – 945 mm** | MIXED: 1 of 3 rests inside the second marker's field |

The longest coast of the four CW platforms, consistent with the highest entry
speed. Mean speed over leg 1 is 154 mm/s and over leg 2 is 85 mm/s — the
deceleration is measurable between the two observed crossings.

### The four CCW pairs — one run each, **insufficient**

| station | leg | elapsed | cumulative | to rest |
|---|---|---|---|---|
| Patio CCW | marker 1 | 2.60 s | 315 mm | 315 – 630 mm |
| Arches CCW | marker 1 | 2.39 s | 300 mm | 300 – 600 mm |
| Bamboo CCW | marker 1 | 1.99 s | 290 mm | 290 – 610 mm |
| Grillers CCW | marker 1 | 1.56 s | 325 mm | |
| Grillers CCW | marker 2 | 3.74 s | 640 mm | 640 – 960 mm |

**Each is a single observation.** No run-to-run variation can be quoted, and
none of them should be used to set a constant.

### Run-to-run variation, where it can be measured

| pair | leg-1 crossing time | spread |
|---|---|---|
| Arches CW | 2.38 / 2.48 / 2.76 s | 0.38 s (15 %) |
| Bamboo CW | 2.01 / 2.17 / 2.08 s | 0.16 s (8 %) |
| Patio CW | 2.45 / 2.55 s | 0.10 s (4 %) |
| Bamboo CW leg 2 | 5.46 / 5.73 s | 0.27 s (5 %) |

Entry speed varies by 2 % (Grillers CW, Patio CW), 4 % (Bamboo CW) and 7 %
(Arches CW) across runs.

---

## 3. Applied to the candidate baseline windows

1,510 candidate 200 ms windows — the collections the cadence rule actually
attempts in this session.

| regime | windows |
|---|---|
| ordinary running | 1,502 |
| inside a Bamboo CW ramp | 3 |
| inside a Grillers CCW ramp | 2 |
| inside a Patio CW / Bamboo CCW / Arches CCW ramp | 1 each |

### Distance covered, ordinary running

| | p1 | p5 | p50 | p95 | p99 |
|---|---|---|---|---|---|
| **mm covered by the 200 ms window** | 24 | 33 | **51** | 60 | 73 |

At the median, the ±13 % speed uncertainty puts the window's travel at
**44 – 57 mm**.

### Position relative to the neighbouring magnets

| | p1 | p5 | p50 | p95 |
|---|---|---|---|---|
| window **start**, mm past the previous magnet's start | 35 | 40 | **48** | 55 |
| window **end**, mm before the next magnet's start | 156 | 174 | **203** | 263 |

Against the measured field extents (from
[NAVI_BASELINE_TIMING](NAVI_BASELINE_TIMING_20260916_C3B93D0B.md), converted to
distance): trailing influence p50 36 mm, p95 40 mm, **max 71 mm**; leading
influence p50 10 mm, p99 18 mm, max 21 mm.

| verdict | windows | % |
|---|---|---|
| clears the p95 field | 1,421 | 94.1 |
| clears only the typical field | 58 | 3.8 |
| **INSIDE the typical field** | **26** | 1.7 |
| **TRAVEL NOT ESTABLISHED** | **4** | 0.3 |
| **CLEARANCE NOT ESTABLISHED** | 1 | 0.1 |
| clears the **worst observed** field (71 mm) | **0** | **0.0** |

**No window in the session provably clears the worst trailing field ever
observed.** The median window starts 48 mm past the previous magnet while the
worst measured tail reaches 71 mm. The rule works because that 71 mm tail is
rare and location-specific — every one of the longest tails is at MM90 — not
because the windows are placed clear of it. That is the travel-based explanation
for the ±13-count residual bias reported earlier.

### The windows that are not suitable

**Inside the typical field (26).** The tightest standoffs:

| t (s) | regime | mm covered | mm past prev | Hall spread |
|---|---|---|---|---|
| 3263.2 | ordinary | 23.5 | **27.1** | 23 |
| 2163.8 | ramp: Bamboo CW | 17.1 | **27.8** | 24 |
| 1343.7 | ramp: Bamboo CW | 17.1 | **27.9** | 22 |
| 1752.3 | ramp: Bamboo CW | 17.1 | **28.0** | 26 |
| 36.4 | ordinary | 22.5 | 31.0 | 23 |

All three Bamboo CW ramp windows are in this group: at 85 mm/s the window covers
only 17 mm and sits 28 mm past the previous magnet, well inside the 36 mm
typical tail.

**Travel not established (4).** All four fall past the last observed crossing of
their template, where the speed is not measured:

| t (s) | station | why |
|---|---|---|
| 1845.2 | Patio CW | past the last observed crossing |
| 2594.3 | Bamboo CCW | past the last observed crossing (and n=1) |
| 2709.6 | Arches CCW | past the last observed crossing (and n=1) |
| 3217.4 | Grillers CCW | past the last observed crossing (and n=1) |

These are exactly the region the templates cannot cover: after the final marker
the locomotive's position is bounded but its speed is not measured, so the
distance a 200 ms window covers there is unknown.

---

## 4. Hall uniformity is not evidence of travel or clearance

Tested directly on the 1,502 ordinary-running windows:

**correlation(Hall spread, mm past the previous magnet) = −0.011**

| Hall spread | n | standoff p5 | standoff p50 | inside the typical field |
|---|---|---|---|---|
| 0–15 counts | 68 | 39 mm | 48 mm | 2 |
| 15–20 | 605 | 40 mm | 48 mm | 6 |
| 20–25 | 758 | 40 mm | 48 mm | 11 |
| 25–33 | 70 | 37 mm | 47 mm | 3 |

**The quietest windows are no further from the previous magnet than the
noisiest.** A steady 200 ms window tells you nothing about how far the
locomotive has travelled or whether it has cleared the magnet behind it — it is
equally consistent with clean line and with resting on a magnet's flat top. It
is reported alongside the travel estimate as a supporting check and never
substituted for one.

---

## 5. Insufficient data — flagged

| pair | runs | what is missing |
|---|---|---|
| **Patio CCW** | 1 | no run-to-run variation; final leg unbounded below |
| **Arches CCW** | 1 | as above |
| **Bamboo CCW** | 1 | as above |
| **Grillers CCW** | 1 | as above, and it is the fastest entry of all eight (170 mm/s) so it matters most |

All four are emitted with `"insufficient": true`. **Patio CW** has 2 runs — usable
but thin.

The whole CCW half of the railway rests on four single observations from one
evening. Nothing here supports a CCW constant.

---

## 6. What this does and does not support

**Supported:**

- the eight templates as *descriptions of this session*, with observed crossings
  separated from bounded estimates throughout
- entry speeds differing by 1.9× between directions at one platform
- Grillers CW's landing, the best-constrained of the eight: three of three runs
  rest inside one marker's field, one spacing (≈315 mm) from the ramp point
- the window audit: 94.1 % clear the p95 field, 1.7 % sit inside the typical
  field, and **none** clears the worst observed tail
- that Hall uniformity carries no information about clearance (r = −0.011)

**Not supported:**

- any CCW constant (n = 1 for all four pairs)
- the resting position of any run that stops between magnets — bounded to one
  spacing, roughly 300 mm, and no better
- the *time* the locomotive comes to rest: once it is between magnets the Hall
  has nothing further to say, so the stop time is not observable from this
  recording
- anything about other locomotives, other loads, or other days; the grade
  compensation in `RouteMap.h` and the station offsets in `Stations.h` have both
  been changed repeatedly, so these templates are tied to the configuration
  flown on 2026-09-16

---

*Analysis: `tools/xhr_ramp_templates.py`. No firmware was modified. X18's
`st_phase` was used only as the recorded ramp-down signal; its navigation
rulings were not examined.*
