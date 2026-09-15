# Session- and lap-level Hall baseline dataset — 2026-09-14

> **Superseded in two places, 2026-09-14.** The lap **anchoring** below uses the
> firmware's `adv` counter, which re-anchors at a direction change and therefore
> moved the warm run's origin from the declared MM071 to MM067 without saying so.
> The §8 replay used a fractional, never-rounded baseline, gave the first lap
> unclamped authority, and reported a residual that is zero by construction
> whenever the cap does not bind. Both are corrected in
> [`NAVI_BASELINE_CONTROLLER_REPLAY_20260914.md`](NAVI_BASELINE_CONTROLLER_REPLAY_20260914.md),
> which also retires `replay_lap_controller.py` in favour of
> `replay_baseline_controller.py`. The extraction, the exclusion rules and the
> reconciliation in §1–§7 stand; `laps.csv` now carries the SET LOCATION
> anchoring and the old cut is in `laps_advepoch_compare.csv`.

**What this is.** A mining and replay-fixture job, nothing more. It reads run
logs and produces three CSVs plus a comparison CSV. It changes no firmware,
builds no image, decides no policy, and grants the shadow median no authority it
does not already have — which, under X17, is none.

**Locomotive.** Otto, 9950011, running
`NAVI_ONE_1_0X17_FIXED_BASELINE_FIELDTEST` (`build_class`
`EXPERIMENTAL_FIELD_TEST`, `field_accepted` 0, `baseline_mode` `startup_fixed`).

---

## 1. Source files

Eighteen run logs written by the Pi recorder on 2026-09-14, pulled from
192.168.68.142 at 15:52 and filed in the telemetry archive outside the working
tree:

```
~/ngr-telemetry/pi/NGR/telemetry/runs/9950011_20260914_*.log
```

| file | span | note |
|---|---|---|
| `100103`, `100629` | 10:01–10:14 | X16, bench reboots |
| `102554` | 10:25–10:32 | X16 → X17 changeover |
| `103247` | 10:32–10:47 | first X17 running attempt |
| `104725`, `105149` | 10:47–11:08 | sun/heat reboot series |
| `110905`, `111835` | 11:09–11:48 | freezer series |
| `114858`, `115221`, `115836` | 11:48–12:05 | one clean lap, then the cold-start setup |
| `120505`, `123151`, `124010`, `124833`, `124938` | 12:05–12:55 | **cold-start run**, the MM140 stop, re-declarations |
| `125505`, `143544` | 12:55–15:16 | **warm-start run — the 30-lap dataset** |
| `154218` | 15:42–15:51 | later boot, no navigation |

The recorder rotates files on its own schedule; a file boundary is **not** a
session boundary and a session routinely spans several files. Sessions here are
rebuilt across files, not per file.

Note: the local archive had only been refreshed by the nightly job at 07:12, so
every file from 10:01 onwards was re-pulled. The three that were still growing
when they were last copied (`125505`, `143544`, `154218`) were truncated in the
old copy; the fresh pull is what this dataset uses.

---

## 2. Parsing rules

Each line is `ISO-8601 local timestamp <TAB> topic <TAB> payload`. Only topics
under `ngr/loco/9950011/` are read. Lines that do not parse are counted and
reported, never silently dropped (there were none today).

Four topics carry everything used here:

| topic | rate | what is taken from it |
|---|---|---|
| `state/bootid` | once per boot | firmware identity, compiled profile constants, boot instant |
| `alert` (`reason":"STATUS"`) | 1 Hz | `uptime_ms`, `mm`, `dir`, `pwm`, `moving`, `baseline`, `shadow_baseline`, `shadow_delta`, `nav_state` |
| `mm/marker` | per passage | `adv`, `mm`, `dir`, `ruling`, `base_open`/`base_close` |
| `state/nav` | per event | `DECLARED`, `DIRECTION`, `DISAGREE`, `NO_POSITION` |

`state/warning` and `diag/acquisition` are read for lap flags (stops, refusals,
82 ms floor rejections).

### Sessions

A session is one boot. It is detected first from `state/bootid`, which the
firmware publishes once at startup and which carries its own identity; and
failing that from `alert.uptime_ms` restarting near zero below the session's
running maximum. All 47 sessions on 2026-09-14 were found by `bootid` — the
fallback never had to fire, and is retained only because a boot whose `bootid`
falls in a gap between recorder files would otherwise be invisible.

