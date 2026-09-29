# 0109 - NAVI_EWO is a target-only navigator: it confirms its known target, shrugs at everything else, and continues

Status: Accepted as principle (recorded 2026-09-29; settled by David with Sam).
Documentation only. No implementation, flashing or deployment is authorized by
this record.
Decided by: David, after review with Sam. The wording set in quotation marks
below is the canonical wording from David's documentation instruction of
2026-09-29.

## Decision

**NAVI is on a quest to confirm its known target.**

NAVI knows which MM it seeks and that MM's mapped polarity, distance,
sequence/direction and context. It does not attempt to identify every magnetic
phenomenon it encounters.

**The NAVI shrug.** A phenomenon that does not confirm the target receives:

> "Interesting observation. It does not confirm my target. Preserve the
> evidence and continue looking."

Failure to confirm a target does not authorize any of:

- alternative-position search;
- map-wide matching;
- identification of another MM;
- a guessed position;
- proximal recovery;
- automatic sequence correction.

**Missed Magnet continuity.** Failure to confirm one target does not destroy
location context. With applicable IR (0111):

```text
target interval passed without confirmation
-> report MISSED MAGNET
-> preserve navigation context
-> advance target to the subsequent mapped MM
-> continue seeking
```

This is the only recovery mechanism currently authorized. Not authorized:
proximal recovery; alternative-position search; sequence correction; map-wide
matching; guessed positions; QUORUM recovery; the 10-MM AUTO recovery limit. If
additional recovery is later demonstrated to be necessary, it must be
architected anew under EWO principles. Nothing in this record pre-designs it.

**Operator declaration and CRM.** Operator declaration or redeclaration is
authoritative for establishing starting navigation context. It establishes the
declared position, direction and corresponding target context, and clears
incompatible accumulated evidence. NAVI does not silently correct a bad
declaration by searching for another position.

The intended Crew Resource Management principle: NAVI accepts the operator's
declaration but should eventually challenge it if accumulated physical
observations demonstrate that the declared context is incoherent. The desired
future operator message is:

> "The MM encountered are inconsistent with the declared starting position."

**No threshold, count or automatic mechanism for making that determination is
authorized or decided.** It is an intended direction, not a specification.

**The confident locomotive** (now also an enduring principle, section 17 of
`../NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md`). NAVI should not stop merely because redundant
evidence becomes temporarily unavailable, if the remaining evidence continues to
coherently confirm its known targets. Loss of redundancy is not automatically
loss of navigation competence. Likewise, uncertainty does not require NAVI to
invent an explanation.

**Station boundary.** Stations are operational consumers of NAVI's established
position and context. They do not determine or correct NAVI's location.
Position-based operational capability is preserved, including starting within an
active station zone and proceeding correctly through in-station speed, stop,
dwell and departure (0101).

**Supersession.** Within NAVI_EWO, decisions 0109-0113 govern where they conflict
with older assumptions in: X22/X22R detector architecture; NAVI_ONE;
NAVI_COHERENCE navigation judgment; proximal recovery; alternative-position
search; legacy Hall readiness/gating; and shadow accepted-MM reference
mechanisms. Older material remains prior art and evidence and is not deleted. The
itemized relationships are in
[`../NAVI_EWO_GOVERNING_DOCUMENTS.md`](../NAVI_EWO_GOVERNING_DOCUMENTS.md).

## Context

The 2026-09-27 authority decisions (0102-0108) established that NAVI is the sole
navigation authority and that X22 is prior art, but they left the recovery and
identification machinery of the NAVI_COHERENCE lineage (0098, sequence
recovery, whole-route matching) unaddressed. Historical evidence shows that
mechanisms built to keep a locomotive moving through a disagreement it cannot
resolve were a recurring source of failure (0056). The 2026-09-28 impostor
challenge found the simple target-conditioned criteria useful, and located the
remaining open questions in missing synchronized IR data, not in a missing
recognizer.

## Alternatives considered

- **Retain proximal recovery (0098) in EWO.** Not chosen: it is an
  identification-by-search mechanism, which target-only navigation excludes.
- **A discrepancy-triggered automatic stop or operator challenge.** Not chosen
  now: no threshold is established, and AGENTS.md section 8 bars adding
  safeguards for hypothetical conditions without evidence.
- **Stop at the first unconfirmed target.** Not chosen: it discards intact
  location context to defend against a condition (a missed magnet) that the
  mapped interval and IR can already bound.

## Consequences

- NAVI_EWO has exactly one recovery behaviour, Missed Magnet, and it requires
  applicable IR. There is no no-IR Missed Magnet (0111).
- A wrong declaration is not corrected by NAVI. Until a CRM mechanism is
  separately decided, the operator remains the corrector.
- **Risk: a wrong incumbent is held indefinitely.** The 0104 run 3 finding (a
  two-MM error held for 32 events) was resolved there by a recovery mechanism that
  EWO does not carry. Under EWO the same error would persist until an operator
  redeclaration or repeated Missed Magnet evidence makes the discrepancy visible.
  Field evidence, not this record, should decide whether more is needed.
- **Risk: Missed Magnet advance can itself be wrong** if the "miss" was a
  sensing failure rather than an absent magnet. The advance is unguessed but
  unverified until the next target confirms.

## Relationship to earlier decisions

- **0056** (navigator reconciles neither declaration nor map; missing magnet and
  wrong declaration produce refusal and stop): *narrowed for NAVI_EWO.* Its
  principle that NAVI never guesses or invents position is preserved. Its
  consequence that a disagreement ends in refusal and stop is replaced, for a
  missed target with applicable IR, by Missed Magnet continuity. **Confirmed by
  David and Sam, 2026-09-29.**
- **0098** (proximal physical recovery): *superseded for NAVI_EWO.* Remains prior
  art for NAVI_COHERENCE.
- **0104** (Evidence Accumulation Before Revision): *refined.* OBSERVE and HOLD
  are preserved (the shrug is OBSERVE plus HOLD). RESOLVE toward a *different
  position hypothesis* has no authorized mechanism in EWO, beyond Missed Magnet
  advance and operator redeclaration. **Confirmed by David and Sam,
  2026-09-29.**
- **0101** (position determines local operating requirements): preserved and
  reaffirmed.
- **0102** (NAVI sole authority; X22 prior art): reaffirmed and extended.

## References

- `0102`-`0108`, `0110`-`0113`
- `../NAVI_EWO_GOVERNING_DOCUMENTS.md`
- `../../field-records/analysis/20260928_navi_eyes_wide_open_historical_impostor_challenge_report.md`
- `../../AGENTS.md` sections 1, 6, 8
