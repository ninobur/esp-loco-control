# Ramp-down travel from the regular run logs

**Correction first.** [NAVI_RAMP_TRAVEL_TEMPLATES_20260916_C3B93D0B.md](NAVI_RAMP_TRAVEL_TEMPLATES_20260916_C3B93D0B.md)
said each CCW station-direction pair had **one run** and was "insufficient". That
was true of the September 16 raw Hall capture and of nothing else. The regular
MQTT run logs hold **1,505 ZERO_RAMP records**, and after exclusions **1,279
usable ramps**, of which **648 are CCW**, across two locomotives and 19 dates.
The CCW half of the railway is among the best-covered data in the repo. That
report's §5 "insufficient data" table is withdrawn; its window audit and its
Hall-uniformity result stand.

**Machine-readable:** [`analysis/ramp_templates/ramp_templates_runlogs.json`](../analysis/ramp_templates/ramp_templates_runlogs.json) — 16 configurations.

---

## 1. Inventory of every log

74 files under `field-records/logs/`. Scanned for `ZERO_RAMP`, `DWELL_BEGIN`,
`mm/marker` and `state/station`.

| category | files | ramp records | why |
|---|---|---|---|
| **carry ZERO_RAMP** | **6** | **1,505** | the station machine ran |
| `state/station` but no ZERO_RAMP | 9 | 0 | only `RESET` / `START_INTERVAL_SET` / `DIR_REFUSED` — session setup, no station machine in that build |
| `mm/marker` only | 9 | 0 | pre-station-machine sketches, surveys (Aug 28, Sep 9), NAVI-2 first lap |
| no marker or station data | 50 | 0 | IR daylight tests, bench traces, waveform slot dumps, netdiag |

The six with ramps:

| log | date | ZERO_RAMP | DWELL_BEGIN | timestamps |
|---|---|---|---|---|
| `20260808_T-sweep_arches_part2.log` | 08-08 | 1 | 1 | **whole seconds** |
| `20260808_T-sweep_arches_part4.log` | 08-08 | 40 | 38 | **whole seconds** |
| `20260810_IR_SPEED_LOCAL_1_2_otto.log` | 08-10 | 42 | 42 | sub-second |
| `20260811_QUORUM_1_13_beta_otto.log` | 08-11 | 60 | 59 | sub-second |
| `20260813_otto_1_13_noquorum_watch.log` | 08-13 → 08-24 | **899** | 889 | sub-second |
| `20260820_morning_session.log` | 08-19 → 08-27 | **463** | 451 | sub-second |

**Duplicates removed:** `20260813_..._watch.log.gz` is byte-identical to the
`.log` (same md5) and is counted once. `navlab-inputs/20260820_morning_session.log`
is a derived partial copy with markers but no station events (14,564 vs 24,394
`mm/marker`, no `state/station`) and contributes nothing.

### Usable ramps by date and locomotive

| date | Otto | Toby | | date | Otto | Toby |
|---|---|---|---|---|---|---|
| 08-08 | 24 | — | | 08-17 | 22 | 24 |
| 08-09 | 13 | — | | 08-18 | 83 | 53 |
| 08-10 | 42 | — | | 08-19 | 181 | 83 |
| 08-11 | 59 | — | | 08-20 | 153 | 131 |
| 08-13 | 17 | 16 | | 08-21 | 33 | 35 |
| 08-14 | 94 | 77 | | 08-24 | 5 | 44 |
| 08-15 | 34 | 33 | | 08-26 | — | 27 |
| 08-16 | 59 | 59 | | 08-27 | — | 76 |

### Paired ramps by station and direction

| station | CW | CCW | total |
|---|---|---|---|
| Arches | 180 | 191 | 371 |
| Bamboo | 179 | 191 | 370 |
| Grillers | 183 | 184 | 367 |
| Patio | 181 | 188 | 369 |
| **TOTAL** | **723** | **754** | **1,477** |

---

## 2. Exclusions, before any fitting

From 1,505 ZERO_RAMP records:

| exclusion | n | reason |
|---|---|---|
| unpaired with `DWELL_BEGIN` | 28 | 24 interrupted by `RESET`, 2 zero-span, 1 by another `ZERO_RAMP`, 1 spanning 362 s |
| **1-second timestamps** | **37** | both T-sweep logs. A ±0.5 s quantisation on a 2 s leg is ±25 % on every speed — unusable for timing |
| entry PWM off nominal | 45 | the ramp began below station speed (values 24–70 against a nominal 60 or 72) |
| ramp span inconsistent with 200 ms/count | 10 | |
| no marker crossed between ramp and dwell | 34 | |
| no pre-ramp marker interval | 0 | |
| **non-standard stop offset** | **68** | early builds used Arches CW +2, Patio CW +2, Grillers +1 etc.; kept out of the standard-geometry templates |
| entry speed outside 80–260 mm/s | 4 | corrupt `dt` (679, 610, 268 mm/s) |
| **RETAINED** | **1,279** | |

