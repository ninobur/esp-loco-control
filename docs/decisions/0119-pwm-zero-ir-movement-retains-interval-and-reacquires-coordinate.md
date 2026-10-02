# 0119 — PWM-zero IR movement retains the interval; only its IR coordinate is lost

Status: Accepted rule, 2026-10-02, by David's scoped correction instruction.
Refines 0112 and supersedes 0116's PWM-zero mandatory-redeclaration rule.
Implementation **NOT field accepted**. No flash, deployment or merge authorized.

## Decision and reason

IR displacement at motive PWM=0 is factual wheel movement, **not signed route
travel**. It cannot advance MM, change the interval/adjacent target/direction,
or cause Missed Magnet. Retain the established MM/context and active Hall
reference. Invalidate only the within-interval IR coordinate/distance-to-target.
Report `INTERVAL_KNOWN_IR_POSITION_UNKNOWN`, separately from genuine IR
frame/source/scale/order failure. Zero-displacement station dwell preserves
the existing coordinate.

After powered movement resumes, a new local IR movement origin plus applicable
positive IR travel and a fresh appropriate target-polarity Hall onset can locate
the known landmark without redeclaration. Like 0118 startup, this unlocalized
condition has no distance window or Missed Magnet. Confirmation establishes the
physical landmark origin; normal ±15% windows and miss authority then resume.
Median-of-five, ±70, direction, IR applicability and observed absent→present
support are retained. Neither Hall alone nor elapsed time is a substitute.

The 28 PWM_ZERO_IR_DISPLACEMENT events in David's 2026-10-02 opposite-direction
session reflected lifting/turning/moving Otto to 045–046. David redeclared
because he knew he changed intervals, not because IR could infer that change.
Handling within the retained interval needs no mandatory redeclaration.

## Supersession and limits

0112's broad “map relationship” means **coordinate**, not interval identity.
Its suggestion that PWM-zero movement could become signed route displacement
does not govern. 0116's mandatory redeclaration survives for genuine frame
failure, not PWM-zero displacement alone. Its withdrawal of the 650-ms/Hall-only
fallback remains absolute. Operator redeclaration still overrides the context.
0118 startup, Hall reference architecture, normal miss/re-anchor policy, IR
source policy, stations and AUTO are not changed.

This applies the design principles of direct physical evidence, NAVI-owned
judgment and separating observation from interpretation; no principle is departed from.
Implementation boundaries and review limitations—including the first powered
report as origin, unchanged reversal math and a missed reacquisition magnet—are
recorded in [the candidate change log](../NAVI_EWO_PWM_ZERO_LOCALIZATION_20261002.md).
An unknown coordinate is not evidence of another interval; silently inventing
one or requiring redeclaration merely because a wheel turned are rejected.

References: 0106, 0108, 0112, 0116, 0118;
[governing directory](../NAVI_EWO_GOVERNING_DOCUMENTS.md).
