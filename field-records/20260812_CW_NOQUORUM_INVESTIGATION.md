# CW NO_QUORUM under QUORUM_1_13 — investigation log

**Opened:** 2026-08-12 · **Locomotive:** Otto (9950011), `QUORUM_1_13`, unmodified
**Capture:** `field-records/logs/20260812_otto_cw_noquorum_1_13.log` (running)
**Predecessor:** `20260812_MAGNET_REPLACEMENT_PHANTOM_VERDICT.md` §4 — the CW leg
that terminated near mm 120 and could not be root-caused because the capture had
lost the marker trail.

**Status: OPEN.** One incident captured in full. Findings below are from the
first 110 seconds of running and are preliminary.

---

## 1. Why this capture exists

The 2026-08-12 CW leg failed in `NO_QUORUM` near mm 120 with the true offset
outside `QUORUM_OFFSETS`, and the trail to the divergence was missing. The
verdict attributed the missing markers to the Pi's degraded SD card.

**That attribution was wrong** — the capture host slept. See decision 0026 for
the evidence. The practical consequence is that **CW was never blocked on the SD
card**, and this re-run could start immediately.

`capture.sh` was fixed first: it now holds the host awake, writes `# STALL`
lines when it goes quiet, and subscribes to `ngr/#` plus `$SYS` counters that
serve as a 10 s heartbeat. A gap in this log means the railway was quiet; a
`# STALL` line means the capture was.

## 2. Protocol

| | |
|---|---|
| direction | CW (`session_dir` CW) |
| firmware | `QUORUM_1_13`, unmodified — no change is being tested |
| objective | catch a CW `NO_QUORUM` **with the complete marker trail leading into it** |
| success | the divergence point is identifiable from markers, not inferred from the terminal ring |

Recovery from each incident is a DECLARE at Otto's true physical position. The
declared position must be **recorded here at the time**, since it is the only
ground truth against which an advisory can be scored.

## 3. Incident CW-1 — 16:23:46Z, NO_QUORUM at claimed mm 83, Grillers

Caught 103 seconds after the capture started. Terminal snapshot:

```json
{"e":"NO_QUORUM","mm":83,"lm":"Grillers","since":20,"dir":"CW",
 "sc":[5,7,7,null,7,6],"ex":[0,0,0,1,0,0],"ld":0,"ru":1,"mg":0,"ev":12,
 "adv":81,"advw":12,"advr":5,"advn":12,
 "ring":[["S",72],["N",73],["S",74],["N",75],["N",76],["S",77],
         ["N",78],["N",79],["N",80],["S",81],["N",82],["N",83]]}
```

- Ring full at `QUORUM_MAX` 12 — HARD_BOUND, same class as before.
- **Three-way tie at 7/12** across three offsets, `mg` 0 against `QUORUM_MARGIN` 2.
- One offset excluded (`ex[3]`=1, `sc[3]`=null).

### 3.1 The advisory returned a non-null answer, for the first time in the field

`adv: 81`, on a full ring (`advn` 12 ≥ `advw` 12) with `advr` 5.

The 2026-08-11 beta verdict recorded this as the outstanding gap: *"What was NOT
demonstrated: a non-null advisory in the field... the feature has never yet told
the operator anything they did not already know."* It has now spoken. **Whether
it was right is unknown** — scoring it needs Otto's true physical position at
16:23:46Z, which was not recorded before recovery. **Capture the declared
position on the next incident; it is the whole value of the exercise.**

### 3.2 The divergence is visible, and it starts at Grillers

The trail the last session lost:

