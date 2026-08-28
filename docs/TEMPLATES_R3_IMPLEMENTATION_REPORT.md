# TEMPLATES 0.3 — R3 target acquisition, implementation report

**Date:** 2026-08-27
**Status:** Built and compile-verified on both locomotive profiles.
**NOT done, deliberately:** flashing, field operation, IR control authority —
separate operator decisions per the spec header.
**Spec:** `docs/TEMPLATES_REVISION_3_TARGET_ACQUISITION.md` · decision 0044
**Sketch:** `firmware/test-programs/TEMPLATES/TEMPLATES.ino`, `SKETCH_NAME`
now `TEMPLATES_0_3` (first commit of the sketch source to the repo).

## What was built

The design hierarchy as adopted (operator + CODEX, 2026-08-27): dead
reckoning predicts, the map supplies the expected magnet's attributes, Hall
and IR observations are compared with that expected landmark, QUORUM checks
sequence, agreement confirms, mismatch triggers bounded consideration of
missed magnets, no candidate → reject with only a diagnostic record, and
later evidence may correct position but never converts a rejected signal
into a magnet. *Individual attributes are evidence about an expected
identity, not universal admission requirements.*

### 1. The gate splits into a magnet test and an identity test

- **Magnet test (hallTask, unchanged in structure).** The universal
  140-count floor is retired as the admission decision.
  `R3_MAGNET_MIN_PEAK 60` + the existing speed-scaled duration floor now
  form a permissive spike/noise filter. Failing it is `SIGNAL_REJECTED`:
  terminal, never queued, audit afterlife on `diag/admit_reject` exactly as
  0.2 — no payload change to that topic.
- **Identity test (loop thread, new — `r3Gate()`),** between the event
  queue and `navOnMarker()`, where `navMm`/`navDir`/gain/history natively
  live. Scores the passage against a bounded, forward-only candidate set:
  the expected next marker plus up to `R3_MISS_SEARCH + unresolved-streak`
  markers ahead (hard cap 6 candidates).

### 2. The attribute set, per spec §4

| Attribute | Weight | Source | Absent when |
|---|---|---|---|
| Polarity | 32 | `dnaAt(cand)` | never |
| Strength | 18 | session gain (trailing median of accepted peaks, same gain §3Q uses) × `strengthAt(cand)` | gain history < 8 events |
| Duration | 18 | PWM-normalized vs new `durationAt(cand)` table | pwm = 0 at detect |
| Timing | 16 | elapsed since last accepted vs map span / measured velocity | mid-ramp, low PWM, no velocity, or dwell detected |
| Sequence | 8 | does the held-passage run exactly fill the candidate's gap | never |
| IR | 8 | pulses on the unpowered wheel since last accepted, as mm vs map span | link stale > 1.5 s, no snapshot, counter reset |

Confidence = Σ(weight × score) / Σ(weight of **available** attributes) —
absence shrinks the denominator and never counts against a candidate
(spec §10). A dwell makes *timing* absent rather than contradicting: elapsed
time then measures waiting, not travel. IR is immune to dwells by
construction — a stopped wheel accrues no pulses — which is exactly the
independent-witness value that justified carrying it.

### 3. Three outcomes (spec §9)

- **`TARGET_CONFIRMED`** — expected candidate ≥ `R3_CONFIRM_PCT` (50).
  Handed to the navigator, which advances and runs its own AGREE/DISAGREE
  unchanged.
- **`R3_CORRECTED`** — a farther candidate ≥ `R3_CORRECT_PCT` (67) *and*
  beating the expected candidate: the missed markers are adopted
  (forward-only), then the event proceeds to the navigator normally.
  Decision order puts correction before confirmation so a
  strong-but-unexpected read is examined, not rubber-stamped.
- **`MAGNET_UNRESOLVED`** — held. Never reaches the navigator, never
  advances position, never reconsidered. Its **count** seeds the sequence
  attribute so a later confirmed passage can close the gap. Not a
  purgatory: nothing held is ever resurrected, and `SIGNAL_REJECTED`
  remains terminal exactly as 0.2.

### 4. When R3 stands aside

