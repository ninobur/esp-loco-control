# Detection → closure at PWM 90 — measured, Otto, QUORUM TRACE corpus

**2026-09-16. Analysis only. No firmware changed, no detector proposed.**

Asked because the NAV guard is being re-referenced: the old one ran 500 ms from
**closure**, and closure is unobservable if the locomotive slows or stops in the
field. A guard started at **detection** needs to know, empirically, how far
detection sits ahead of closure.

## The measurement

```
PWM 90 exactly, no widening needed.

detection -> first held return inside +/-25      N=1115
    min 28   p10 110   median 125   mean 133   p90 146   p95 160   p99 196   max 2040 ms

detection -> EVENT_CLOSED as the firmware raises it (that return held 20 ms)
    min 48   p10 130   median 145   mean 153   p90 166   p95 180   p99 216   max 2060 ms
```

99.1% of passages close within 200 ms of detection.

```
 touch - detection, ms      count
   50- 99                     14
  100-109                     65
  110-119                    274
  120-129                    346
  130-139                    217
  140-149                     87
  150-159                     34
  160-169                     27
  170-179                     13
  180-199                      6
  200+                        10
```

## Definitions used

* **Detection** — `|raw - baseline| >= 70` for 2 consecutive 1-kHz samples;
  timestamp is the **second** qualifying sample.
* **Closure** — return to `|raw - baseline| <= 25`, the deadband the closure-based
  detector actually used (`HALL_DEADBAND_COUNTS 25`, QUORUM_1_13X), held
  continuously for `EVENT_EXIT_HOLD_MS 20` — which is the firmware's own rule,
  `evReturnMs` resetting on any sample that leaves the band. Both the first
  in-band sample and the instant the hold completes are reported, because the old
  guard timed from the second.
* The 27.6 mm `>=70` field width is **not** used anywhere here. Every number is
  read off corpus samples.

## Population

Genuine track-magnet passages only, established independently of waveform shape:
the `>=70/2` opening must survive the 500 ms guard **and** match the polarity of
the next expected mapped marker, inside a segment demonstrated to be moving by
QUORUM's own contemporaneous marker advances. That is the same staging as
`SIMPLE_DETECTOR_REPLAY_QUORUM.md`.

1381 accepted passages across the corpus; **1117 of them at PWM 90 exactly**, so
no expansion around 90 was needed. 1115 have a determinable closure.
Provenance: `qt_20260824_205508.qtcap` (744), `qt_20260824_201746.qtcap` (313),
`qt_20260825_140423.qtcap` (58), on 192.168.68.142.

Excluded before measurement: PWM != 90 (which removes every stopped and dwell
sample), openings with no accepted-chain membership, and any event whose samples
were not continuous to within 5 ms (there were none — the corpus is intact here).

## Outliers — present in every figure above, not removed

Ten passages exceed 200 ms and two never close inside a 3 s search. They are one
phenomenon, and it is not a long magnet field.

```
t=1364019 mm=124  2040 ms    after the arc the trace settles on +30..+48 and stays
t=389126  mm=39   1388 ms    settles on -25..-38
t=242679  mm=78   1304 ms    settles on +25..+50
t=399055  mm=37   1078 ms    settles on -29..-36
t=1359255 mm=121  1045 ms    settles on +33..+44
t=1355082 mm=117  no closure  sits on +32..+48 for the whole 2.6 s window
t=2448128 mm=43   no closure  +145/-25 alternation, PWM ramping 90->0
t=213570  mm=51    580 ms    already at +69..+79 sixty ms BEFORE detection
t=252084  mm=89    513 ms    genuine long opposite lobe, ~+100 sustained
t=235230  mm=77    352 ms    already at +72..+79 before detection
t=1262287 mm=65    228 ms    ordinary shape, slow tail
t=209848  mm=49    201 ms    ordinary shape, slow tail
```

Seven of the twelve are the **firmware baseline displaced by ~30 counts**: the
magnet's arc is over in the usual 150-200 ms, but the signal it returns to is
already outside the deadband, so the closure condition can never be met. That is
a reference fault, not a field duration, and it is exactly the failure mode a
detection-referenced guard is immune to.

Twenty-two further passages had the baseline move more than 10 counts during the
event. Their intervals are 28-130 ms plus one at 1304 — indistinguishable from
the main body, so their inclusion changes nothing: with them the median is 125
and p90 146; without them, 125 and 146.

## The arithmetic the question asks for

To start the guard at detection and approximately preserve 500 ms after closure
at PWM 90, add the detection→closure interval:

```
typical      500 + 145 = 645 ms      (median, firmware closure instant)
p90          500 + 166 = 666 ms
p95          500 + 180 = 680 ms
```

145 ms is the median distance between the two reference instants. That is the
measured answer; whether to buy margin above it is a policy call and is not made
here.

## Provenance

`scratchpad/d2c.py` (replay + staging + closure measurement), `tr90.py` (outlier
traces). Corpus: 4 August QUORUM TRACE captures plus 2026-08-25, Otto 9950011.
