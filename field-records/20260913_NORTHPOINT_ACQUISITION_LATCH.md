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

A superficially similar episode exists in **Otto's 2026-09-09 CCW survey** at
21:29:51–58, and on examination it is **a different fault**. Those three long
records (1,358 / 4,944 / 2,261 ms) decode to a flat line at −33 to −36 counts
with no magnet in them at all, and the clean arcs either side end at exactly
−35: the reference was about 33 counts off the true idle line, which is more
than the 25-count exit margin and less than the 70-count entry margin, so a
passage opened once and could never close. It was preceded by two standstills
totalling about a minute. That is finding 10 / decision 0074 — a reference
learned while parked — on QUORUM 1.13X, which predates the motion gate
(`NAVI_BASELINE_ADAPT_PWM`) that NAVI_ONE now uses against exactly this.

Two corrections to an earlier reading of that survey, recorded so they are not
repeated. Its `clip=1` flag and the saturated samples are the **base64
encoding** clipping at ±384 on one entry-crossing sample, not the ADC reaching a
supply rail; and its `mm` jump from 45 to 42 across 151 ms of device time is
QUORUM adopting a −3 offset, not three magnets physically passing. The real
measure of what was lost there is the device-time gaps between clean magnets —
1,391 / 4,967 / 2,288 ms against a ~1,200 ms cadence, roughly four intervals
unaccounted for.

The Northpoint event shares only the symptom. Its offset is 86 counts peak to
peak rather than 33 and static; it moves while the throttle is constant; it
reaches the entry margin rather than staying below it; one of its plateaus
carries a genuine magnet arc on top; and no standstill precedes it.

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

---

# Addendum, same day: the second stop, and the measured root cause

**Second stop:** 11:51:37 PDT, `WRONG MAGNET at MM061: expected S at MM060,
read N`, during the Grillers stop ramp at PWM 27. Operator confirms the
locomotive is physically between MM062 and MM061, which matches navigation's
reported position — so position was correct at the strike.

The operator re-declared position after the first stop without power cycling.
`uptime_ms` confirms it: 10,441,953 ms unbroken, no reboot. `declare()` calls
`capture.reset()`, which clears a jammed-open passage and re-anchors the
measurement reference but deliberately preserves the baseline. It ran 210 clean
advances over about eight minutes and four station stops, then failed again.

## The second failure ran navigation one marker AHEAD

Polarity here is 63:N, 62:S, 61:N, 60:S.

```
11:51:31  MM063 Grillers   N ok   clean arc, peak 191, pwm 61, reference 1975
11:51:33  MM062            S ok   clean arc, peak 157, pwm 49, reference 1988
11:51:35  refused  1558 ms  N     three humps, never returns to zero, pwm 41
11:51:35  MM061            N      637 ms, lumpy, pwm 37, reference 1991
11:51:37  refused   871 ms  N     plateau 60-90, never returns, pwm 31
11:51:37  STRIKE    277 ms  N     expected S at MM060, pwm 27, reference 2009
11:51:39  (stopped)                                    pwm  0, reference 2021
```

The locomotive had not yet reached MM061 when navigation counted it, so one of
the lumpy merged events was accepted as MM061. The real MM061 — an N magnet —
then arrived where navigation expected MM060, an S. The strike is correct.

At Northpoint navigation fell three to four markers BEHIND; here it ran one
marker AHEAD. Opposite errors, one cause.

## Root cause: the resting level steps, and it is mechanical

Two discriminating tests over 3,194 samples taken while moving:

```
resting level vs motor current   r = -0.023
resting level vs PWM             r = -0.002
resting level vs time            r = +0.625
```

Mean level by current band is flat — 1956, 1953, 1955, 1955, 1953, 1951 from
0.15 A to over 0.40 A. A shared-ground or supply-sag fault would appear here and
does not.

The level does not drift. It sits on plateaus and steps between them:

```
10:19-10:38   ~1955
10:39-10:45   ~1936      step down
10:46-11:06   ~1950      step up
11:07-11:09   ~1965      step up
11:10-11:19   ~1955      step down
11:20-11:21   1947 -> 1981 -> 1920     first stop
11:40-11:50   ~1975
11:51         -> 2021                  second stop
```

