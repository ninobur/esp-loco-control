# Otto — a phantom marker on departure from Grillers, 2026-08-12

Status: **Cause identified, single occurrence.** A spurious marker was admitted
at mm 65 on departure from a 39-second stop. It agreed with the expected
polarity, so it raised no flag, advanced the position counter by one, and put
every subsequent marker out of phase — producing four DISAGREE reads.

The operator predicted this from the disagreement pattern alone, before the
marker timings were examined: *"That suggests that a 'correct' phantom, agreeing
with expected, was inserted before this group."* The data confirms it.

## The evidence

| time | mm | obs | peak | ms | dt | dt_expected |
|---|---|---|---|---|---|---|
| 15:17:20.548 | 63 | N | 228 | 355 | 2162 | 1762 | ← Grillers |
| 15:17:23.582 | 64 | S | 120 | 452 | 2953 | 0 |
| **15:18:02.188** | **65** | **N** | **41** | **59** | **38975** | **0** | ← **phantom** |
| 15:18:02.706 | 66 | N | 130 | 401 | **195** | 0 |
| 15:18:03.860 | 67 | N | 147 | 157 | 1403 | 0 |
| 15:18:04.851 | 68 | S | 122 | 134 | 1010 | 813 |
| 15:18:05.814 | 69 | S | 145 | 136 | 965 | 827 |
| 15:18:06.729 | 70 | N | 133 | 134 | 917 | 813 |
| 15:18:07.677 | 71 | S | 128 | 130 | 919 | 813 |
| 15:18:08.570 | 72 | N | 104 | 124 | 892 | 799 | ← Westpoint |

Four independent tells identify mm 65 as spurious:

1. **Peak 41.** Every other marker in the stretch is 104–228. The entry
   threshold is ±38 counts from baseline (`QUORUM_STATIONARY_BASELINE_POISONING.md`),
   so it cleared the threshold by **three counts**.
2. **Duration 59 ms.** Real markers here run 124–452 ms. This is two to seven
   times too brief.
3. **`dt` 195 ms to the next marker.** At ~300 mm spacing and the speed Otto was
   actually doing, two real magnets cannot be 195 ms apart.
4. **Three consecutive `obs":"N"`** at mm 65, 66, 67. Real markers alternate
   N/S. The phantom is the surplus N.

`dt=38975` on mm 65 records that Otto had been **stationary for 39 seconds**
before this marker. This is the first detection after departure, at PWM ramping
45 → 58.

**`dt_expected` was 0 across mm 64–67** and only re-armed at mm 68. The
conservation gate was inactive for the entire insertion; nothing was in a
position to reject it.

## Why the disagreements followed

The phantom agreed with the expected polarity, so the adjudicator accepted it
silently and incremented position. From that point the locomotive's idea of
which marker it was at was one ahead of the track. Every subsequent real marker
therefore presented the opposite pole to what was expected:

| mm | observed | expected |
|---|---|---|
| 67 | N | S |
| 69 | S | N |
| 70 | N | S |
| 71 | S | N |

`nav_state` stayed `NORMAL`, `lost` stayed 0, and the adjudicator recovered on
its own — `disagree` never moved past 4 for the rest of the session
(final tally: **agree 1494, disagree 4, lost 0** over 1403 markers).

## The speed spike was a symptom, not the cause

The locomotive's own speed estimate read **1564 mm/s** at 15:18:03 while the IR
spoke read **292 mm/s** — a five-fold overestimate, physically impossible.

That is arithmetic on the phantom, not an independent fault:

```
300 mm spacing ÷ 0.195 s  =  1538 mm/s
```

which is the 1564 reported. The phantom's impossible `dt` *is* the speed spike.

An earlier draft of this record had the causality reversed — speed spike causing
the phase error. One phantom explains both, and the operator's insertion
hypothesis is what separated them.

### What the IR sensor still shows

The independent measurement is what makes the overestimate visible at all:

| time | Otto `est_mm_s` (Hall) | IR `speed_mmps` |
|---|---|---|
| 15:18:02 | 110 | 83.21 |
| **15:18:03** | **1564** | **292.48** |
| 15:18:04 | 213 | 311.35 |
| 15:18:05 | 301 | 311.35 |
| 15:18:07 | 327 | 332.83 |

`ROAD_TO_CTO.md` states the hazard: position and speed both derive from the Hall
sensor, so a bad read corrupts both at once and nothing internal can notice.
This is that hazard, observed. Every signal inside the locomotive was
self-consistent. Only the IR spoke shows the estimate was wrong — and the IR
spoke is only in the record because `ngr_runlog.py` subscribes to `ngr/#` rather
than the old loco-only pattern.

## What is still unknown

- **Why this departure.** Otto passed mm 63–72 eight times in the session. The
  phantom appeared once. Whether it needs the 39-second stop, the particular
  baseline drift while parked, or is simply intermittent is not answerable from
  one occurrence.
- **Whether the parked baseline is the mechanism.** The peak cleared the entry
  threshold by three counts, and the baseline is documented as drifting ~19
  counts across a session. A baseline poisoned while stationary would lower the
  effective threshold exactly this way — but that is inference, not measurement.
  The `baseline` field in `state/loopstat` at that moment would test it.
- **Whether the gate being disarmed (`dt_expected=0`) through mm 64–67 is
  correct behaviour on departure**, or a window that should be closed.

## Next time

```bash
# disagreements
grep -a 'state/nav' /home/david/NGR/telemetry/all_*.log | grep -a DISAGREE

# the markers around them — peak and ms are what expose a phantom
grep -a 'mm/marker' /home/david/NGR/telemetry/all_*.log
```

Look for: a low peak near the ±38 entry threshold, a duration well under 120 ms,
an impossibly short `dt`, and a break in N/S alternation. A deliberate test would
be to stop at Grillers repeatedly and watch the first marker after departure.

## Provenance

First finding produced by the continuous logger enabled the same day (0028), on
a Pi rebuilt the same day after its SD card failed. This session would previously
have been reconstructed by hand from an ad-hoc capture, if it were noticed at all.
