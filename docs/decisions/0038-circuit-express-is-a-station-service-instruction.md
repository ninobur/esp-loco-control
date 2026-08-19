# 0038 — Circuit Express is a station-service instruction, not a chase

Status: Proposed (operator specification 2026-08-19; firmware built, not flashed)

## Decision

The dispatcher's `ce` command assigns **missions** to a paired fleet and severs
the pairing. The leader becomes the **EXPRESS**, the follower the **LOCAL**.

| | cruise PWM | stations | dwell |
|---|---|---|---|
| ordinary service | 90 (`CRUISE_PWM`) | all | 15 s |
| EXPRESS | 110 | skips every third station met | 5 s |
| LOCAL | 75 | all | 15 s |

The skip **rotates**: with four stations the express omits Arches, then
Grillers, then Patio, then Bamboo, repeating every three laps. No platform is
permanently unserved.

**No logic watches for the express catching the local.** Layer 5's traffic
protection already decelerates and holds behind a slower train, and Q1/Q2
already re-derives roles from geometry.

**CE ends the moment traffic first slows or holds the express** — that is the
express having caught the local. Missions clear, ordinary service resumes, and
the pair re-forms by geometry with the roles swapped, because the local is now
ahead. Only the EXPRESS terminates CE this way; a LOCAL held by something else
has caught nothing, and clearing its mission there would return it to full
cruise while it is boxed in.

CE is **refused** when the locomotive has no derived role, and says so
(`CE_REFUSED / NO_ROLE_NOTHING_TO_SEVER`). Without a pairing there is nothing to
sever and no way to say which train should run fast.

## Context

The console has published `ngr/dispatcher/cmd/ce` since v1.9.5 with nothing
subscribed — the button existed, the firmware never listened.

`BUBBLE_V1_SPEC.md` §9 describes CE as severance plus a chase. The operator's
later notes supersede that framing:

> circuit express becomes much simpler. It just becomes a command to temporarily
> suspend the station stops for a locomotive that happens to be in the lead.
> — `docs/CTO3/resources/PHYSICAL_ENVELOPE_NOTE.txt:13`

> If it catches a local train, it slows and follows… through ordinary
> collision-aware traffic control.
> — `docs/CTO3/resources/NEW_PARADIGM.txt:17`

That is the design followed here. CTO2 died of special two-train scripts, and
every clever addition to CE would be a re-implementation of the traffic layer
in a second place.

The one divergence from the notes is deliberate and operator-specified: the
notes have the express *resume* skipping when track clears, making CE a standing
mission. The operator specified the original CTO2 behaviour instead — CE is a
routine that runs and completes, ending when the express catches the local.

## Alternatives considered

- **CE persists through the encounter** (the notes' reading) — rejected by the
  operator: CE should be a routine with an end, not a mode.
- **A designated station skipped every lap** — rejected: one platform would
  never be served.
- **CE permitted solo** — rejected: no role means no basis for assigning
  express versus local. Costs solo testability, which is accepted.
- **Missions override grade-section cruise** — rejected: grade cruise values
  were tuned to pull specific hills. A service pattern is not a licence to
  re-tune the railway's physics, so grades keep their own cruise and only
  open-main cruise follows the mission.

## Consequences

- Station approach ramps derive from the **mission's** cruise, not the bare
  constant, so a LOCAL approaching from 75 is not handed a ramp built for 90.
- Speeds are PWM, not speed. Decision 0014's SPEED_HOLD is unbuilt, so CE
  inherits every limitation 0014 names — notably that the same PWM produces
  different speeds by grade, curve, load and battery voltage.
- EXPRESS at 110 is the fastest automation has run. `SPEED_CONTROL_DISCUSSION.txt`
  records clean navigation at 50–69 pKPH and cascade failures at 78–82; PWM 90
  measured ~240–290 mm/s (~45–54 pKPH) on 2026-08-19, so 110 should sit inside
  the clean band. **First express laps must be watched for `MM_OVERSPEED` and
  missed markers**, and this is the reason to watch.
- CE cannot start a stopped locomotive, cannot clear an E-stop, and cannot
  raise any cap the traffic layer has applied. It changes service pattern only.
- **Requires two locomotives.** Untestable solo by construction, and Toby is
  currently powered down.

## References

- `docs/CTO3/BUBBLE_V1_SPEC.md` §9
- `docs/CTO3/resources/PHYSICAL_ENVELOPE_NOTE.txt`, `NEW_PARADIGM.txt`
- decisions 0013 (chambers), 0014 (speed is the controlled variable), 0033
