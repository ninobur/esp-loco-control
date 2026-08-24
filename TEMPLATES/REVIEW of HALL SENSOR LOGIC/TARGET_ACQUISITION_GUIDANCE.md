# Templates Target-Acquisition Guidance

**Status:** Investigatory guidance. Nothing in this document is approved for implementation.

## Governing approach

Templates should use a target-acquisition model. The processing exists to identify the one mile-marker magnet physically possible next, not to build a general Hall-response classifier.

Prefer the simplest method that the evidence demonstrates is adequate.

A technique with a known red-flag failure mode shall not be adopted unless:

1. simpler approaches have proved inadequate;
2. evidence demonstrates that the technique contributes necessary target-identification capability; and
3. its failure mode and required response are explicitly captured in the design.

## What QUORUM presently approximates

- Adaptive baseline: a rolling median recenters the thresholds.
- Hysteresis: separate opening and closing thresholds reduce chatter.
- Peak hold: the largest excursion is retained.
- Short-event rejection: events below a 40 ms duration floor are discarded.
- Crude lockout: an event remains active until the reading is near baseline for 20 ms.
- Contextual checks: polarity, timing and route sequence are considered downstream.

QUORUM does not provide true CFAR, full-waveform retention, matched filtering, a demonstrated event-boundary rule, a deliberate post-event dead-time tied to physical movement, or target-specific flux fingerprints.

## Investigation order

1. Record complete Hall waveforms at the existing high acquisition rate.
2. Establish what genuine marker responses and reported “phantoms” physically look like.
3. Determine reliable event opening and closing rules.
4. Evaluate a second Hall sensor.
5. Build target fingerprints from repeated, independently anchored passes.
6. Test simple target gates first: polarity, physically impossible timing and minimum target flux.
7. Test matched filtering and adaptive thresholds against the same recordings only if simpler gates prove inadequate.
8. Add dead-time only after measuring how long one physical magnet can continue producing responses.

## Red-flag techniques

- Dead-time can hide the next genuine target if its duration is guessed.
- CFAR can raise its threshold around a weak genuine marker.
- Matched filtering can reject genuine responses when speed, sensor clearance or orientation changes.
- Full-waveform matching may duplicate evidence already provided more reliably by polarity, maximum flux, timing and two-sensor confirmation.

These techniques are candidates, not a required package. Raw waveform evidence must determine whether each earns a place in Templates.
