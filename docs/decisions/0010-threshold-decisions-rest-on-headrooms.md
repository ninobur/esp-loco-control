# 0010 — Threshold decisions rest on headrooms, not crossing margins

Status: Accepted  (2026-08-06, CODEX finding; recorded same day)

## Decision
Which threshold is binding is judged from headrooms: `rh` = pulse peak
minus the rising threshold, `fh` = falling threshold minus the preceding
gap's trough, both against thresholds in force at the rise. Crossing
margins (`rm`/`fm`) remain published as edge-slew measurements but decide
nothing. Invalid `fh` samples (no observed prior gap) are flagged and
excluded from statistics rather than encoded as zero.

## Context
A crossing margin exists only because the edge crossed: every emitted
pulse necessarily has positive `rm`, and a spoke that never reaches the
threshold produces no sample at all. Survivor medians look healthy
precisely while weak spokes vanish — the instrument would have reassured
exactly when it should have alarmed.

## Alternatives considered
- Comparing rm/fm medians (the original Change 4) — rejected as the
  decision basis: survivor-biased by construction.
- Inferring misses from interval ratios — rejected as primary: hours-later
  inference, and what the phase breakdown exists to replace.

## Consequences
STATS and the per-phase breakdown report `rh`/`fh`. A weakening spoke
shows a shrinking `rh` before it disappears. General rule for this
project's instruments: a statistic conditioned on detection cannot judge
detection.

## References
Commits `e788377`, `3a54b90` (fh validity); `IR_DIAG_DAYLIGHT_PREP.md`
addendum.
