> Current scoped correction: [IR_VALIDITY_CORRECTION.md](IR_VALIDITY_CORRECTION.md).
> Stale Type-6 authority and timing-only withdrawal are corrected; the full
> +/-15% window now qualifies encounters. Genuine physical missed-pulse
> discrimination remains unresolved. **Not ready for field testing.**
> The original implementation/verification snapshot below describes the prior
> candidate; its timing-authority claims are superseded by the correction report.

# NAVI_EWO_0_3 — implementation and verification status

2026-10-11. Complete operational sketch implemented and ESP32 compiled. This is
an **independent-review candidate, not field accepted**. No merge, flash, Pi
access, deployment, or live-service change was performed.

Starting branch: `sam/navi-ewo-0-3-initial-restoration`, commit `60dd1bc`.
Successor: `codex/navi-ewo-0-3-complete`.
Operational foundation: `85a2cf2a16d262abdf7258c60f60508b823913ee`,
`NAVI_EWO_0_1_MM045_STOP`. The defective 0.2 candidate was not used.
The October 11 assignment supplies implementation authority and supersedes the
older target-only/mandatory-IR rules for this isolated candidate. Existing
numbered governing records have not been edited.

## Implemented

- `NAVI_EWO_0_3.ino` contains the complete 0.1 operating shell plus the scoped
  navigation integration. Hardware acquisition takes one native ADC conversion
  per 1-ms FreeRTOS tick, retaining the five-sample legacy recording batch.
- Detection uses raw departure >=70 counts, two consecutive native samples,
  same polarity, consecutive serials, and 0.5–1.5-ms timestamp separation around
  the intended 1-ms cadence. Opening polarity, serial, time, detection time,
  and opening IR association remain separate from geographic confirmation.
- A continuous-field latch prevents repeated identity from one sustained field.
  With credible Type-6 timing and a usable opening coordinate, duplicate exclusion
  uses the existing 85% lower edge of the mapped interval. Otherwise the retained
  650-ms detection-to-detection fallback applies. Reversal preserves the latch.
- Every qualified encounter enters a bounded 12-entry DNA ledger. Ordinary
  confirmations do not clear it. Map polarity and consecutive assigned journey
  identities are checked continuously, including direction changes.
- A qualified expected-polarity encounter identifies the next sequential MM.
  Type-5 mathematical coherence alone is not evidence of complete pulse counting.
  No IR-only `evaluateMissing()` progression remains in 0.3.
- An opposite-polarity encounter is retained. The reported location becomes
  unreliable while identity is unresolved. Retrospective judgment can distinguish
  a leading extra encounter from a leading real encounter with contradictory
  polarity after ten subsequent mapped poles. Both interpretations walk the
  incumbent journey; there is no map-wide search, backward correction, or
  invention of unobserved geographic markers. Earlier contradictory evidence
  remains in the ledger.
- Type-6 is validated by the unchanged receiver before NAVI consumes it. Received
  PWM/direction are captured at ingress, rather than read after a queue delay.
  Source/boot must agree with Type-5. Gaps, invalid intervals, ordering faults,
  and source changes remove timing observability; none means an inferred physical
  pulse loss or an added distance count.
- Timing uses a five-interval population and unchanged nonzero received PWM.
  An interval above 1.5 times its median is a timing question; a following interval
  within the observed pre-event range can bracket a 2–8-times interval consistent
  with uncounted cycles. One such bracket is sufficient to question the span.
  Range comparison includes one 1-ms transmitter sampling tick of resolution.
  **Neither finding proves physical pulse loss.** NAVI conservatively withdraws
  IR's magnet veto on a timing question. It never replaces pulses or recalibrates.
- A verified Hall boundary establishes a fresh geographic/IR origin. Boundary
  lookup uses the latest valid same-boot snapshot received at or before the Hall
  opening, from a bounded 64-snapshot history. Later decision-time packets cannot
  move that anchor. A subsequent credible pulse measurement can restore the IR
  veto after re-anchoring. The cumulative counter is preserved.
- First target after declaration retains an observed post-declaration absence,
  a subsequent qualifying onset, and positive applicable IR travel when present;
  it has no mapped distance window. Without usable IR, the authorized Hall/
  sequential fallback applies. Stale queued observations cannot qualify it.
- Reversals retain DNA, current MM, and direction history. A reversal within the
  ongoing field seeks the adjacent marker; a return after leaving it can identify
  the same MM without counting forward progress. Stale/missing distance does not
  manufacture a frame-loss deadlock. Genuine Type-5 boot/source/frame loss and
  PWM-zero displacement retain the existing operator-declaration rules.

## STOP and operating preservation

`NaviStopOverlay.h`, `EwoStationStop.h`, the actuator, throttle/brake handling,
E-stop, battery logic, MQTT commands, Type-5 parsing, IR pitch **9,652 um/pulse**,
STOP target, grade PWM, final ramp, five-second applied-zero dwell, restart, and
acceleration are preserved from the foundation. Header equality and actual
operating-function comparisons are executable regression checks.

Two shell integration changes are explicit:

1. A verified retrospective geographic revision resets the overlay's execution
   latch so an old final-braking instruction cannot outlive corrected geography.
2. Pending DNA judgment reports `position_reliable=0` without adding an AUTO
   withdrawal threshold. An already-running operation uses the existing overlay's
   unavailable-geography behavior, retaining its current instruction and timed
   execution. Existing position-based AUTO/GO admission still refuses an unknown
   position; Manual and E-stop authority remain unchanged.

Existing MQTT topics and wire layouts remain unchanged. Navigation evidence adds
opening polarity/timestamps/association-validity fields and event IDs 17–22.
The NSR consumption ledger adds numeric input kind 6 for Type-6 consumption;
record structure/version is unchanged. The existing `median5` field remains a
NAVI-computed diagnostic, not the new detector's authority.

## Verification

Run `sh firmware/programs/NAVI_EWO_0_3/run_tests.sh` from a repository checkout.
See [VERIFICATION.md](VERIFICATION.md) for exact results, migrations, replay
counts, build command, exclusions, and independent source inspection.

## Limits requiring attention in independent review

- The real logs cannot distinguish wheel slowdown/slip/binding from an uncounted
  optical cycle solely from completion timing. Even the healthy October 9 control
  flags timing questions. Review the conservative quality-withdrawal rule before
  field use; do not describe its missing-compatible finding as proven pulse loss.
- Received PWM is not transmitter-time PWM. Historical replay has only the last
  one-second reported PWM and missing logged events; it cannot certify the complete
  receiver's real-time credibility decisions.
- Boundary association is to an observed, quantized Type-5 snapshot, with its
  sampling/delivery uncertainty. There is no invented sub-packet distance. If the
  opening precedes every retained/fresh snapshot, the MM can be identified but a
  fine IR coordinate is unavailable; a later packet is never substituted.
- Retrospective resolution is deliberately narrow. Multiple unobserved magnets,
  multiple contradictory excursions, or an unresolved episode exceeding the
  12-entry ledger are not automatically repaired. Their observations and unreliable
  status remain visible; operator verification/declaration may be necessary.
- October 9–10 MQTT logs omit the complete two-polarity raw Hall observations.
  They support pulse-timing replay and specific deterministic reconstructions,
  **not a full magnetic DNA replay or proof of corrected field identity**.
- Host checks establish software rules, not physical railway performance. An
  independent reviewer and separately authorized field validation remain necessary.