**A bug I had to fix first.** My initial pass returned zero ramps from both
T-sweep logs. They use ISO timestamps (`2026-08-08T22:19:31-0700`) rather than
epoch seconds, and my parser silently dropped every line. Fixing it recovered 37
ramps, which the 1-second resolution then excluded anyway — but the exclusion is
now on the data's merits rather than on a parsing failure.

### Ramp configuration changes

The ramp is fully specified by (station, direction, stop offset, entry PWM,
200 ms/count). Configuration moved repeatedly:

| sketch | Arches CW | Bamboo CW | Grillers CW | Patio CW | Grillers CCW |
|---|---|---|---|---|---|
| QUORUM_1_9 / 1_10 / 1_11 | +2 @ 45 | +1 @ 60 | +1 @ 72 | +2 then +1 @ 45 | — |
| QUORUM_1_12C / 1_13 | +1 @ 60 | 0 @ 60 | 0 @ 72 | +1 @ 60 | 0 @ 72 |
| QUORUM_1_14A | — | — | — | — | −1 @ 72 |
| QUORUM_1_16R / _IR_TEST_A | +1 @ 60 | 0 @ 60 | 0 @ 72 | +1 @ 60 | −1 @ 72 |
| TEMPLATES_0_2 / 0_3A / 0_3B | +1 @ 60 | 0 @ 60 | 0 @ 72 | +1 @ 60 | −1 @ 72 |

The templates below use only the **standard geometry** — Arches CW +1 / CCW 0,
Bamboo CW 0 / CCW −1, Grillers CW 0 / CCW −1, Patio CW +1 / CCW 0 — which
`QUORUM_1_12C` onward share. Sketches contributing to each configuration are
listed in the JSON.

### How events are associated with a ramp

- A ramp is `ZERO_RAMP` → the **next** `DWELL_BEGIN` for the **same locomotive**,
  abandoned if `RESET`/`ARMED`/another `ZERO_RAMP` intervenes.
- Direction is taken from the **sign of the marker sequence** around the ramp
  (ascending mm = CW), not from a declared session direction.
- Entry speed is the surveyed spacing of the last pre-ramp marker interval
  divided by its logged `dt`. That marker's message is a median of **0.0 s**
  before the ramp signal, so it is genuinely the entry condition.
- **Distance is anchored on the last observed pre-ramp crossing**, not on the
  station table. Computing the trigger from `centre + offset` disagreed with the
  logs in 26 % of ramps — at Arches CCW the ramp consistently fires one marker
  later than the table predicts — and produced a first-leg speed of 205 mm/s
  against a 130 mm/s entry, which is impossible during a deceleration. Anchoring
  on what was observed removes the assumption and makes every first-leg speed
  fall below its entry speed.

### How accurately the logs locate the stop

**They do not locate it directly.** `DWELL_BEGIN` fires when the PWM ramp
reaches zero: across 1,477 ramps the ZERO_RAMP→DWELL_BEGIN span matches
`entry PWM × 200 ms` to within 0.4 s in **99.2 %** of cases (median difference
−0.03 s). It is a **command boundary**, not a physical stop. So:

- distance to the last marker crossed is **MEASURED** (surveyed spacing, logged time)
- distance from that marker to rest is **INFERRED**, bounded above by the next
  surveyed spacing because that marker was demonstrably not reached

Every travel figure below is reported as that interval.

---

## 3. Travel, by configuration

Standard geometry only. Locomotives kept separate throughout — Toby enters
every platform faster than Otto.

| station | dir | loco | n | entry mm/s | MEASURED to last marker (p5/p50/p95) | total bounded |
|---|---|---|---|---|---|---|
| Arches | CCW | Otto | 89 | 130 | 300 / 300 / 300 | 300 – 600 |
| Arches | CCW | Toby | 67 | 151 | 300 / 600 / 600 | 300 – 900 |
| Arches | CW | Otto | 89 | 132 | 305 / 305 / 305 | 305 – 605 |
| Arches | CW | Toby | 76 | 150 | 305 / 305 / 605 | 305 – 905 |
| Bamboo | CCW | Otto | 101 | 124 | 290 / 290 / 610 | 290 – 915 |
| Bamboo | CCW | Toby | 69 | 144 | 290 / 610 / 610 | 290 – 915 |
| Bamboo | CW | Otto | 84 | 124 | 290 / 610 / 610 | 290 – 910 |
| Bamboo | CW | Toby | 76 | 147 | 610 / 610 / 910 | 610 – 1210 |
| Grillers | CCW | Otto | 91 | 180 | 640 / 640 / 960 | 640 – 1275 |
| Grillers | CCW | Toby | 58 | 196 | 640 / 960 / 960 | 640 – 1275 |
| Grillers | CW | Otto | 78 | 149 | 325 / 325 / 640 | 325 – 945 |
| Grillers | CW | Toby | 76 | 159 | 325 / 640 / 640 | 325 – 945 |
| Patio | CCW | Otto | 92 | 122 | 315 / 315 / 315 | 315 – 630 |
| Patio | CCW | Toby | 68 | 135 | 315 / 315 / 315 | 315 – 630 |
| Patio | CW | Otto | 81 | 111 | 310 / 310 / 310 | 310 – 610 |
| Patio | CW | Toby | 84 | 136 | 310 / 310 / 610 | 310 – 910 |

