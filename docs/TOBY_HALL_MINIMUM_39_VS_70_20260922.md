# Toby's weakest Hall signal: June "39" vs today "70" are different measurements

2026-09-22. Question (operator): why did Toby's measured minimum change from 39
in June to 70 today? We need comparable measurements before choosing the
NAVI_COHERENCE threshold.

## Short answer

The two numbers measure different quantities.

- **June's 39 was a smoothed value.** On a like-for-like raw basis, June's
  weakest passage was at least 98 counts.
- **Today's 70 is one specific magnet, MM012.** It read 70, 71 and 72 in
  today's three runs. Every other magnet on the route bottoms out at about
  125 counts.

## Where the June 39 comes from

Toby's config header (`LL_LocoConfig_9950012.h`) says:

> Hall sensor profile — measured with NGR Hall Probe diagnostic 2026-06-26.
> Noise: mean 4.2 counts, observed maximum 19. Weakest real magnet signal: 39
> counts.

The matching data is
`~/NGR/hallprobe_logs/Toby Towed after lowering sensor hallprobe_20260623_190937.csv`,
which is not in the repo. Its smallest `delta` is exactly 39. The CSV
columns show what the probe measured:

| Property | June probe | NAVI_COHERENCE today (`peak_signed`) |
|---|---|---|
| Sample/report rate | **10 Hz** (one row per 102 ms) | 1 kHz |
| Value | `delta` = `avg` − `baseline` | max \|raw − lock\| in a 400 ms window after opening |
| Smoothing | `avg` is an exponential average, α ≈ 0.25 per row (raw 1938 after avg 1875 gives avg 1891) | none |
| Baseline | fixed at boot (1872 in that run) | X22 locked baseline |
| Motion | towed, motor off | powered, PWM 17–100 |
| Sensor geometry | the run was recorded just after "lowering sensor" | as installed now |
| Magnets | pre-August (before decision 0025 replaced stacked doubles with single disks) | current |

The smoothing attenuates heavily.
- Across the June runs, `delta` peaked at a median of 0.34–0.58 of the
  raw peak.
- **The very passage that produced 39 had a raw peak of 112.**
- The weakest raw peak in that run was 98. Raw was itself sampled at only
  10 Hz, so the true peaks were at least as large as these.

| June 06-23 19:09 run (87 passages) | Smallest | Median |
|---|---|---|
| Smoothed `delta` | **39**, 41, 43 | 64 |
| Raw peak vs local rest (10 Hz) | 98, 100, 110 | 168 |

## Where today's 70 comes from

Accepted advances by magnet, across all three runs on 2026-09-22 (967
advances). Lap 1 was mapped to true MM using its −1 declaration offset, and
the Arches false-advance stretch was excluded.

| Magnet | Reads | Weakest peaks | Strongest |
|---|---|---|---|
| **MM012** | 6 | **70** (run 2, PWM 92), **71** (lap 1, PWM 100), **72** (AUTO, PWM 60) | **99** |
| MM110 | 5 | 101*, 140, 170 | 172 |
| MM140 | 5 | 125, 127, 127 | 133 |
| MM040 | 8 | 126, 179, 194 | 214 |
| MM108 | 5 | 137, 142, 142 | 143 |

\* The 101 at MM110 is serial 243, the Arches departure at PWM 30.

- MM012 is consistently weak at every speed and in every run. It is a
  property of that magnet or its placement, not of the detector or the
  session.
- Setting it aside, the route's weakest magnets read about 125.
- Today's non-magnet openings (magnets read twice as the train slowed, and
  weak openings) peaked at 38, 40, 45 and 63.

## What explains the change

| Candidate | Verdict |
|---|---|
| Definition: smoothed 10 Hz delta vs raw 1 kHz peak | **Explains June's 39 against a raw ≥ 98** |
| A specific weak magnet | **Explains today's 70 against the rest at ≥ 125: MM012** |
| Baseline method | Minor here. June's resting level stayed within 1872 ± 3 around the 39 passage |
| Sensor geometry | June's measurement came straight after lowering the sensor. Its effect relative to today is unknown |
| Magnet changes since June (decision 0025) | Could move individual magnets either way. Whether MM012 was touched is not known |
| Signal conditions (motor on vs towed) | Not separable from this data |

## Comparable measurements before choosing a threshold

1. **Adopt one measure:** NAVI_COHERENCE's `peak_signed` (1 kHz, locked
   baseline, 400 ms window). It is already published on every `nav/discrepancy`
   row. The June probe's `delta` should not be compared with it.
2. **Build a per-magnet table** over several laps in both directions and at
   two or three speeds, including crawl. Today already gives MM012 six reads.
3. **Inspect MM012 physically:** height, placement, whether it is a single
   disk or a remedial stack, polarity. By the no-silent-magnets principle its
   weakness has a findable cause.
4. **Keep the non-magnet population alongside it.** Today it was 38–63. The
   threshold question is the gap between that population and the weakest real
   magnet:
   - with MM012 as it is today: 63 against 70, a 7-count gap;
   - with MM012 at the route's typical floor: 63 against about 125.

No threshold change is proposed here. 0.5 continues at 38.
