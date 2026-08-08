# Repository review — NGR console authority alignment, Draft 5

**Reviewer:** Codex  
**Review date:** 2026-08-08  
**Subject:** `NGR_DASHBOARD_AUTHORITY_ALIGNMENT_SPEC.md`, Draft 5  
**Repository state:** `409a251`  
**Disposition:** Approve after two specification corrections; no new operator ruling required

This review was performed directly against the repository. It records findings;
it does not amend the controlling specification.

## Verdict

Draft 5 resolves the substantive Draft 4 authority problems. E-STOP now
preserves DIRECTION through both branches, NEUTRAL is an enlistment gate,
navigation refusal states are mapped explicitly, and the BEGIN-gate invariant
exposes the accepted post-enlistment NO_QUORUM residual.

Two specification inconsistencies remain. Both are editorial corrections to
decisions already made, but both affect implementation scope. Correct them
before implementation.

## C1 — Remove the deferred −2 candidate change from the sequence

**Blocking for scope.** Section 11 says the −2 candidate-window change rides
the same firmware bump. The operator subsequently decided to retain the current
six-candidate window and mitigate hand repositioning operationally: if the Hall
sensor is moved across a marker by hand, location must be reset/re-declared.

The candidate-window analysis remains useful evidence, but it was not approved
for implementation. Remove the −2 change from this version bump and record it
as deferred rather than rejected.

Relevant specification: §11, lines 453–457 at `409a251`.

## C2 — P13 still omits `STOP_IGNORED`

**Blocking for implementation precision.** R14 correctly defines the dedup
bypass set as `*_REFUSED` plus `STOP_IGNORED`. P13, the controlling firmware
proposal, still instructs the implementer to bypass dedup only for
`*_REFUSED`. Following P13 literally would recreate Draft 4 finding C2 for
repeated dispatcher STOP commands addressed to a locomotive that is not
enlisted.

Amend P13 to state that both `*_REFUSED` events and `STOP_IGNORED` bypass the
station-transition dedup and carry the monotonic sequence number. Routine
station transitions retain their existing deduplication.

Relevant specification: R14 and P13, lines 122 and 324–330 at `409a251`.

## Material reviewed and accepted

- P11 adds the energisation, ORIENTATION, DIRECTION and navigation enlistment
  gates without restricting `cmd/auto 0`.
- P14 removes the NEUTRAL mutation from both E-STOP branches and relies on the
  continuously enforced `estopped` interlock.
- The BEGIN-gate standing check is sound and should be rerun whenever a new
  BEGIN refusal is introduced.
- Post-enlistment NO_QUORUM is explicitly owned as a residual rather than
  silently treated as solved.
- T2, T6 and T9 are correctly identified as blocked until their respective
  firmware changes land.
- The console may deliver the independently useful authority-handoff work
  before the firmware bump, subject to the stated blocked tests.

## Disposition

Approve Draft 5 for implementation after:

1. removing the −2 candidate-window change from §11; and
2. making P13's bypass set explicitly match R14: `*_REFUSED` plus
   `STOP_IGNORED`, each with a monotonic sequence number.

No new operator ruling or additional design cycle is required.

## References

- `docs/NGR_DASHBOARD_AUTHORITY_ALIGNMENT_SPEC.md`, Draft 5 (`409a251`)
- `docs/NGR_DASHBOARD_AUTHORITY_ALIGNMENT_DRAFT4_REVIEW_CODEX_20260808.md`
- `docs/NGR_DASHBOARD_AUTHORITY_ALIGNMENT_DRAFT4_REVIEW_CLAUDE_20260808.md`
- `docs/QUORUM_CANDIDATE_WINDOW_ANALYSIS.md`
- `firmware/QUORUM/QUORUM.ino`
