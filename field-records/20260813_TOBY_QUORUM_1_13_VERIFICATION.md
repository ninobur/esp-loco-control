# Toby on QUORUM 1.13 — verification run, 2026-08-13

**Verdict: the magnet work is confirmed on a second locomotive. 1682 markers,
8 disagreements, all 8 from a single direction-reversal artefact, zero weak
events, zero losses.**

**Locomotive:** Toby (9950012), `QUORUM_1_13`, flashed earlier the same day.
**Session:** 12:49:12 → 13:53:16, ~64 minutes, both directions.
**Recording:** `ngr-runlog` on the Pi, continuous, `throttled=0x0` throughout.
Evidence: `~/ngr-telemetry/pi/NGR/telemetry/runs/9950012_20260813_124929.log`.

---

## Headline

| | |
|---|---|
| markers | **1682** |
| polarity mismatches vs `NGR_DNA1` | **8** (0.48%) |
| distinct markers crossed | **171 of 171** |
| `lost` | **0** |
| weak events (peak < 80) | **0** |
| markers below peak 123 | **0** |
| peak median / min / max | 184 / 123 / 304 |

## 1. The phantom is gone — cross-locomotive confirmation

**Zero events below peak 80 in 1682 markers.** The route's weakest reading all
session was 123, against a median of 184.

Against Otto's pre-replacement baseline in the same week:

| | Otto, before | Otto, after | **Toby** |
|---|---|---|---|
| markers | 235 | 214 | **1682** |
| weak events (peak < 80) | 12 (5.1%) | 0 | **0** |
| minimum peak | 39 | 113 | **123** |
| `PHANTOM_REJECTED` | 4 | 0 | **0** |

This is independent confirmation of decision 0025 on a different locomotive and
a different Hall installation.

**Correction, 2026-08-13:** an earlier revision of this record also cited "the
opposite `HALL_POLARITY_INVERTED` setting" as evidence of independence. That was
wrong. Per `docs/CLAUDE.md` the Hall sensors are mounted identically on both
locomotives and **no firmware reads that symbol** — polarity comes solely from
which threshold was crossed. It is dead config and carries no evidential weight
here. The cross-locomotive result stands on the different installation alone.

The stacked double magnets were the cause;
replacing them with single disks on the crosstie tops fixed it. Nothing in
firmware was changed to achieve this.

## 2. Every repaired site reads correctly, every crossing

Ten crossings apiece across both directions:

| mm | n | observed | `NGR_DNA1` | result | peak range |
|---|---|---|---|---|---|
| 61 | 10 | N | N | ok | 264–275 |
| 62 | 10 | S | S | ok | 187–198 |
| 63 | 10 | N | N | ok | 267–285 |
| **99** | 10 | **S** | **S** | **ok** | **286–304** |
| 100 | 10 | N | N | ok | 268–280 |
| 101 | 10 | S | S | ok | 264–270 |
| 102 | 10 | S | S | ok | 264–270 |
| 149 | 10 | N | N | ok | 153–161 |
| 150 | 10 | S | S | ok | 170–180 |

**mm 99 is fixed.** It read inverted on Otto (obs N against an expected S, peak
279) once per lap over five laps. The operator flipped the disk; it now reads S
on all ten crossings. That closes the only genuine defect the Otto run found.

Peak spreads are tight — mm 102 reads 264, 265, 266, 264, 264, 264, 269 across
laps. That is a stable installation, not a marginal one, and it is the property
that matters: a magnet read consistently is a magnet that will keep being read.

## 3. The 8 disagreements are a reversal artefact, not magnets

All 8 fall inside a 17-second window, immediately after three direction changes
in 60 seconds:

```
13:28:51  DIRECTION          -> CCW
13:29:27  DIRECTION          -> CW
13:29:50  DIRECTION          -> CCW   + SESSION_DIRECTION
13:30:10  DISAGREE  mm=16  obs=N exp=S  peak=175  ms=1554  pwm=30  gate=LOW_PWM
13:30:14  DISAGREE  mm=15  obs=N exp=S  peak=184  ms=360   pwm=61  gate=RAMP
13:30:18  DISAGREE  mm=12  obs=S exp=N  peak=191
13:30:19  DISAGREE  mm=11  obs=S exp=N  peak=193
13:30:23  DISAGREE  mm=8   obs=N exp=S  peak=227
13:30:24  DISAGREE  mm=7   obs=N exp=S  peak=203
13:30:25  DISAGREE  mm=6   obs=S exp=N  peak=237
13:30:27  DISAGREE  mm=5   obs=S exp=N  peak=255
```

Peaks of 175–255 throughout — every one a strong, clean read. The **magnets were
detected correctly and attributed to the wrong numbers**: the locomotive turned
around between markers and re-crossed the one it had just passed, so the
odometer ran one ahead of the truth. Note the first two carry `gate=LOW_PWM` and
`gate=RAMP` with `ms=1554` and `dt=4691` — the signature of the reversal shuffle
itself, at a crawl.

Alternating obs/expected polarity across consecutive markers is what an
off-by-one looks like against a pseudorandom sequence, not what a bad magnet
looks like.

### QUORUM recovered unaided, in four seconds

```
13:30:25  QUORUM_OPEN     mm=6
13:30:25  QUORUM_TIED     mm=6
13:30:27  QUORUM_TIED     mm=5
13:30:28  QUORUM_ADOPTED  mm=2
13:30:29  QUORUM_CLOSED   mm=1
```

Then **670 consecutive markers to the end of the session with zero
disagreements.** This is a clean worked example of the fence doing precisely
what it exists for: an offset inside `QUORUM_OFFSETS`, adopted and closed
without operator action. Worth keeping alongside the 2026-08-10 incident C and
the 2026-08-12 CW failure, both of which had true offsets **outside** the fence
and could not recover. The contrast is the point.

## 4. Direction segments

| time | direction | note |
|---|---|---|
| 12:49:12 | CW | `DECLARED` at mm 40 |
| 13:28:51 | CCW | |
| 13:29:27 | CW | |
| 13:29:50 | CCW | final leg — 678 markers, 171 distinct, 8 mismatches then clean |

The final CCW leg alone covered all 171 markers with peaks median 181, min 123,
and zero weak events.

## 5. What this does not establish

- **The CW NO_QUORUM near mm 120 from 2026-08-12 is not addressed here.** That
  was Otto, with a true offset outside the fence, and its cause remains unknown
  because the marker trail was lost. Toby's clean CW leg does not explain it.
- **The reversal off-by-one was not investigated.** It recovered, so it cost
  nothing this time, but three reversals in a minute reliably producing an
  off-by-one is a behaviour worth understanding before it happens somewhere the
  fence cannot express.
- Toby's value as a pre-1.13 control is spent, as recorded at flash time.

## References

- `docs/decisions/0025-the-phantom-was-a-maintenance-artefact-not-a-firmware-defect.md`
- `field-records/20260812_MAGNET_REPLACEMENT_PHANTOM_VERDICT.md` — Otto's verdict
- `field-records/20260812_TOBY_QUORUM_1_13_flash.md` — the flash
- `field-records/20260813_PI_POWER_SUPPLY_ROOT_CAUSE.md` — why this session could be recorded at all
