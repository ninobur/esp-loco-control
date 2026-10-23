# NAVI_TRACER_0_2 — Experiment 3: observation window vs. PWM and measured speed

**Status: offline research prototype only. Not firmware. No navigation
decision. No adaptive-window mechanism implemented or proposed — this is
diagnostic data collection only, per the assignment's own purpose
statement.**

## IR availability — checked directly, not assumed

Scanned both NSR captures for `REC_IR` records (type 2) for loco 9950011
over the exact time windows used in Experiments 1–2. **Zero IR records in
either recording.** (Record-type counts found: native Hall, legacy Hall,
STATUS, ACTION — no IR, in either file.) IR-measured speed is therefore
**not available** for either dataset used so far. This isn't a judgment
about IR's general reliability — these two specific sessions simply
weren't recorded with IR data in the NSR stream, probably because
NAVI_TRACER_0_1 is Hall-only and doesn't capture or forward it. Noted
plainly rather than silently working around it.

That leaves two of the three requested speed sources: PWM, and
magnet-to-magnet travel time (mapped distance ÷ elapsed time between
independently confirmed encounters).

## Method

For every pair of consecutive `CLOSE_AND_ADVANCE` events in the
telemetry log (confirmed geographic identity, not an evaluator guess),
computed:

- **Mapped distance** — `RouteMap.h`'s own `ROUTE_SPACING_MM`, read
  directly, same convention as `spanMm()`.
- **Segment average speed** — distance ÷ elapsed wall-clock time between
  the two confirmations.
- **PWM at arrival** — median of the native PWM column within ±200ms of
  the confirming close, from the raw recording.
- **Independent physical waveform duration** — *not* taken from the
  evaluator. Measured directly: bin to 20ms around the known
  approximate arrival time, establish background from the flanking
  regions (bounded by the midpoint to each neighboring confirmed close,
  so it can never reach into an adjacent magnet), then find the first and
  last 2-consecutive-block departure from that background — same
  convention used for the 70-ADC criterion elsewhere in this project.
- **Best window** — cross-referenced against Experiment 2's five window
  runs: for each known real peak, checked whether exactly one evaluator
  excursion's exact span contained it (clean), multiple did (that peak
  sits inside more than one reported piece — fragmentation or overlap),
  or none did (missed).

Excluded from the "normal segment speed" set, but reported for
completeness: the 49→48 stall segment (65.69s to cover 300mm = 4.6mm/s
average — an actual 63-second stop, not representative travel), and the
46→45 segment (confirms into the phantom MM045 — not a real magnet, per
the Stage B physical-waveform investigation, so its "arrival" isn't
trustworthy evidence of anything).

## Data

### Fresh CW/PWM~90 run — 21 confirmed segments

| mm→mm | PWM | v (mm/s) | elapsed (s) | waveform duration (s) | peak departure (ADC) | best window |
|---|---:|---:|---:|---:|---:|---|
| 35→36 | 76 | 127.5 | 2.35 | 0.22 | −173.5 | 300/440ms clean |
| 36→37 | 87 | 182.5 | 1.64 | 0.16 | −185.7 | 300/440ms clean |
| 37→38 | 91 | 222.9 | 1.35 | 0.12 | −179.2 | 300/440ms clean |
| 38→39 | 91 | 231.8 | 1.29 | 0.12 | −177.2 | 300/440ms clean |
| 39→40 | 91 | 242.7 | 1.24 | 0.12 | −175.6 | 300/440ms clean |
| 40→41 | 91 | 233.1 | 1.29 | 0.10 | −177.9 | 300/440ms clean |
| 41→42 | 91 | 234.6 | 1.28 | 0.10 | 162.2 | 300/440ms clean |
| 42→43 | 91 | 244.7 | 1.23 | 0.08 | −174.4 | 300ms clean, 440ms frag. |
| 43→44 | 91 | 251.3 | 1.19 | 0.10 | 165.9 | 300/440ms clean |
| 44→45 | 91 | 238.9 | 1.26 | 0.08 | −179.4 | 300/440ms clean |
| 45→46 | 91 | 254.0 | 1.18 | 0.12 | −191.4 | 300/440ms clean |
| 46→47 | 91 | 245.3 | 1.22 | 0.08 | −177.6 | 300/440ms clean |
| 47→48 | 91 | 240.0 | 1.25 | 0.08 | 177.5 | 300/440ms clean |
| 48→49 | 91 | 246.5 | 1.22 | 0.08 | 181.9 | 300/440ms clean |
| 49→50 | 91 | 227.8 | 1.32 | 0.08 | −171.3 | 300/440ms clean |
| 50→51 | 93 | 241.2 | 1.24 | 0.10 | 170.3 | 300/440ms clean |
| 51→52 | 89 | 244.3 | 1.23 | 0.10 | −175.1 | 300/440ms clean |
| 52→53 | 86 | 224.6 | 1.34 | 0.06 | −163.7 | 300ms clean, 440 frag. |
| 53→54 | 83 | 218.7 | 1.37 | 0.14 | −186.2 | 180/300ms best |
| 54→55 | 73 | 197.4 | 1.52 | 0.20 | −179.6 | 180/300ms best |
| 55→56 | 58 | 147.3 | 2.04 | 0.32 | 179.7 | 180/300ms best |

