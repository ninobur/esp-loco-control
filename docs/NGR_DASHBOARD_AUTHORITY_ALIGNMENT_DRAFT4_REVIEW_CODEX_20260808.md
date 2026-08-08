# Repository review — NGR console authority alignment, Draft 4

**Reviewer:** Codex (current repository review)  
**Review date:** 2026-08-08  
**Subject:** `NGR_DASHBOARD_AUTHORITY_ALIGNMENT_SPEC.md`, Draft 4  
**Repository state:** `8c338c6`  
**Disposition:** Do not implement until C1 is fixed; C2-C4 require specification tightening

This review was performed directly against the repository. It records findings;
it does not amend the controlling specification.

## Verdict

Draft 4's authority model is substantially stronger. The ENLISTED/RUNNING
separation, retained-state seeding, Flask fan-out, asymmetric E-STOP controls,
and BEGIN/PAUSE/END semantics are sound. One firmware/spec mismatch still makes
T6 impossible, and three additional contracts remain underspecified.

## C1 — P14 changes only the clear branch, but assertion already destroys DIRECTION

**Blocking.** Current QUORUM forces `DIRECTION_NEUTRAL` both when E-STOP is
asserted and when it is cleared. P14 removes only the clear-path assignment, so
the value preserved at clear would already be NEUTRAL. BEGIN then refuses with
`NEUTRAL_SELECT_DIRECTION`, while R7 has withdrawn the locomotive-page DIRECTION
control.

R13 requires either preserving DIRECTION during assertion as well, or saving and
restoring the prior direction. The minimal coherent design is to leave DIRECTION
unchanged on both E-STOP set and clear while `estopped` continuously clamps PWM
to zero.

Relevant source: `firmware/QUORUM/QUORUM.ino`, E-STOP handler near lines
2619-2633 at `8c338c6`; Draft 4 P14.

## C2 — R14's response guarantee is broader than P13

R14 says every operator command gets an observable response, including repeats.
P13 bypasses transition dedup only for events ending in `*_REFUSED`.
`STOP_IGNORED` and repeated successful setup acknowledgments can therefore still
be suppressed by the existing event+offset deduplication.

Either narrow R14 to refused commands, or define a general command-response
publisher/sequence contract separate from station-transition publication.

## C3 — P11 must distinguish navigation refusal states

P11 maps "navigation not ready" to `NO_POSITION_DECLARE_START_MM`, but QUORUM
distinguishes `NAV_UNSET` from `NAV_NO_QUORUM` and already uses different GO
reasons. Specify the enlistment mapping explicitly:

- `NAV_UNSET` -> `NO_POSITION_DECLARE_START_MM`;
- `NAV_NO_QUORUM` -> `NO_QUORUM_DECLARE_POSITION`;
- `NAV_EVALUATING` remains usable, if the existing GO contract is retained.

This matters because Draft 4 deliberately surfaces raw firmware reasons.

## C4 — The implementation sequence omits P13 and P14

Section 11 schedules only P11 before the console, although T6 requires P14 and
T9 requires P13. Section 8 marks only T2 blocked by delayed firmware. Name all
three firmware changes in sequence step 2, or explicitly mark T6 and T9 blocked
as well. Otherwise the console can be declared ready while two acceptance tests
remain impossible.

## Editorial consistency

R11 still calls dispatcher E-STOP a "toggle," while §4.3 and P12 correctly
conclude that no aggregate toggle exists. Rename the ruling to "broadcast set
with per-locomotive clear."

## Disposition

Do not approve implementation until C1 is corrected in the specification. C2,
C3, and C4 should be resolved in the same editorial pass so the implementation
and field-test contracts are unambiguous.

## References

- `docs/NGR_DASHBOARD_AUTHORITY_ALIGNMENT_SPEC.md`, Draft 4
- `docs/CTO3/AUTHORITY_MODEL.md`
- decision 0013
- `firmware/QUORUM/QUORUM.ino` at `8c338c6`
