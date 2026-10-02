# 0119 — Firmware builds are numbered sequentially and registered

Status: Accepted, 2026-10-02. (Drafted as 0118; renumbered because 0118 was
assigned to the first-target rule in `ab0938b`.)
Decided by: David — "A basic AI task is to keep track of the firmware
versions. Set up a system now … And have CODEX start numbering the versions."

## Decision

1. Every locomotive firmware build has a short sequential number per line:
   `EWO-14`, `EWO-15`, … The number is the whole value of `SKETCH_NAME`, so it
   appears in every boot message and every `loopstat`, and therefore in
   every run log.
2. Any commit that changes what the firmware does (any `.ino`/`.h` change
   other than comments) bumps the number, in that same commit.
3. The same commit adds the build's row to `firmware/BUILDS.md`: number,
   commit, date, branch, what changed, status.
4. Every flash is recorded in the flash log in `firmware/BUILDS.md`. The
   person flashing does not have to do it: the first agent to analyse a log
   from a newly seen build records it, citing the log.
5. Descriptive suffixes (`_R2`, `_FT1`) are not used. What changed belongs in
   the register, not in the name.
6. The host test suite fails if the sketch's `SKETCH_NAME` has no row in
   `firmware/BUILDS.md`, so an unregistered build cannot pass its tests.

## Context

Between 2026-09-29 and 2026-10-02 the EWO sketch went through twelve builds
with different behaviour. Eleven reported the same name,
`NAVI_EYES_WIDE_OPEN_INTEGRATED_R2`. A field log could not show which build
produced it. On 2026-10-02 the build on Otto had to be identified by
comparing telemetry fields against commits. The EWO-11 flash was never
recorded. Decision 0018 already said to put the version in `SKETCH_NAME`,
but nothing required changing it, and it was not changed.

## Alternatives considered

- Embedding the git commit hash at compile time: exact, but the Arduino IDE
  build has no step that can insert it. Possible later; the number does not
  depend on it.
- Keeping named revisions (`R2`, `R3`): this is the scheme that failed.
- Asking the operator what was flashed: puts the agents' bookkeeping on
  David. The firmware reporting its own number makes the logs the record.

## Consequences

- EWO-1 to EWO-13 were numbered after the fact. Their firmware does not
  report the number; the register says how to tell them apart from logs.
- The dashboard shows the `sketch` string only as text; no tool parses it.
- Other lines (QUORUM, NAVI_COHERENCE…) adopt the same rule at their next
  build.

## References

`firmware/BUILDS.md`; `firmware/README.md`; decision 0018;
`field-records/20260929_OTTO_EWO_WIFI_DIAGNOSIS_AND_FLASH.md`.
