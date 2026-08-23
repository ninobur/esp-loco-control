# Autonomous-acquisition acceptance harness

Frozen host-model acceptance tests for
`docs/AUTONOMOUS_POSITION_ACQUISITION_SPEC.md`, written **before** the
navigator they test, per `docs/AUTONOMOUS_ACQUISITION_ACCEPTANCE_TESTS.md` and
decision 0042.

There is no navigator here, and none is to be added to this package. It
contains tests, generators, invariant monitors and deliberately defective test
doubles.

## Running

```bash
python3 -m tools.navlab.acceptance.run_acceptance
```

Machine-readable report, and regenerating the manifest:

```bash
python3 -m tools.navlab.acceptance.run_acceptance --json /tmp/acceptance.json
python3 -m tools.navlab.acceptance.make_manifest
```

Once a navigator exists, point the suite at it:

```bash
NGR_NAVIGATOR=mypkg.mynav:Navigator python3 -m tools.navlab.acceptance.run_acceptance
```

Run from the repository root. No third-party dependencies; Python 3 standard
library only.

## Why failures are expected right now

The replacement navigator does not exist. Every navigator-dependent family
therefore reports **NOT_IMPLEMENTED** with a named reason. That is the correct
state, not a defect, and the runner exits 0 for it so CI can tell "not built
yet" apart from "built wrong". The runner exits non-zero only on FAIL or a
suite failure.

Four statuses, and they mean different things:

| status | meaning |
|---|---|
| `PASS` | the property was checked and holds |
| `FAIL` | the property was checked and does not hold |
| `NOT_IMPLEMENTED` | no navigator to check against; never a pass |
| `NOT_DEMONSTRATED` | the evidence class needed does not exist and generated data cannot supply it |

**No stub navigator is provided.** A navigator that always returns STOP would
satisfy every safety gate and no usefulness gate; self-test `H6` exists
specifically to prove the suite rejects one.

## What runs today

- **P0.1–P0.7** — map prerequisites, **computed from the committed map**, never
  assumed. These are blocking: if a uniqueness length did not exist, the
  corresponding acquisition mode would be reported unimplementable and the map
  would be left untouched.
- **H1–H8** — harness self-tests. Each drives a deliberately broken test double
  and requires the suite to catch it: a false confirmation, an
  under-approximated `COMPLETE` set, unsafe peer authority, receipt-time and
  firmware-label contamination, a prohibited `LAUNCH_HOLD`, a stop-only
  navigator presented as success, occupancy compression that drops candidates,
  and a demand for a manual MM declaration.

### Computed prerequisite result

| quantity | value |
|---|---|
| `W_dir` — unique within one direction plane | **10** |
| `W_both` — unique across both planes | **12** |

The inherited claim ("windows of length ≥ 10 are unique; W = 9 collides four
ways") is **confirmed for a single direction plane** — and does not extend
across planes. At W = 10 six polarity strings still match a position in both
directions; at W = 11, one does. Orientation-known acquisition may therefore
use 10 observations, orientation-unknown needs 12. The firmware's existing
`DNA_W = 12` matches `W_both`.

## Design rules the harness enforces on itself

- The DNA map and spacing table are parsed read-only from
  `firmware/QUORUM/QUORUM.ino`. The harness never writes, patches or
  "corrects" the map. A prerequisite failure is recorded; the map stands.
- Every stream is generated from a named integer seed and is byte-reproducible.
- Every detection carries explicit ground truth: the true marker, the true
  direction, whether it was genuine or a ghost, the true elapsed time, and how
  many markers were missed before it.
- **Travel direction is delivered as an explicit motion event**, not inferred
  from a detection. `Monitor.run` calls `nav.direction_changed(direction)` at
  every index in `Stream.reversals`. A native reversal is commanded motion
  state, not evidence about position: it says which way the locomotive is now
  going and nothing about where it is. This channel was added on 2026-08-22
  after `counterexample_t4_direction` showed the suite was otherwise
  unsatisfiable -- see "Corrections" below.
- Detections carry **no `dt` field**. Elapsed time is branch-local
  (spec §3.6), so it is a property of an (event, branch) pair and the harness
  refuses to precompute it.
- Every detection carries three **decoy** fields — a firmware MM label, a
  firmware verdict and an MQTT receipt timestamp. A conforming navigator
  ignores all three. `H4` proves the harness detects a navigator that does not,
  by running the same stream twice with different decoy values and requiring
  identical behaviour.
- Operator decisions that remain **open** (spec §9) are `navapi.Policy`
  fields, so a family can be run under either ruling. Decisions the operator
  has **made** are not configurable.

## Safety and usefulness are separate

Safety gates (`S1`–`S5`) must never fail. Usefulness gates (`U0`–`U7`) must be
met on the CLEAN families. A family marked **CLEAN** treats a stop as a
failure; a family marked **AMBIGUOUS** accepts a safe stop but never a wrong
confirmation. Every stop is classified `NO_STOP` / `SAFE_STOP` /
`MODEL_DEFECT_STOP`, and a model-defect stop in a CLEAN family fails the suite.

A single false confirmation raises `SuiteFailure` and stops the run — it is not
one family's failure, it is the suite's.

## Corrections

The harness is frozen against the implementation it tests. It has been
corrected once, and only on evidence independent of any navigator.

**2026-08-22 — travel direction was unreachable from the contract.**
`with_reversal` recorded a native reversal in `Stream.reversals` and nothing
delivered it, while `Detection` carries no direction and `NavigatorContract`
exposed no motion method. `counterexample_t4_direction` shows on the
committed map that 42 of 171 markers hide a reversal in **both** polarity and
interval length, builds two worlds over one detection sequence whose records
are identical on every field spec 3.2 permits, and enumerates all 342
single-element hypothesis sets to show none satisfies S2 in both. S2 and
specification 4.1 (`|H| = 1` in `POSITIONED`) therefore could not both hold at
a reversal. The correction adds the missing input — `direction_changed` on the
contract, delivered by `Monitor.run` — and weakens neither requirement. No
family, no generated truth, no invariant semantic and no expected outcome
changed; the no-navigator result is still
`PASS=18 FAIL=0 NOT_IMPLEMENTED=31 NOT_DEMONSTRATED=3`.

Run the evidence:

```bash
python3 -m tools.navlab.acceptance.counterexample_t4_direction
```

It exits non-zero if the committed map ever stops supporting the witness, so
the correction cannot be inherited once its justification has expired.

## Files

| file | role |
|---|---|
| `ngrmap.py` | the real committed map, geometry, stations, launch region |
| `prereq.py` | P0 uniqueness computation |
| `navapi.py` | navigator contract, policy record, detection/peer records, loader |
| `generate.py` | deterministic streams with explicit truth |
| `invariants.py` | continuous invariant monitors and the conservative reference oracle |
| `families.py` | the frozen acceptance families |
| `selftests.py` | defective doubles the suite must reject |
| `counterexample_t4_direction.py` | navigator-independent evidence for the one harness correction |
| `run_acceptance.py` | runner |
| `make_manifest.py` | regenerates `manifest.json` |
| `manifest.json` | plan family / invariant → implementing function |

## What this harness does not do

It does not implement, and must not be extended to implement, the replacement
navigator, production QUORUM firmware, MQTT or CTO behaviour, station firmware,
or the timing-envelope generator beyond the interface it needs. It also cannot
substitute for validation: `N1` records that no untouched capture exists, and
every existing Toby and Otto session is permanently development data.
