# Otto, 2026-09-13 — two stops on the X16 floor-82 field test

**For handoff. Read the "Withdrawn" section before building on anything here.**

**Locomotive:** Otto, 9950011
**Build:** `NAVI_ONE_1_0X16_FLOOR82_FIELDTEST` (decision 0085, experimental, not
field-accepted)
**Source data:** `field-records/logs/20260913_otto_x16_floor82_session.log.gz` —
a live MQTT capture of `ngr/loco/9950011/#`, 2026-09-12T11:20:52 to
2026-09-13T13:17:33 PDT. Every figure below is drawn from it. `pub_drop`,
`stale` and `cmd_drop` are 0 for the entire session and the 1 Hz status stream
is unbroken from 10:17 to 11:29, so nothing is missing from the periods
analysed.
**Full detail:** `field-records/20260913_NORTHPOINT_ACQUISITION_LATCH.md`
(this summary is the short form; that file carries the corrections in order).

---

## 1. What happened

**Stop 1 — 11:21:05, MM090, CCW at cruise PWM 90.**
`WRONG MAGNET at MM090: expected S at MM089, read N.` Navigation had fallen
three to four markers **behind** the railway. Operator found the locomotive
consistent with that.

**Stop 2 — 11:51:37, MM061, CCW during the Grillers stop ramp at PWM 27.**
`WRONG MAGNET at MM061: expected S at MM060, read N.` Navigation had run one
marker **ahead**. Operator confirmed the locomotive physically between MM062 and
MM061, which matches navigation exactly — so position was correct at the strike
and a merged event had been accepted as MM061 before the real one arrived.

Between the two: the operator re-declared position **without power cycling**
(`uptime_ms` 10,441,953 ms, unbroken, no reboot in the record) and Otto ran 210
clean advances over about eight minutes and four station stops before failing
again.

Both strikes are **correct behaviour**. The one-strike polarity rule did its job
in each case.

---

## 2. The 82 ms duration floor is not implicated

`floor_rej` read 2 before, during and after Stop 1, and moved only at 11:51:33
onward. Only two `diag/acquisition` records exist for the whole day, both at
10:17 and 10:19 before the run. Every event in either failure lasted 95–2,446
ms, far above 82. **The floor rejected nothing at either failure.** It neither
caused nor prevented them.

---

## 3. The mechanism, established

### 3.1 The reference is a median, and nothing screens what enters it

`HallCapture::updateBaseline()` keeps a rolling **median** — not an average — of
41 samples taken one per 25 ms, about a one-second window. It has exactly three
gates:

```cpp
if (nowMs - lastBaseMs_ < cfg_.baselineMs) return;        // 25 ms rate limit
if (!mayAdapt) { adaptSinceMs_ = 0; return; }             // PWM below tractive floor
if (open_ && nowMs - from < cfg_.openMigrateMs) return;   // a passage is open, < 2 s
med_[medHead_] = raw;                                     // otherwise: straight in
```

No amplitude test, no duration test, no shape test. The 82 ms floor, the 0.34
amplitude ratio, the 500 ms guard and median-of-three all act on **completed
passages**, downstream. They never touch this path.

Consequence: **a disturbance below the 70-count entry margin opens no passage,
so `open_` stays false, so it is ingested with no screening at all.** To move the
median it must displace a majority of the window — more than 20 of 41 samples,
i.e. **longer than about 525 ms**. A spike contributes one sample in 41 and does
nothing.

### 3.2 How stable the reference actually is

Measured over 1,698 consecutive marker-to-marker pairs today:

```
|change| median    1 count      within ±3 counts   91.9%
         75th      2            within ±5 counts   95.8%
         90th      3            within ±10 counts  99.0%
         99th     10            over 20 counts      0.2%  (4 of 1698)
         max      58
```

Cruise and slow running are the same. The session's environmental drift
(1936 → 2021, about 85 counts over three hours) is spread over thousands of
markers — roughly 0.006 counts per marker — and never appears as a step.

**Every marker-to-marker change above 15 counts today, complete:**

```
10:19:44   mm 34   -29   pwm 90    first lap after priming
10:39:30   mm 88   -26   pwm 90
11:07:16   mm 88   +20   pwm 90
11:20:59   mm 92   +26   pwm 90    <- Stop 1 begins
11:21:04   mm 90   -58   pwm 90    <- Stop 1
```

