# NAVI Power of Normal — architecture and decision record

This folder preserves the architectural documents reconstructed from David's
discussion and the associated design and implementation audits. The source
documents are preserved unchanged as historical snapshots.

## Reading order

1. [Original decision record](NAVI_ORIGINAL_DECISION_RECORD_20261024.md)
2. [Twenty original agreements](NAVI_TWENTY_ORIGINAL_AGREEMENTS_20261024.md)
3. [Governing design specification](NAVI_POWER_OF_NORMAL_GOVERNING_DESIGN_SPEC_20261024.md)
4. [Reconciled specification](NAVI_POWER_OF_NORMAL_RECONCILED_SPEC_20261024.md)
5. [Design fidelity audit](NAVI_DESIGN_FIDELITY_AUDIT_20261024.md)
6. [Twenty-agreement fidelity audit](NAVI_TWENTY_AGREEMENTS_FIDELITY_AUDIT_20261024.md)
7. [Implementation audit inventory](NAVI_IMPLEMENTATION_AUDIT_INVENTORY_20261025.md)

## Later explicit instructions take precedence

The following instructions from David in the October 24–25 discussion amend
older passages in these snapshots. Their inclusion here records the discussion;
it does not establish that every requirement has passed a field test.

- Remove the navigation stop-and-hold / latest-arrival stop feature. Timing
  serves recognition of the expected magnet. AUTO/GO authorization remains a
  separate matter.
- Geography establishes one next target. Timing does not replace Hall evidence
  or override wrong polarity. All established identifiers must qualify.
- Multiple 70x2 crossings within a magnet passage belong to its ensemble.
- Long Hall-baseline collection begins when the signal appears to return to
  ordinary track, independently of the short return confirmation, and ends at
  the next genuine opening. Remove the arbitrary 50-block minimum; retain the
  preceding reference when there are zero eligible samples.
- The timing map uses direction-specific segment factors normalized to the
  standardization sections MM035–055 and MM115–125. At PWM 90, measure the
  current locomotive and consist over those sections to establish the session
  base, then scale the directed survey factors by that base.
- Recalibration on each lap is a possible later refinement. Session-start
  standardization is the initial requirement.
- PWM 60 requires its own supervised survey. The supplied PWM-90 study does
  not provide valid PWM-60 onset-pair evidence.

## Timing evidence and firmware

The source survey table, evidence JSON, and report are in the parent `docs/`
directory of this repository. The operational map is
`firmware/test-programs/NAVI_POWER_OF_NORMAL_MANUAL_ONLY/NaviTimingProfiles.h`.
Firmware implementation and field validation status must be assessed separately
from the architectural decisions preserved here.

## Save or share

Share this folder's GitHub URL. Open an individual file and use **Raw** or
**Download raw file** to save a copy. To save the branch, use the repository's
**Code → Download ZIP** command, or pull the branch into a local checkout.
