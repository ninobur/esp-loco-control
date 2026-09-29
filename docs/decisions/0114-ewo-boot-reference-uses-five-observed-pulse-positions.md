# 0114 — EWO boot reference uses five distinct observed pulse positions

Status: Historical and superseded/withdrawn by 0115 on 2026-09-29. This record
preserves the human decision and reason for withdrawal; it is not current
NAVI_EWO authority.

## Decision

Replace the first-10-mm startup rule with the first **five distinct completed
IR pulse positions actually observed by NAVI after physical progression begins**.
David explicitly selected observed positions when asked about report jumps such
as 0 → 2. Do not interpolate skipped counts or invent Hall representatives.

For each position, NAVI calculates the median of its associated native Hall
observations. Each position contributes exactly one representative. The median
of those five representatives becomes the initial active Hall reference.
Storage is bounded. There is no upstream Hall averaging or admission authority.
PWM-zero Hall observations remain non-actionable under 0112.

Five consecutive pulses at the current nominal 9.652-mm pitch represent about
48.3 mm. **Five observed positions need not be consecutive**, so this is not a
fixed 48.3-mm acquisition interval when reports skip counts.

## Implementation boundary and remaining field evidence

The candidate closes each population on the next distinct pulse report. Thus
the fifth representative is finalized when that next report arrives; no timer
or arbitrary Hall sample limit closes it early. This is an implementation
consequence of taking the median of the position's complete observed population,
not a change to the five-position weight rule.

The candidate reports incomplete evidence if a selected position has no usable
Hall population, a collecting frame is invalidated, observations are lost, or
its bounded counters cannot represent the population. It does not synthesize
a reference, silently switch to stationary sampling, or choose later substitute
positions. No automatic retry or new stop policy is established by this record.

The leading-boundary 0–100 / 100–200-mm replacement cycle in 0113 is unchanged.
Its unresolved bin-width/coverage questions remain unresolved. Track behavior,
report-phase effects and boot operation near magnetic fields need controlled
hardware evidence. Neither this record nor a passing compile authorizes flashing.
