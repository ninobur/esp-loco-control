# 0083 — A post-stop opposite-polarity successor may bypass the time guard

**Date:** 2026-09-10
**Status:** **Accepted, 2026-09-10**
**Decided by:** operator
**Amends:** 0081 only in the narrowly stated post-stop case

## Decision

After any **controlled stop** that actually reaches zero propulsion while NAVI
still has valid position, the first subsequently accepted Hall passage becomes
a departure anchor. Until the exception is consumed or expires, a candidate
that opens less than 500 ms after that anchor closes is handled as follows:

- the same polarity is still `TOO_SOON`;
- the opposite polarity may bypass the time guard, but must still pass the
  ordinary amplitude test and then Navigator's expected-polarity test;
- a successful opposite-polarity successor consumes the exception;
- the exception also expires when the ordinary 500 ms floor is reached.

The exception applies after station stops and operator-commanded controlled
stops. It must not arm for an emergency stop, low-voltage stop, or navigation
withdrawal/strike. A declaration or direction change clears it.

This is not morphology, stitching, a PWM model, or station identification. The
recognizer still does not know position or marker identity. The stop supplies
only the scope; alternating Hall polarity supplies the evidence that the close
candidate can be the adjacent marker rather than a re-read of the anchor.

## Deliberate limits and consequences

- The stop reason is latched for the entire ramp to zero. Battery-voltage
  recovery during a low-voltage ramp cannot convert that safety stop into a
  controlled stop.
- An opposite-polarity artifact that passes amplitude is allowed to reach
  Navigator. If it is not the expected polarity, Navigator will strike rather
  than silently discard it. That is the deliberate cost of leaving marker
  identity with Navigator.
- A controlled stop between markers still arms the rule: the next accepted
  passage is the anchor even though it is not tied to a named station.

## Field evidence

On Otto X14 at Bamboo, CW on 2026-09-10:

- MM160 was accepted after departure, North, peak 84;
- genuine MM161 opened South only 136 ms after MM160 closed and was refused
  `TOO_SOON`;
- MM162 then arrived North while Navigator still expected South, producing the
  one-strike stop; Otto coasted to MM163.

The prior morphology/stitch authority did not fire. The fixed guard correctly
performed its ordinary rule but lacked the explicitly bounded post-stop case.

## Verification required before flash

- Ordinary opposite-polarity candidates inside 500 ms remain refused.
- A post-stop same-polarity re-read remains refused.
- A post-stop opposite-polarity candidate still must pass amplitude.
- A weak candidate does not consume the exception; an accepted successor does.
- The 499/500 ms boundary, cancellation and frame reset are covered.
- The recorded MM160–MM162 sequence advances once per marker without a strike.
- All pre-existing NAVI_ONE gates remain green.
