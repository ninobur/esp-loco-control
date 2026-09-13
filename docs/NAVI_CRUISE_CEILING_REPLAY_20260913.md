# NAVI_ONE — offline replay of a bounded acquisition reset

**Date:** 2026-09-13
**Status:** Host replay only. No production header, sketch, configuration or
decision record was changed, and this gate is not in `run_tests.sh`.
**Task:** the operator's, after the Northpoint stop — establish by offline
replay whether a cruise passage ceiling separates the magnets that incident
merged, while leaving every legitimate slow or stationary passage unchanged.

## What was built

| file | what it is |
|---|---|
| `tests/fixtures_northpoint_20260913.h` | the six passages published during the incident, verbatim |
| `tests/HallCaptureCeiling.h` | a verbatim copy of `HallCapture.h` with one addition |
| `tests/gate_cruise_ceiling.cpp` | the replay, 34 checks |

The reset consults duration, PWM and the signal. It does not consult
morphology, map polarity, station identity or navigation position, and it
cannot reach any of them.

```
  if (cruiseCeilingMs && pwm >= ceilingPwm && nowMs - openedAtMs_ >= cruiseCeilingMs)
      return forceClose(nowMs, raw, pwm);
```

`forceClose()` calls the unchanged `close()`, so a forced-closed passage is
assembled, oriented, judged and floor-tested by exactly the same code as any
other passage — **it is not a rejection**. It then rebases `baseline_` and
`entryBaseline_` onto the level the line is actually sitting at, re-seeds the
41-sample rolling median with it, drops the pre-roll, re-arms, and records a
`ForcedClosure` for telemetry.

The median re-seed is not optional. Without it the median still holds the
pre-excursion window, snaps the reference back on its next 25 ms tick, and
re-opens the same passage.

## The two constants, and where they come from

**Ceiling 550 ms.** The longest clean passage at cruise anywhere in the
corpora is 212 ms (2026-09-09 survey) and 302 ms (2026-09-13, PWM 72). 550 ms
is 1.8× the larger and still well inside one marker interval, which measured
1,121–1,289 ms on the seven markers before the incident.

**Armed at PWM ≥ 70.** At PWM 70 and above no clean passage in either corpus
exceeds 302 ms. Below it the measured maximum climbs through 462 ms (PWM 55–69)
to 33,762 ms at a standstill. All nineteen non-incident long passages recorded
on 2026-09-13 closed at PWM 22–47.

## Results

### A. the copy is production `HallCapture`, plus one thing

With `cruiseCeilingMs = 0` the copy matches production on every sample — same
open/close decision, same baseline, same assembled passage, same floor-reject
count — across the six incident records, an Arches departure fixture, the
finding-09 stationary latched-offset fixture, a flat line, a clean 150 ms
passage and a sustained offset.

### B. the incident: the latch is cut, and acquisition re-arms

Replaying the recorded 2,446 ms latch:

```
reset off : 1 passage, held open 2492 ms
reset on  : 2 passages, 1 forced closure
  forced  : dur=550 ms  entry_base=1981  base_before=1981  rebased_to=1921  peak=77  admitted
  amplitude at the forced close: 77 / gain 174 = 0.443   (floor 0.34)
acquisition is live again 1942 ms earlier than it was in the field
```

The forced-closed passage still clears the recogniser's amplitude floor, so the
magnet is not traded for a miss — which was the operator's stated requirement.
1,942 ms is more than one marker interval at the measured cruise cadence.

### C. a magnet arriving after the latch gets a passage of its own

The recorded 156 ms cruise magnet from the same lap, superimposed on the
recorded latch one measured marker interval (1,200 ms) after it opened:

```
reset off : 1 passage  -- the magnet is entirely inside the latch
reset on  : 3 passages -- 550 ms (forced), 89 ms peak 118 N, 112 ms peak 151 S
```

### D/E. Otto's cruise, slow and stationary corpora — 4,090 records

```
at PWM >= 70: 4018 records, 3132 of them clean (untruncated, unclipped, admitted)
longest CLEAN passage at PWM >= 70: 212 ms          (ceiling 550 ms)
the reset would fire on 4 of 4018 (0.10%):
   dur=  597 pwm=90 peak= 27 rej=2 truncated=1 clipped=0
   dur= 1358 pwm=80 peak= 48 rej=0 truncated=1 clipped=1  mm=48
   dur= 4944 pwm=90 peak=217 rej=0 truncated=1 clipped=1  mm=47
   dur= 2261 pwm=90 peak=204 rej=0 truncated=1 clipped=1  mm=45
at PWM < 70: 72 records, longest 33762 ms — all untouched, the reset is disarmed
```

Every record it fires on is truncated. **Not one clean crossing is touched** —
none of the four is a magnet arc; all four are flat lines held open by a
reference that was in the wrong place.

