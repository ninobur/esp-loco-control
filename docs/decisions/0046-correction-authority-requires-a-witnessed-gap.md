# 0046 — R3 correction authority requires a witnessed gap; silent misses belong to QUORUM

Status: Accepted (field falsification, first 0.3B run, 2026-08-27 evening)

## Decision

R3 may propose a position correction ONLY for the candidate whose offset
equals its unresolved streak — a gap R3 itself witnessed as held,
magnet-like passages (`MAGNET_UNRESOLVED`). Single-event claims of silently
missed magnets are no longer correction-eligible at any confidence; silent
misses are QUORUM's jurisdiction, with its existing multi-event standard
(three disagreements plus sequence margin).

Confirmation reverts to the plain threshold on the expected candidate.
0.3B's "expected must also be the best candidate" rule is removed: a
wrong-polarity but otherwise-matching passage confirms, publishes DISAGREE,
and feeds QUORUM's missStreak exactly as the 0.2 lineage did.

## Context

The first 0.3B field run (2026-08-27 evening, CW from MM40) produced 34
committed corrections in roughly one lap, ~28 with `seq=0` — one to five
claimed silent misses each, on a single event's evidence, with zero held
passages behind the claim. Operator's report from the ground: *"It is
advancing the nav like crazy; if the pattern disagrees, it says, oh, there
must have been a silent magnet."* The locomotive ran its Arches stop at
Northpoint and later announced Bamboo near MM130.

Mechanism, visible in the very first record (`mm=41→43, cf=83,
s=[100,-,90,-,0]`): with strength, timing, and IR absent, the
available-evidence denominator shrinks until polarity+duration alone score
83%. Duration barely discriminates between adjacent markers, so on
alternating DNA every ordinary polarity misread polarity-matches the
candidate one ahead: pol's 32 points beat timing's 16-point objection, and
the "silent magnet" wins. After station dwells even off=5 was reachable.

The 0.3B position contract itself held: both `PHANTOM_REJECTED` refusals
kept position and shadow intact, and the two witnessed-gap corrections
(`seq=100`) plus the MM142–144 weak-zone confirmations worked as designed.
The spec already contained this rule and the implementation ignored it —
§3.3: *"where a correction to held position is at stake — from accumulated
agreement across successive markers rather than a single event."*

## Alternatives considered

- **Raise the 67% threshold.** Rejected: the defect is denominator
  inflation, not the bar. Polarity+duration alone reached 83%; no
  achievable threshold separates that from legitimate witnessed-gap
  corrections, which scored 67–90 in the same run.
- **Minimum-evidence floor for corrections** (e.g. timing must be
  available). Rejected as primary fix: dwells legitimately remove timing
  exactly where corrections matter most (station restarts).
- **Keep the ambiguity-hold confirmation rule.** Rejected: holding on
  every polarity misread starves QUORUM's missStreak — disabling the very
  recovery machinery silent misses now depend on.

## Consequences

- The operator's falsification criterion for the 67% starting value ("if
  it lets spurious signals through, it is too low") fired for the silent-
  miss class and is answered by authority removal, not tuning. For the
  witnessed-gap class 67% stands, still experimental.
- Gaps longer than `R3_CAND_MAX-1` (5) held passages exceed R3's search
  and resolve through QUORUM, as before R3 existed.
- Regression T9 pins the failure: a wrong-polarity-else-clean event with no
  held passages must advance exactly one marker, produce no correction, and
  publish DISAGREE. Suite: 177 checks, 0 failures.

## References

- `docs/decisions/0045-one-authoritative-position-outcome.md` — the
  contract this run validated
- `docs/TEMPLATES_REVISION_3_TARGET_ACQUISITION.md` §3.3, §6
- First 0.3B field run, 2026-08-27 evening (live MQTT observation)
- `firmware/test-programs/TEMPLATES/tests/harness_r3.cpp` T9
