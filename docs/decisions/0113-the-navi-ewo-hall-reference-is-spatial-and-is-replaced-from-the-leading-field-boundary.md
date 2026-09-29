# 0113 - The NAVI_EWO Hall reference is spatial and is replaced from the leading field boundary

Status: Accepted as principle (recorded 2026-09-29; settled by David with Sam).
Refines 0107 for NAVI_EWO. Documentation only. No implementation, flashing or
deployment is authorized by this record.
Decided by: David, after review with Sam.

**Startup clause superseded by [0114](0114-ewo-boot-reference-uses-five-observed-pulse-positions.md)
on 2026-09-29:** five distinct observed pulse positions with NAVI-owned per-position
medians replace first-10-mm collection. The historical text below is retained;
the post-confirmation spatial cycle is unchanged.

## Decision

**The reference belongs to NAVI.** Progression is based on **physical travel, not
elapsed stationary time.**

**Startup.** NAVI establishes the initial Hall reference using the agreed
**first-10-mm movement method:** it collects native Hall observations over the
first 10 mm of measured travel, and the median becomes the initial reference.

**After target confirmation,** the observed leading magnetic-field boundary (0110)
is the origin:

```text
leading field boundary
        |
0-100 mm       clearance
100-200 mm     Hall-reference collection
at 200 mm      NAVI calculates the replacement reference
```

A failed target confirmation does not start this cycle. No new timer is
introduced.

## Context

0107 ruled the baseline spatial information: a quiet stationary value is one
physical point. The 2026-09-27 spatial-reference field test supported that with
a capture of 622,320 native Hall samples: a robust median over one representative
per distance bin held 1824.5-1825 counts through a 22,399-sample stationary
station interval where time- and PWM-weighted medians moved to about 2003, and
stayed within a few counts elsewhere. It supported the principle of spatial
weighting and explicitly **did not select a bin width.**

## Alternatives considered

- **Time-weighted or PWM > 0 medians.** Not chosen: demonstrated failure of up to
  179 counts against the moving comparator at a station stop; PWM > 0 did not
  help because the ramp context included positive PWM at a stationary wheel.
- **A stationary boot reference only (0107 permitted it).** Not chosen: the
  agreed method uses movement.
- **RTB recognized from waveform structure (0107's candidate list).** Not adopted
  for NAVI_EWO: waveform closure has no independent authority (0110).

## Consequences

- The reference cannot be biased by how long the locomotive stood still.
- **Risk: the reference is not updated between confirmations.** It is replaced
  only after a confirmed target, so a run of Missed Magnets or degraded-mode
  operation keeps the last reference.
- **Risk: the 0-100 mm clearance and 100-200 mm collection distances are as
  agreed but are not derived from the field test.** The test evidence does not
  set them.
- Bin width, minimum coverage and the per-location representative rule are **not**
  decided here. The implementation documentation describes the candidate's
  current representative rule; that is implementation description, not
  canonical.

## Relationship to earlier decisions

- **0107:** *refined.* Spatial baseline retained. The boot stationary-reference
  allowance is replaced by the first-10-mm movement method for NAVI_EWO. 0107's
  RTB-from-the-phenomenon methodology is not used for navigation authority in
  NAVI_EWO. Its D4 and D8 are resolved or refined in part; see below.
- **0104 (D6):** consistent; the 3 s cadence gate is not carried.

## Resolution of 0107 D4 and D8, to the extent this architecture answers them

**D4** asked how RTB and the eligible baseline region are recognized.
- *Resolved for NAVI_EWO:* the eligible baseline region is defined by physical
  travel from the observed leading field boundary: 0-100 mm clearance, with no
  reference collection; 100-200 mm collection. NAVI does not recognize RTB from
  the waveform to find it (0110: no closure or morphology authority).
- *Not resolved:* whether the 100 mm clearance is physically sufficient across
  locomotives, tracks and speeds. It is an agreed geometry, not a measured one.
  Whether any other NAVI lineage needs a general RTB recognizer is also not
  answered here.

**D8** asked whether a position advance without a confirmed MM opens a baseline
opportunity, with the prerequisite question of whether the baseline is
acquired-and-held or continuously maintained.
- *Resolved for NAVI_EWO:* the reference is held and replaced, not continuously
  maintained, and the origin of a replacement is an *observed* leading boundary.
  A Missed Magnet advance observes no boundary, so it provides no origin and opens
  no reference cycle. Only target confirmation starts one.
- *Not resolved:* the handling of the +/-15% target origin after a Missed Magnet
  (0111 open point), and how long the last reference may reasonably be held
  across successive Missed Magnets or degraded operation.

## References

- `0107`, `0110`, `0111`
- `../NAVI_EWO_GOVERNING_DOCUMENTS.md`
- `../../field-records/analysis/20260927_navi_eyes_wide_open_spatial_reference_report.md`
- `../../firmware/programs/NAVI_EYES_WIDE_OPEN/README.md`
