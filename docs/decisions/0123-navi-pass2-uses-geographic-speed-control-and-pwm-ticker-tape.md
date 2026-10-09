# 0123 — Reconciled under the mandatory NAVI architectural guide

**Current status:** Partially superseded and reconciled October 9, 2026, on
David's explicit documentation-reconciliation instruction.
The original record and simplified-fallback revision are retained below as
history. File name is retained for existing references.

**Further October 9 supersession:** the mandatory primary guide now defines
the ordinary base layer as cruise-only geographic PWM **60**, with all station
and traffic operating modifications supplied by temporary overlays.
Exactly one overlay has PWM authority at a time; geographic collision
prevention is highest priority. The universal STOP overlay is to be proven
before station service, using individually assigned IR-pulse geographic PWM
positions, initial moving creep at **50**, and a final profile toward PWM
**20** at the geographic aim point. Early/late actual stopping is accepted;
completion of a timed STOP requires dwell expiry with applied PWM **zero**
concurrently, independent of exact stopping coordinate. The former station
+1-MM moving timer and base station STOP tiles are superseded. After release,
the normal ramp-up resumes and the next applicable tile provides the
instruction. The primary guide §§4–5, 8, 13–14 governs all conflicts;
older text below is retained as historical context. The detailed
early-stop PWM-zero actuator progression and subordinate overlay priority
require explicit decision before coding.

## Current governing decision

[NAVI_EWO_0_1 — Architectural Inheritance and
Simplification](../NAVI_EWO_0_1_ARCHITECTURAL_NOTE_20261009.md) is the mandatory
primary architectural guide for all future NAVI firmware revisions.
Compatible earlier decisions remain binding. Proposed departures require
David's explicit approval; every review must assess guide compliance and the
Necessity and Simplicity Test.

Required continuous speed homeostasis, adaptive glide control, the 95% handoff,
and rejection of geographic PWM as the initial physical controller are
**superseded**. Use the guide's demonstrated geographic PWM physical-control
precedent and existing actuator ramps. The original ticker-tape requirement was
superseded by `7a31d913` and remains superseded; no recording/playback,
historical PWM memory, missed-pulse prediction, or recovery controller is required.

**The simplified IR-loss fallback survives:**

- At steady speed with valid IR feedback unavailable, hold existing PWM.
- During established deceleration, continue the already-established monotonic
  PWM reduction, with no new feedback correction while IR is unavailable.
- Current geographic requirements retain authority. Recovery restores
  applicable IR evidence under existing validity and continuity rules; it does
  not restart the superseded homeostasis or adaptive glide controller.

Decisions **0116 and 0120 are preserved unchanged**. The fallback provides
actuator continuity only; it cannot advance navigation by Hall alone, manufacture
geographic progress, re-anchor an IR coordinate, or bypass genuine frame loss or
a PWM-zero-movement hold. A motionless PWM-zero dwell preserves localization;
measured PWM-zero movement requires operator verification and declaration.

Geographic tile authority, the universal **DECELERATE → STOP → WAIT → RESTART →
ACCELERATE** mechanism, and the prohibition on procedural entry latches remain
binding. Station target +1.5 MM, preferred +1.0 to +1.7, acceptance 0 through +3,
and five-second base wait follow guide §5. The ordinary clock begins at +1 MM;
departure requires waiting satisfied **and** completed physical stop, rather
than the historical rule that starts every dwell only after stop. The
higher-priority-stop exception in guide §8 survives. Departure targets the
underlying geographic instruction rather than always cruise.

## Remaining questions and evidence boundary

Hall-checkpoint scheduling during IR loss remains unresolved. No checkpoint may
be assumed to supply navigation progress or substitute for coherent IR distance.
Minimal tile-local execution-state representation/reset, exceptional timer
initialization, physical-stop recognition during unavailable IR evidence, and
arbitrary-location braking performance remain open in guide §14. The earlier
PWM-zero/50-ms-no-pulse recognition criterion is a prior candidate criterion,
not an invented loss-specific solution. Field validation is still necessary.

This reconciliation considers guide commit
`db329782d422559dcb886f93db8c1f3e3922a7ab` and fallback commit
`7a31d913ef2218687b488936a998f82655f5d3c6`.
It authorizes no firmware changes, merge, flashing, or field activation.

## Historical Decision 0123 — simplified-fallback revision at 7a31d913

**Historical text follows.** Homeostasis, adaptive glide correction, their
return-on-recovery wording, mandatory speed-controller architecture, and
stop-first dwell initiation below are superseded by the current decision above.
The remaining compatible fallback and navigation constraints survive.
The original ticker-tape proposal is retained in
`b646cab5995124ff84ae2173c0d9912c84357815`; it is historical evidence only.

