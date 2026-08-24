# QUORUM navigation audit, and an offline target-acquisition gate prototype

**Status: investigatory. Nothing here is approved for implementation. No
firmware was changed or flashed; no locomotive was operated.** This
document records: (A) a traced audit of QUORUM's current event-acceptance
path, with exact code references; (B) a proposed acquisition-event
contract; (C-D) a host-replay prototype and gate evaluation built to test
the operator's intended decision model against real captures; (E) an
explicit accounting of what would and would not constitute evidence of
accurate detection; (F) synthetic test coverage.

Companion review documents (read before this one): `TEMPLATES/REVIEW of
HALL SENSOR LOGIC/REVIEW_NOTES.md`, `TEMPLATES/REVIEW of HALL SENSOR
LOGIC/TARGET_ACQUISITION_GUIDANCE.md`, `TEMPLATES/REVIEW of HALL SENSOR
LOGIC/HALL_PROCESSING_PREBUILD_REVIEW_PLAN.md`.

---

## A. Current QUORUM behavior, traced

One Hall event's path, `firmware/QUORUM/QUORUM.ino`, checked out at this
session's HEAD:

1. **Detector** (`detectorSample()`, line 667) opens an event when the
   averaged ADC crosses `northEnter`/`southEnter`, closes it after 20 ms
   back inside the exit band (`EVENT_EXIT_HOLD_MS`), and drops it silently
   if the whole thing was under 40 ms (`EVENT_FLOOR_MS`, line 698:
   `if(dur<EVENT_FLOOR_MS){ floorRejects++; return; }`) — the *only*
   morphology test anywhere in this pipeline. A `MarkerEvent` is queued
   (line 699-707).
