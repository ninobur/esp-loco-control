# 0043 — Magnet admission favors recoverable omission over false inclusion

Status: Accepted (operator decision, 2026-08-24)

## Decision

TEMPLATES Hall processing will favor precision over recall at the navigation-admission boundary. A doubtful or physically impossible response is discarded before it can advance position, enter an evidence ring, or score any navigation hypothesis.

Upstream classification bias must align with downstream recovery capability. Navigation is designed to compensate for omitted valid markers, so conservative rejection deliberately converts uncertainty into that modeled omission error. Admitting a false marker instead creates an insertion, corrupts the coordinate system, and causes later genuine magnets to be evaluated against the wrong map positions.

Admission is layered in this order:

1. qualify a distinct, broad magnet passage from the waveform;
2. reject or merge pulses and companion/edge lobes that are not separate passages;
3. enforce an evidence-derived earliest physically possible next-marker arrival;
4. validate the credible passage against the expected map target before position advances;
5. give downstream recovery only credible magnet evidence.

Numerical thresholds and the implementation remain unapproved pending anchored replay.

## Context

The 2026-08-24 traces separated Otto's normal magnet curves from a large added pulse population. Toby CCW produced one under-40-ms rejection; Otto produced 626 CCW and 639 in the restarted CW session, overwhelmingly 20–22 ms pulses. Some marginal pulses survived the simple threshold/floor, advanced navigation, and caused later broad genuine responses to disagree with an already-corrupted map. QUORUM then attempted to repair an inclusion error with machinery intended for bounded position omissions.

The operator's ruling is that Hall sensors detect the magnets; “missing magnet” in this architecture may be a valid passage deliberately omitted by conservative classification. That is preferable because it presents recovery with the error class it was built to handle.

## Relationship to branch-dependent earlier decisions

This record uses 0043 because concurrent branches already contain decisions through 0042.

It supersedes decision 0035 **in part** when that branch is consolidated: a categorically pulse-like or physically impossible response is discarded, not quarantined for later resurrection. Diagnostic logging remains required, but awareness grants no navigation candidacy. This record does not decide how a credible-but-map-contradictory broad passage affects confidence or recovery.

It does not ratify or repeal decision 0040's Otto-specific ±90-count entry threshold. That numerical decision belongs to a different firmware/evidence era and remains subject to reconciliation. TEMPLATES may use amplitude as evidence, but amplitude crossing alone is not target identification.

## Alternatives considered

**Favor recall to avoid missing magnets.** Rejected. It optimizes against the recoverable error while admitting the error that corrupts every later comparison.

**Let QUORUM/MHT sort out all threshold crossings.** Rejected. Obvious non-marker artifacts must not testify for alternate position hypotheses.

**Use polarity alone.** Rejected. Opposite-polarity magnetic edge structure is physically possible, while electrical pulses can have either polarity. Passage morphology and physical timing precede map-polarity validation.

## Consequences

- Every TEMPLATES implementation must prove rejected responses are inert to position and hypothesis state.
- False inclusion and deliberate omission must be reported separately in replay; a blended accuracy score is insufficient.
- Recovery candidates and bounds must be able to represent the omissions conservative admission intentionally creates.
- Opposite-polarity edge lobes must be merged into one passage or rejected as artifacts, not counted automatically as another magnet.
- Physical elimination of Otto's pulse source remains the first remedy; software admission is defense in depth.

## References

- `TEMPLATES/REVIEW of HALL SENSOR LOGIC/MAGNET_ADMISSION_DOCTRINE.md`
- `TEMPLATES/REVIEW of HALL SENSOR LOGIC/TARGET_ACQUISITION_DECISION_LOGIC.md`
- `docs/QUORUM_NAV_AUDIT_AND_GATE_PROTOTYPE.md`
- Commit `240f1bf` (mandatory doctrine; later commits may carry the same content after history amendment)
- Cross-branch decisions 0035 and 0040, to be reconciled during branch consolidation