Grillers CCW is the longest coast (640–1275 mm) and the fastest entry
(180–196 mm/s); Patio CW Otto the shortest and slowest (310–610 mm at
111 mm/s). Both directions at Grillers differ by 30 mm/s and by a full marker
of travel — confirming, now on 300+ ramps rather than two, that no pair may
stand in for another.

---

## 4. How much of the variation does entry speed explain?

Dependent variable: `D_mid`, the midpoint of the bounded total travel.

| station | dir | loco | n | D_mid sd | v0 sd | r | **r²** | const RMS | +v0 RMS | gain |
|---|---|---|---|---|---|---|---|---|---|---|
| Arches | CCW | Otto | 89 | 32 | 14 | +0.22 | 0.05 | 32 | 31 | 3 % |
| Arches | CCW | Toby | 67 | 150 | 21 | +0.88 | **0.78** | 150 | 71 | 53 % |
| Arches | CW | Otto | 89 | 44 | 7 | +0.46 | 0.21 | 44 | 40 | 11 % |
| Arches | CW | Toby | 76 | 154 | 10 | +0.05 | **0.00** | 154 | 154 | 0 % |
| Bamboo | CCW | Otto | 101 | 137 | 20 | +0.61 | 0.37 | 137 | 109 | 20 % |
| Bamboo | CCW | Toby | 69 | 132 | 25 | +0.74 | 0.54 | 132 | 89 | 32 % |
| Bamboo | CW | Otto | 84 | 162 | 9 | +0.71 | 0.50 | 162 | 114 | 29 % |
| Bamboo | CW | Toby | 76 | 100 | 16 | +0.67 | 0.44 | 100 | 75 | 25 % |
| Grillers | CCW | Otto | 91 | 212 | 10 | +0.39 | 0.15 | 212 | 195 | 8 % |
| Grillers | CCW | Toby | 58 | 149 | 15 | +0.70 | 0.49 | 149 | 106 | 29 % |
| Grillers | CW | Otto | 78 | 187 | 18 | +0.25 | 0.06 | 187 | 181 | 3 % |
| Grillers | CW | Toby | 76 | 132 | 16 | +0.54 | 0.29 | 132 | 111 | 16 % |
| Patio | CCW | Otto | 92 | **0** | 14 | — | — | 0 | 0 | travel never varies |
| Patio | CCW | Toby | 68 | 37 | 17 | +0.18 | 0.03 | 37 | 36 | 2 % |
| Patio | CW | Otto | 81 | 75 | 13 | +0.74 | 0.55 | 75 | 50 | 33 % |
| Patio | CW | Toby | 84 | 150 | 8 | +0.65 | 0.43 | 150 | 113 | 24 % |

**Median r² = 0.37** (range 0.00–0.78). RMS prediction error falls from
**137 mm to 106 mm** — a median improvement of 24 mm, or 20 %, against a marker
spacing of about 300 mm.

### The same question asked ordinally, which is more revealing

Travel is quantised by marker spacing, so the honest question is whether a
faster entry crosses one more marker. Mean entry speed by markers crossed:

| configuration | 1 marker | 2 markers | 3 markers |
|---|---|---|---|
| Arches CCW Toby | 130 (n=32) | **167** (n=35) | — |
| Bamboo CCW Toby | 108 (n=16) | **151** (n=53) | — |
| Bamboo CCW Otto | 117 (n=73) | **143** (n=28) | — |
| Bamboo CW Otto | 116 (n=38) | **129** (n=44) | 131 (n=2) |
| Grillers CW Otto | 140 (n=54) | **159** (n=20) | 161 (n=4) |
| Grillers CW Toby | 145 (n=12) | **162** (n=61) | 196 (n=3) |
| Patio CW Toby | 131 (n=44) | **142** (n=40) | — |

**The sign is the same in every configuration**: runs that carry a marker
further entered faster. The effect is real and physically sensible. But the
residual after fitting it is still ~106 mm, a third of a spacing, so entry speed
sharpens the template without determining it.