Five, out of 1,698. Stop 1's two are 26x and 58x the median change. These are not
the tail of ordinary variability; they are a separate population.

### 3.3 Stop 1, in order

```
11:20:57  MM093 accepted   dur 129  peak 178  base 1954     normal
          788 ms gap, nothing open, adaptation running normally
11:20:58  baseline 1980                                      +26, sub-threshold, unrecorded
11:20:59  MM092 accepted   dur 1569 peak 251  base 1980     <- jammed
11:20:59  refused          dur 95   peak 112  gap 52
11:21:01  refused          dur 1200 peak 76   gap 215
11:21:01  MM091 accepted   dur 96   peak 117  gap 1472
11:21:03  refused          dur 2446 peak 257  gap 62   base_open 1981 -> base_close 1936
11:21:04  MM090 accepted   dur 160  peak 199  base 1923
11:21:05  STRIKE           dur 156  peak 188  base 1920
```

1. A shelf of about +26 counts sat on the line for essentially the whole 788 ms
   gap between MM093 and MM092. Sub-threshold, so no passage, so no record
   anywhere — **its only trace in the system is the baseline field changing
   between two marker records.** The median ingested it and the reference moved
   to 1980.
2. MM092's magnet opened a passage against that new reference.
3. While the passage was open, adaptation is suspended by design
   (`openMigrateMs` = 2000 ms). During that window the line fell back roughly 60
   counts to about 1921, and the reference could not follow.
4. The passage could not close: the line was now 49–70 counts from its frozen
   entry reference and the exit margin is 25. 1,569 ms, then 1,200, then 2,446.
5. Real magnets arriving inside those windows were merged; fragments emerging
   after them fell inside the unconditional 500 ms guard and were refused.
   Navigation advanced on whichever fragments happened to clear it.

Six of the seven passages ran with the baseline **frozen** (`base_open ==
base_close`). The only one that moved is the 2,446 ms latch, where
`openMigrateMs` engaged and that migration is what finally let it close.

### 3.4 What the disturbance is, from the waveforms

Binary `int16` records, measured against each passage's frozen entry reference,
so no median behaviour can produce them:

```
MM090, 160 ms, genuine:   55  67  80  97 115 138 147 171 183 193 198 196 192
                         182 167 148 129 106  86  68  45  39  28  16     an arc

1200 ms latch:            69  72  64  67  61  62  68  67  60  64  60  61  60
                          64  60  63  60  65  61  62  62  62  57  36      flat

2446 ms latch:            70  68  63  63  67  60  58  58  59  59  54  60
                        [164 172] 57  49  53  56  60  65  59  60  62  62  flat,
                                                      with a real magnet arc on top
```

- Rise rate: a genuine magnet at cruise rises about **2.2 counts/ms**; the
  latches rise about **0.3 counts/ms** and then flat-top.
- Sample-to-sample scatter on the 1,200 ms plateau is **1.7 counts** — quieter
  than a magnet's own slope. Not noise, not contact chatter.
- **Additive, with the magnet response intact.** MM092 read peak 251 where that
  marker normally reads about 190: 190 plus a ~60-count pedestal. Sensor gain is
  unaffected; only the offset moved.

---

## 4. Ruled out, with the evidence

| Candidate | Why not |
|---|---|
| The 82 ms duration floor | `floor_rej` never moved at either failure |
| Departure plateaus / post-stop | Arches DEPARTED 11:20:42; last `diag/departure` 11:20:42; Stop 1 began 11:20:58, 16 s and 13 markers later at full cruise |
| Supply sag / shared ground / motor load | baseline vs motor current **r = −0.023**, vs PWM **r = −0.002**, over 3,194 moving samples. Mean baseline flat at 1951–1956 across every current band from 0.15 A to over 0.40 A |
| Communications, queue pressure, timing | `pub_drop` 0, `stale` 0, `cmd_drop` 0; 1 Hz stream unbroken 10:17–11:29 |
| Bad magnets at MM089–093 | 10–12 cruise passes each, all normal (table below) |
| Bad track at MM089–093 | Toby's 2026-08-28 survey crossed MM084–098 14–15 times each with nothing anomalous |
| Route geometry | MM088–093 is uniform 300 mm spacing, no grade compensation, nothing in the cruise table |
| A static magnetic or ferrous source between the rails | Operator ran a steel bike brake cable down the track MM089–093 (would be pulled by a stray magnet), then repeated with a small magnet on the end (would be pulled by ferrous material). Both attracted only at the marker magnets |

