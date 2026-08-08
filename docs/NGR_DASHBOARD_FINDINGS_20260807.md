# Dashboard findings, 2026-08-07 — AUTO opacity and stale-recovery state loss

Date: 2026-08-07
Raised by: operator, during QUORUM 1.8 Stage C testing
Status: **diagnosed from firmware source + broker telemetry; no dashboard
change written.** The Flask app on ngr-pi (`/home/david/ngr_app.py`,
service `ngr-app`) was not modified.

---

## Finding 1 — the console under-reports the locomotive's authority

**Symptom:** pressing AUTO appeared to do nothing; the tile flashed "no
telemetry" and reverted to MANUAL.

**Firmware truth at that moment:** `state/auto = 1`, uptime 3393 s
(56 minutes continuous — **no reboot**; the operator correctly inferred
this from the start interval surviving).

So enrollment *succeeded and persisted*. The console displayed MANUAL
while the locomotive was enrolled in AUTO.

**Mechanism:** when telemetry goes stale the dashboard flashes the
no-telemetry state and, on recovery, re-renders mode from a local default
rather than from the retained `state/auto` topic. The whole-screen flash
the operator saw is that re-render.

**Why this matters more than a cosmetic bug:** it errs in the dangerous
direction — the console shows *less* authority than the locomotive
actually holds. An operator reading MANUAL may believe the onboard
software cannot take the throttle when it can. Under the bicameral model
(decision 0013) the enrollment door is one of the four crossings; the
console must never misreport its state.

**Fix direction:** on reconnect/recovery, repopulate mode from the retained
`state/auto` (and `state/estop`, `state/session_direction`,
`state/start_interval`) rather than local defaults. Dashboard v1.10.x
already ignores retained deliveries by design (the 2026-07 stale-tile
fix) — so this needs a deliberate "read retained once on (re)connect,
then ignore" path, not a blanket re-enable.

## Finding 2 — GO refusals are published but not surfaced

Pressing AUTO alone never moves the locomotive: `cmd/auto` only sets
`autoEnrolled` ([QUORUM.ino:2535](../firmware/QUORUM/QUORUM.ino)). Motion
requires a separate `cmd/go`, which enforces seven gates and **publishes a
reason for every refusal**:

| Gate | Published reason |
|---|---|
| E-stop active | `ESTOP_ACTIVE` |
| Still rolling | `WAIT_FOR_STOP` |
| Direction NEUTRAL | `NEUTRAL_SELECT_DIRECTION` |
| Not enrolled | `NOT_ENROLLED_IN_AUTO` |
| `navState == NO_QUORUM` | `NO_QUORUM_DECLARE_POSITION` |
| `navState == UNSET` | `NO_POSITION_DECLARE_START_MM` |
| `navDir == UNSET` | `NO_SESSION_DIRECTION` |

These go to `station/*`. The dashboard does not display them, so a refused
GO looks identical to a dead button. The firmware comment at the
`NOT_ENROLLED` gate records that this exact silence was fixed once before
in firmware ("a lost loco could be sent GO all night and neither move nor
explain itself") — the console now reintroduces it at the UI layer.

**Fix direction:** surface the most recent `station/*` event in a status
line. Low effort, high value: it converts an opaque control into a
self-explaining one.

## Finding 3 — AUTO under QUORUM has never been exercised

Operator note, 2026-08-07: **no autonomous running has been done under the
QUORUM navigator.** All validation to date — including the whole 1.8
acceptance matrix — is MANUAL. Untested: enrollment → GO →
`cruiseForPosition()` → station phases, against QUORUM's position truth
rather than SOLONAV's.

This is a scope gap, not a defect, and it should get its own planned
session with a written protocol — not a tail-end experiment. It is also a
prerequisite for CTO3 Station Stop v1.

## Finding 4 — telemetry gaps persisted all session (unresolved)

Stale flashes recurred throughout with **no reboots** (uptime continuous).
Earlier characterisation stands: the MQTT session holds
(`mqtt_attempts` 1) while delivery stalls 5–15 s and then flushes in a
burst — an RF-level dropout bridged by TCP, not a reconnect cycle. A
fractured Hall harness wire was found and repaired the same morning, which
removed one candidate cause (rail loading) but not the symptom.

Untouched. Next investigation.

## References

- `QUORUM_1_8_STAGE_C_VERDICT.md`
- Decision 0013 (bicameral authority, four crossing doors)
- `field-records/logs/20260807_C_acceptance_1_8*.log`
