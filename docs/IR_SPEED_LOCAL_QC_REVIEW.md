# IR_SPEED_LOCAL 1.0 — QC review

**Date:** August 10, 2026
**Reviewed:** `f034ecb` on `agent/ir-speed-local` —
`firmware/test-programs/IR_SPEED_LOCAL/` (sketch 447 lines, README, decision 0021)
**Reviewer:** Claude
**Verdict:** One blocking defect before the daylight field gate. Everything
else is minor or governance. The architecture is sound and the discipline is
visible throughout.

Reviewed from the commit blob without checking the branch out, so the working
tree stayed on `main`.

## Builds

All three switch combinations compile clean against `esp32:esp32:esp32`,
core 3.3.11, `--warnings all`:

| Variant | Flash | RAM | Warnings |
|---|---:|---:|---|
| `IR_LOCAL_WIFI` (default) | 907,388 B (69%) | 49,712 (15%) | none |
| WiFi commented (USB only) | 280,716 B (21%) | 24,852 (7%) | none |
| WiFi + `IR_LOCAL_DEBUG` | 907,496 B (69%) | 49,712 (15%) | none |

**Zero warnings in all three.** IR_DIAG and IR_TEST both still carry
pre-existing `-Wvolatile` warnings; this sketch introduces none.

Built against a placeholder `credentials.h` in a scratchpad copy — no secrets
file was created in the repo. `.gitignore` correctly covers
`firmware/config/credentials.h` via both `credentials.h` and
`firmware/*/credentials.h`.

---

## P1 — BLOCKING: ADC noise passes the contrast gate and publishes `speed_valid:1` on a stationary car

### The defect

The only contrast gate is line 231:

```c
const bool contrastGood = envelopePrimed && span >= MIN_USABLE_SPAN;   // 120
```

There is no `MARGINAL` concept, no debounce, and no plausibility bound on the
interval — confirmed absent by search, and stated as deliberate in the README
("There is deliberately no inferred missed-spoke counter, debounce guard...").

The 2026-08-06 stationary observation on record had `base=79 span=159`. **159
is greater than 120.** So that trace passes `contrastGood`, the 2/3 and 1/3
thresholds are computed from the noise band itself, and ADC noise crossing them
produces completed pulses with short intervals. Five intervals fill the ring,
validity becomes `IR_VALID`, and the 1 Hz publisher emits `speed_valid:1` with
a real-looking `speed_mmps`.

### Replay evidence

The detector (lines 172–317) was transcribed to a host harness and replayed
against three cases:

| Case | Envelope | Result |
|---|---|---|
| Stationary, ADC noise in [0,170] | span **175** — passes the 120 gate | **VALID after 791 ms; 1167 pulses in 8 s; median 1930 mm/s, max 4826 mm/s; VALID for 90% of the run — car stationary** |
| Stationary, flat parked trace | span 31 — fails the gate | Correctly suppressed throughout |
| Moving wheel, real spokes at 63 ms | span 2815 | VALID, 153 mm/s — arithmetically correct for 9.652 mm/pulse |

The two control cases behave correctly, which isolates the failure to the
noise-band case specifically. The flat-trace case is the one the contrast floor
was designed for, and it works; the noise-band case is the gap.

### Why this is blocking

This is the exact defect recorded as a production requirement in
`docs/IR_DEV_REQ/QUALITY_GATES_SPEED_OUTPUT.md` (commit `c97cad8`), written
from the observed 2026-08-06 stationary window. That requirement states that
`MARGINAL` and `UNAVAILABLE` must **suppress** the speed output rather than
label it. IR_SPEED_LOCAL has no `MARGINAL` state at all, so the band
`120 <= span < 300` — precisely where the documented defect lives — is
unguarded, and the output is `speed_valid:1` rather than a qualifier a consumer
might ignore. The failure is therefore worse than the original: there is no
qualifier to ignore.

It also lands in the one place the sketch is explicitly designed to protect.
Decision 0021 makes `LocalSpeedSnapshot` the governor's seam, and the README
says "Only `VALID` carries a consumable speed." A governor honouring that
contract exactly as written consumes ~1930 mm/s from a parked car.

### On decision 0020

