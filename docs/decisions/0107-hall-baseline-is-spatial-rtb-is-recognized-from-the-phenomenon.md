# 0107 - The Hall baseline is spatial; Return to Baseline is recognized from the phenomenon

Status: Accepted as principle (2026-09-27, David with Sam). Settles D5 and D10;
**D4 and D8 remain OPEN**. Documentation only.
Decided by: David, after review with Sam. The quotations are from David's
codification instruction of 2026-09-27.

## Decision

**The baseline is spatial information (D5).** "A new operational Hall baseline
requires sampling across physical movement/distance. A quiet stationary Hall value
represents one physical point and does not establish a new spatial baseline."
- At boot, under the operator's known condition that Toby is clear of magnets, NAVI
  may establish an initial stationary reference sufficient to recognize the first
  excursion.
- After movement begins, NAVI can build richer, spatially grounded baseline
  information from samples across ordinary track.
- The required sampling distance is **not** specified.

**Return to Baseline (RTB)** names the Hall signal returning to ordinary-track
behavior. Use it instead of "magnet closure" where that is the phenomenon meant.
Candidate evidence:
- agreement with the established, current or previous baseline;
- historical baseline information for the physical interval;
- return of Hall level toward ordinary-track behavior;
- ordinary-track spread and noise;
- disappearance of systematic slope or trend;
- persistence of baseline-like behavior, rather than a transient zero crossing or
  return-flux lobe;
- subsequent waveform structure;
- IR distance and map geometry as context when available.

Methodology: "observe the real waveform → identify RTB from the phenomenon → determine
mathematically what made it recognizable → implement the general algorithm →
validate across locomotives." Do not start from 80 ms, 71 mm, or any Toby-specific,
Otto-specific or other predetermined answer.

**General model before locomotive-specific calibration (D10).** Do not assume each
locomotive needs unique constants. Build the general model, test it across Toby and
then other locomotives, and add locomotive-specific accommodation only if field
evidence shows a meaningful difference. "This is not a Six Sigma measurement
problem": the model needs enough discrimination for reliable railway navigation.
Fresh Toby raw Hall + IR data is useful validation when convenient, not a
prerequisite.

## Context

X22R re-locked by a fixed timetable:
- 1,064 of 1,067 locks in the 2026-09-26 runs were accepted 710–712 ms after the
  opening;
- the 80 ms SETTLE traces to an Otto time fit for a return-flux shelf that Otto's raw
  data show is a distance;
- the 3000 ms cadence was a motion proxy.

Prior evidence (Otto, 2026-09-16, `../NAVI_BASELINE_TIMING_20260916_C3B93D0B.md`,
`../HALL_WAVEFORM_VS_SPEED.md`):
- the magnet is a spatial object;
- the tail is a 15–18-count return-flux shelf, not a decay;
- a leading lobe precedes the next magnet;
- the magnet's own ≥70 span predicted usable timing better than a fixed time.

This record uses that as prior evidence, not as an answer.

## Open and not decided here

- **D4:** how RTB and the eligible baseline region are recognized. It waits for the
  general RTB model.
- **Baseline model (prerequisite of D8):** is NAVI's baseline an acquired-and-held
  value, or a continuously maintained model of ordinary-track behavior that learns
  whenever NAVI has adequate evidence the sensor is on ordinary track?
- **D8:** whether a POSITION_ADVANCED_SANS_MM opens a baseline opportunity. If
  learning is continuous, it may need no special action. Revisit after the baseline
  model and RTB.

## Consequences

- No new timers or locomotive-specific distances are introduced for RTB.
- **Risk.** Until a general RTB model exists, any implementation must either keep the
  current field-proven locking or wait. Replacing it with an unvalidated recognizer
  risks the stable locks observed on 2026-09-26 (|Δ| ≤ 11 counts at cruise). The
  sequencing is for David to decide at implementation time.

## References

- `../NAVI_DECISION_MODEL.md` §3, §6
- `../NAVI_UNIFIED_AUTHORITY_X22_RECONCILIATION_PLAN_20260926.md` §1.2–1.3, §4
- `../NAVI_BASELINE_TIMING_20260916_C3B93D0B.md`, `../NAVI_BASELINE_DRIFT_20260916_C3B93D0B.md`,
  `../HALL_WAVEFORM_VS_SPEED.md`
- Otto raw capture: on the Pi, `/home/david/NGR/hall_records/xhr_20260916_191904.xhr`
  (not in the repository)