Of 3,150 one-second changes while running, 20 are 10 counts or more, 9 are 20 or
more, and 3 are 30 or more. The largest single-second moves are -60, +60, +31,
-28, -27, +26 — about one jump every two and a half minutes, each holding its
new level afterwards. Thermal drift is smooth and monotonic; this is not.

### Correction: the at-rest records are NOT evidence of a live fault

They were presented that way and they should not have been. The reference is
frozen whenever throttle is below the tractive floor (`if (!mayAdapt) return;`
in `updateBaseline()`, floor `NAVI_BASELINE_ADAPT_PWM` = 24). Otto's froze at
**2021**, captured on the last tick before the throttle reached zero — the worst
moment of the failure — and has read exactly 2021 every second since 11:52.

The true resting line is around 1920-1960, where it has been all day. So the
reference is 60-100 counts from the line permanently, and a passage that opens
and will not close is the correct response to that. It requires no movement.
The four open/close cycles over eighty minutes need only a few tens of counts of
ordinary slow settling to cross a threshold that is already in the wrong place.

### And the step count is weaker than stated

20 jumps of 10+ counts out of **3,150** one-second samples is 0.6%, not a
steady rhythm, and several come in pairs that cancel within seconds
(10:53:00 -10 then 10:53:04 +10; 10:19:33 +31 then 10:19:44 -28). A rolling
median does that when its window straddles a disturbance. There is no control
here separating "the line stepped" from "the median wobbled".

**The mechanical conclusion is therefore NOT established and is withdrawn to a
hypothesis.**

### What does still stand

The Northpoint waveforms are measured against each passage's FROZEN entry
reference, not against the rolling median, so no median behaviour can produce
them. They show the line flat at 50-70 counts from the reference for 1.2 and
2.4 seconds, with a genuine magnet arc riding on top of one. The line really did
move, that once. What is not supported is that it does so routinely, or why.

## The check

With the locomotive left where it is, powered and publishing:

0. FIRST, and it takes ten seconds: nudge Otto above PWM 25 for two or three
   seconds. The reference should re-learn the true line and `baseline` should
   fall from 2021 to somewhere near 1940, closing the phantom passage. If it
   does, nothing is wrong at rest and the question is confined to what happens
   to the reference while running.
1. If the baseline settles and then walks off again within a minute or two of
   ordinary running: watch `baseline` while gently flexing the sensor cable and
   tapping the mount. A step while doing so locates a physical fault.
2. If tapping does nothing, meter the sensor's supply and output at the SENSOR
   end. Output moving while supply holds means the sensor or its mount; both
   moving means the feed.

## What this does to the cruise passage ceiling

`docs/NAVI_CRUISE_CEILING_REPLAY_20260913.md` would have contained both of
today's stops. It contains MERGED magnets. It does nothing about a resting level
that keeps moving, and if a future step goes the other way the failure becomes
MISSED magnets, which no timing rule can catch. It is containment for a
mechanical fault and should not be treated as a repair.

---

# Second addendum: Otto was sitting in the sun

Operator, 2026-09-13. This is the missing variable and it reorganises
everything above.

## The level is uniform in SPACE and variable in TIME

Baseline by nav position, one column per lap of the continuous 10:19-11:21 run:

```
nav mm   10:21  10:28  10:35  10:42  10:49  10:56  11:03  11:10  11:17  11:47
  170     1948   1955   1959   1937   1949   1952   1949   1958   1955   1977
  120     1953   1955   1958   1936   1945   1949   1946   1955   1954   1982
   70     1963   1957   1936   1949   1952   1953   1969   1957   1974   1971
   20     1957   1961   1938   1951   1954   1951   1963   1957   1975      -
```

Down a column: flat around all 171 markers. **No stretch of track is special**,
which rules out a fixed trackside object, a magnet, or a place-specific cause.

Across a row: the whole level moves bodily between laps by up to 40 counts, and
it falls as well as rises — 1953, 1957, 1937, 1949, 1950, 1968, 1955, 1975. Not
a monotonic warming curve. Consistent with sun and cloud.