Bypass (event passes straight through, counted): any state but
`NAV_NORMAL`, unknown direction, not yet synced, adoption validation
pending, or §3Q arbitration pending. QUORUM's machinery owns those regimes
(spec §3.4); R3 must not withhold its evidence. Sync is self-healing —
any relocation R3 didn't make (adoption, self-resolution, declaration,
quarantine commit) is detected by `navMm` disagreeing with R3's shadow and
resolves by resync, never by trusting a stale velocity.

### 5. `durationAt()` — new per-marker table

`durationMs90[171]` PROGMEM (uint16 ms at the PWM-90 reference), built from
**2,210** confirmed crossings across today's three sessions (121418
CW-partial, 123552 CCW, cw_full CW): all 171 markers covered, ≥ 6 samples
each, median per-marker CV **0.049**. Construction mirrors `strengthPct[]`
(ms × pwm / 90, per-marker median). Ships **static** — resolving spec §12's
open question for the first build the same way `strengthPct[]` is shipped;
calibration establishes session *gain* and never rewrites tables (§7).
Builder script: `tools/navlab` is not involved — the derivation is
`ms*pwm/90` medians directly from `mm/marker` records.

### 6. Telemetry — `diag/r3_admit` (new topic, spec §10)

Every identity-test decision publishes: outcome, position before/after,
direction, expected/decided/best candidate, correction offset, observed and
expected polarity/peak/duration (raw + normalized)/arrival time, map span,
IR distance or `null`, all six per-attribute scores (−1 = unavailable), the
available-evidence denominator, both confidences, both thresholds in force,
the unresolved streak, and cumulative outcome counters. Worst-case payload
511 bytes against the 512-byte PubMsg bound (realistic ~460); snprintf is
truncation-safe. No existing topic's payload changed.

## Verification performed

1. **Compile:** clean on Toby's profile (76% flash) and Otto's (77%;
   exercises the `IR_TEST_A_ON 0` stub path). `LocoConfig.h` restored to
   Toby after the check.
2. **Arithmetic replay** of the exact scoring steps against today's real
   field events:
   - All 10 sampled weak-but-real amplitude rejects (MM140–142, PWM 84/90/
     118) score **87–96%** against their true markers → `TARGET_CONFIRMED`
     directly, at the marker where they physically happened. The offset lag
     never forms.
   - Both genuine noise events score **42%** even granting a lucky polarity
     match — below the 50% bar — *and* are already `SIGNAL_REJECTED` by the
     magnet test (peak < 60, duration ratios 0.09–0.14).
   - The offset-lag correction case (MM143 arriving with navMm held at 139,
     3 unresolved): candidate MM143 scores **97%** vs the expected
     candidate's **57%** → correction fires at the 67% bar.

## Honest limits

- Every threshold, weight, and tolerance is an **experimental test
  setting** — flat `#define`s, changeable without restructuring, to be set
  by single-train loop results (spec §6). None is derived.
- The arithmetic replay above is supporting evidence, not validation: it
  reuses the events the tables were tuned around and does not exercise the
  live gate, sync, velocity, or IR paths. Live testing follows spec §11's
  seven-step progression.
- Precision remains unmeasured (spec §8). The lowered magnet-test floor
  admits the 60–139 valley to the identity test; the identity test refusing
  today's noise at 42% is one data point, not a precision claim. Step 6 of
  the live progression (Otto's contaminated conditions) is the test.
- `durationAt()` is one locomotive, one day, self-derived. Unvalidated
  cross-locomotive.
- Velocity for the timing attribute is a single-interval estimate with a
  sanity band (50–600 mm/s), invalidated on any relocation or dwell — the
  weakest attribute by construction, weighted accordingly.
- Held (`MAGNET_UNRESOLVED`) events do not update the navigator's
  `lastMarkerMs` timing chain — the next accepted event's dt spans the gap,
  which is what makes the correction candidates' timing come out right, but
  it is a behavioral difference from 0.2's "dt advances on every received
  event" and should be watched in the field records.

## First commit of the sketch source

`firmware/test-programs/TEMPLATES/` was previously untracked. It enters the
repo with this change (TEMPLATES.ino, IRSpeedWire.h, LocoConfig.h, both
locomotive profiles). `credentials.h` and `build/` are git-ignored; the
profiles reference credentials by include, holding no plaintext secrets.
