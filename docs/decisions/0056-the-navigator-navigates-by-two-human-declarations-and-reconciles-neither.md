# 0056 — The navigator navigates by two human declarations, and reconciles neither

Status: Accepted (operator, 2026-08-29)

## Decision

The navigator holds two truths it cannot verify for itself:

1. **Where the operator says it is** — the declaration.
2. **What the map says the railway is** — `NGR_DNA1` and `spacingMm`.

Both are maintained by people. When either stops matching the world, the
navigator's whole job is to **detect the disagreement and stop saying where it
is**. It never reconciles them, never adapts to them, and never invents a fact
that would let it keep moving.

This is the principle decisions 0043, 0053 and 0055 descend from, and the reason
"silent magnet" is a banned term and a banned concept.

### What destroys the truth

Anything that changes the world without changing a declaration:

- the locomotive moved by hand — lifted, pushed, re-railed (0055)
- a break in the track
- a magnet missing, moved, or reversed
- the route physically changed

The navigator cannot tell these apart from a bad declaration, and it must not
try. A missing magnet and a wrong `start_mm` produce the identical signature —
polarity disagreement, refusal, stop — and that identity is correct, because both
mean the same thing: *the map and the world disagree, and a person is needed.*

### Our part

The obligations are the operator's, and they are the price of a navigator with
no recovery machinery:

- **Declare position after handling.**
- **Re-survey and commit the map when the route physically changes.**
- **Treat a repeatable refusal at the same place as a track fault, not a
  firmware fault.**
- **Never ask the sketch to paper over any of it.**

The last is the whole design in a sentence.

## Context

Every failure this project has spent months chasing came from a mechanism built
to keep a locomotive moving through a disagreement it could not resolve. The
QUORUM offsets relocated position by committee. "Silent magnets" invented
untravelled track to explain a jump forward. The conservation gate refused real
magnets to defend a velocity model. Each was an attempt to reconcile the map
with the world from inside the locomotive.

The 2026-08-27 record shows the cost: position advanced four markers in 1,005 ms
while the wheel turned 29 mm; across that run, 254 markers of believed advance
against 47.0 m of travel — roughly half a lap never driven. The sketch's own
comments call the same failure an 82-marker drift.

On 2026-08-29 the operator generalised the manual-movement ruling of 0055 to its
proper scope: *"Or if there is a break in the track, or a missing magnet, or the
track route is physically changed. It changes the truth. We have to do our part."*

## Alternatives considered

- **Detect track changes automatically** (timing anomalies, missing-marker
  inference, IR distance disagreement). Rejected: every such mechanism must
  guess between "the world changed" and "I misread", and guessing wrong in the
  permissive direction is precisely how the offsets destroyed position.
- **Let the navigator continue on dead reckoning through a disagreement.**
  Rejected: this is the silent-magnet mechanism with a different name.
- **Treat refusals as faults to be engineered away.** Rejected: a refusal is the
  navigator's only honest output when the map and the world disagree. Suppressing
  it removes the signal, not the problem.

## Consequences

- **A refusal is information, not a malfunction.** Field records must not count
  refusals against the navigator without establishing which of the two
  declarations was stale.
- **The map is a versioned artifact with an owner.** A physical route change is
  not complete until `NGR_DNA1` / `spacingMm` are re-surveyed and committed. The
  map is as much a declaration as `start_mm`, only made months earlier.
- **Repeatable refusals localise faults.** A refusal at the same place across
  runs is a track fault. Where `trust` was `PROVEN` beforehand, the ten-magnet
  word names where the pattern says the locomotive actually is, and a consistent
  one-marker offset after a known-good stretch reads as a missing magnet rather
  than a bad declaration. The navigator cannot repair it; it can say which
  declaration to go and check.
- **Damaged magnets reveal themselves**, which is the operator's standing
  requirement and only achievable because nothing papers over them.
- **No mechanism may be added whose purpose is to survive a disagreement.** New
  tests may make admission more accurate; none may make a disagreement
  survivable.

## References

- decisions 0024, 0043, 0053, 0055; the silent-magnet ban (operator, 2026-08-28)
- `docs/research/20260828_WHAT_THE_HALL_SENSOR_SEES.md`
- `docs/research/20260829_A_CLEAN_LAP.md`
- field evidence: `field-records/logs/20260829_navi2_first_lap/`