## The night control already exists

`field-records/logs/20260909_survey/` was collected **21:04-21:30**, after dark:
4,090 waveforms, both directions, PWM 90. None of today's behaviour appears in
it. Its only long passages are the separate parked-baseline event (see the first
addendum). That is an unplanned but real control.

`LL_LocoConfig_9950011.h` already carries the operator's own 2026-08-20 warning:
"The 14-count margin rests on ONE session at one temperature (92 F). If Otto
starts missing markers, this is the first thing to revisit."

## The at-rest records are now fully explained

Reference frozen at 2021 when throttle reached zero; Otto then sat in the sun
and the line kept moving. A frozen reference plus a moving line produces exactly
the observed phantom passages. No fault is required.

## What is still open: heat or light

They have different remedies and telemetry cannot separate them.

- Heat moves over minutes. It accounts for the between-lap shifts and the
  at-rest wander.
- The one thing heat cannot account for is Northpoint: **+26 counts in 788 ms**,
  then -60. At 265 mm/s a locomotive crosses a shadow edge in roughly half a
  second to a second — the same timescale. The 2026-08-26 daylight tests include
  a "rolling boundary" run, so a sun/shade edge on this layout is established.
- Against the shade-edge reading: the mid-lap steps went -23, +21 and +15 on
  different laps. A fixed edge crossed in one direction should give one sign.
  Either the shadow geometry moved through the morning, or the fast steps are
  something else.

No Hall sensor part number is recorded anywhere in the repository, so whether
the package is light-tight is not known from here.

## Experiment, cheapest first

1. Nudge above PWM 25 for two seconds. `baseline` should fall from 2021 to
   ~1940 and the phantom passage should close. Confirms the at-rest picture.
2. Shade the SENSOR — something opaque over it, not the whole locomotive — and
   run the same route in the same sun. Failures stopping means light at the
   sensor, and the fix is a shield.
3. If shading the sensor changes nothing, run after dark or with the layout
   shaded. Clean then means bulk temperature, and the answer is thresholds and
   reference tracking rather than hardware.

Step 2 before step 3: it is the one that distinguishes them and it takes a
minute.

This is most likely an environmental operating limit rather than a broken
locomotive. The mechanical hypothesis in the first addendum is not needed to
explain anything that survives examination, and should be treated as retired
unless step 2 and step 3 both come back clean.

---

# Provenance, and a boundary on the record

Every number in this record comes from a live MQTT capture of
`ngr/loco/9950011/#`, preserved verbatim at
`field-records/logs/20260913_otto_x16_floor82_session.log.gz`
(2026-09-12T11:20:52 to 2026-09-13T13:17:33 PDT). `pub_drop` is 0 throughout
the periods analysed, so nothing is missing from them.

**Everything after roughly 13:10 on 2026-09-13 is handling, not railway data.**
The operator lifted Otto and carried him to the charger. Between 13:11 and
13:15 the capture shows twenty-one passages of mixed polarity, peaks 41 to 184
and gaps as short as 4 ms — that is a locomotive being moved past whatever
magnetic environment lies between the track and the bench. None of it bears on
the fault. Battery voltage also falls from 15.66 to 15.44 V across that
boundary.

The reference remains frozen at 2021 throughout, because throttle is zero.

## What the charger period cannot tell us

It is not a thermal experiment. Otto is in a different place, with a different
magnetic and thermal environment, on a supply that is now charging. The three
experiments above are deferred to the next run, not invalidated.

## The single most valuable number on the next run

Where the reference LANDS once Otto is driven above PWM 25 and it re-learns —
which takes about two seconds. Today it sat near 1950 through the cool part of
the morning and had reached 1975-2021 by 11:51 in the sun. If the next run is
in shade or after dark and it settles near 1940, the environmental reading is
confirmed without any further instrumentation. Record the weather and where the
shadow line falls alongside it.

---

# Third addendum: the fault is a PLACE — MM089 to MM093

Everything above treats the disturbance as a condition of the locomotive. It is
not. Plotting every 1 Hz baseline step of 10+ counts against the position it
occurred at settles it.