### Cases the model cannot explain

- **Patio CCW Otto, n=92**: all 92 runs cross exactly one marker. There is no
  variation for entry speed to explain, and a constant template is exact.
- **Arches CW Toby, n=76, r² = 0.00**: travel varies by a full marker (sd
  154 mm) while entry speed varies by only 10 mm/s. Whatever moves this landing
  is not entry speed.
- **Grillers CW Otto (r² = 0.06) and Grillers CCW Otto (r² = 0.15)**: both have
  large travel variation and tight entry speed. Grillers is the grade; something
  other than entry speed — most likely traction on the climb — dominates.

**Recommendation: use the constant per-configuration template.** A linear entry
speed term buys 24 mm of RMS on a 300 mm quantum, helps in 9 of 16
configurations and does nothing in 4. Nothing here justifies a more elaborate
model, and the two-parameter fit is not worth carrying into firmware.

---

## 5. What a 200 ms baseline window can rely on

Leg speeds are **measured**: surveyed distance over logged time. The figure is
the 5th percentile, so it is a floor rather than an expectation.

| station | dir | loco | travel in a 200 ms window, by elapsed time into the ramp |
|---|---|---|---|
| Arches | CCW | Otto | 0.0–3.0 s ≥ 17 mm |
| Arches | CCW | Toby | 0.0–2.4 s ≥ 17 mm ; 2.2–5.9 s ≥ 14 mm |
| Arches | CW | Otto | 0.0–3.1 s ≥ 17 mm |
| Arches | CW | Toby | 0.0–2.5 s ≥ 21 mm ; 2.3–6.4 s ≥ 12 mm |
| Bamboo | CCW | Otto | 0.0–2.5 s ≥ 14 mm ; 2.1–6.4 s ≥ 12 mm |
| Bamboo | CCW | Toby | 0.0–1.9 s ≥ 20 mm ; 1.8–5.0 s ≥ 13 mm |
| Bamboo | CW | Otto | 0.0–2.4 s ≥ 18 mm ; 2.4–6.7 s ≥ 12 mm |
| Bamboo | CW | Toby | 0.0–1.9 s ≥ 27 mm ; 1.9–4.6 s ≥ 19 mm |
| Grillers | CCW | Otto | 0.0–2.0 s ≥ **28 mm** ; 2.0–4.2 s ≥ 24 mm ; 3.9–8.1 s ≥ 11 mm |
| Grillers | CCW | Toby | 0.0–1.9 s ≥ **30 mm** ; 1.9–3.9 s ≥ 27 mm ; 3.4–5.9 s ≥ 16 mm |
| Grillers | CW | Otto | 0.0–2.6 s ≥ 18 mm ; 2.3–6.8 s ≥ 12 mm |
| Grillers | CW | Toby | 0.0–2.3 s ≥ 23 mm ; 2.3–6.2 s ≥ 12 mm |
| Patio | CCW | Otto | 0.0–3.7 s ≥ 13 mm |
| Patio | CCW | Toby | 0.0–3.3 s ≥ 12 mm |
| Patio | CW | Otto | 0.0–3.4 s ≥ 15 mm |
| Patio | CW | Toby | 0.0–2.5 s ≥ 21 mm ; 2.2–6.5 s ≥ 12 mm |

**The headline for baseline sampling: a 200 ms window taken anywhere in the
measured part of any ramp covers between 11 and 30 mm of track.** The best case
is the first two seconds of a Grillers CCW ramp (≥28–30 mm); the floor
everywhere else is 11–21 mm.

For comparison, the trailing magnetic influence of a marker measured in the
September 16 capture reaches **36 mm typical and 71 mm worst**. **A 200 ms
window during a ramp-down does not travel far enough to clear a marker it is
sitting near.** Whether such a window is usable depends entirely on *where* it
sits between markers, not on how far it moves — the movement is too small to
help.

Beyond the last leg in each row the locomotive is still moving but no marker is
crossed, so **no speed is measured there and no travel can be claimed**.

---

## 6. What this supports

**Supported:** the 16 configurations as descriptions of August 2026 running
under `QUORUM_1_12C` onward; CCW coverage of 648 usable ramps; entry speed
explaining a median r² of 0.37 with a consistent sign; the 11–30 mm window
travel floors.

**Not supported:** the resting position of any ramp (bounded to one spacing,
~300 mm, never measured); the physical stop *time* (`DWELL_BEGIN` is a command
boundary); anything about the non-standard offsets of `QUORUM_1_9`–`1_11`;
anything about `NAVI_ONE`, whose station offsets differ again from every
configuration measured here.

---

*Analysis from `field-records/logs/`. No firmware was modified.*
