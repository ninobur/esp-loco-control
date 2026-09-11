# 0081 — The rebound guard is 500 ms

**Date:** 2026-09-10
**Status:** **Accepted, 2026-09-10**
**Decided by:** operator
**Supersedes:** the later 200 ms value in 0079; reaffirms 0057
**Amended by:** 0083, for one opposite-polarity successor after a controlled stop

## Decision

`NAVI_GUARD_MS` is restored to **500 ms** for Otto and Toby. The guard is
measured from the close of the last accepted passage to the opening of the next
candidate. No agent may lower it by classifying an isolated short-gap event as
a genuine magnet without first establishing that the locomotive could
physically have crossed the intervening track distance in that time and
presenting the evidence to the operator.

For the present production candidate this is the complete timing rule. There
is no PWM-dependent or adaptive extension. The operator considered that option
on 2026-09-10 and deferred it: 500 ms should be adequate. Revisit adaptive
timing only if field evidence shows problems at low speed, including starts or
stops, and present the design and replay evidence before changing firmware.

Field evidence later that day did show one specific post-stop failure. Decision
0083 adds a narrow exception without changing this 500 ms value: after a
controlled stop, the opposite-polarity successor to the first accepted
departure passage may proceed to the amplitude and Navigator tests inside the
guard. Same-polarity candidates and all ordinary running remain subject to the
500 ms floor.

## Context

The guard was changed from the operator's 500 ms value to 200 ms after a
436–438 ms event was interpreted as a genuine magnet. That interpretation was
not checked against physical speed. Crossing the shortest 280 mm marker spacing
at such a close-to-open interval would require approximately 473 mm/s (88
pKph), corresponding to about PWM 144 by Toby's recorded fit; the dataset's
highest throttle was 120 and the fleet maximum was lower. The event was a
re-read or transient, not evidence that a genuine marker could arrive inside
500 ms.

The 200 ms guard also admitted Otto's known 209 ms and 290 ms double reads. The
500 ms guard rejects them while retaining at least 336 ms of margin below the
smallest physically credible genuine close-to-open gap in the cited evidence.

The firmware restoration was already made in commit `4e72ff1`; this record
corrects the decision log, including the stale 200 ms block in 0079.

## Consequences

- Both NAVI_ONE locomotive profiles and the recognizer default must remain at
  500 ms.
- The X14 field-test candidate uses no adaptive, station-specific, or
  PWM-dependent timing floor.
- Tests and documentation that assert or justify 200 ms are stale and belong
  in the NAVI foundation audit.
- A proposed future change must distinguish a physically possible adjacent
  marker from a re-read or transient before treating it as calibration data.

## References

- `firmware/test-programs/NAVI_ONE/LL_LocoConfig_9950011.h`
- `firmware/test-programs/NAVI_ONE/LL_LocoConfig_9950012.h`
- `firmware/test-programs/NAVI_ONE/MagnetRecognizer.h`
- `docs/decisions/0057-the-rebound-guard-is-measured-close-to-open-and-there-is-no-motion-gate.md`
- `docs/decisions/0079-ottos-recognizer-constants-are-measured-on-otto.md`
- Commit `4e72ff1`
