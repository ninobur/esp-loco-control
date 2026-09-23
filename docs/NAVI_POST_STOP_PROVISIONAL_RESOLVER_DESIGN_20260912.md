# NAVI_ONE — bounded post-stop provisional-passage resolver

**Date:** 2026-09-12  
**Status:** Experimental design and host replay only. Not an operator decision,
not integrated into firmware, not ready to flash.

## Problem established by the field records

Neither existing extreme is safe:

- X15 admits an opposite-polarity passage inside the 500 ms guard. At Arches
  this promoted a weak North component before Navigator could reject it, moved
  the recognizer anchor, and caused a strike.
- Making 500 ms unconditional would repair Arches but would again reject the
  genuine MM161 Bamboo passage and the successful MM64 Grillers passage.

The 136, 113 and 318 ms values are close-to-open gaps between field envelopes.
They are not the time taken to travel the full map spacing. Center-to-center
speed arithmetic therefore cannot classify these three passages.

## Proposed bounded rule

The ordinary close-to-open floor remains 500 ms, unchanged. A controlled stop
opens one exceptional observation window after its first accepted departure
passage. Inside that window:

1. A passage is initially provisional. It cannot move the recognizer anchor or
   update its gain history merely by appearing.
2. The narrow bypass requires both:
   - the polarity Navigator expects next; and
   - peak/gain at least 1.0.
3. A candidate that lacks either credential is quarantined, not struck and not
   accepted.
4. A field component opening no more than 64 ms after the quarantined component
   may replace it when the replacement has the expected polarity and its own
   opening is at least 500 ms from the last genuinely accepted anchor.
5. Acceptance or ordinary-floor expiry closes the exceptional window.
6. Declaration, reversal, emergency stop, low voltage or navigation withdrawal
   cancels it.

The 1.0 strength credential is intentionally conservative and local to this
exception. It is not a new general magnet threshold. The measured false
population in decision 0052 peaked at 52 counts and ratios no higher than 0.280;
the two observed genuine close successors are 1.513 (Bamboo) and 1.385
(Grillers). The Arches North component is 0.413. This is evidence for a replay
candidate, not yet enough evidence for production adoption.

The 64 ms companion window is the measured upper bound of the ordinary rebound
population in decision 0052. It allows the Arches South component, which opened
58 ms after the North component closed, to be evaluated against MM106's
unchanged genuine anchor.

## Field replays encoded

`tests/gate_post_stop_resolver.cpp` uses the recorded timestamps, peaks, gains
and polarities for:

- X14 Bamboo: MM160 N, genuine strong MM161 S at +136 ms, then MM162 N;
- X15 Grillers: MM63 N, genuine strong MM64 S at +113 ms, then MM65 N;
- X15 Arches: MM106 S, weak/wrong N component at +318 ms, then the expected S
  component 58 ms later.

It also asserts that a decision-0052-strength re-read cannot advance, a strong
wrong-polarity component cannot advance, and the 500 ms boundary remains the
ordinary path.

## Replay result

The isolated model passes all nine assertions. The existing NAVI_ONE suite also
remains green because production firmware has not been modified.

## Required before firmware integration

1. Audit every available controlled-stop departure for the proposed 1.0 ratio
   credential, in both directions and on both locomotives. A genuine close
   successor below 1.0 is a counterexample and blocks this design.
2. Decide the cross-core ownership explicitly. The Hall task owns recognizer
   anchors and gain; Navigator owns expected identity on the loop task. No
   direct cross-thread mutation is permitted.
3. Add cancellation, delayed-message and back-to-back-passage adversarial gates.
4. Preserve one accepted physical passage = at most one position advance.
5. Present the completed integration and diff to the operator before flashing.

## Files in this experiment

- `firmware/programs/NAVI_ONE/variants/NAVI_ONE/tests/post_stop_resolver_model.h`
- `firmware/programs/NAVI_ONE/variants/NAVI_ONE/tests/gate_post_stop_resolver.cpp`
- `firmware/programs/NAVI_ONE/variants/NAVI_ONE/tests/run_tests.sh`

No production header or sketch is changed by this experiment.