Boot wall time is `first STATUS timestamp − uptime_ms`, which agrees with the
`bootid` timestamp to under a second throughout.

A record on a topic the extractor does not use never extends a session. This
matters: the `online` LWT publishes ~11 ms *before* `bootid`, and treating it as
session traffic stretched the warm run's end from 15:16:24 to 15:42:18 across a
reboot until it was fixed.

---

## 3. Lap boundary rules

**Laps are anchored to `SET LOCATION`, not to MM000.** The firmware publishes
its own accepted-advance counter, `adv`, on every `mm/marker`. It resets to zero
at boot, at every `SET LOCATION` (`state/nav` event `DECLARED`) and at every
session-direction change (event `DIRECTION`). That counter is the anchor.

- An **epoch** is the run of markers belonging to one anchor. Markers belong to
  the latest anchor at or before their timestamp.
- **Lap k** of an epoch is `adv ∈ [(k−1)·171 + 1, k·171]`. `ROUTE_N = 171`
  (`RouteMap.h:15`).
- A lap is **complete** when the epoch contains `adv = k·171`, at which point
  the locomotive is back on the marker it was anchored at.
- Lap 1 starts at the anchor instant; lap *k* starts where lap *k−1* ended. The
  windows tile the epoch with no gap and no overlap, so no observation is
  counted twice.
- A later declaration or direction change opens a **new epoch** and is reported
  as a discontinuity. Observations are never combined across one.
- Rejections, recognizer refusals and withdrawals republish the same `adv` and
  therefore do not advance lap progress — which is what the brief asks for and
  what the firmware already does.
- Incomplete laps are kept and marked `complete=0`. Every epoch that saw any
  movement ends in one.
- A reset of `adv` inside an epoch with no anchor to explain it is recorded in
  `adv_reset_unexplained` rather than quietly starting a new epoch. It is zero
  everywhere in this dataset.

### The whole-lap centre

`center_permm_median` is the primary figure: collapse the lap's observations to
one value per route position (median within each MM), then take the median
across positions.

STATUS arrives at 1 Hz **in time, not in distance**. A slow stretch of track
contributes more samples than a fast one, so a straight average over samples is
weighted by dwell rather than by geography. Per-MM first gives every marker one
vote — and it is also the shape of evidence a whole-lap controller would be
working from. The plain observation-level median, mean, SD, min, max and span
are carried alongside in the same row.

This also turns out to be the estimator behind the figures already reported by
hand; see §6.

---

## 4. Inclusion and exclusion rules for the estimator

The rolling/shadow median is **measurement data only**. Under X17 it has no
navigation authority whatsoever, and nothing here gives it any.

A STATUS sample is **kept** only if all of these hold:

1. It is not a duplicate of a `uptime_ms` already seen **in the same session**.
   Deduplication must be per session: `uptime_ms = 1000` recurs at every boot,
   so a per-file check would throw away legitimate samples from a later boot.
   (Zero duplicates survived in the three run sessions; the morning bench files
   had them.)
2. Its `uptime_ms` is not below the session's running maximum — an out-of-order
   delivery, flagged rather than reordered. None in the run sessions.
3. `shadow_baseline` is present and non-zero. The X16 sessions (10:01–10:26)
   do not publish the field at all: 814 samples, all excluded.