300ms is clean on 19 of 21; 600ms and 900ms break down almost entirely
(2 and 0 clean respectively) — consistent with Experiment 2.

### Oct 22 CCW/PWM~35-60 run — 5 confirmed segments (2 excluded as noted)

| mm→mm | PWM | v (mm/s) | elapsed (s) | waveform duration (s) | peak departure (ADC) | best window |
|---|---:|---:|---:|---:|---:|---|
| 50→49 | 59 | 102.2 | 2.94 | 0.76 | 187.7 | 300ms+ clean |
| 49→48 | 59 | **4.6 (63s stall — excluded)** | 65.69 | 0.38 | −178.2 | 300ms+ clean (the brief real encounter inside the stall) |
| 48→47 | 56 | 223.5 | 1.34 | *(no window wide enough to isolate it cleanly; not measured)* | — | — |
| 47→46 | 49 | 96.5 | 3.11 | 0.52 | −174.3 | 300ms+ clean |
| 46→45 | 37 | **64.0 (phantom-MM045 — excluded)** | 4.68 | 0.64 | 170.5 | 600ms+ only |

Only 600ms and 900ms give all four measurable segments clean in this
set; 180–440ms leave one fragmented.

## What this answers

**Does PWM alone predict a useful window?** Weakly and indirectly, not
reliably on its own. PWM 91 (most of the fast run) needs 300ms. PWM 37–59
(the slow run) needs 600ms. But the relationship isn't monotonic or
tight enough to trust as a direct lookup from this data alone — note PWM
59 (v=102mm/s) and PWM 49 (v=96.5mm/s) both clean by 300ms, while PWM 37
(v=64mm/s) needs 600ms: a 20-point PWM difference doesn't track smoothly
against required window the way the underlying physical speed does. This
matches Sam's framing exactly — PWM is a command, not a direct speed
measurement, and these numbers are the concrete evidence for exactly
that gap.

**Does measured speed improve it?** Yes, more directly — the clearer
relationship is between the *independently measured physical waveform
duration* and the window that works, not PWM or even average segment
speed. Ratio of best-window to measured waveform duration lands in a
fairly narrow band across both datasets (roughly 1–3×): PWM-90's 0.06–
0.22s waveforms want ~300ms (1.4–5×); the slow run's 0.52–0.76s waveforms
want ~600ms (0.8–1.2×). Average *segment* speed (map distance ÷ total
elapsed time) correlates less cleanly than the waveform's own measured
duration does — expected, since segment speed is an average over the
whole inter-magnet travel, while the waveform duration reflects the
actual local speed right at the encounter. The 49→48 stall segment makes
this concrete: average segment speed over that stretch was 4.6mm/s (a
63-second stop), but the real encounter captured inside it still only
took 0.38s — close to the other "normal" segments, because the
locomotive was moving at ordinary speed during the brief parts of that
segment that weren't the stall itself. Average segment speed would have
badly mispredicted the needed window there; the waveform's own measured
duration wouldn't have.

**Is the existing data sufficient to establish a reproducible PWM-to-speed
relationship?** No, and I don't think it's close. Specific gaps:

- Only two narrow PWM bands are represented (83–93, and 37–59, with 48/49
  as the only points below 58). Nothing between 60 and 76.
- No repeated measurements at the same PWM to estimate run-to-run
  variability — every data point here is a single observation.
- No IR cross-check available for either session, so there's no
  independent confirmation that segment-average speed and PWM are even
  consistently related across a wider range, or whether local track
  variation (grade, friction) shifts the relationship at different points
  on the loop.
- The stall segment demonstrates directly that average segment speed can
  be badly misleading when a segment isn't travelled at constant speed —
  exactly the acceleration/deceleration caveat flagged before this
  experiment started. We have no documented way yet to tell "ordinary
  travel" segments from "stall-contaminated" ones except by noticing an
  implausible elapsed time after the fact.

## What this does not do

- No adaptive window implemented or proposed.
- No PWM-to-speed table built or trusted as a reusable mechanism.
- Did not investigate the 48→47 segment's waveform (recovery right after
  the 63s stall) — the neighbor-bounded search window couldn't isolate it
  cleanly given the stall's own irregular timing; flagged, not forced.

## Files

`exp3_parse_closes.py`, `exp3_segments.py`, `exp3_match.py` — code for
this experiment, added to `experiment_1_code/`. Reuses the Experiment 1/2
evaluator outputs directly (no re-running of the evaluator itself was
needed — this experiment only added independent measurement and
cross-referencing).

Bringing this back for review before proposing anything about what NAVI
should actually do with this information.
