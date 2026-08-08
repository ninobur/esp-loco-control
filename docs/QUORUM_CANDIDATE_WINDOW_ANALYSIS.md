# QUORUM candidate-window analysis — adding −2, measured against the route DNA

Date: 2026-08-09
Question (operator, 2026-08-07): extend `QUORUM_OFFSETS` from
`{−1,0,+1,+2,+3,+4}` to include −2, so hand-repositioning over-counts are
recoverable? Concern raised against it: every added candidate is another
way to tie, so widening the window might *increase* NO_QUORUM.
Method: exhaustive offline replay against the real 171-marker DNA — every
believed position × every odometer error in −2…+4 × both orientations,
scoring a 12-reading window exactly as `decideEvaluation()` does (unique
leader, margin ≥ 2). No locomotive involved.
Prior: `QUORUM_HAND_REPOSITION_HAZARD.md` (proposed this analysis).

## Verdict: **the concern was overcautious — adopt −2.**

| | 6-candidate set | 7-candidate set |
|---|---|---|
| err −2 (the hand-push case) | **unrepresentable — 171/171 terminal NO_QUORUM** (field-confirmed 2026-08-07) | **recovered at 170/171 positions**, both orientations |
| err −1, 0, +1, +2, +3 | clean 171/171 | **clean 171/171 — zero degradation** |
| err +4 | clean 171/171 | one margin-1 refusal (same site as below) |
| Wrong adoptions introduced | — | **none, anywhere** |
| Mean margin shift | — | −0.06 to −0.20 of a point |

Total cost across all 1,197 scenarios: **two additional honest refusals**,
both at one physical site. Total benefit: the entire −2 error class goes
from 100 % terminal to 99.4 % self-recovered.

## The one aliasing site

Believed mm 101 (CW) / mm 92 (CCW) — the same physical stretch, roughly
mm 95–107, where the DNA nearly repeats with **period 6**. Candidates −2
and +4 differ by exactly 6, so there they score 12 vs 11: the correct
candidate still leads, but margin 1 < 2, and QUORUM refuses — the honest
outcome, not a wrong adoption. (`sc=[12,4,4,9,5,3,11]` for err −2 CW.)
The existing operational recovery — re-declare — covers it, and the
navigator's behaviour there is indistinguishable from today's.

Worth recording independently: this is the route's only lag-6
autocorrelation strong enough to matter, and it sits under the Arches
approach. Any future widening beyond −2/+4 should re-run this analysis
first — the margin structure, not intuition, is what decides.

## Recommendation — and disposition

The analysis recommended adopting `{−2,−1,0,+1,+2,+3,+4}`.
**Operator ruling (2026-08-08): DEFERRED, not adopted.** The
six-candidate window is retained; hand repositioning is mitigated
operationally — location is re-declared whenever the Hall sensor is
moved across a marker by hand (`QUORUM_HAND_REPOSITION_HAZARD.md`).
This record stands as evidence for any future revisit; the margin
structure measured here, not intuition, is what should decide then.

Analysis script inline in the 2026-08-09 session; reproducible from the
DNA table at [QUORUM.ino:397](../firmware/QUORUM/QUORUM.ino).