4. `moving == 1`.
5. `pwm > baseline_adapt_pwm` (24, read from that session's own `bootid`).
6. Condition 4 and 5 have both held for at least `shadow_median_n ×
   baselineMs` = 41 × 25 ms = 1.025 s.

**Why 4–6.** The shadow median only advances while the capture task's
`mayAdapt` gate is open, which is PWM above the tractive floor. A value
published below it is the last value from before, republished — a frozen
reading at rest, not a fresh observation. Counting it would count one
measurement many times over and would drag every station stop and every
standing period into the lap statistics. After PWM rises back above the floor
the 41-sample window still holds stale samples until it has refilled, so the
first second of qualifying motion is excluded too.

### What the rules actually removed

| reason | samples | share of all 18,005 |
|---|---:|---:|
| kept | 8,193 | 45.5% |
| `not_moving` | 8,739 | 48.5% |
| `no_shadow_field` (X16 sessions) | 814 | 4.5% |
| `pwm_at_or_below_adapt_floor` | 213 | 1.2% |
| `shadow_window_refilling` | 46 | 0.3% |

### A worked example of why rule 4 exists

The end of the warm run shows the failure mode in one place. Otto brakes to the
14:33 stop standing on the MM040 magnet:

```
14:33:41  MM040  pwm 90  moving 1  shadow 1970  (+21)   <- magnet flank, kept
14:33:42  MM040  pwm 75  moving 1  shadow 1968  (+19)
14:33:43  MM040  pwm 44  moving 1  shadow 1936  (-13)   <- last sample above the floor
14:33:44  MM040  pwm 14  moving 1  shadow 1936  (-13)   <- excluded: below the floor
14:33:45  MM040  pwm  0  moving 0  shadow 1936  (-13)   <- excluded: not moving
...
14:35:44  MM040  pwm  0  moving 0  shadow 1936  (-13)
```

That single 1936 is then republished **2,440 times over the following
41 minutes** — every sample in the `143544` file, PWM 0 throughout, `shadow`
never anything but 1936. A reader who took the stationary tail at face value
would conclude the resting level had fallen 13 counts below the fixed baseline.
It had not. 1936 is one sample taken while the locomotive was decelerating onto
a magnet, frozen by `updateBaseline`'s `if (!mayAdapt) return` at
`HallCapture.h:343` and echoed once a second until the next boot.

Almost all of the exclusion is the long stationary stretches of the bench
morning. **Inside the 30 lap windows of the warm run the rules barely bite**:
5,875 of 5,889 samples are kept, and the 14 dropped have a median of 1950
against the kept median of 1953. The rules matter for the cold and bench
sessions, not for the continuous run.

---

## 5. Sessions recovered

47 sessions, every one identified by its own `bootid`. 15 X16, 32 X17. In every
one of the 47 the post-prime fixed baseline is a single constant value — which
is what `startup_fixed` is supposed to mean, and is true of the X16 sessions too
only because they were stationary and the adaptive median never met its gate.

`identity_matches_profile` is 1 for all 47: `bootid.loco` is 9950011 and the
compiled profile carries Otto's constants (entry 70, exit 25, floor 82 ms, guard
500 ms, `seq_n` 10, `shadow_median_n` 41, `baseline_adapt_pwm` 24).

The five sessions that moved:

| session | boot | startup baseline | duration | markers | complete laps |
|---|---|---:|---:|---:|---:|
| `B20260914T103024` | 10:30:24 | 1906 | 515 s | 85 | 0 (1 partial) |
| `B20260914T114813` | 11:48:13 | 1907 | 99 s | 2 | 0 (1 partial) |
| `B20260914T115155` | 11:51:55 | 1903 | 736 s | 171 | **1** |
| `B20260914T120419` | 12:04:19 | 1905 | 2,760 s | 1,362 | **7** (3 partial) |
| `B20260914T125025` | 12:50:25 | 1949 | 8,759 s | 5,308 | **30** (2 partial) |

The other 42 are stationary bench boots. They carry no laps but they are the
startup-baseline stress material: the freezer series (11:18:35 → 1873, 11:23:34
→ 1891, 11:25:26 → 1895, 11:29:20 → 1900, 11:44:51 → 1906) and the sun series
(10:54–11:12, 1950 to 2006) are each a sequence of startup baselines under a
controlled thermal history, which is exactly what a startup-fixed reference has
to survive.

### The two run sessions in detail

**Cold start, `B20260914T120419`.** Boot 12:04:19, primed to **1905**.
`SET LOCATION` 12:05:05 at **MM040, CW** — one declaration, then three more
after the run broke (12:40:10, 12:48:33, 12:49:38) and three direction changes
(12:05:03, 12:39:00, 12:47:30). 7 complete laps, then lap 8 ends in the MM140
stop: `WRONG MAGNET at MM140` at 12:29:50, 51 withdrawal events, 6 floor
rejections. Three epochs carry laps; the later re-declarations produced no
further complete lap.

**Warm start, `B20260914T125025`.** Boot 12:50:25, primed to **1949**. Two
direction changes while position was UNSET (12:51:19 CW, 12:53:05 CCW), then
`SET LOCATION` 12:55:05 at **MM071, CCW**.

