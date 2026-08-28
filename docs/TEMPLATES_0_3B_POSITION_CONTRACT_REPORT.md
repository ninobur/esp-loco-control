# TEMPLATES 0.3B — position-contract correction, implementation report

**Date:** 2026-08-27
**Status:** Built, compile-verified both profiles, host regression suite
161/161. **NOT FLASHED** — flashing awaits David's approval, per the fix
directive.
**Trigger:** first R3 field run drifted 82 markers (physical 042–043,
software 124), session `9950012_20260827_180317`.
**Requirements:** CODEX synthesized eight-point contract, 2026-08-27.
**Decision record:** 0045.

## Root cause (confirmed from the run log)

```
18:05:16.4  r3_admit  TARGET_CONFIRMED   (R3 approves, mutates nothing yet — but 0.3A had already committed corrections this way)
18:05:16.4  nav       PHANTOM_REJECTED   (inherited conservation gate refuses the same passage; ratio 1.30)
18:05:17.8  r3_admit  R3_CORRECTED off=1 (R3, blind to the refusal, "corrects" past it)
```

R3 and the inherited ladder acted as two sequential admission authorities.
R3 mutated `navMm` during correction *before* `navOnMarker()`; the phantom
detector could then refuse the passage; R3 never received that disposition
and resynced its shadow to whatever `navMm` ended up as. Corrections are
forward-only, so the error only ever grew. A second defect compounded it:
physically impossible candidates (up to ~1,090 mm/s implied) cost only the
timing attribute's 16 weight points and were adopted at 76–83% confidence.

## The fix, against each requirement

1. **R3 no longer mutates `navMm`.** `r3Gate()` is replaced by
   `r3Evaluate()`, which produces an `R3Proposal` and touches no navigation
   state. `navMm` is written in exactly one place on the event path —
   `acceptEvent()`.
2. **Explicit dispositions.** `navOnMarker()`/`navLadder()` return
   `NavDisposition`: `ACCEPTED`, `QUARANTINED`, `PHANTOM_REJECTED`,
   `NO_POSITION`, `NO_DIR` (plus `NOT_PRESENTED` for events R3 holds).
3. **Atomic correction.** The proposed offset rides the event into the
   ladder (`navOnMarker(e, corrOff)` → `acceptEvent(e, corrOff)`), so
   correction + acceptance commit in one statement or not at all. A
   quarantined event holds its proposal with it (`qPendingCorrOff`) and the
   arbitration's H-genuine frame accounts for it
   (`mmG = navMm + (2 + corrOff)·dir`).
4. **Refusal changes nothing.** On any non-`ACCEPTED` disposition the
   confirmed position is untouched (nothing was ever written) and
   `r3NoteAfterNav()` returns without moving the shadow, updating velocity,
   or snapshotting IR. Only `ACCEPTED` resynchronizes.
5. **Telemetry separates proposal from commitment.** One `diag/r3_admit`
   record per identity decision, published *after* the navigator rules:
   `prop` (proposed outcome), `nav` (disposition), `cm` (committed 0/1),
   `mm` (at proposal), `mmf` (final `navMm`), plus the full §10 evidence
   set. Short keys forced by the 512-byte PubMsg bound; the fixed mapping
   is documented at `r3PublishDecision()`.
6. **Phantom detector and QUORUM recovery preserved unchanged.** No
   vouching exemption for R3-confirmed passages — that remains an explicit
   future decision if wanted (0045 records the alternative as rejected for
   now).
7. **Regression test for the observed sequence.** T2 reproduces the
   R3-confirms/ladder-refuses collision four consecutive times and asserts
   `navMm` and the R3 shadow are bit-identical throughout, then that one
   proper event advances exactly one marker.
8. **The full path matrix is tested.** T1 ordinary confirmation, T3
   committed correction (atomic two-marker jump), T4 refused correction
   (commits nothing — the fallback that would let this test silently skip
   the branch is itself a failure), T5 quarantine hold → successor commit,
   T6 quarantine hold → successor discard, T7 operator relocation → bypass
   once → resync on acceptance, T8 EVALUATING owned by QUORUM → R3 bypasses
   → resyncs after resolution. **161 checks, 0 failures.**

## Physical reachability veto

Per the operator's question ("is the algorithm permitted to consider
physically impossible navigation solutions?") — it was, and no longer is.
A candidate whose map span over the measured elapsed time implies more than
`R3_MAX_MMPS` (800 mm/s — the same bound `Q_FLOOR_MS` derives from: fleet
p99.9 max 441 mm/s, ~1.8× margin) is **excluded from scoring outright**,
never merely down-weighted. When every candidate is excluded the passage is
`MAGNET_UNRESOLVED` with the exclusion count published (`ex`). CODEX's
trace is respected: this was not the initiating defect, and it is
implemented as a constraint on the candidate set, not as the fix.

## Deliberate behavioral changes beyond the requirements

- **Confirmation is stricter.** 0.3A confirmed the expected candidate at
  ≥50% even when a farther candidate scored higher without reaching
  correction authority. 0.3B holds in that ambiguous case: the expected
  candidate must be the best non-excluded candidate. Recoverable omission
  over false inclusion (0043).
- **Counters record committed outcomes.** `nc`/`nx` count only
  navigator-accepted confirmations/corrections; refusals get their own
  bucket (`nr`).

## The regression suite

`firmware/test-programs/TEMPLATES/tests/` — `run_tests.sh` regenerates the
prototype-inserted sketch with `arduino-cli --preprocess` (the same
generator the firmware build uses: one code path, no test copy), compiles
it against `tests/stubs/` (simulated clock, real FIFO queues, captured
publishes), and runs `harness_r3.cpp`. Requires `arduino-cli` with the
esp32 core and the local git-ignored `credentials.h` (any values).

## Verification

- ESP32 compile: Toby profile 76% flash, Otto profile 77% (stub-path
  check); `LocoConfig.h` restored to Toby.
- Host suite: 161 checks, 0 failures, all eight scenarios exercising the
  genuine `drainMarkers()` → `r3Evaluate()` → `navOnMarker()` path.

## Honest limits

- The host suite simulates the loop thread only; hallTask, networking, and
  real concurrency are not exercised. The clock is simulated.
- T5/T6 note in code: some weak-event shapes are now held by R3 before the
  quarantine can see them — the two layers overlap on dim events. Which
  layer should own that class is field data we don't have yet.
- The stricter confirmation rule is untested in the field.
- Everything in decision 0044's provisional-status section still applies:
  thresholds, weights, tolerances, and the veto bound are experimental
  test settings.