0020 requires evidence before adding a guard, and the author has clearly held
that line — correctly, in most places. This guard clears the bar: the evidence
is the observed 2026-08-06 window (`base=79 span=159`, every pulse `MARGINAL`,
`~624 mm/s` reported stationary, `int min=15` equal to the debounce floor),
plus the replay above. This is not a speculative guard.

### Suggested fix

Cheapest change that satisfies the committed requirement: gate `IR_VALID` on a
marginal-span threshold as well as the usable floor, keeping detection running.

```c
static constexpr int MARGINAL_SPAN = 300;   // matches IR_TEST / IR_DIAG
...
const bool contrastMarginal = span < MARGINAL_SPAN;
const Validity validity = (intervalCount >= SPEED_WINDOW_N && !contrastMarginal)
                        ? IR_VALID : IR_REACQUIRING;
```

Keep `MIN_USABLE_SPAN` where it is so the detector and envelope keep adapting
(decision 0006) — suppress the *answer*, not the *measurement*. A distinct
`IR_MARGINAL` validity name would be more honest than reusing `IR_REACQUIRING`,
since the two conditions differ, and would make the status beat diagnosable.

A physical plausibility bound (an interval implying a speed far beyond anything
the railway does) would catch the same case from the other direction and is
also evidence-backed. It is not a comparison against prior intervals, so it does
not conflict with decision 0004. The span gate is the more direct match to the
committed requirement; either or both.

---

## P2 — `telem/pulse` reuses an existing topic with an incompatible payload

`topicSpeed` is `ngr/spoke/IR_SPEED_SENSOR/telem/pulse` (line 421), with the
comment "Existing IR_TEST JSON topic, now deliberately latest-state at 1 Hz."

The payload shape changes completely. IR_TEST's schema
(`IR_TEST_STATE_AND_REACQUISITION.md` §4) carries `interval_ms`, `width_ms`,
`state`, `raw`, `quality`, `accepted`, `rssi`, `ev_drops`, `rej`. The new
payload carries `seq`, `t_ms`, `speed_valid`, `speed_mmps`, `span`, plus
`state` only when invalid. Both the fields and the cadence differ — per
physical pulse versus 1 Hz latest-state.

The README acknowledges the name is historical, but any Pi-side parser,
replay tool, or archived-schema consumer pointed at that topic now receives a
different contract under the same name, and nothing in the payload announces
the change. If IR_TEST and IR_SPEED_LOCAL are ever run against the same broker
during commissioning, one topic carries two incompatible schemas.

Suggest `telem/speed` for the new semantics, or a `"schema":2` field so a
parser can dispatch. Publishes are non-retained, so no retained ghost is
created — good.

## P2 — decision 0008 is still Accepted and now contradicts the hardware

0008, "The 7-spoke finescale steel wheel is the production speed target," is
still `Accepted` with no superseding record. It explicitly records that the
LGB-style wheel "kept a phase-locked blind arc through a repaint" and lists
"bare 10-spoke LGB → this" as the history that led away from it.

The sketch, the README, and the firmware catalog all now target the 10-spoke
plastic wheel at 96.52 mm. The 2026-08-09 record documents that hardware change
and flags the stale 7-spoke constants as a "major configuration defect," so the
sketch's constants are right for what is installed — but the decision record
that names the production target was not updated.

Standing practice is that a decision is never edited away and a change gets a
new record marking the old one superseded. Either 0008 needs superseding on the
new evidence, or the sketch should state that it targets the survey wheel and
not the production target. This is a records question, not a code defect — I
cannot adjudicate the field evidence, only note that the record and the
hardware now disagree.

## P2 — `MM_PER_PULSE` is a hand-computed duplicate

```c
static constexpr float WHEEL_CIRCUMFERENCE_MM = 96.52f;
static constexpr float MM_PER_PULSE           = 9.652f;
```

The value is correct today. But it is a derived quantity stated independently
of its inputs, so changing `SPOKES_PER_REV` or the circumference leaves it
silently wrong — and this project has changed spoke counts five times
(10 → 5 → 2 → 7 → 10). The 2026-08-09 record calls exactly this class of stale
constant a major configuration defect.

```c
static constexpr float MM_PER_PULSE = WHEEL_CIRCUMFERENCE_MM / SPOKES_PER_REV;
```