The three from 2026-09-09, however, are **not** the Northpoint fault and should
not be counted as independent support for it. They decode to a flat −33 to −36
count line with no magnet in them, follow about a minute at a standstill, and
are finding 10 / decision 0074 — a reference learned while parked — on QUORUM
1.13X, which predates the `NAVI_BASELINE_ADAPT_PWM` motion gate NAVI_ONE now
has. Their `clip=1` is the base64 encoding saturating on one entry sample, not
the ADC railing. The ceiling would still have cut them, but the evidence for it
rests mainly on the single Northpoint episode.

### F. the arming threshold against 2026-09-13

Of the 22 passages of 400 ms or more recorded that day, three closed at
PWM ≥ 70 — and all three are the incident. The other nineteen are station and
slow crossings and are disarmed.

### G. ceiling sweep over the whole cruise corpus

| ceiling | fires | clean hits | shortest hit |
|---:|---:|---:|---:|
| 300 | 5 | 0 | 394 |
| 400 | 4 | 0 | 597 |
| 500 | 4 | 0 | 597 |
| **550** | **4** | **0** | **597** |
| 600 | 3 | 0 | 1358 |
| 900 | 3 | 0 | 1358 |

400–550 ms are indistinguishable on this corpus. 600 ms and above stop catching
the 597 ms record. 300 ms starts reaching down toward a 394 ms one.

### H. a sustained offset, and what happens when it goes away

At Otto's real 70-count entry margin, over a 20 s offset followed by its
disappearance:

| offset | production passages / longest | with reset passages / longest |
|---:|---:|---:|
| 30, 60, 69 | 0 / — | 0 / — |
| 71, 90, 120 | 2 / 2483 ms | 2 / 550 ms |

The reset does **not** chain and does not produce more events than production
over the same offset: because the rebase puts the reference where the line
actually is, nothing re-opens until the line moves again.

### I. the residual risk: commanded at cruise but not actually moving

PWM is the reset's only speed evidence. A locomotive commanded at 90 but held
still — slipping, obstructed, stopped against a magnet — looks like cruise to
it. Parked twelve seconds in a 200-count field at commanded PWM 90:

```
production : 2 passages, longest 2483 ms
with reset : 2 passages, longest  550 ms, 2 forced (1954->2154, then 2154->1954)
```

Both end up referenced to the field; `mayAdapt` already has the same exposure
above PWM 24. The reset arrives there in one step where the median walks there
over about 525 ms. Same destination, different speed — not a new class of
hazard, but the fastest path to it.

## Acceptance criterion

> Separate the missed magnets in this incident while leaving every legitimate
> slow or stationary passage unchanged.

**Second half: met, without qualification.** Zero clean passages touched in
4,018 cruise records; all 72 slow and stationary records untouched; all
nineteen non-incident long passages of 2026-09-13 disarmed; equivalence to
production proven sample-for-sample when disabled.

**First half: met as far as the record allows, and no further.** The magnets
this incident lost never opened a passage, so no waveform of them exists and no
replay can show them recovered. What the replay does show is that the latch
which covered their ground is cut at 550 ms, that acquisition is live again
1,942 ms earlier, that the magnet which opened the latch is still judged and
still clears the amplitude floor, and that a measured cruise magnet arriving
inside that window gets a passage of its own instead of being swallowed.

## What the replay cannot establish

1. **The MM092 waveform does not exist.** Its 1,569 ms passage was accepted, and
   this build dumps a waveform only for a refusal or a strike. The two latched
   records replayed are from the same episode and carry the same failure.
2. **Sections B, C, H and I drive reconstructed streams.** The published records
   are oriented, entry-baseline-relative and decimated; each stored sample is
   held for its decimation interval and the gaps between records are filled at
   the measured baseline. Levels and durations are the field's; the millisecond
   detail between decimated samples is not recoverable (decision 0070). The
   recorded 2,446 ms latch replays as 2,492 ms.
3. **Three of the four records are from a retired firmware and a failure mode
   already guarded against** (see D/E). The corpus therefore supports the
   ceiling on one live episode, not four. Their base64 encoding also saturates
   at ±384, so the amplitude a forced close would carry on them is not
   computable from the published data; only the Northpoint record, which is
   binary `int16`, yields a real number (0.443).
4. **PWM is a proxy for speed, not speed.** It is not in the operator's excluded
   list and `mayAdapt` already depends on it, but it is the one input here that
   can be wrong about the world. Section I measures what happens when it is.
5. **This does nothing for the multi-second departure plateaus.** They occur
   below the arming threshold and the reset is disarmed there, by design and by
   the operator's instruction.

## Recommendation, offered as a recommendation

The evidence supports a 550 ms ceiling armed at PWM ≥ 70 for a bounded
experimental field test on Otto. 400–550 ms are equally supported by the corpus;
550 leaves the most headroom above the longest clean crossing while still
catching every latched record. Nothing here is integrated into firmware, and the
prerequisites an integration would still need are the ones the audit trail
already names: the shared constant in `NAVIFieldConfig.h` so the gates cannot
drift from the flashed value, a `diag/acquisition`-style one-shot publication of
each `ForcedClosure`, and an entry in the flash-verification block.

The operator makes the decision.
