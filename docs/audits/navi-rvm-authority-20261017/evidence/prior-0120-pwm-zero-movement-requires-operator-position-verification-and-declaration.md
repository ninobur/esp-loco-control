# 0120 — PWM-zero movement requires operator position verification and declaration

Status: Accepted by David and Sam, 2026-10-02; supersedes 0119's automatic
coordinate-reacquisition rule. Candidate NOT field accepted. This record does
not authorize flashing, deployment or merge.

## Decision

**PWM=0 with zero IR displacement preserves localization and requires no
intervention.** Normal station dwell retains MM, interval, target, direction,
IR/MM coordinate and Hall reference.

**PWM=0 with measured IR displacement requires operator intervention.** Record
the wheel movement as factual, never signed route travel. Hold navigation and
withdraw AUTO. Preserve the last context for diagnosis, but do not advance MM,
rule Missed Magnet, seek alternative positions, use Hall-only recovery or
automatically reacquire the coordinate. No later same-polarity magnet can
substitute for the previous target while this hold is latched.

Report `PWM_ZERO_MOVEMENT_REDECLARE`: movement occurred while PWM was zero;
verify/reposition the locomotive and declare position before navigation resumes.
This is not an IR instrument fault. Only an explicit valid operator declaration
clears the movement hold; powered travel, elapsed time and Hall cannot clear it.
Manual positioning remains available; AUTO admission and GO remain blocked.

## Why / alternatives rejected

Unknown starting coordinate and repeated polarities make automatic reacquisition
ambiguous. For example, CW 001→002 is 340 mm and 002→003 is 330 mm; both 002 and
003 are South. An onset at 335 mm can be 002, or 003 after missing 002 at 5 mm.
Even a full-interval bound cannot resolve that geometry. The first powered IR
report can also follow passage of the expected magnet. Neither a timer nor an
invented ambiguity cutoff establishes its identity. Automatic reacquisition is
withdrawn, not replaced with another recovery mechanism.

David's 28 PWM_ZERO_IR_DISPLACEMENT events on 2026-10-02 recorded lifting,
turning and repositioning Otto. David then established 045–046 and declared it;
the subsequent CW run proceeded normally. That is the intended operator model.
NAVI need not infer whether the cause was a nudge, lift, spin or repositioning.

## Scope and supersession

0119 is historical. Its distinction between factual IR movement and instrument
failure survives, but its no-redeclaration/automatic-localization policy does
not. This also governs conflicting recovery language in 0112 and its 0119
annotation on 0116. Genuine frame/source/scale/order failure remains distinct;
the 650-ms Hall-only fallback stays withdrawn. 0118 startup, normal ±15%
confirmation, normal Missed Magnet, Hall reference, reversal, IR source and
health policies, and station behavior are unchanged. AUTO changes only through
the required movement-hold withdrawal/admission block.

This applies direct evidence, NAVI-owned judgment and operator architectural
authority; no design principle is departed from. Human verification itself is
not machine-verifiable: declaration is the operator's assertion that it is done.

References: [governing directory](../NAVI_EWO_GOVERNING_DOCUMENTS.md), 0112,
0116, 0118, 0119, and
[implementation change log](../NAVI_EWO_PWM_ZERO_DECLARATION_20261002.md).
