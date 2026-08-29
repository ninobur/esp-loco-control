# 2026-08-28 — session record

**Locomotive:** Toby (9950012). **Branches:** `agent/toby-1-13-flash` (firmware,
decisions, field records) and `claude/quorum-hall-waveform-diagnostic-plutez`
(NAVI, calibration tools, tables).

**Ends with Toby on `QUORUM_1_13D`**, battery disconnected, `nav` UNSET.

## What was established

**The Lowline is magnetically empty.** Three runs: 90 s stationary silent, 100 s
with the motor at PWM 90 silent, and a full circuit in which every one of 153
non-magnet excursions was a magnet's own rebound — 14–64 ms behind it, opposite
pole 149 of 150. Zero isolated disturbances. A hand-passed magnet confirmed the
instrument was awake, so the silences are measurements rather than a dead
capture path.
[Survey record](20260828_LOWLINE_HALL_ENVIRONMENT_SURVEY.md)

**False signals fall into three kinds, and only one threatens the odometer.**
Rebounds (~150/lap) never clear the 38-count entry threshold. Boot artefacts are
excluded by the `NO_POSITION` gate. Re-reads (~2/lap) do open events: 139–347 ms
after the magnet, 38–44 counts against 140+, opposite pole 13 of 13.
[Decision 0052](../docs/decisions/0052-a-taxonomy-of-false-signals-and-how-each-is-excluded.md)

**The conservation test was refusing real magnets.** 42 rejections, **29 of them
genuine** on full intervals of 1116–2410 ms, because `expected` derives from
`3.90 × pwm − 99.2` and PWM is a request. Four, ratios walking 1.23 → 1.30,
immediately preceded the 14:39 NO_QUORUM. That shutdown was the guard working
correctly on data corrupted upstream.

**Calibration is complete**: 171/171 markers in both directions, and the tables
must be per-direction because grades make duration direction-dependent — 26 of
26 consecutive markers slower CW than CCW across MM060–085.
[Decision 0048](../docs/decisions/0048-expectation-tables-are-per-direction-because-the-railway-has-grades.md)

**Waveform shape does not identify a magnet.** Tested four ways — whole curve,
residual, principal components, per-marker templates — all converging near 60%.
It *does* read magnet **type** at 99.4%, validated blind against the operator's
walking survey, 166 of 167. The miss was MM012, by nine ten-thousandths, and it
was found physically off-centre on concrete.
[Decision 0049](../docs/decisions/0049-waveform-shape-does-not-identify-a-magnet-on-this-railway.md) ·
[type map](20260828_MAGNET_TYPE_MAP_AND_SHAPE_VALIDATION.md)

**The map is deterministic, so the metric is detection, not discrimination.**
12.1 laps: 2064 clean advances, one miss, zero false advances.
[Decision 0050](../docs/decisions/0050-the-map-is-deterministic-so-the-metric-is-detection-not-discrimination.md)

**Physical faults found in data before they were found by hand:** MM128 loose,
displaced 66 mm and inverted — the displacement measured from timing alone and
matching the operator's "two sleepers". MM152 cleared as stable-weak, no
excavation. MM012 re-centred, so its calibration is now stale.

## Firmware

| build | what it is | state |
|---|---|---|
| `1.13W` | waveform capture, instrumentation only | superseded |
| `1.13X` | captures every excursion: admitted, floor-refused, sub-threshold | superseded |
| `1.13D` | 500 ms debounce, conservation conjunctive, **offsets removed** | **on Toby** |

Each was proven inert against the replay suite before any behavioural change was
made. 1.13D diverges deliberately — it declines to repeat the capture's mistake
at MM082, and with `QUORUM_OFFSETS = {0}` nothing can advance `navMm` by more
than one.

## NAVI — corrected

`NAVI_1_6` is on the other branch, never flashed. It is **not** the clean break
it was described as. It sits on top of QUORUM's navigator, still carries
`QUORUM_OFFSETS = {-3…+4}`, `adoptLeader` and the scoring machinery, and its
identity gate stands aside whenever `navState != NAV_NORMAL` — which is exactly
when position is in doubt. 424 references across seven mechanisms.

An earlier claim that the offsets and candidate list were "deleted rather than
fixed" was wrong.

## Open

- **The navigator is to be rebuilt from scratch**, to a contract still to be
  agreed item by item. One target per event, conjunctive identity, advance by
  exactly one or refuse and stop. Nothing downstream to correct bad data,
  because bad data is refused.
- **MM012's calibration is stale** — re-centred today, needs re-measuring.
- **IR wheel distance** remains the one measurement that would replace the PWM
  velocity model with a real one. TX 1.2 and the RX expectation have diverged.
- **The eight magnets that threw rebounds** — 004, 061, 063, 080, 081, 100, 109,
  144 — are mostly the strongest on the railway and are to be swapped for the
  smaller disk.

## Method notes worth keeping

**In-sample validation of a per-marker template is not evidence.** A shape band
read 0.0% false refusals in-sample and 7.6% under leave-one-out. Every figure
quoted since is leave-one-out.

**A negative result needs a positive control.** Two silent survey runs meant
nothing until a hand-passed magnet proved the capture path was alive.

**Check provenance, not just consistency.** A stretch with a magnet skipped is
internally consistent — steps of one, polarity matching — so it survives the
ten-magnet proof while sampling the wrong marker's field.

**An instrument that saturates must say so.** 14 of the first 204 captures came
back with flat tops and no indication; it took decoding them to notice.