> **Discontinuity, reported not smoothed.** 23 seconds and three advances after
> that declaration, at 12:55:28, the session direction was reversed to **CW**.
> The firmware reset `adv` and re-anchored on **MM067**. All 30 complete laps
> sit in that post-reversal epoch (epoch 4). The three CCW advances before it
> are a separate epoch of their own and are not combined with them.

30 complete laps, 12:55:28 → 14:30:56, then a partial 31st ending in a stop at
14:33 (1 floor rejection, 30 withdrawal events). No missing telemetry in any
complete lap: the largest hole in the 1 Hz STATUS stream across all 30 is
1,143 ms, and `adv_reset_unexplained` is zero for every lap in the dataset.

---

## 6. Validation

`validate_baseline_laps.py` output, all structural checks passing:

```
== structure ==
  [PASS] every complete lap has exactly 171 accepted advances  -- 38 complete laps checked
  [PASS] every complete lap ends on its origin marker
  [PASS] lap windows tile each epoch with no gap or overlap  -- 0 discontinuities
  [PASS] no observation lands in two laps

== lap anchoring is SET LOCATION, not MM000 ==
    anchor event ...... DIRECTION at 2026-09-14T12:55:28.460, origin MM067, CW
    lap 1, anchored ... 12:55:28 -> 12:59:02   (ends on MM067)
    lap 1, MM000 wrap . 12:57:44 -> 13:01:00   (ends on MM000)
  [PASS] the two anchorings do not coincide  -- the first MM000 wrap is 136 s
         after the anchor -- MM000 is 104 advances downstream of MM067
  [PASS] anchored laps all share one origin marker  -- MM067 for all 30
```

Every complete lap in the dataset — all 38 of them, across every session —
carries exactly 171 accepted advances and ends on the marker its epoch was
anchored at. That is the anchoring demonstrated, not asserted: the anchored laps
begin 136 s and 104 advances away from where MM000-cut laps begin, and the two
segmentations produce visibly different lap boundaries for the same run.

### Reconciling the four reported findings

| reported | found | verdict |
|---|---|---|
| fixed baseline remained 1949 | startup 1949; the only post-prime value in the whole session is 1949 | **confirmed** |
| typical within-lap span ≈ 7 counts | median 7, quartiles 7–8 over the 30 complete laps; range 6–26 | **confirmed** |
| lap 4: median ≈ +5, mean +6.34, peak +19 near MM003–MM020 | per-MM median **+5.00**, per-MM mean **+6.34**, max **+19** at MM002–MM010, 13:07:31–13:07:40 | **confirmed exactly** |
| final five lap centres +6.0, +6.0, +6.75, +7.5, +7.5 | **+6.00, +6.00, +6.75, +7.50, +7.50** | **confirmed exactly** |

Two things fell out of getting these to agree, and both are worth stating.

**The reported figures are MM000-cut, not anchored.** They reconcile against the
MM000-wrap segmentation, which gives **30 complete laps from 12:57:44 to
14:32:53** — the reported window was 12:57:45–14:32:52, agreeing to one second.
Under the anchored definition the same 30 laps run **12:55:28 to 14:30:56**.
Same run, same 5,130 advances, different cut points. `laps_mm000_compare.csv`
carries the MM000 segmentation for exactly this purpose and for no other; it is
not the deliverable.

**The estimator behind the quarter-count values is per-MM.** `+6.75` and `+7.50`
cannot come from an integer-valued median over samples. Collapsing to one value
per MM and taking the median across the 171 positions reproduces all five values
to the hundredth, and reproduces lap 4's `+6.34` mean to the hundredth as well.
That is now the primary `center_permm_median` column.

**Where the two definitions disagree.** Cut at MM067 instead of MM000, the same
final five laps centre on **+6.00, +6.00, +6.00, +7.00, +7.00** rather than
+6.00, +6.00, +6.75, +7.50, +7.50. This is not a measurement discrepancy; it is
the definition. A lap boundary moved 104 markers along the route puts a
different ~2.5 minutes of a rising drift inside each lap. The difference, about
half a count at the end of a 95-minute run, is the size of the error you accept
by cutting laps somewhere other than where the navigator anchors.

---

## 7. The measured data

### Warm start — 30 complete laps, anchored MM067 CW, startup 1949

Relative to the 1949 startup baseline:

