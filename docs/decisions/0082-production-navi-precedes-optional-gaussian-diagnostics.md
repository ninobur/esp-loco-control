# 0082 — Production NAVI precedes optional Gaussian diagnostics

**Date:** 2026-09-10
**Status:** **Accepted, 2026-09-10**
**Decided by:** operator
**Follows:** 0080 and 0081

## Decision

A working production NAVI sketch is the primary objective. Diagnostic analysis
serves that objective and may not become an independent development program
that delays, complicates, or governs the locomotive.

Gaussian morphology analysis is removed from the production navigation path.
If retained, it is an optional, modular troubleshooting observer with a
versioned, read-only passage interface. The production sketch uses a null
observer or does not compile the package at all. The Gaussian observer may
calculate, publish, archive, and replay measurements, but NAVI never reads its
result and the observer has no reference or path to position, stations, motor
control, motion authority, or safety interlocks.

The corrective sequence is:

1. Restore and verify the operator-agreed NAVI production behavior.
2. Prove station operation and the Grillers case without morphology authority.
3. Only after that foundation works, extract useful Gaussian code into the
   optional observer package and verify that enabling it cannot alter NAVI's
   operational results.

## Context

Gaussian analysis is clever and useful for troubleshooting waveform quality,
sensor installations, electrical noise, and acquisition faults. On this
railway it added complexity but did not add demonstrated discrimination. Its
presence inside NAVI allowed experimental morphology and stitching behavior to
gain operational authority after the field test that introduced it had ended.

The operator's ruling is:

> Please keep a working production sketch as our primary objective. Diagnostics
> are interesting but they should only serve the bigger project and not become
> an end in themselves.

## Consequences

- Production acceptance tests are based on navigation and operating behavior,
  not on Gaussian output or preservation of experimental diagnostic gates.
- The optional observer must be removable without changing passage admission,
  marker identity, position, station behavior, throttle, or stopping.
- Diagnostic telemetry must identify its own observer/schema version so later
  NAVI iterations can adapt at one explicit boundary.
- Work on the optional package pauses until the corrective NAVI build and its
  station replays are satisfactory to the operator.

## References

- `docs/decisions/0080-field-test-rulings-expire-and-morphology-has-no-navigation-authority.md`
- `docs/NAVI_ONE_OPERATOR_RULING_AUDIT_20260910.md`
- `firmware/test-programs/NAVI_ONE/MagnetRecognizer.h`
- `firmware/test-programs/NAVI_ONE/TwoSided.h`
