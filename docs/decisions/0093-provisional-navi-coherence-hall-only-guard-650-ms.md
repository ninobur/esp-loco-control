# 0093 — Provisional NAVI_COHERENCE Hall-only guard: 650 ms, open-to-open

**Date:** 2026-09-22
**Status:** **Accepted (provisional), 2026-09-22**
**Decided by:** operator
**Scope:** NAVI_COHERENCE only (0.4 / 0.5 test-program lineage). Does not
change 0081's 500 ms value for the QUORUM / NAVI_ONE production lineage.
**Not yet implemented:** `MIN_MARKER_MS` in `Navigator.h` remains `500` in
committed source as of this record. This decision sets the target value for
a future firmware change, not a change made today.

## Decision

For NAVI_COHERENCE's Hall-only fallback guard (the path taken when IR
movement evidence is not usable for a judgment — `Navigator.h`,
`MIN_MARKER_MS`), the operator adopts **650 ms, measured open-to-open from
the previous accepted detection** as the provisional working value,
replacing the un-re-examined carry-over of 0081's 500 ms into this
architecture's different timing reference. No PWM-dependent scaling is
adopted at this time.

This is provisional, not final. The operator's own framing: the evidence is
adequate to set a considered value now, and real problems the analysis
didn't foresee will make themselves known in the field rather than needing
to be pre-empted by further desk analysis today.

## Context

`docs/NAVI_MM_HALL_GUARD_TIMING_EVIDENCE_20260922.md` (same date) is the
evidence this decision rests on. Summary of what it found:

- NAVI_COHERENCE has no closure concept; `MIN_MARKER_MS` is already
  measured open-to-open in the running code, unlike 0081's close-to-open
  definition. 0081's 500 ms was carried into this different reference frame
  without being re-validated there — this record does that validation.
- `gap_ms` in NAVI_COHERENCE's own logs is a dead field (declared, never
  assigned, in both 0.4 and 0.5 source). Every timing number behind this
  decision was computed independently from `opened_ms`.
- Today's Toby data (2026-09-22, NAVI_COHERENCE 0.4/0.5, 958 genuine
  consecutive-MM advances): the only 4 confirmed Hall-only false events all
  land at exactly 402 ms, independent of PWM (29–42 tested) — consistent
  with the Hall detector's 400 ms identification window plus
  NAVI_COHERENCE running that detector's refractory period at 0
  (`HallObserver.h`; the shared library it wraps defaults refractory to
  645 ms). The closest any genuine advance ever came was 894 ms (PWM 100,
  n=239). 650 ms sits with margin on both sides of that gap: 248 ms above
  the false cluster, 244 ms below the nearest genuine case.
- The proposed PWM-scaled 700→1000 ms ramp, a fixed 700 ms, and the
  current 500 ms all scored identically against this same event stream
  (342 genuine admitted / 0 suppressed / 0 false admitted / 4 false
  rejected) — today's evidence does not favour a PWM-dependent rule over a
  flat one.
- One caveat carried forward, not resolved: Otto's most recent comparable
  data is 10 days old, under a different (closure-anchored) architecture,
  and is not pooled with Toby's numbers. One reconstructed false-event
  timing from that data (1007 ms) would defeat any flat guard below it if
  taken at face value; the evidence report judges it more likely a
  reconstruction artefact (it implies a physically-implausible speed for
  its PWM) than a genuine counterexample, but this was not independently
  confirmed against a waveform capture.

## Alternatives considered

- **Proposed 700→1000 ms PWM-scaled ramp** (the idea the analysis was
  commissioned to evaluate). Not adopted now: retrospectively tied with
  650 ms flat on every event in today's data, and the false-event
  population it was designed to guard against (a fixed-offset sensor
  artefact, §2.4/§4.2 of the evidence report) does not scale with PWM, so
  the added complexity is not shown to buy anything.
- **Leave at 500 ms** (0081's value, unexamined for this reference frame).
  Not adopted: still inside the empirically safe range, but closer to the
  false cluster (98 ms margin vs. 650's 248 ms) for no demonstrated
  benefit.
- **Wait for more data before choosing any value.** Not adopted: the
  operator judged today's evidence adequate for a considered provisional
  value.

## Consequences

- `Navigator.h`'s `MIN_MARKER_MS` is **not** changed by this record. Moving
  NAVI_COHERENCE to 650 ms is a separate, future firmware change.
- This value applies to NAVI_COHERENCE only. QUORUM / NAVI_ONE production
  firmware stays at 0081's 500 ms; nothing here reopens that.
- Field evidence at low PWM (≤40) behind this number is thin (2–5
  Hall-only samples) — the next NAVI_COHERENCE field session is the
  natural place to watch for a problem this analysis couldn't see.
- If a genuine magnet is ever suppressed, or a false event ever admitted,
  at 650 ms, that field observation supersedes this record on its own
  terms.

## References

- `docs/NAVI_MM_HALL_GUARD_TIMING_EVIDENCE_20260922.md`
- `tools/mm_guard_timing_toby.py`, `tools/mm_guard_timing_otto_x15.py`
- `firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_4/Navigator.h`,
  `firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_5_AUTO_ENABLED/Navigator.h`
- `docs/decisions/0081-the-rebound-guard-is-500-ms.md`
- `docs/decisions/0087-hall-navigation-is-the-opening-and-a-645-ms-guard.md`
  (related but separate: a different, never-ratified derivation, for a
  different, closure-removed but not-NAVI_COHERENCE test build)
