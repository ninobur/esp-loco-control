# 2026-09-13 — Otto stops between Northpoint and MM086: an acquisition latch

**Locomotive:** Otto, 9950011
**Build:** `NAVI_ONE_1_0X16_FLOOR82_FIELDTEST` (decision 0085, experimental)
**Direction:** CCW, cruise PWM 90
**Stop:** 11:21:05 PDT, `WRONG MAGNET at MM090: expected S at MM089, read N`
**Source:** live MQTT capture of `ngr/loco/9950011/#`. `pub_drop` is 0 across the
whole episode, so the record is complete.

## It is not the 82 ms duration floor

`floor_rej` reads 2 before, during and after the episode — the same two
rejections from the 10:17 and 10:19 bench events. Only two `diag/acquisition`
records exist for the entire day, both before the run began. Every event in the
incident lasted 95–2,446 ms. The floor rejected nothing and is not implicated.

## The episode, in device time

The six passages below were published on `diag/waveform` and are preserved
verbatim in `firmware/test-programs/NAVI_ONE/tests/fixtures_northpoint_20260913.h`.
The first line is reconstructed from its `mm/marker` record: it was accepted, and
this build dumps a waveform only for a refusal or a strike.

| device ms | dur | peak | pol | ratio | gap | outcome |
|---:|---:|---:|:--|---:|---:|:--|
| 3846934→3848503 | **1569** | 251 | S | 1.443 | 788 | accepted as MM092 |
| 3848555→3848650 | 95 | 112 | N | 0.640 | 52 | refused, TOO_SOON |
| 3848718→3849918 | **1200** | 76 | S | 0.434 | 215 | refused, TOO_SOON |
| 3849975→3850071 | 96 | 117 | N | 0.669 | 1472 | accepted as MM091 |
| 3850133→3852579 | **2446** | 257 | S | 1.477 | 62 | refused, TOO_SOON |
| 3852740→3852900 | 160 | 199 | N | 1.144 | 2669 | accepted as MM090 |
| 3854007→3854163 | 156 | 188 | N | 1.080 | 1107 | **WRONG_MAGNET, strike** |

The seven markers immediately before ran 129–160 ms with close-to-close spans of
1,121–1,289 ms. From MM093's close to the strike is **8,017 ms** — about
**6.7 marker intervals** at that measured cadence. Navigation registered four
events. Roughly three magnets produced no accepted event at all. Navigation said
MM090; the locomotive was found at MM086.

## Cause

A sustained excursion of the Hall signal, 61 counts peak to peak, against an
entry threshold of 70 and an exit margin of 25.

```
11:20:57  baseline 1954    MM093 read normally: peak 178, dur 129, base_open = base_close
11:20:58  baseline 1980    +26 counts in one second, BETWEEN passages
11:21:00  baseline 1981
11:21:03  baseline 1981    the 2446 ms passage: base_open 1981, base_close 1936
11:21:04  baseline 1921    -60 counts
11:21:05  baseline 1920
```

The decoded samples show the mechanism directly. The 2,446 ms record's minimum
sample is **+45** relative to its frozen entry baseline — it never returned below
the 25-count exit margin for two and a half seconds, so the passage could not
close on signal. It closed only when `openMigrateMs` (2,000 ms) allowed the live
reference to migrate, which is visible inside that one record as
`base_open 1981 → base_close 1936`.

Once a passage is latched open for 1.2–2.4 seconds, the real magnet arrivals
emerge as fragments 52, 215 and 62 ms after an accepted close, and the
unconditional 500 ms guard refuses them. The 2,446 ms event carried ratio 1.477
and peak 257 and was almost certainly a genuine magnet, discarded purely on
timing. Navigation advanced on whichever fragments happened to fall outside the
guard, fell three to four markers behind the railway, and stopped correctly when
a polarity finally disagreed.

The +26 count step happened **between** passages, with nothing open, so the
sensor's quiescent reading moved first and the adapter followed it. It is not a
magnet being ingested by the baseline.

## Not a departure, and not the first time

Arches DEPARTED at 11:20:42; the last `diag/departure` record is 11:20:42. The
episode began at 11:20:58 — sixteen seconds and thirteen markers later, at full
cruise. **A station departure is not a necessary condition for this fault.**

Otto crossed the MM096–MM084 block nine times on 2026-09-13. The baseline
stepped by 20–29 counts in that same block on at least three earlier laps
without consequence:

```
10:39:29-31   mm 89->88   1965 -> 1943 -> 1936   (-29)
10:46:25-27   mm 91->90   1936 -> 1929 -> 1926   (-10)
11:07:15-16   mm 89       1951 -> 1971          (+20)
11:20:58-11:21:04  mm 93->90  1954 -> 1981 -> 1921  (+26 then -60)
```

Of the 22 passages of 400 ms or more recorded on 2026-09-13, nineteen were slow
crossings at stations (PWM 22–47, `base_open == base_close`). The only three at
cruise PWM 90 are the three in this incident.

The same fault is present, undiagnosed, in **Otto's 2026-09-09 CCW survey**, at
21:29:51–21:29:58 between MM050 and MM041: passages of 1,358, 4,944 and 2,261 ms
at PWM 80–90, all truncated and clipped. The marker stream there runs
`mm 47 → 46 → 45 → 42` — three markers lost in one step — with
`dt_conserve_ratio` climbing to 5.34 and 4.31. That is the signature
`LL_LocoConfig_9950011.h:170-176` names for silently lagging position.

## Not determined

Why the quiescent Hall level steps by tens of counts within a second. Battery
telemetry is clean — 15.66 V steady, 0.13–0.16 A, no low-voltage flag, no
reboot, no queue loss — but the INA219 is on the battery side and would not see
a local 3V3 or ADC-reference disturbance. MM065–MM098 are all bar magnets, so
the block is not mixed. The recurrence within one block across four laps is
suggestive of place, but the baseline also drifts 1936↔1980 across the whole
route between sessions, so "this stretch of track" cannot be separated from
"the sensor or its mount moving intermittently" on this data alone. Otto's
sensor was realigned 2026-08-20 and the board and pin lineage changed
2026-09-10; both are in scope for a physical check.

## Operator's determination, same day

> HallCapture permits one passage to remain open indefinitely unless the signal
> falls below the 25-count exit threshold. During this incident it stayed above
> that threshold while Otto travelled across multiple magnets, so those magnets
> could not create new passage openings. The two-second `openMigrateMs` escape
> is too slow at cruise.

The offline replay of the proposed bounded acquisition reset is in
`docs/NAVI_CRUISE_CEILING_REPLAY_20260913.md`. No firmware was changed.
