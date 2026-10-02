# 0117 — First target after declaration accepts any onset within (0, interval + 15%]

Status: Accepted rule, 2026-10-02; implementation NOT field accepted.
Decided by: David, in the scoped first-target change instruction.

## Decision

For the first target after an operator declaration, use applicable IR travel
`0 < traveled <= expectedCumulativeUm + tolerance`, where tolerance remains
15% of the target's mapped interval. Require target support to be observed
absent and subsequently present in full NAVI-owned median-of-five windows
from actionable Hall samples acquired after declaration. An initially supported
field must drop and return; initialization/reset is not an observed absence.

Clear the exception on confirmation, a recorded Missed Magnet, or reversal.
All later targets retain the existing lower and upper distance bounds. Missing
the first target still uses the existing upper-bound test and physical-origin
policy. This narrowly qualifies 0116's ordinary lower bound after declaration;
IR authority, target-only judgment, polarity, threshold, direction, landmark
selection, spatial reference and command/record formats are unchanged.

## Evidence and consequence

David reports Otto's CCW run `9950011_20261002_120424.log`: declaring interval
044-045 selected MM45 while the locomotive was about 194 mm into the interval.
Correct-polarity MM44 support after 106 mm failed the 300 ±45 mm window,
followed by 24 misses (44 through 21) before MM20 confirmed. A declaration
identifies the interval but does not establish exact position at its boundary.
The wider first window permits that uncertainty; the observed-onset condition
guards against accepting a field already present when declaring. This keeps
judgment in NAVI and uses measured distance, consistent with design principles
1, 3 and 9; no departure from those principles is intended.

Known limit left unchanged: reversal before first confirmation still computes
position from the assumed declaration origin. No missed-origin repair, timer,
alternative-position search or additional policy is authorized here.

Rollback: `d0185be25ec51f9ba6d58567458a59d3f1c289ca`, from
`origin/codex/ewo-ir-authoritative`. Host regressions reconstruct the reported
case; they are not a full field-log replay or field acceptance. David decides
when to flash and runs one CW and one CCW lap with NSR1 recording enabled.
