# 0018 — Firmware artifacts are classified independently by role and evidence status

Status: Accepted (operator ruling 2026-08-08)

## Decision

Every runnable sketch has an explicit **role**—production control, diagnostic /
experiment, or historical reference—and an independent **evidence status**.
`firmware/README.md` is the authoritative catalog.

QUORUM remains the single production-control lineage even when a narrow feature
is awaiting field test. Diagnostic sketches remain instruments; their measured
findings may produce a separately reviewed QUORUM change, but the sketch itself
is never promoted implicitly.

## Context

Names such as `QUORUM_1.9_ONE_STATION` would make a development purpose visible,
but would also suggest a second control sketch and obscure whether later changes
belong in production. IR development demonstrates the opposite problem: several
valuable test programs exist, but a successful experiment does not necessarily
belong in the locomotive controller.

A single word such as “current” cannot distinguish latest source, latest built
candidate, latest field-tested capability, and the firmware actually accepted
for operation.

## Alternatives considered

- Name a separate control sketch for each test mission — rejected because it
  violates decision 0003 and creates drifting firmware branches.
- Treat everything under `firmware/` as production — rejected because
  diagnostics deliberately omit control and safety behavior.
- Rely only on filenames or version numbers — rejected because they do not carry
  validation status or explain whether experimental behavior was promoted.

## Consequences

The stable production path remains `firmware/QUORUM/QUORUM.ino`. Git commits,
tags, reports, and the published `SKETCH_NAME` identify its versions. Diagnostic
programs live under `firmware/test-programs/`, state their purpose and limits,
and require an explicit promotion step before their findings alter QUORUM.

Repository reviews include a librarian check: classification, evidence status,
provenance, promotion outcome, and catalog currency. “Latest” never means
“field accepted” without a recorded field verdict.

## References

- `firmware/README.md`
- decision 0003
- `docs/CLAUDE.md`
- `docs/IR_DEV_REC/`
- `docs/CTO3/station-stop-v1/IMPLEMENTATION_REPORT.md`
