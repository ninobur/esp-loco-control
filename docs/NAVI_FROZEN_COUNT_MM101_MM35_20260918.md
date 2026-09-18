# What is particular about MM101 and MM35

Date: 2026-09-18
Question (operator): what is particular about MM101 and MM35? Are these false
signals in addition to the valid magnet signal?

Read-only analysis. No firmware changed. Follows on from
`NAVI_VMAX_PWM90_20260918.md`, where these clusters were noticed and not pursued.

---

## Short answer

**No. They are not extra signals added to a valid one. They are the valid
signals, and the count is what failed.**

At MM101, 69 of 70 repeat records are full-strength magnets of normal duration
arriving at normal marker cadence. The locomotive is running. The railway is
delivering markers on schedule. The published position simply stops moving.

Phantoms do exist and are interleaved with them, but they are the minority:
62 of 486 moving repeat records (13%) are below 70 counts.

## The discriminator: duration

A magnet passing at running speed occupies the sensor for a characteristic time.
Across 54,307 clean arrivals the median is **174 ms**, p95 326 ms.

Splitting the 522 repeat records on that:

| class | n | peak p50 | duration p50 | dt p50 | reading |
|---|---:|---:|---:|---:|---|
| duration < 600 ms | **486** | 175 | — | **1,466 ms** | locomotive moving, normal passages |
| duration ≥ 600 ms | 36 | 149 | 936 ms | 598 ms | stopped or creeping inside a field |

The 486 have a duration median of 175 ms against the clean-arrival median of
174 ms — statistically the same passage. Their dt median of 1,466 ms is one
marker spacing at the speeds involved (300 mm / 1.466 s = 205 mm/s, which is what
PWM 60–70 produces). **These are ordinary markers, correctly detected, that did
not advance the count.**

Only 36 records are the stopped-in-a-field case that §5.9 of the prospectus
addresses.

## What is particular about MM101

| | MM101 | route |
|---|---|---|
| repeat rate | **14.4%** (55 of 383) | 0.64% |
| direction | **100% CW** (70 of 70) | — |
| locomotive | **94% Toby** (66 of 70) | — |
| PWM | 60, 61, 62, 67, 68, 73 | — |
| peak p50 of the repeats | 179 counts | 171 |
| duration p50 of the repeats | 212 ms | 174 |
| weak (<70 counts) | **1 of 70** | — |
| frozen-run length | 2 to 6 consecutive records | — |

MM101 repeats 22× more often than the route average, in one direction only, on
one locomotive, at low PWM, with essentially no weak signals involved.

MM35 is the same shape: 7.2% repeat rate, 93% CW, 93% Toby, 27 of 29 records at
PWM 68, 28 of 29 full strength. It also holds the longest single frozen run in
the corpus — **27 consecutive records at the same `mm`**.

Other markers above 2.5%: MM52 (4.6%), MM81 (3.5%), MM126 (3.5%), MM130 (3.2%),
MM15 (3.0%), MM86 (3.0%), MM48 (2.8%), MM100 (2.7%).

## A worked episode

Toby, 2026-08-13, PWM 70 held throughout, CW:

```
  MM  99 obs S dt   1550 pwm 70 peak  217 ms  274
  MM  99 obs S dt   1654 pwm 70 peak  314 ms  280
  MM  99 obs S dt   1375 pwm 70 peak   46 ms  183   <- phantom
  MM 100 obs S dt   1485 pwm 70 peak  297 ms  247
  MM 100 obs S dt   1457 pwm 70 peak  287 ms  246
  MM 100 obs S dt   1225 pwm 70 peak   42 ms   90   <- phantom
  MM 100 obs S dt   1493 pwm 70 peak  185 ms  243
  MM 100 obs S dt   1467 pwm 70 peak  201 ms  259
  MM 100 obs S dt   1572 pwm 70 peak  188 ms  315
  MM 100 obs S dt   1558 pwm 70 peak   38 ms  112   <- phantom
  MM 100 obs N dt   1766 pwm 70 peak  153 ms  197
  MM 100 obs N dt   1745 pwm 70 peak  156 ms  245
  MM 100 obs N dt   1908 pwm 70 peak  163 ms  263
  MM 101 obs N dt   2154 pwm 70 peak  191 ms  239
```