```
16:22:45 mm 62  obs=S  peak=163  ms=379  dt=2629  gate=ACTIVE
16:22:45 mm 63  obs=N  peak= 42  ms= 46  dt= 493  gate=ACTIVE   <-- Grillers
16:22:48 mm 64  obs=N  peak=230  ms=543  dt=2200  gate=RAMP     >> DISAGREE
16:22:48 mm 65  obs=S  peak= 45  ms= 68  dt= 683  gate=RAMP     >> DISAGREE
         [36 s station dwell at Grillers]
16:23:25 QUORUM_OPEN mm 66 ... 8 x QUORUM_TIED ... QUORUM_ADOPTED mm 73
16:23:34 DISAGREE 74, 75, 76 -> QUORUM_REOPENED -> 10 x QUORUM_TIED
16:23:46 NO_QUORUM mm 83
16:23:46 mm 83  obs=N  peak=177  ms=216  dt=1535  gate=ACTIVE
16:23:46 mm 84  obs=S  peak= 42  ms= 67  dt= 263  gate=RAMP
```

Three weak events in 57 markers (`peak` < 80), against a session peak median of
149. Every one of them shares a signature:

| pair | strong event | companion | Δt | polarity |
|---|---|---|---|---|
| 62 → 63 | peak 163, ms 379 | **peak 42, ms 46** | 493 ms | S → **N** |
| 64 → 65 | peak 230, ms 543 | **peak 45, ms 68** | 683 ms | N → **S** |
| 83 → 84 | peak 177, ms 216 | **peak 42, ms 67** | 263 ms | N → **S** |

**Each weak event immediately follows a strong wide one and reads the opposite
polarity.** That is the return-flux mechanism decision 0025 identified for
stacked double magnets — a second, opposite-polarity event opening beyond the
magnet edge once `EVENT_EXIT_HOLD_MS` has closed the first.

Two observations sharpen it:

1. **The companions are narrow while the locomotive is slow.** mm 62 was 379 ms
   wide; mm 63, 493 ms later and slower still, was 46 ms. A real magnet crossed
   at that speed cannot produce an event eight times narrower. These are not
   marker crossings.
2. **The conservation gate was OFF for two of the three.** mm 64/65 and 83/84
   read `gate=RAMP`; the sequence also shows `NO_PREV`. The gate is inactive
   exactly where the locomotive is changing speed — decelerating into the
   Grillers dwell and accelerating out — which is where these events occur.
   This is decision 0024's analysis reappearing in CW.

### 3.3 What this means for decision 0025 — do not close it yet

0025 concluded the phantom was stacked double magnets and that replacing them
**eliminated** the phenomenon: 0 weak events in 214 markers, minimum peak 113.

That result was measured **CCW**. This CW capture shows 3 weak events in 57
markers, and two of them are at **mm 64 and mm 83 — markers that were never
treated.** mm 63 *was* replaced, which makes its companion event the more
interesting of the three.

The honest reading, on 110 seconds of data:

- 0025's *mechanism* is not contradicted — the signature seen here is exactly
  the one it describes.
- 0025's *scope* is: the phantom was not eliminated railway-wide, only at the
  seven markers that were replaced, and it is **visible in CW at untreated
  markers**. Whether CCW at these same markers is clean is not yet known.
- This raises the priority of the open action **"enumerate every marker that
  received the doubling remedy"**. mm 64 and mm 83 are now predicted sites.

**Not yet established:** whether the CW divergence is *caused* by these phantom
insertions, or whether both are downstream of something else. The adoption at
mm 73 and the reopen at mm 74–76 need working through against `NGR_DNA1` before
that claim can be made.

## 4. State and open items

- Otto is stopped in `NO_QUORUM` at claimed mm 83, `pwm` 0, `auto` 1. Recovery
  needs a DECLARE — **record the true position here when it is made.**
- The capture is running and healthy. **It is on battery.** `caffeinate` holds
  off idle sleep but cannot survive a flat battery, and a closed lid sleeps
  regardless. Plug the Mac in.
- mm 99 is still installed inverted (verdict §3). Unrelated to this fault.

## References

- `docs/decisions/0026-*` — why the previous CW capture was empty
- `docs/decisions/0025-*` — the phantom mechanism; its scope is questioned above
- `docs/decisions/0024-*` — the conservation gate measures rather than predicts
- `docs/decisions/0023-*` — the advisory; first non-null field result recorded here
- `field-records/20260811_QUORUM_1_13_beta_verdict.md` — the CW Grillers phantom, seen before