2. **Handoff** (`drainMarkers()`, line 3006): dequeues the event, calls
   `navOnMarker(e)` (line 3016), then unconditionally publishes the marker
   telemetry line — using `navMm` as it stands *after* `navOnMarker()`
   returns. This publish happens for every dequeued event regardless of
   what `navOnMarker()` did with it (the "received, published exactly
   once" contract, documented at lines 1384-1389).
3. **Timing gate** (`navOnMarker()`, line 1397), gates in this fixed order:
   `NO_POSITION` (`navState==NAV_UNSET`, line 1412) → `NO_DIR`
   (`navDir==MAP_UNSET`, line 1419) → `LOW_PWM` (`pwmActualAtDetect < 40`,
   line 1429) → `RAMP` (`|actual-commanded| > 10`, line 1437) → `NO_PREV`
   (no established predecessor interval, line 1446) → `ACTIVE` (the
   conservation test, line 1458).

**The decisive fact:** of those six gate outcomes, only three —
`NO_POSITION`, `NO_DIR`, and `ACTIVE`-with-conservation (`PHANTOM_REJECTED`,
line 1489) — can prevent `acceptEvent()` from running. `LOW_PWM`, `RAMP`,
and `NO_PREV` all call `acceptEvent(e)` unconditionally (lines 1434, 1443,
1454). None of the six gates ever inspects the event's polarity, duration
(beyond the detector's own 40 ms floor), peak, or shape.

4. **`acceptEvent()`** (line 1292):
   ```
   1292  static void acceptEvent(const MarkerEvent& e){
   1293    navMm=nextMm(navMm,navDir);                    // ADVANCE — unconditional
   1294    if(markersSinceConfirmed<65535) markersSinceConfirmed++;
   1295    pushRing(e.polarity,navMm);                     // RING — unconditional, post-advance mm
   1296    switch(navState){
   1297      case NAV_NORMAL:
   1298        processNormalComparison(e);                 // polarity checked HERE, AFTER 1293/1295
   ```
   `navMm` advances and the event is pushed into the evidence ring
   (`pushRing`, line 1295, tagged with the *already-advanced* `navMm`)
   **before** polarity is compared to anything. `processNormalComparison()`
   (line 1263) then reads `dnaAt(navMm)` — the position `acceptEvent()`
   already moved to — and on a mismatch does `navDisagree++`,
   `navPublishState("DISAGREE",&e)`, `missStreak++` (lines 1275-1277) and
   **nothing else**: `navMm` is not reverted, the ring entry already pushed
   is not removed. This is the exact mechanism behind the observation this
   task opened with: *a wrong-polarity event increments the magnet count
   even though QUORUM reports DISAGREE.*

5. Under `NAV_EVALUATING`, `acceptEvent()`'s switch calls
   `scoreNewestRingEntry()` (line 1301) — scoring the ring entry that was
   *already pushed* against every non-excluded `QUORUM_OFFSETS` hypothesis
   (`scoreEntry()`, line 996). This is deliberate multi-hypothesis design,
   not a bug — but it means an event that disagreed with the *assumed*
   current position still has full standing to support a *different*
   hypothesis.

### Audit table

| Path (in `navOnMarker`/`acceptEvent`) | `navMm` advances? | Ring receives it? | Scored (during EVALUATING)? | Polarity checked before/after advancement? | Rejection final? | Can it later affect navigation? |
|---|---|---|---|---|---|---|
| `NO_POSITION` (1412) | No | No | No | never compared | — (gated before timing) | No |
| `NO_DIR` (1419) | No | No | No | never compared | — (gated before timing); also invalidates `previousAcceptedDt` | No |
| `LOW_PWM` (1429) | **Yes, unconditional** | **Yes, unconditional** | Yes, if evaluating | **AFTER** (inside `acceptEvent`, called at 1434) | — (bypasses conservation) | **Yes — full standing** |
| `RAMP` (1437) | **Yes, unconditional** | **Yes, unconditional** | Yes, if evaluating | **AFTER** (called at 1443) | — (bypasses conservation) | **Yes — full standing** |
| `NO_PREV` (1446) | **Yes, unconditional** | **Yes, unconditional** | Yes, if evaluating | **AFTER** (called at 1454) | — (bootstraps, doesn't test) | **Yes — full standing** |
| `ACTIVE`, conserved → `PHANTOM_REJECTED` (1477-1491) | No | No | No | never compared (timing-only) | **Yes, final** — `acceptEvent()` never called, and this event does NOT become the new `previousAcceptedDt` | No |
| `ACTIVE`, not conserved (1495-1496) | **Yes** | **Yes** | Yes, if evaluating | **AFTER** (called at 1496) | — (passed) | **Yes — full standing** |

**Existing tests:** `firmware/QUORUM/tests/fixtures/*.replay` and
`synthetic_manifest.json` describe exactly this behavior in prose (e.g.
`syn_adv_double_phantom`, `syn_pair_strong_then_weak` — event pairs the
conservation test is meant to catch) and assert expected outcomes
(`must_contain`/`must_not_contain`/`final_mm`/`final_disagree`), but **no
runner for these fixtures exists anywhere in this repository** (checked:
no `.py`/`.sh`/`.cpp` file in the repo reads `.replay` or
`synthetic_manifest.json`; `QUORUM.ino` has no host-testable build mode —
no `main()`, no `#ifdef`-gated test path). The fixtures corroborate the
audit's reading of the code's *intent* but could not be executed to
independently confirm it in this session.

---

## B. Acquisition-event contract (proposed, not implemented)

`tools/hwt_gate_replay.py`'s `EVENT_COLUMNS` (see that file) implements
this, grouped by prefix so physical measurement, detector interpretation,
map expectation, and disposition never mix in one field:

- `phys_*` — open/close sample+time, duration, positive/negative peak,
  signed and absolute integrated flux, baseline value and quality
  (`pre_range_counts`, `pre_stdev_counts`, from the prior baseline-merging
  investigation), completeness/gap flags, PWM (actual+commanded) and
  direction **at opening** (mirroring QUORUM's own §3 "sampled at event
  open" discipline).
- `det_*` — opening polarity, and a continuity/smoothness measure (see §D).
- `ctx_*` — time since the previous *accepted* marker, the minimum
  physically-possible time to the next one, and what that previous marker
  was — replay state, not a property of the event alone.
- `map_*` — the expected next marker and its expected polarity, populated
  only once a position exists.
- `disp_*` — final disposition (one of the eight below), a plain-language
  reason, and predicted position before/after.

Dispositions actually used by this prototype's gate pipeline:
`ACCEPT_EXPECTED_MARKER`, `REJECT_SPIKE`, `REJECT_INCOMPLETE`,
`REJECT_PHYSICALLY_TOO_SOON`, `REJECT_PROBABLE_RETURN`,
`REJECT_WRONG_EXPECTED_POLARITY`. `REJECT_UNSTABLE_BASELINE` and
`REVIEW_AMBIGUOUS` are in the enum but **not wired into an active rule** —
see §D for why.

---

## C-D. Host-replay prototype and gate evaluation

`tools/hwt_gate_replay.py`. Candidate events come from
`tools/hwt_excursions.py`'s frozen-baseline detector (the corrected
measurement layer from the prior investigation) — this tool does not
re-detect excursions. The track map and QUORUM's own navigation constants
are read directly out of `firmware/QUORUM/QUORUM.ino` by
`tools/quorum_map.py` (regex extraction of `NGR_DNA1`, `spacingMm`, and the
named `#define`/`static const` constants) — never hand-copied, and it
raises rather than silently drifting if a future edit renames or reshapes
what it looks for.

Gate order (fixed, logged on every event):

1. **Completeness** — `phys_incomplete` → `REJECT_INCOMPLETE`.
2. **Morphology** — duration, absolute integrated flux, and
   `continuity_ratio` (the fraction of sample-to-sample sign changes in the
   baseline-relative deviation trace, with a noise floor) must all clear
   their bars, or `REJECT_SPIKE`. See the calibration note in
   `continuity_ratio()`'s docstring: a naive zero-noise-floor version of
   this measure was dominated by ordinary ADC jitter and could not tell a
   clean 333 ms single-hump response from a known 5.5 s merged excursion
   (both scored ~0.68); requiring the sample-to-sample delta to exceed 20
   counts before counting as a "direction" separated them cleanly (0.000
   vs 0.681) on that one pair. This is a documented empirical finding, not
   a validated general threshold.
3. **Physical timing** — elapsed time since the last accepted marker must
   be at least `spacing / max_credible_speed`, where the spacing comes from
   the extracted map and the speed bound is QUORUM's own
   `VEL_MODEL_SLOPE`/`VEL_MODEL_INTERCEPT` evaluated at PWM 255 — an
   **inherited, explicitly-provisional** figure (QUORUM's own comment: *"PWM
   is a request, not a result"*), not independently validated against
   wheel-speed ground truth, because none of these captures have any. No
   dead-time is guessed. Failing this alone → `REJECT_PHYSICALLY_TOO_SOON`.
4. **Return-response conjunction** — physically-too-soon **AND** opposite
   polarity to the previous accepted marker → `REJECT_PROBABLE_RETURN`
   (more specific than a bare timing failure).
5. **Expected-map polarity** — a plausible, timely curve whose polarity
   doesn't match the expected next marker → `REJECT_WRONG_EXPECTED_POLARITY`
   ("No Way"); matching → `ACCEPT_EXPECTED_MARKER`, predicted position
   advances by exactly one marker.
6. **Repeated disagreement** — a streak of `REJECT_WRONG_EXPECTED_POLARITY`
   reaching `QUORUM_TRIGGER` (3, extracted from QUORUM.ino) is *reported*
   ("this is where the operator's model would request a declaration") with
   **no new recovery policy implemented**.

`REJECT_UNSTABLE_BASELINE` is deliberately unused: the prior independent-
comparison investigation found `pre_range_counts` does **not** cleanly
separate trustworthy from untrustworthy excursions in real data (merged
long-duration excursions' pre-windows were statistically indistinguishable
from ordinary ones) — wiring up a cutoff here would be exactly the
unjustified rule the task warns against. `REVIEW_AMBIGUOUS` is reserved for
"no accepted predecessor and no manifest-declared start"; it did not fire
on any of the three real captures, each of which has a manifest-declared
(if not always operator-*confirmed*) start.

### Run manifests

`tools/manifests/{grillers,pwm40_run,pwm90}.json`. Each states direction,
declared start (mm, time, and a `confirmed` boolean), the assumed
polarity-orientation convention (also `confirmed`), known
stalls/stops/anchors, and free-text uncertainty notes. None of the three
captures has a fully operator-confirmed starting marker:

- **grillers**: anchor `START-066-067-CCW` at t=1176.689714s confirms
  interval and direction, but not which of 66/67 is "start" — `mm=66` is
  this manifest's stated assumption.
- **pwm40_run**: **zero anchors** in this capture. `mm=40`/`t=0.0` come
  only from the filename, by analogy with grillers — explicitly flagged as
  not evidence for this specific file.
- **pwm90**: the operator's own anchor text is literally
  `START-###-###-CCW-PWM90` — an acknowledged, unfilled placeholder.
  `mm=40` is inferred weakly from an earlier, un-keyworded `040-041` anchor.

### Corrected statistics (all three captures, default gate parameters)

| capture | candidate events | ACCEPT | REJECT_SPIKE | REJECT_INCOMPLETE | REJECT_TOO_SOON | REJECT_PROBABLE_RETURN | REJECT_WRONG_POLARITY | predicted position |
|---|---|---|---|---|---|---|---|---|
| grillers | 201 | 1 | 142 | 37 | 1 | 7 | 13 | mm 66 → 65 |
| pwm40_run | 1286 | 35 | 1090 | 90 | 5 | 1 | 65 | mm 40 → 5 |
| pwm90 | 2465 | 22 | 2410 | 16 | 0 | 0 | 17 | mm 40 → 18 |

Every gate fired on real data at least once except `REJECT_UNSTABLE_BASELINE`
(unused by design) and `REVIEW_AMBIGUOUS` (no capture lacked a declared
start). Repeated-disagreement streaks (≥3 `REJECT_WRONG_EXPECTED_POLARITY`
in a row) were reported 6 times on pwm40_run, 3 times on pwm90, once on
grillers.

---

## E. Avoiding circular validation

**None of the numbers above are a claim of accurate magnet detection.** The
detector's own accepted-event count validates nothing by itself. What each
category above actually supports:

- **Obvious-spike rejections** (`REJECT_SPIKE`, the large majority in every
  capture) are supported by morphology alone — duration/flux/continuity —
  independent of any map comparison.
- **Physically-impossible rejections** (`REJECT_PHYSICALLY_TOO_SOON`,
  `REJECT_PROBABLE_RETURN`) are supported by the map's spacing and
  QUORUM's own (inherited, provisional) speed model — not by polarity
  matching, so they are not circular with the polarity gate.
- **Plausible curves matching expected polarity** (`ACCEPT_EXPECTED_MARKER`)
  are consistent with the map, which is evidence they *could* be genuine —
  it is not proof, because a spurious curve with matching polarity by
  chance would look identical here. A visually clean single-hump example
  (PWM-40 event #544, 731 ms, matches `pwm40/plots/ACCEPT_EXPECTED_MARKER/`)
  supports this by shape, independent of the map match.
- **Plausible curves contradicting expected polarity**
  (`REJECT_WRONG_EXPECTED_POLARITY`) are exactly where map-internal
  consistency and physical reality could diverge — these are the events
  that would need an operator anchor to adjudicate, not more map
  consistency.
- **Where predicted position gains or loses agreement with operator
  anchors**: not established this session for any of the three captures.
  All three manifests' starting positions are unconfirmed or only
  partially confirmed (see above), so predicted-vs-anchor agreement at the
  *end* of a run cannot be checked without first resolving the start.
- **Cannot be adjudicated without new anchors**: the absolute starting
  marker for all three captures; whether the assumed polarity-orientation
  convention (`positive deviation = N`) is correct for this sensor
  (untested here — CLAUDE.md confirms `HALL_POLARITY_INVERTED` is dead
  config QUORUM never reads, but that does not establish this tool's own
  sign convention); and whether QUORUM's `MAP_CW`/`MAP_CCW` enum matches
  the physical sense operators mean by "CW"/"CCW" in anchor text.

---

## F. Test results

New: `firmware/test-programs/HALL_WAVEFORM_TEST/tests/test_gate_replay.py`,
12 tests / 52 checks, all passing, against a small synthetic 6-marker map
(not the real firmware map, so these do not depend on QUORUM.ino's current
contents). Full suite after this change:

```
capture engine (g++)          691 checks, 0 failures
direction gate (g++)           44 checks, 0 failures
decoder / receiver (python3)   60 checks, 0 failures
excursion analysis (python3)  101 checks, 0 failures
gate-replay prototype (python3) 52 checks, 0 failures
control-authority audit (py)   66 checks, 0 failures
---------------------------------------------------
TOTAL                        1014 checks, 0 failures
```

---

## Limitations, honestly

- This prototype does not replay QUORUM's own raw ADC pipeline (8-sample
  averaging, its own median baseline) — it reuses `hwt_excursions.py`'s
  already-validated frozen-baseline measurement instead, because no wired
  ground truth exists in this repo to reconcile the two numerically. The
  map and navigation *constants* are QUORUM's real ones; the *signal
  processing* is not.
- The physical-timing gate's speed bound is inherited from QUORUM's own
  acknowledged-provisional model, not independently measured.
- Representative plots (not exhaustive coverage) were generated per
  disposition — this task asked for representative examples, unlike the
  prior task's explicit no-cap requirement for specific merge categories.
- `continuity_ratio`'s noise-floor calibration rests on one example pair,
  not a systematic sweep.

## Next investigatory step

Record one short, deliberately anchor-dense capture — frequent `ANCHOR`
calls at known physical points, including at least one confirmed absolute
starting marker — so a run's predicted-vs-confirmed position agreement can
finally be checked directly, closing the "cannot be adjudicated without new
anchors" gap that limits every finding in §E above.