```
lap    1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
Δ0    +1  +1  +2  +2  +4  +3  +2  +2  +2 +2.5 +3 +3.5 +4  +4  +4
step   -   0  +1   0  +2  -1  -1   0   0 +0.5 +0.5 +0.5 +0.5  0   0
sd   1.6 1.4 1.4 5.3 1.4 1.4 1.4 1.4 1.4 1.4 1.4 1.5 1.6 1.5 1.4
span  10   7   9  26   7   6   8   7   7   7   8   8  10   8   8

lap   16  17  18  19  20  21  22  23  24  25  26  27  28  29  30
Δ0    +4  +4  +4  +4  +4  +4  +4  +4  +5  +5  +6  +6  +6  +7  +7
step   0   0   0   0   0   0   0   0  +1   0  +1   0   0  +1   0
sd   1.4 1.3 1.4 1.4 1.4 1.4 1.5 1.5 1.5 1.5 1.7 1.6 1.6 1.7 1.6
span   6   6   6   7   6   7   8  10  10  10   7   8   7   7   7
```

Within a lap the level is quiet — SD 1.3–1.7 counts and span 6–10 on 29 of the
30 laps. Between laps it creeps: **+6 counts across 95 minutes**, never more
than +2 in one lap, and flat or falling on 20 of the 29 steps. Lap 4 is the one
exception on both counts (SD 5.3, span 26) and it is an excursion, not noise:
see below.

### Cold start — 7 complete laps, anchored MM040 CW, startup 1905

```
lap    1   2   3   4   5   6   7   8(partial, ends in the MM140 stop)
Δ0    +0 +11 +15 +17 +22 +25 +26 +30
step   - +11  +4  +2  +5  +3  +1  +4
sd   1.3 4.2 1.9 2.9 2.1 1.9 2.0 9.9
span   6  14   9  11   9  10   9  41
```

Same within-lap quiet (SD ~2), an order of magnitude more lap-to-lap movement.
The single largest step, **+11 counts in one lap**, is the second lap of a cold
boot.

### Conspicuous excursions

Listed per lap in `laps.csv` (`excursions`, `max_excursion_pos/neg` with their
MM and time). Threshold: 5 counts from the lap centre.

- **The +19 at MM002–MM010, 13:07:31–13:07:40.** A step, not a spike: the level
  jumps ~14 counts in one second at MM002, holds +17 to +19 through MM010, then
  decays over the next ~40 markers to a plateau near +6 to +8 that persists to
  the end of that lap. It is the whole of lap 4's SD 5.3 and span 26. A 41-sample
  median cannot manufacture that shape; the underlying level moved.
- **Negative excursions lean towards MM074–MM099.** Fourteen of the 30 complete
  laps have a negative excursion of 5 counts or more; **8 of the 14** put it in
  MM074–MM099 (laps 1, 3, 7, 11, 12, 15, 19, 25), typically −5 to −7. That is
  inside the MM065–MM098 bar-magnet stretch, so a magnetic rather than thermal
  explanation is at least as likely. The other six are scattered (MM048, MM112,
  MM132, MM142, MM146, MM152) with no repeat position.
- **The partial laps that end in stops** (cold lap 8, warm lap 31) carry the
  extreme values: +36 at MM140 at 12:38:39, and ±21 at MM040 at 14:33. These are
  stationary-at-a-magnet artefacts of a stopped locomotive, flagged by
  `has_stop`, `has_withdrawal` and `floor_rejections`, and should be excluded
  from any fit.

---

## 8. Replay material for the proposed controller

`replay_lap_controller.py` takes the rule exactly as stated — startup baseline
authoritative until the first complete lap, one unclamped adjustment on the
first lap because it rests on route-wide evidence, at most N counts per
completed lap afterwards, all state cleared at reboot — and prints what the
reference would have been against what was measured. **It decides nothing.** The
step limit is a parameter; N = 2 is shown because that is the figure in the
brief.

**Warm start, N = 2:** the limit never binds. Thirty laps, tracking error 0.00
counts at every one. The largest correction the controller ever wants is +2.

**Cold start, N = 2:** the limit binds from lap 2 onward.