The calibration arithmetic itself checks out: (38 + 37⅞ + 38⅛) / 3 = 38.0 in
per 10 revolutions = 3.8 in = 96.52 mm; 96.52 / 10 = 9.652.

---

## P3 — minor

**`health` is read under the mutex but written without it.** `networkTask`
copies `s = latest` and `h = health` inside one `portENTER_CRITICAL`
(lines 371–374), but `sensorTask` only takes the mux for `latest` (via
`publishLocalSnapshot`); every `health.*++` is unguarded. So `latest` is
coherent and `health` is not, under a critical section that looks like it
covers both. Impact is low — diagnostic counters, off by at most an increment —
but the mixed guarantee is a trap for the next reader.

**`STALE` recovery does not clear `previousRiseMs`.** The contrast-loss branch
(line 239) and the open-abort branch (line 255) both clear it; the stale branch
(304–312) does not. The first pulse after a silence therefore produces an
interval spanning the outage, which enters the ring. In practice it is
contained — validity is `REACQUIRING` until five intervals exist, and by then
the outage value is one of five and the median ignores it — but it is
inconsistent with the rule established in IR_DIAG commit `d3b6324`, that an
interval spanning an outage must not be dressed as a measurement.

**Contrast loss clears `awaitLowAfterAbort`.** Line 237 clears the flag whose
comment says it exists so one plateau cannot "manufacture repeated rises." A
signal stuck above `thrHigh` that spans a contrast-loss episode can register a
fresh rise immediately on recovery. Narrow, but it defeats the stated intent in
that one path.

**During `STALE`, both `seq` and `t_ms` freeze.** The stale branch publishes
once (guarded by `latest.validity != IR_STALE`), so the 1 Hz publisher resends
byte-identical payloads. A consumer cannot distinguish "sensor reports stale"
from "publisher wedged" on that topic alone. The 5 s status beat and the
retained LWT cover liveness, so this is an observation rather than a defect.

---

## What is right

Worth stating plainly, because most of this sketch is careful work:

- **Invalid never means stopped.** `speed_mmps` is `null`, never `0.00` — the
  second instance of the historical failure shape is properly closed here.
- **The envelope updates unconditionally before the contrast branch**
  (line 229), so adaptation continues through blindness. This sidesteps the
  decision 0006 reacquisition deadlock by construction rather than by a guard.
- **The rolling-window envelope declares `INVALID_CONTRAST` when parked**
  rather than holding a stale band — the honest behaviour, and the one that
  caused the 2026-08-05 incident when it was absent.
- **Decision 0004 is respected**: every nonzero interval is admitted, and
  robustness lives in the output median. The outage interval above is absorbed
  exactly as that decision intends.
- **The open-pulse abort rearm logic is sound** in the normal path — requiring
  a physical fall below `thrLow` before a new rise is a real fix for the
  105-second latch, not a timeout band-aid.
- **Sampling is honest about its own gaps**: real slots counted, no fabricated
  catch-up samples, max gap reported.
- **The catalog entry** in `firmware/README.md` is present and correctly
  separates role from evidence status, per the new librarian rule.
- **Decision 0021 and the README are unusually good** — the field gate has
  falsifiable pass criteria, including the one that matters most, that received
  message count is never used as the physical pulse count.

## Recommendation

Fix P1 before the daylight field gate. The gate's pass criterion 3 — "speed
remains VALID during uninterrupted motion and becomes non-valid rather than
publishing stale or zero speed during edge silence" — does not currently test
the stationary-noise case, and would pass while the defect is present. Consider
adding a criterion: with the car stationary and the sensor blinded or
decoupled, no `speed_valid:1` is published for the duration.

P2 items are cheap and worth doing in the same pass. P3 items can ride along or
wait.

## Verification limits

The three builds, the constants, the absent debounce/marginal logic, the
gitignore coverage, decision 0008's status, and the catalog entry were each
checked directly and are stated as verified. The replay used a transcription of
the detector, not the compiled firmware; it reproduces the documented
observation and the two control cases behave correctly, but a transcription can
always diverge from the original in a way the controls do not expose. The
noise model is uniform random in a band, chosen to match the observed
`base=79 span=159` — real ADC noise is correlated, which changes the interval
distribution but not the conclusion that the band passes the gate.

No hardware was flashed and the working tree was not moved off `main`.
