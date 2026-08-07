# Otto Hall incident — stranded wire clamped directly in screw terminal

**Date:** 2026-08-06
**Locomotive:** Otto, 9950011
**Disposition:** physical termination corrected; clean moving and stationary
checks observed; longer repeat run still appropriate

## Verdict

Otto's alarming Hall behaviour had a plain physical cause: the fine conductors
of the shielded cable had been twisted by hand and inserted directly into the
screw-down terminal. That is not a reliable termination. The screw can miss,
spread, or only lightly capture the strands, leaving a connection that changes
with handling and vibration even though the cable itself is shielded.

The working repair was to splice the shielded-cable conductors to minimal,
approximately one-inch solid-wire pigtails with solder/heat-shrink connectors,
then clamp the solid conductors in the screw terminal. Keeping the pigtails short
limits the unshielded length and mechanical leverage.

This was an assembly mistake, not evidence that shielding was ineffective and not
a QUORUM algorithm defect.

## Electrical check

With the termination accessible, the observed Hall circuit values were:

| measurement | observed |
|---|---:|
| sensor supply | 3.3 V |
| sensor signal at rest | about 1.8 V |
| signal with magnet, one polarity | about 2.4 V |
| signal with magnet, opposite polarity | about 1.2 V |

The two-direction response demonstrates that the Hall element and analog signal
path were alive. The fault was the unreliable connection into the terminal.

## Post-repair observations

The first moving check showed:

- 697 agreements and 0 disagreements;
- 100% polarity agreement;
- reported 49.9 pKpH against a 50 pKpH target;
- stable navigation at the observed position.

A later stationary check showed 0.0 pKpH, 26 agreements and 0 disagreements in
the displayed session, and no phantom Hall events while the locomotive sat still
during the observation.

These are strong operational confirmation of the repair. They are not a formal
lifetime qualification of the splice, strain relief, or terminal under months of
outdoor service.

## Consequences for earlier data

Hall-derived conclusions from the affected Otto runs must be treated cautiously.
The bad termination can explain intermittent phantoms, implausible roughly
400 pKpH estimates, polarity disturbances, and eventual navigation loss without
requiring a changing magnet field or a firmware change between good and bad laps.
It does not prove that every anomaly in every earlier run had this one cause.

The IR diagnostic data remains an independent record. The Hall termination fault
can corrupt comparisons that use Otto as the motion/position witness, but it does
not manufacture the IR sensor's locally measured `rh`, `fh`, pulse, or phase data.

## Wiring standard from this incident

For fine stranded or shielded sensor cable entering a screw terminal:

1. Do not twist bare fine strands and clamp them directly.
2. Use a correctly sized crimp ferrule, or splice to a short solid-wire pigtail
   with insulation and strain relief.
3. Keep any unshielded pigtail as short as serviceability permits; approximately
   one inch worked here.
4. Ensure the terminal clamps conductor, not insulation or heat-shrink.
5. Support the cable so truck movement cannot work the conductor at the terminal.
6. Check each conductor with a gentle pull test, then verify supply, resting
   signal, and both magnet polarities before reassembly.
7. Finish with both a moving agreement run and a stationary phantom check.

## Remaining check

Run Otto for several complete laps on genuine QUORUM 1.7 and retain the Hall
event log. The repair passes that check if polarity remains clean, no implausible
speed bursts recur, navigation remains NORMAL, and an extended stationary period
produces no phantoms.
