# NAVI_WOLF_PACK — Checkpoint 1

**Branch:** `research/navi-wolf-pack-20261021`  
**Base:** `e56923c`  
**Scope:** offline research only; no operational firmware was changed, built for
flashing, or deployed.

## Recovered candidates

| Family | Recovery | Fidelity result |
|---|---|---|
| DNA | historical v2.0/v2.1 lineage identified by retrospective | sufficient source archaeology, no common raw replay adapter yet |
| QUORUM | source recovered at `31c4898`; historical field baseline `ca27f1c` | source recovered; historical asymmetric fence has not been altered |
| NAVI_SIMPLIFIED | reconstruction at `c19ec1f5` recovered | exact flashed-image equivalence remains unproved |
| RVM continuity | `8986c5f`, including C++ source and recorded firmware-parity outputs | strongest recovered fidelity evidence; common-input adapter remains required |
| continuous landscape | October 16 frozen aligner identified in historical package | causal replay model, Otto-specific, no field proof |
| CPR | `f9cc9493`–`fca9e5b8` recovered in full | portable offline package, replay-only |

The new `*_EXPERIMENTAL_RECONSTRUCTION` controls are deliberately separate
from their historical counterparts. They cannot receive historical scores.

## Available recordings and provenance

The frozen manifest contains four byte-verified native Hall recordings recovered
with CPR:

| Recording | Partition | Native Hall verification | Geographic truth |
|---|---|---|---|
| Otto train, 2026-10-15 | development | verified | annotations only |
| Otto CW, 2026-10-19 | development/calibration | verified | annotations only |
| Otto CCW, 2026-10-19 | validation | verified | annotations only |
| Toby CW, 2026-10-19 | final test | verified | annotations only |

The source package reports 66 verified research artifacts and native-byte SHA-256
matches for all four recordings. Existing RVM source/parity outputs and field
records are recoverable as separate evidence; their raw common stream and an
independent truth mapping are not yet in this frozen common manifest.

## Independent ground truth

**No manifest recording is eligible for definitive geographic accuracy scoring.**
The retained event annotations are historical/model comparison material rather
than independent physical encounter/location truth. This prevents using any
candidate's MM declaration to prove itself correct.

The historic full-lap and field claims remain valuable field evidence in their
own right, but do not permit a new fair cross-model comparison without a common,
causal input and independent mapping.

## Replay fidelity

- CPR: portable data and 66-package artifact verification succeeded. Its own
  frozen evaluation was rerun unchanged. It produced zero Toby CW recognized
  events against 46 historical annotations; that negative replay result is
  retained.
- RVM continuity: original source and previously recorded C++/Python parity
  material are present, but this experiment has not claimed a new adapter as
  faithful until it is exercised against a common native stream.
- DNA, QUORUM, and NAVI_SIMPLIFIED: no unqualified replay-fidelity claim is
  made. The framework labels any provisional control an experimental
  reconstruction.

## Frozen scoring criteria

`research/navi_wolf_pack/scoring_protocol.md` is frozen before final scoring.
It requires native chronological input, declared initial state/map, independent
truth, and session-level partitioning. It separately reports recognition,
geographic continuity, operational resilience, and simplicity; it prohibits a
single weighted score that hides confident geographic failures.

## Material obstacle

The substantive competition cannot yet be validly run. The final-test session
has no independent geographic truth, and no common raw causal stream has been
connected to every recovered historical implementation. Inventing labels or
feeding one model's declarations to rivals would violate the experimental rule.

## What was implemented and checked

- `research/navi_wolf_pack/replay.py`: chronological replay contract with no
  future-observation access and explicit uncertainty output.
- Four contract tests passed: causality, refusal on ambiguity, duplicate-event
  suppression, and rejection of time-reordered input.
- CPR package verification and frozen evaluation executed successfully; the
  complete generated output remains in its recovered historical package.

## Next required evidence

1. Recover/export shared native Hall plus recorded PWM/IR/direction streams for
   chosen whole sessions.
2. Create immutable, independently derived encounter/location annotations for
   those sessions, including uncertainty ranges.
3. Build and parity-check per-model adapters before any candidate score.
4. Only then run the frozen final-test session once and conduct controlled
   combination/ablation experiments.