Ten consecutive events published as MM100, spanning 14 s. Three are phantoms
(38–46 counts, 90–183 ms). Seven are full-strength magnets of normal duration at
~1,500 ms cadence. At PWM 70 the locomotive runs about 190 mm/s, so 14 s is
roughly 2,660 mm — about nine marker spacings. The event count and the physical
travel agree. The markers were there and they were seen.

The `alert` stream over the same window shows `nav` = **NORMAL** throughout,
`lostm` = 0, `lost_ms` = 0, `candidate_mm` = −1, `viable` = []. `lc_mm` sat at
100 for 19 s and then jumped to 103. Meanwhile `disagree` climbed from 5 to 7.

So the navigator was not lost, not in acquisition, and had no candidate. It was
registering disagreements and declining to advance, while reporting NORMAL.

## Route-wide, the repeats concentrate at low PWM

| PWM | moving repeat records | share of all arrivals |
|---:|---:|---|
| 90 | 125 | 54% (dominant duty) |
| **40** | **71** | negligible |
| 60 | 65 | 13% |
| **68** | **32** | negligible |
| **44** | **30** | negligible |
| 120 | 25 | 4% |

PWM 40, 44 and 68 together contribute 133 repeat records while barely appearing
in the arrival population at all. Low-PWM running is heavily over-represented.

## What this does and does not establish

**Established.** The clusters are real magnets, correctly detected, that failed
to advance the count. They are direction-specific, locomotive-specific, and
concentrated at low PWM. Phantoms are present in the same episodes but are 13%
of the records, not the mechanism.

**Not established.** Why the navigator declines. That lives in QUORUM's quorum /
adoption logic, which this analysis did not trace — the `disagree` counter
climbing during the freeze is the handle to pull. Nor is it established why MM101
CW and MM35 CW specifically; candidate factors not separated here include the
Arches CW approach (MM101 is seven markers before Arches at 108), the unusually
strong magnets in that stretch (MM99/100/101 peak medians 270/264/217 against a
route median of 171), and Toby's looser entry threshold.

**Toby's entry threshold is worth noting regardless.** Both locomotives ran
`entry_margin` 13 (38-count entry) in these sessions. Otto has since moved to 45
(70-count entry); **Toby has not**. 94% of the MM101 repeats and 93% of the MM35
repeats are Toby's. That correlation is suggestive but it is not the explanation:
only 1 of 70 MM101 repeat records is below 70 counts, so raising Toby's entry
threshold to Otto's would remove almost none of them.

## Relevance to NAVI_SIMPLIFIED

This population is the case the prospectus's three-stage split is built for, and
it lands in the third stage:

- Not **§5.1 detection** — the signals are full-strength and well-formed.
- Not **§6 hard protection** — a marker arriving after one full spacing of travel
  is physically reachable; hard protection must let it through.
- **§11 judgment**, squarely. A real magnet arrives on time, and NAV declines it.
  Under the prospectus, declining it is permitted — but §14 requires that the
  reasoning be reconstructable, and here the only trace is a `disagree` counter
  incrementing while `nav` reports NORMAL.

§5.9 (stop-on-magnet) covers the other 36 records, not these 486.

The measurement that would settle the "why" is not available in this corpus and
is exactly what §18.3 specifies: per-judgment records carrying the evidence used
and the alternatives considered.

## Method

Same corpus and gating as `NAVI_VMAX_PWM90_20260918.md`: `ngr/loco/+/mm/marker`
from the 2026-08-13 and 2026-08-20 sessions, `timing_gate == ACTIVE` only,
locomotive-clock `dt` and `ms`. A "repeat" is a record whose `mm` equals the
previous record's `mm` within the same run. Frozen runs were measured by maximal
extent, not pairwise, which is why the record count (522) exceeds the pair count
(327) quoted in the vMAX report — MM101 and MM35 produce runs of 4 to 27, not
pairs.
