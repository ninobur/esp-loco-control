# 0033 — Separation is the occupancy bubble plus six markers clear

Status: Accepted (2026-08-13, operator ruling on CODEX review finding 1).
Supersedes decision 0030 in part, and the extent values in
`CTO3_INTENT_BASELINE.md`, as detailed below.

## Decision

**The safety invariant is bound-to-bound:** at rest, the follower's front
bound must sit at least **6 markers** clear of the leader's rear bound.

```
gap( follower.front_bound  →  leader.rear_bound )  ≥  6 MM
```

Collision logic consumes **published occupancy bounds** — navigation bound
composed with configured extent, applied by the producer (0030's surviving
rules). It never consumes Hall positions.

**Hall-to-Hall targets are derived configuration, not the invariant.** With
the current extents (+2 ahead, −4 behind), the invariant derives to the
12-marker Hall-to-Hall stop target the bubble spec uses. Change a consist and
the derived Hall number changes; the bubble-plus-six invariant does not.
Hall-to-Hall figures may appear in telemetry and operating procedures as
diagnostics for the *current* configuration only.

**Extent is expressed in markers** (+2 / −4 for the current consists),
confirming 0030's amendment and superseding the inch values in the intent
baseline.

## Context

CODEX review of `BUBBLE_V1_SPEC.md` (2026-08-13, finding 1): the spec stated
safety in fixed Hall-to-Hall gaps, which silently bakes today's +2/−4 consist
into a "universal" number — lengthen a consist and 12 Hall-to-Hall can put
bounds in contact. Operator ruling: *"bubble + 6 MM gap."* The bubble is
authoritative; Hall numbers are configuration-specific derivations.

This also resolves a three-way documentary conflict about extent:

- `CTO3_INTENT_BASELINE.md` (2026-08-05): 18 inches forward / 48 inches
  behind, explicitly rejecting "marker-count approximations";
- 0030 as first written (2026-08-13): 2 ft / 4 ft in millimetres;
- 0030 as amended the same day (operator): **+2 / −4 markers** — the railway
  does not measure in feet.

The marker ruling is the operator's latest and controls. One honest caveat it
carries: at the route's minimum spacing (280 mm), −4 markers is 1120 mm,
about 100 mm **less** than the baseline's 48-inch tail — the marker form can
under-cover a long consist's rear at the tightest spacing. The 6-marker clear
gap (≥ 1680 mm) absorbs that comfortably, which is a further reason the
invariant is bubble-plus-clearance rather than extent alone. The stopping
trials required by the spec should confirm the rear extent against the
physical consists.

## Alternatives considered

**Fixed Hall-to-Hall separation as the invariant** (the spec's first
wording). Rejected by the review and the ruling: correct only while every
consist matches the constants — the same defect as CTO2's fixed 5/5 offsets,
one abstraction layer up.

**Extent in inches/feet per the intent baseline.** Rejected by the 0030
amendment: nothing on the railway is measured or reasoned in imperial units;
converted numbers masquerade as measurements.

**A larger clear gap.** Nothing precludes raising 6 later; it is one
configuration value. Six is the operator's ruling, double the sensors-touch
observation, and the number field testing will validate.

## Consequences

- `BUBBLE_V1_SPEC.md` §3 and §7 are reworded: invariant bound-to-bound,
  12/18 Hall-to-Hall labeled as derived, configuration-specific values.
- The **18-marker deceleration trigger is provisional** (operator ruling on
  finding 2: keep it, field test it). It consumes the entire nominal margin
  and includes no allowance for packet latency, evaluation delay, bound
  uncertainty, stop scatter, grade, battery or load. Worst-case stopping
  trials — full consist, Grillers grade, low battery — are a named
  precondition to treating it as a proven safety minimum.
- The v1.14 occupancy-bound work (0030) becomes a hard prerequisite of any
  bubble firmware: the invariant cannot be evaluated without published
  bounds.
- `CTO3_INTENT_BASELINE.md` extent values are superseded; the baseline's
  surrounding principle — envelope-gap reasoning, values in configuration —
  is exactly what this record enforces.
- 0030 status updated to "Amended; superseded in part by 0033" — its
  producer-applies-extent and per-consist-configuration rules stand; its unit
  discussion is closed here.

## References

- CODEX review of `BUBBLE_V1_SPEC.md`, findings 1, 2 and 7 (2026-08-13)
- `docs/decisions/0030-*` — extent rules; unit question closed here
- `docs/CTO3/CTO3_INTENT_BASELINE.md` — superseded extent values, retained principle
- `docs/CTO3/BUBBLE_V1_SPEC.md` — the routine this governs
