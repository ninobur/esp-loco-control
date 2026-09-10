# 0080 — Field-test rulings expire with the test, and morphology has no navigation authority

**Date:** 2026-09-10
**Status:** **Accepted, 2026-09-10**
**Decided by:** operator
**Supersedes:** 0070 and 0074

## Decision

Decision 0070 was a developmental directive for a named experimental field
test. Its pause/resume morphology, stop-spanning stitch, special
`STITCHED_REFUSED` shutdown, and the statement that a stop meant the build was
working expired with that test. They are not standing NAVI policy and may not
be cited to justify the behavior of a later build.

The governing principle from 0074 is accepted and made unambiguous:
**morphology is observation only.** It may be measured, published, archived,
and used by an engineer to understand a recording. It may not admit or refuse a
magnet; pause, resume, or stitch a navigation measurement; advance or alter
position; withdraw motion authority; or stop the locomotive. This applies at
stations and everywhere else.

Station behavior and marker accounting must be implemented from NAVI's magnet
evidence and explicit navigation state. A station state does not manufacture a
magnet observation. The station design must account explicitly for whether its
arrival marker was already counted and must not hide a missing or duplicate
count behind a morphology-derived stitch.

## Context

During Otto's 2026-09-10 Stage 1 run, two departures after Grillers stopped in
the same way. A 139-count fragment was followed 158 ms later by an 80-count
fragment. Acquisition stitched them; the ordinary timing guard returned
`TOO_SOON`; and the surviving 0070 `kind == 1` path converted that ordinary
refusal into `STITCHED_REFUSED`, position withdrawal, and a stop. Two-sided
shape analysis was not reached.

Decision 0074 had already said that shape was diagnostic and explicitly listed
`STITCHED_REFUSED` as a shutdown to retire. The implementation retired the
shape verdict but not that shutdown, while retaining 0070's morphology-driven
stitching. Stage 1 therefore ran on a partially implemented and internally
contradictory foundation.

The operator's ruling is:

> The design is not production ready because design decisions have not been
> implemented as agreed. You cannot build a building on a mostly built
> foundation.

And, concerning 0070's “a stop is the build working” language:

> It was a developmental directive, not a justification to be used anytime the
> train stops in perpetuity.

## Consequences

- NAVI-CTO Stage 1 is paused until NAVI is audited against the operator's
  governing decisions and verified independently.
- The current X13 behavior is not production-ready merely because its shutdown
  is conservative or was once useful diagnostically.
- `STITCHED_REFUSED` has no continuing authority as a special shutdown path.
- Existing code and tests that implement, require, or praise the superseded
  behavior are defects to be identified by the NAVI foundation audit; this
  record does not silently edit them.
- A replacement station-accounting design and its replay evidence must be
  presented to the operator before firmware is changed or fielded.

## References

- `docs/decisions/0070-a-passage-may-not-span-a-stop.md`
- `docs/decisions/0074-shape-is-recorded-not-refused.md`
- `field-records/20260910_GRILLERS_STITCH_SHUTDOWN.md`
- `firmware/test-programs/NAVI_ONE/NAVI_ONE.ino`
- `firmware/test-programs/NAVI_ONE/HallCapture.h`
- `firmware/test-programs/NAVI_ONE/MagnetRecognizer.h`