```
  lap  measured  ref_before  wanted  allowed  ref_after  error
    1   1905.00     1905.00   +0.00    +0.00    1905.00  +0.00
    2   1916.00     1905.00  +11.00    +2.00    1907.00  +9.00
    3   1920.00     1907.00  +13.00    +2.00    1909.00  +11.00
    4   1922.00     1909.00  +13.00    +2.00    1911.00  +11.00
    5   1927.00     1911.00  +16.00    +2.00    1913.00  +14.00
    6   1930.00     1913.00  +17.00    +2.00    1915.00  +15.00
    7   1931.00     1915.00  +16.00    +2.00    1917.00  +14.00
    8   1935.00     1917.00  +18.00    +0.00    1917.00  +18.00   (partial, no adjustment)
```

The first-lap unclamped adjustment does not help here: lap 1 of a cold boot is
still at the startup value, and the drift arrives afterwards. Otto opens a
passage at `|raw − reference| ≥ 70` and closes below 25, so an 18-count tracking
error makes a magnet of the polarity that pulls the wrong way need 88 counts
instead of 70 — which is the mechanism behind the MM140 stop that ended this
very run.

That is the data. What N should be, whether the first-lap authority should
extend further, and whether a cold boot should be treated differently at all are
not settled here.

---

## 9. Ambiguities and missing data

1. **The warm run's origin is a direction change, not the declaration.** The
   `SET LOCATION` was MM071 CCW; the anchor that all 30 laps hang from is the
   reversal to MM067 CW 23 seconds later. Both are in `sessions.csv`. Anyone
   reading `set_location_mm` as the lap origin for this session will be off by
   four markers and one reversal.
2. **1 Hz is coarse for a lap.** A lap takes ~190 s and yields ~190 samples for
   171 positions. Coverage is 168–171 of 171 positions per lap
   (`n_mm_covered`), so a few positions are missed each lap and a few are
   sampled twice. This is the resolution limit of the dataset, not of the
   instrument — the capture task computes the median at 40 Hz and publishes one
   of them per second.
3. **The shadow median is a 1-second window, not an instantaneous level.** It
   lags the raw level by up to ~0.5 s, which at 3 m/s is a metre of track. The
   per-MM assignment inherits that lag.
4. **Frozen-at-rest samples are excluded, not corrected.** A stopped locomotive
   still publishes a shadow value; §4 drops it rather than trying to infer what
   the level was. Long stops therefore contribute nothing, which is why the two
   partial laps that end in stops have unrepresentative statistics despite large
   `n_obs_usable`.
5. **The PWM gate is not a "not standing on a magnet" gate.** The sample at
   14:33:43 above passes every rule — moving, PWM 44, window full — and is still
   magnet flank rather than resting level. The same is true of the +21 three
   seconds earlier. Both land in the incomplete lap 31, which is flagged
   `has_stop` and `has_withdrawal` and should not be fitted; no complete lap in
   the dataset contains a braking-to-stop sequence. A controller reading these
   rows would want its own guard against adjusting on a lap that ended in a stop.
6. **X16 sessions carry no shadow field.** The 15 sessions before 10:30 have
   startup baselines and nothing else usable. They are in `sessions.csv` for
   completeness.
7. **No temperature telemetry.** The thermal history that drives all of this is
   in the operator's notes, not in the logs. Nothing here can attribute a drift
   to a cause.
8. **The 14:35–15:16 tail.** `143544` belongs to the warm session and runs to
   15:16:24, but carries no markers — Otto was powered and stationary. It
   contributes no laps.
9. **Local wall-clock timestamps.** The recorder writes local time with no zone.
   Everything here is relative and within one day, so no conversion was applied.

---

## 10. Artifacts and how to regenerate

| file | rows | what |
|---|---:|---|
| `tools/baseline_laps/out/sessions.csv` | 47 | one row per boot |
| `tools/baseline_laps/out/laps.csv` | 45 | **one row per lap**, `SET LOCATION` anchored |
| `tools/baseline_laps/out/observations.csv` | 18,005 | one row per STATUS sample |
| `tools/baseline_laps/out/laps_mm000_compare.csv` | 36 | MM000-wrap segmentation, reconciliation only |

```bash
python3 tools/baseline_laps/extract_baseline_laps.py --out tools/baseline_laps/out --mm000-compare
python3 tools/baseline_laps/validate_baseline_laps.py --out tools/baseline_laps/out
python3 tools/baseline_laps/replay_lap_controller.py  --out tools/baseline_laps/out --step 2
```

`--logs` defaults to `~/ngr-telemetry/pi/NGR/telemetry/runs`; `--loco` defaults
to 9950011; `--date` defaults to 20260914 and accepts `all`.