Status: Accepted (2026-10-08). Documentation only; implementation, merge,
flashing, and field activation are not authorized.
Decided by: David, through the ten-question Pass 2 design reconciliation.

## Decision

Pass 2 executes the current geographic operating requirement with one
physical-speed controller. For the initial Four-Station Local in either
direction, it uses the settled Station 0 + 1.5-MM terminal, −10 to −5-MM
45→20 glide, −5 to 0-MM 20-pKPH region, 0 to +1.5-MM final glide, and
five-second dwell. The boundaries remain configurable geography.

Steady-speed homeostasis uses the median of five fresh IR speed observations,
±5% target tolerance, one-PWM-count corrections, and the established
five-beat correction moratorium. Glide paths use constant physical
deceleration. Their ±5% trajectory tolerance requires three consecutive
outside observations before a bounded ramp change; PWM never increases during
deceleration. Glides start at actual PWM. Manual's 150-ms/count acceleration
ramp runs until 95% of target before homeostasis takes over.

A station stop is complete at actual PWM zero plus 50 ms without a new IR
movement pulse. Decision 0120 remains fully binding for unexpected
PWM-zero movement.

The earlier geographic PWM ticker-tape proposal is superseded. NAVI records no
tape and keeps no historical PWM memory. It has no tape playback, missed-pulse
prediction subsystem, elapsed-time alignment, or added recovery controller.

When valid IR feedback is unavailable at steady speed, NAVI holds the existing
PWM and suspends feedback corrections. The current geographic instruction
remains in force. When valid IR returns, the existing homeostasis corrections
resume.

During an established deceleration, NAVI continues the existing monotonic PWM
reduction. It does not add a feedback correction while IR is unavailable. Hall
geographic checkpoints may be used only where compatible with Decision 0116;
they may not advance NAVI's position, re-anchor the IR/MM relationship,
manufacture geographic progress, or create Hall-only navigation or control
authority. When valid IR returns, the existing glide-path correction resumes
through its established ramp.

## Context

The October 8 reconciliation settled the numerical and behavioral questions
needed to make Document C's physical-speed architecture concrete, while
preserving NAVI as the sole navigation authority and current geography as the
source of operating requirements. Subsequent agreement removed the ticker-tape
fallback: continuity of the already-established PWM behavior is sufficient
without recording, playback, or prediction machinery. No fallback may replace
the stop, speed, direction, or overlay instruction that NAVI is executing.

## Alternatives considered

- Fixed geographic PWM as the normal controller was rejected: physical speed,
  not a presumed PWM, is the operating objective.
- Proportional PWM chasing and rapid repeated correction were rejected in favor
  of sparse, evidence-based homeostasis and bounded ramp adjustment.
- Recorded PWM playback, historical PWM memory, and missed-pulse prediction
  were rejected: they add actuator-history machinery without changing the
  applicable geographic instruction.
- Hall-only navigation or control recovery was rejected: Decision 0116
  continues to require coherent IR distance for navigation advancement.

## Consequences

Implementation must preserve Decisions 0102, 0116, 0120, and 0121: NAVI owns
navigation; valid IR distance remains required for its normal navigation
relationship; PWM-zero movement requires operator intervention; and historical
station procedure cannot retain authority. First-lap/no-record response,
no-IR tape timing/alignment and indexing, missed-pulse calculation, and
return-to-live blending are not implementation questions: the ticker-tape
architecture that required them has been removed.

**Unresolved Hall-checkpoint semantics:** the agreement permits Hall geographic
checkpoints during deceleration only where compatible with Decision 0116. It
does not decide which, if any, checkpoint can help continue the
already-established monotonic PWM reduction without becoming Hall-only
navigation advancement or an IR-coordinate substitute. That boundary must be
explicitly decided before implementation; no exception is implied here.

Telemetry may record IR loss/recovery and relevant current values without a
new mode-history subsystem or repetitive status traffic. Field testing remains
the evidence for physical stopping and this fallback behavior. The stationless
firmware baseline is unchanged.

## References

- [Document C, §35](../NAVI_POSITION_OVERLAY_OPERATING_ARCHITECTURE_20261005.md)
- [October 8 architectural decision record](../NAVI_DOCUMENT_C_ARCHITECTURAL_DECISIONS_20261008.md)
- [0120 — PWM-zero movement](0120-pwm-zero-movement-requires-operator-position-verification-and-declaration.md)
- [0121 — current geography and overlay authority](0121-current-position-direction-and-overlay-determine-navi-operating-authority.md)
