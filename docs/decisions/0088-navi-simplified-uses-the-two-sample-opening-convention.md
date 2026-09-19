# 0088 — NAVI_SIMPLIFIED uses the two-sample opening convention

**Date:** 2026-09-18
**Status:** **Provisional pending NAVI_SIMPLIFIED field testing**
**Applies to:** `NAVI_SIMPLIFIED` only
**Decided by:** operator — “Yes. But without evidence. It is a convention as far as I know.”
**Consulted precedent:** 0087

## Decision

For NAVI_SIMPLIFIED, a Hall magnet opening is declared on the second of two
consecutive 1 kHz samples whose absolute departure from the current baseline is
at least 70 counts. Detection time is the time of the second sample. Opening
polarity is recorded from the signed departure at detection.

This is an operating convention for the first NAVI_SIMPLIFIED implementation.
It is not claimed to be an evidence-proven optimum. It remains provisional
until field testing supplies evidence to retain or revise it.

## Context

Decision 0087 used the same two-sample opening rule in X21 and fixed polarity
at opening rather than allowing a later waveform feature to reverse it. That
record is explicitly proposed and is not authority for NAVI_SIMPLIFIED. Its
history was consulted as required, but the operator approved this convention
directly for NAVI_SIMPLIFIED.

The available history supports separating opening polarity from later waveform
morphology. It does not establish that two samples, 1 kHz, or 70 counts is the
best possible detector configuration.

## Alternatives considered

- Five-sample persistence used by `NAVI_ONE_SIMPLE`: not selected for
  NAVI_SIMPLIFIED.
- Later waveform-derived polarity: not selected; morphology has no navigation
  authority under decision 0080.
- A different sample count, rate, or amplitude threshold: not rejected for all
  time, but not selected for the provisional first implementation.

## Consequences

- Navigation may receive an opening approximately 2 ms into a sustained
  qualifying departure, subject to actual sampling phase.
- Later peak, closure, width, signed sum, or waveform shape cannot revise the
  recorded opening polarity.
- Field telemetry must preserve both qualifying samples, baseline, signed
  departures, detection time, and event identity so the convention can be
  evaluated.
- Field testing must report missed magnets, duplicate openings, and qualifying
  transients. A successful compile or replay alone does not validate the
  convention.
- This record decides detection only. It does not adopt 0087's 645 ms guard,
  its PWM-zero mechanism, or any physical-reachability rule.

## References

- `docs/decisions/0080-field-test-rulings-expire-and-morphology-has-no-navigation-authority.md`
- `docs/decisions/0087-hall-navigation-is-the-opening-and-a-645-ms-guard.md`
- `docs/NGR NAVI_SIMPLIFIED.md`
- `docs/NAVI_SIMPLIFIED_RECONSTRUCTION_20260918.md`