**Otto's MM084–098, today, 10–12 cruise passes per marker, against Toby's
2026-08-28 survey:**

```
mm    | TOBY durations   peaks      | OTTO durations   peaks
89    | 128-182          183-239    | 153-195          184-209
90    | 141-191          184-220    | 156-196          167-199
91    | 127-178          175-193    | 164-192          160-174
92    | 148-178          180-220    | 155-170          187-202
93    | 125-163          191-212    | 129-155          178-193
```

Otto's MM089–093 sit inside his own range for MM084–088 and MM094–098, and track
Toby's allowing for Otto's documented-weak sensor giving peaks 20–30 counts
lower throughout. MM093 read 129 ms two seconds before Stop 1 began; MM090 read
160 and 156 ms during the strike itself. Only the two values that *are* the
fault stand out.

---

## 5. Withdrawn during this investigation — do not resurrect

Four conclusions were reached and then retracted. Each is recorded so the same
ground is not covered again.

1. **"The ADC railed on 2026-09-09, which no magnet can do."** Wrong. Each of
   those records has exactly one encoded byte at the top of its range, and it is
   the first sample — the entry crossing. The survey encodes samples as one byte
   scaled by 3, so anything beyond ±384 saturates. It is the **base64 encoding**
   clipping on a real magnet edge, not the input reaching a supply rail. This was
   the strongest evidence offered for an electrical fault and it does not exist.

2. **"The 2026-09-09 marker stream jumped 45 → 42, three markers lost in one
   step."** Wrong. Those two records are 151 ms apart in device time; three
   spacings at 265 mm/s is impossible. It is QUORUM 1.13X adopting a −3 offset
   (visible as `drift=-3`, later `-6`). The real loss is measured by the
   device-time gaps between clean magnets: 1,391 / 4,967 / 2,288 ms against a
   ~1,200 ms cadence, about four intervals unaccounted for.

3. **"A mechanical instability in the Hall sensor's path."** Withdrawn. It
   rested on at-rest activity and a regular step rhythm, and neither survives.
   The at-rest records need no fault: the reference is frozen below the tractive
   floor by design, Otto's froze at 2021 — the last tick before throttle reached
   zero, the worst moment of the failure — while the true line is ~1920–1960, and
   a passage that opens and will not close is the correct response to a reference
   100 counts from the line. The "rhythm" was 20 jumps of 10+ counts out of 3,150
   samples, 0.6%, several of them pairs that cancel within seconds — which is
   what a rolling median does when its window straddles a disturbance.

4. **"Inspect MM088–094 for a displaced magnet or debris."** Withdrawn twice
   over: first because the marker readings there are normal across ten laps, then
   confirmed by the operator's physical probe finding nothing.

**Also note:** the 2026-09-09 episode at MM050–041 is a *different* fault. Its
three long records decode to a flat −33 to −36 count line with no magnet in them,
the clean arcs either side end at exactly −35, and it follows about a minute at a
standstill. That is finding 10 / decision 0074 — a reference learned while parked
— on QUORUM 1.13X, which predates the `NAVI_BASELINE_ADAPT_PWM` motion gate.
It is not evidence about today.

---

## 6. Two effects, and only one of them is the fault

- **Slow, whole-circuit, hours.** The reference drifted 1950 → 1975 → 2021 across
  the session, **uniform around all 171 markers within any one lap** and shifting
  bodily between laps, falling as well as rising. Otto was sitting in the sun;
  the layout has a documented sun/shade boundary (2026-08-26 daylight tests), and
  `LL_LocoConfig_9950011.h` already warns that the margins rest on one session at
  92 °F. Baseline vs time is r = +0.625. This never caused a stop by itself. It
  ate margin.
- **Fast, local, seconds.** A sub-threshold shelf of 20–60 counts lasting most of
  an inter-marker gap. This is the trigger.

