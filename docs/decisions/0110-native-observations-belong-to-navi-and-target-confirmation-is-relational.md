# 0110 - Native observations belong to NAVI; target confirmation is relational

Status: Accepted as principle (recorded 2026-09-29; settled by David with Sam).
Documentation only. No implementation, flashing or deployment is authorized by
this record.
Decided by: David, after review with Sam.

## Decision

**Native observations.** One ADC conversion is one Hall observation. Individual
ADC observations reach NAVI in order, with provenance. No upstream Hall median or
other evidence-selection calculation determines what NAVI gets to see. NAVI may
calculate robust statistics from observations it possesses. That is fundamentally
different from upstream preprocessing, which decides what NAVI may see.

Likewise, factual IR observations and their health and provenance go to NAVI.
NAVI determines applicability (0111).

**Hall target evidence.** NAVI already knows its target polarity, so it does not
determine an abstract unknown-magnet polarity.

- The Hall statistic is a **NAVI-owned rolling median of five native Hall
  observations.**
- Hall **supports the target** when that median departs from the active Hall
  reference by at least **70 counts** in the known target-polarity direction.
- The earliest expected-direction raw observation of at least 70 counts within the
  qualifying five-sample population is retained as the **observed leading
  magnetic-field boundary landmark.**
- Waveform morphology, lobes, tails, closure, Gaussian shape, duration, slope and
  abstract magnet classification have **no independent navigation authority.**

**Target confirmation is relational.** Normal confirmation requires agreement
among independent evidence:

- NAVI-owned Hall support;
- expected target polarity;
- applicable IR physical-distance evidence within the established +/-15% mapped
  target interval;
- sequence, direction and context.

No individual source substitutes for the others during normal Hall+IR operation.
The physical magnet is not NAVI's observed landmark: NAVI observes the leading
boundary of the magnetic field.

## Context

0102 and 0105 established that NAVI must receive the data and that unlike
evidence is relational, not additive. The 2026-09-26 X22R audit found upstream
suppression and quiet/cadence/readiness gating acting before NAVI could judge
(N1-N8). The 2026-09-28 historical impostor replay found that expected opening
polarity and sequence context rejected several known impostors, and that the
shape, duration and return-flux gates of older builds had falsely rejected
genuine magnets, while stating plainly that the surviving cases could only be
adjudicated with synchronized IR distance.

## Alternatives considered

- **Upstream Hall median delivered as the single observation.** Not chosen: it
  makes the evidence-selection decision before NAVI has context (0102).
- **Two-consecutive-sample persistence (X22 heritage; the replay's "70x2").** Not
  chosen for the canonical statistic: the target Hall statistic is the
  NAVI-owned median of five. The replay report is evidence about the earlier
  two-sample characteristic and is not evidence for the median-of-five statistic.
- **Abstract observed-polarity classification, then map lookup.** Not chosen:
  NAVI already knows the target polarity.
- **Morphology as a supporting or tie-breaking authority.** Not chosen: shape has
  caused genuine false rejections and re-read confusion in the historical record.

## Consequences

- Hall support alone never confirms a target. IR distance alone never confirms a
  target.
- The raw threshold sample, not the later median decision point, is the physical
  boundary landmark.
- **Risk: a median of five delays the decision** relative to the boundary sample.
  That is why the boundary is defined from the raw sample and not from the sample
  at which the median crosses.
- **Risk: the historical evidence does not test the whole model.** No synchronized
  IR distance exists for the historical impostor rows. Three moving candidates
  (X21 Grillers departure, QUORUM 243211, QUORUM 263774) survive all the gates that
  could be applied and remain to be adjudicated by track evidence, not by adding a
  rule (AGENTS.md section 8).

## Relationship to earlier decisions

- **0102:** its preserved X22 concepts (70-count departure, two-sample persistence,
  opening polarity) are prior art; the 70-count departure is retained here as a
  NAVI-owned median criterion. Two-sample persistence is not carried.
- **0105:** applied. The +/-15% interval constrains one known target; it is not a
  vote, and it is not a search over the map.
- **0107:** waveform "return to baseline" recognition is not used by NAVI_EWO for
  navigation authority (see 0113).

## References

- `0102`, `0105`, `0109`, `0111`, `0113`
- `../NAVI_EWO_GOVERNING_DOCUMENTS.md`
- `../../field-records/analysis/20260928_navi_eyes_wide_open_historical_impostor_challenge_report.md`
- `../../firmware/programs/NAVI_EYES_WIDE_OPEN/README.md`
