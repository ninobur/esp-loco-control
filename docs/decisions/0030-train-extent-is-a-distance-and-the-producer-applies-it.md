# 0030 — Train extent is a distance, and the producer applies it

Status: Accepted (2026-08-13)

## Decision

A train's physical extent, relative to its Hall sensor, is:

| | | |
|---|---|---|
| ahead of the sensor | **2 ft** | 610 mm |
| behind the sensor | **4 ft** | 1219 mm |

Three rules follow.

1. **Extent is expressed as a distance in millimetres, never as a marker
   count.** Marker spacing on the Lowline runs from 280 to 355 mm
   (`spacingMm[]`), so a marker-count extent is not a fixed quantity. The
   consist's length is.
2. **Extent lives in per-locomotive configuration**, alongside the station
   speeds, in `LL_LocoConfig_<id>.h`. It is a property of the consist and
   changes when cars are added or removed.
3. **The producing locomotive applies its own extent before publishing.** A
   consumer receives occupied track, not a sensor location it must correct.

## Context

`ROAD_TO_CTO.md` M5 expressed extent as "Hall +2 markers to Hall −4 markers"
with a parenthetical conversion to 0.6 m and 1.2 m. The operator ruled
(2026-08-13) that the quantity is **feet**: plus two, minus four. The marker
form was a convenient approximation at 305 mm nominal spacing and is not
correct where spacing departs from it.

**Extent is not the same thing as navigation uncertainty, and today's telemetry
publishes only the latter.** In `QUORUM_1_13`:

- `rear` is the last confirmed **marker index**;
- `front` is the dead-reckoned marker index plus `LOST_FRONT_MARGIN_MARKERS`,
  because a locomotive misreading markers may also be missing them;
- `env` is the span between them **in millimetres**.

The source says so directly: these are *"not a computed occupancy bound, which
is M5."* They describe where the **sensor** might be. Extent describes how much
train hangs off that sensor. The two must be composed, not conflated:

```
occupancy = navigation bound  ⊕  consist extent
```

They also differ in kind. **Extent is exact and known**; navigation uncertainty
is neither, and grows with markers since the last confirmation. Collapsing them
into one number loses the ability to say which part of a bound is measurement
and which is ignorance.

**Why the producer applies it.** A follower holding clear of a reported
*position* holds clear of the power car and drives into the last coach. That is
the CTO2 failure shape — a consumer reasoning correctly about a number that did
not mean what it assumed. Making every consumer know every peer's consist
length multiplies the chances of getting it wrong, and guarantees a stale answer
when a car is added.

## Alternatives considered

**Keep extent in markers.** Rejected: 280–355 mm spacing makes it a variable
quantity, and the error is worst exactly where markers are widest.

**Let the consumer add extent.** Rejected: every consumer would need every
peer's consist configuration, and would silently use a stale one after a
consist change. The producer is the only party that knows its own train.

**One combined bound with extent folded into navigation uncertainty.** Rejected:
it destroys the distinction between what is measured and what is estimated, and
makes the bound impossible to validate against offline-aligned truth.

## Consequences

- M5's validation must test the **occupancy** bound, not the navigation bound:
  across five laps the true occupied track must lie inside the published bound
  100% of the time.
- Adding or removing a car is a **configuration change**, not a tuning session.
  This is worth stating because per-consist hand-tuning was already identified
  as a reliability hazard.
- **Naming hazard, and it is live.** In this codebase `mm` means MILE MARKER,
  while `spacingMm[]` and `envelope_mm` mean millimetres. The same two letters
  already carry both meanings. Extent constants and any new bound fields must
  carry an unambiguous unit in the name, and the mixed convention should not be
  extended further.
- Extent is a property of the **consist**, so a locomotive running light has a
  different extent from the same locomotive with three coaches. The value is
  configuration, and nothing should assume it is constant across a session.
- Nothing here authorises a firmware change. Publishing an occupancy bound is
  M5 work and owes its own record.

## References

- `docs/ROAD_TO_CTO.md` — M5, where extent was first stated in marker form
- `docs/CTO3/CTO3_INTENT_BASELINE.md` — collision avoidance protects physical
  consists, not point locations or Hall sensors; the values must be
  configuration, not assumptions scattered through routine logic
- `docs/CTO3/resources/CTO2_BUBBLE_PRINCIPLE.txt` — the bubble envelope
- `firmware/QUORUM/QUORUM.ino` — `rear`/`front`/`env` as published today
