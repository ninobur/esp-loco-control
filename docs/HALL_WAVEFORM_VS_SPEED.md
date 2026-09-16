# How the Hall waveform after the ≥70 opening changes with speed

**Analysis. No firmware changed.** 1,288 genuine, map-confirmed magnet
encounters with an independently measured speed.

## The answer in one line

**Amplitude does not change with speed. Time does, as 1/v.** The passage is a
fixed *spatial* object; speed only rescales the time axis.

## The invariants — measured per event, not as bucket averages

| | median | p10 | p90 |
| --- | --- | --- | --- |
| **distance from the ≥70 crossing to the apex** | **13.4 mm** | 11.3 | 16.0 |
| **spatial width of the ≥70 region** | **27.6 mm** | 24.0 | 31.1 |
| peak amplitude (counts) | 177 | 152 | 207 |

These hold across the whole 85–450 mm/s range in the corpus. Amplitude shows no
trend with speed, which confirms the 2026-08-29 finding from a much larger set.

## What that means in time

```
speed mm/s      n     peak   t_to_peak   width>=70   rise in 10 ms
   60-120       6      180     136 ms      144 ms          8
  120-160      22      142     106 ms      138 ms          6
  160-200      14      142     107 ms      138 ms         11
  200-240      83      170      59 ms      116 ms         23
  240-270     464      180      52 ms      108 ms         26
  270-300     518      177      47 ms       98 ms         28
  300-450     181      175      40 ms       80 ms         30
```

Time-to-apex is 13.4 mm ÷ v, and it predicts the measurement:

```
   60-120 mm/s   measured 136 ms   predicted 133 ms
  240-270 mm/s   measured  52 ms   predicted  52 ms
  300-450 mm/s   measured  40 ms   predicted  40 ms
  160-200 mm/s   measured 107 ms   predicted  76 ms   <- see below
```

The 160–200 band is the one that misses, and the reason is real rather than
noise: those are September **station approaches**, where the locomotive is
decelerating *through* the magnet. Speed here is a marker-to-marker average,
so for a decelerating pass the average overstates the speed at the magnet and
the passage is wider than the average predicts. Every other band is at
approximately constant speed and lands within 3 ms.

## The rise rate, and why it matters

Near the threshold the arc climbs from 70 to ~177 counts over 13.4 mm — about
**8 counts per millimetre**. So the rise visible in a fixed time window is
`≈ 8 · v · t`, i.e. **proportional to speed**:

```
rise in the first 15 ms          n      min   median    max
   60-120 mm/s                    6      13      18      28
  120-160                        22       6      12      35
  160-200                        14      10      25      32
  200-240                        83       0      36      56
  240-270                       464       0      40      87
  270-300                       518       0      42      83
  300-450                       181       0      43      69
```

**This qualifies the earlier 15 ms separation result.** A fixed sample count is
not speed-neutral: at a station approach there is roughly **four times less
rise** in the same window than at cruise. The genuine floor of +4 that set that
boundary came from the slowest passages, and the slowest encounters in the
whole corpus are exactly the marginal ones:

```
   85 mm/s  pwm 40  peak 159  t_pk 136 ms   rise@15=14  @30=35  @60=44
   89 mm/s  pwm 60  peak 171  t_pk 137 ms   rise@15=13  @30=35  @60=51
   96 mm/s  pwm 60  peak 164  t_pk 149 ms   rise@15=22  @30=22  @60=51
  105 mm/s  pwm 60  peak 196  t_pk 135 ms   rise@15=18  @30=36  @60=77
  114 mm/s  pwm 60  peak 189  t_pk 141 ms   rise@15=13  @30=32  @60=68
```

Anything that reads the waveform in a fixed time window is reading a different
fraction of the passage at a station than at cruise. Reading it in a fixed
*distance* would not be — but nothing in the current design measures distance.

## Corpus

| source | n | speed range | how speed was measured |
| --- | --- | --- | --- |
| QUORUM TRACE, 2026-08-24/25 | 1,210 | 245–312 mm/s (p10–p90) | opening-to-opening interval × surveyed spacing |
| September X13–X20 framed records | 78 | 85–302 mm/s | marker `gap_ms` × surveyed spacing |

Genuine = the navigator's own map agreement (`COMPARISON_AGREE` for QUORUM,
`ADVANCED` for September). The ≥70-for-2-consecutive-samples opening rule was
**applied in software to the QUORUM sample stream**, not taken from QUORUM's own
framing — its `EVENT_OPENED` timestamp is not the threshold crossing (the signal
is typically still near 45 counts there).

## Limits

- Both speed measures are **marker-interval averages**, not the instantaneous
  speed through the magnet. That is exactly why the decelerating station
  approaches deviate, and it means the slow end of this analysis is the least
  precise part of it.
- QUORUM is almost entirely PWM 90: it supplies the cruise band with large n but
  no speed range of its own. The whole slow end rests on 78 September records.
- 5 of 1,246 fast encounters show no rise at all in the first 15 ms — they sag
  for a few samples at the opening and then climb normally. Rare, but they exist.
- Nothing here is a proposal. No threshold, no detector change.