Stop 1 happened when the second met the margin left by the first, which is why
the same disturbance at MM088 was survivable at 10:39 (−26) and 11:07 (+20) and
not at 11:20 (+26 then −58).

**Stop 2 is a weaker and probably different case.** It occurred at stop-ramp
throttle (PWM 61 → 27) where slow crossings and the reference freeze already
account for most of it; the reference climbed 1975 → 2021 over eight seconds as
the throttle fell, and passages stopped separating. The two should not be treated
as one fault.

---

## 7. Open, and the one test that resolves it

Every non-priming outlier fell in the gaps **MM089→088, MM089→088, MM093→092,
MM091→090** — four inside six markers, from a population of 1,698 pairs whose
median change is one count, spread over 171 markers and eleven laps. If they were
scattered uniformly that is roughly a one-in-four-thousand coincidence.

Against that, the operator's physical probe found nothing. Its blind spots are
real — swept volume (between the rails at rail height, not beside, below or
overhead), sensitivity (the disturbance is a quarter to a fifth of a marker
magnet's strength at the sensor; a hanging cable may not visibly respond), and
above all intermittency (four of eleven passes; a static sweep tests the static
case).

**So the statistics and the physical evidence disagree, and n = 4.**

**Recommended next test: run clockwise.** Same physical stretch, opposite
direction, different point in the lap cycle, different preceding station,
different approach speed. If the outliers still land at MM088–093 it is the
place, and the question becomes height and lateral offset rather than whether
something is there. If they land elsewhere or scatter, the clustering is
coincidence. Failing that, simply more laps: eleven passes gave four outliers,
thirty would give a distribution nobody has to argue about.

---

## 8. Related work in this branch, and its status

`docs/NAVI_CRUISE_CEILING_REPLAY_20260913.md` — an offline replay of the
operator's proposed bounded acquisition reset (a cruise passage ceiling, 550 ms,
armed at PWM ≥ 70). Test-only: `tests/HallCaptureCeiling.h` (a verbatim copy of
`HallCapture.h` plus one addition, proven identical to production when
disabled), `tests/gate_cruise_ceiling.cpp` (34 checks),
`tests/fixtures_northpoint_20260913.h`. Not in `run_tests.sh`, no production
header changed, full NAVI suite green.

It would have contained both of today's stops. **It is containment, not a
repair.** It bounds merged magnets; it does nothing about a reference that moves,
and if a future disturbance goes the other way the failure becomes *missed*
magnets, which no timing rule can catch. Its corpus support also rests mainly on
the single Northpoint episode — three of the four records it fires on come from
the retired QUORUM 1.13X firmware and the already-guarded parked-baseline mode.

**Do not treat freezing the baseline during an open passage as the fix.** It
already is frozen, for 2,000 ms, and it was frozen through the passage that did
the damage at Stop 1 (`base_open 1980, base_close 1980`). Permanent freezing was
the behaviour in 0.1–0.6 and produced a documented 72-minute deadlock on
2026-09-01 (finding 09, quoted in `HallCapture.h`): a wrong reference opens a
passage, the open passage prevents the reference being corrected, nothing
escapes. `openMigrateMs` exists to break exactly that.

The real defect is upstream of open passages: the reference is learned in the
gaps between markers — the only time it can be — and a 41-sample median cannot
distinguish a genuine level change from a transient shelf that outlasts its own
memory. Both look identical to it: a majority of the window at a new value.

---

## 9. Commits

```
412ffc2  Northpoint acquisition latch, and an offline replay of a bounded reset
4c69d7e  the 2026-09-09 survey episode is a different fault, and two corrections
669909e  second stop at MM061, and the root cause is mechanical      [superseded]
0dcc408  withdraw the mechanical conclusion -- the at-rest records prove nothing
3b70896  Otto was in the sun -- the level is uniform in space, variable in time
36f179d  preserve the source capture, and mark where the railway data ends
5e18c67  the fault is a place -- every cruise disturbance is at MM089-093
e6aa2d1  withdraw the MM088-094 track search -- the magnets there are healthy
```

Read them in order if the reasoning matters; read
`field-records/20260913_NORTHPOINT_ACQUISITION_LATCH.md` end to end if only the
conclusions do, because its addenda correct each other in sequence.
