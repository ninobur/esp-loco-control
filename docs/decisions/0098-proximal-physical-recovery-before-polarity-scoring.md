# 0098 - Physical proximal recovery precedes polarity scoring

Status: Accepted (2026-09-23, David's pasted twelve-point recovery instructions)

Supersedes the whole-route sequence-recovery portion of0090. AUTO enablement,
station behavior and the +/-15% nominal window are not redefined here.

## Decision

Retain the incumbent world view. Consider only physically plausible candidates
within an absolute +/-10-MM outer boundary. Score actual observations in a
rolling ten-position history; missed slots are UNKNOWN, never invented polarity.
One wrong observation cannot relocate NAVI. With accumulated contradictions,
only a unique strictly better candidate corrects position; no extra score margin.
Keep observations after correction. IR supplies travel, not identity or decisions.

## Evidence

At14:36:36 today the old matcher moved MM65->166, offset-70, during AUTO,
despite8/10 agreement with incumbent history. See the recorded failure timeline.

## Engineering Choices for Review

PROXIMAL_R1 treats startup declaration as locally provisional. A mature entirely
consistent observed history, or an accepted correction, establishes a NAVI
reference. Established recovery needs compatible same-epoch IR travel from that
fixed reference; absent it, hold. Only later history assignments may shift.
Startup candidates are checked against same-epoch measured segment spacing.
Missing IR never expands the outer boundary. A full new-direction history is
required after reversal, without discarding retained raw observations.

These reference-establishment details are implementation choices, not additional
operator rulings. The existing15% tolerance is not certified accuracy. No PWM
calibration is assumed. Exactly ten map-position slots approximate the requested
3m history; UNKNOWNs are recorded at later supported Hall advances, not a new
background dead-reckoning subsystem. Remaining Twenty Questions work is separate.

## Consequences

The build now consumes health-layer measurements for NAVI recovery and is no
longer observation-only. New diagnostics expose candidate exclusions and scoring.
No flashed firmware, TX/RX, Pi controller, or motor interface changes are made
as part of preparing this revision. Independent review and a manual field check
must precede treating it as an operating baseline.