## Twenty steps, and where they happened

```
time      navmm   from     to   delta   pwm
10:19:33     41   1937   1968     +31    38     first lap after priming
10:19:44     35   1973   1945     -28    90       (pairs with the above, 11 s)
10:22:47    154   1943   1954     +11    90     Bamboo ramp
10:24:52    105   1951   1963     +12    90     Arches ramp
10:37:12    154   1951   1961     +10    90     Bamboo ramp
10:39:30     89   1965   1943     -22    90     ** CRUISE **
10:51:05    157   1949   1937     -12    39     Bamboo ramp
10:53:00    107   1950   1940     -10    42     Arches ramp
10:53:04    105   1940   1950     +10    90     Arches ramp
10:58:01    157   1952   1942     -10    45     Bamboo ramp
11:05:01    155   1937   1948     +11    90     Bamboo ramp
11:06:55    105   1940   1950     +10    90     Arches ramp
11:07:16     89   1951   1971     +20    90     ** CRUISE **
11:20:58     93   1954   1980     +26    90     ** CRUISE -- the fault begins **
11:21:04     90   1981   1921     -60    90     ** CRUISE -- the fault **
11:40:49     90   1927   1987     +60    33     (nav STRUCK, mm stale at 90 -- discard)
11:41:38     90   1986   1959     -27    46     (nav STRUCK, mm stale at 90 -- discard)
11:49:48    109   1985   1970     -15    60     Arches ramp
11:51:33     63   1978   1988     +10    50     Grillers ramp
11:51:38     61   1999   2021     +22    11     Grillers ramp
```

The station clusters at MM105-109, MM154-157 and MM61-63 are the reference
freeze/resume: adaptation is suspended below `NAVI_BASELINE_ADAPT_PWM` and
catches up on departure. Expected, not a fault.

Two rows must be discarded: after the 11:21:05 strike navigation latched STRUCK
and `mm` froze at 90, so the 11:40 and 11:41 entries report a stale position,
not a physical one.

**What remains: every full-cruise disturbance today occurred at MM089-MM093.**
Four events on four separate laps out of ten passes through that stretch, and
none anywhere else on 171 markers. The two smaller ones (-22 at 10:39, +20 at
11:07) passed without incident. The fault was the same disturbance two to three
times larger.

## Toby exonerates the track as such

2026-08-28 survey, MM084-MM098, 14-15 passes each: durations 118-198 ms, peaks
175-274, every one admitted. Nothing anomalous. Either something arrived at
MM089-093 in the intervening two weeks, or it only troubles Otto — whose Hall
sensor has been documented weak since 2026-08-20 and has less margin to spare.

## Two effects, not one

- **Slow, whole-circuit, hours.** The level drifted 1950 -> 1975 -> 2021 across
  the session, uniform around all 171 markers (see the second addendum). That is
  environmental. It never caused a stop by itself. It ate margin.
- **Fast, local, seconds.** 20-60 counts at MM089-093 on roughly half the
  passes. That is the trigger.

The stop happened when the second met the margin left by the first, which is why
MM089 was survivable at 10:39 and 11:07 and not at 11:20.

## Character of the local disturbance

A clean, quiet DC offset. Sample-to-sample scatter on the 1200 ms plateau is
1.7 counts, quieter than a magnet's own slope, and it adds to the reading
without altering the magnet response: MM092 read peak 251 where that marker
normally reads about 190 — 190 plus a 60-count pedestal. It rises at about
0.3 counts/ms where a genuine magnet at cruise rises at about 2.2. That is an
external field, or a ferrous object displacing one. It is not sensor noise and
not a failing sensor.

## Action

Inspect the track between MM088 and MM094:

- the marker magnets at MM089, 090, 091, 092, 093 — proud, tilted, loose, shifted
- ferrous debris in or beside the rails: screw, washer, wire offcut, staple
- any bracket, fixing or rail joint that has worked loose since August

The second stop at Grillers occurred at stop-ramp throttle, where slow crossings
and the reference freeze already account for most of it. The two should not be
treated as one fault until MM088-094 has been looked at.
