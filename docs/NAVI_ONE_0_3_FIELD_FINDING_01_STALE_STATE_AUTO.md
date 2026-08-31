# NAVI_ONE 0.3 field finding 01 — a retained `state/auto` outlives the boot

**Date:** 2026-08-30
**Found by:** the operator, on the first attempt to set up a run on 0.3
**Severity:** blocks startup. The locomotive cannot be given a position.
**Fixed:** not yet. Proposed for 0.4.

## What the operator saw

Toby showed ONLINE with good telemetry (16.1 V, 0.2 A). The Toby page showed
**ENLISTED — "Dispatcher holds this locomotive"**, and both SESSION ORIENTATION
buttons and SET LOCATION were disabled. There was no way to declare a position,
so there was no way to run.

## What was actually true

```
ngr/loco/9950012/state/auto        1     <- STALE, retained from a previous session
ngr/loco/9950012/state/nav_ready   0     <- live and correct
ngr/loco/9950012/state/nav      ... "nav_state":"UNSET"   <- live and correct
```

The firmware had `autoEnrolled = false`. The console had a retained `1`. The
console was right to believe the topic; the topic was wrong.

## The defect

`serviceStatus()` republishes `state/throttle`, `state/direction`,
`state/estop` and `state/nav_ready` every second. **`state/auto` is not among
them.** It is published only from inside `handleCommand()`, on `cmd/auto` and on
`cmd/dispatcher_release`.

So a retained `state/auto 1` from any earlier session survives a power cycle and
speaks for a locomotive that has just booted with no enrolment and no position.
Nothing the firmware does afterwards contradicts it.

The trap closes on itself: `cmd/dispatcher_release` — the console's own escape
hatch, and the button the operator would reach for — is guarded by
`if (autoEnrolled || autoRunning)`. Both are false on a fresh boot, so RELEASE
publishes nothing and the stale value stands. The only command that clears it is
`cmd/auto 0`, which the startup page does not offer because the page believes
the locomotive is already enlisted and held by the dispatcher.

## Immediate remedy, used tonight

```bash
mosquitto_pub -h localhost -t "ngr/loco/9950012/cmd/auto" -m "0"
```

The locomotive then publishes the truthful `state/auto 0` itself. This is
deliberately not a broker-side deletion of the retained topic: the locomotive
should be the thing that corrects its own record.

## The fix for 0.4

Publish `state/auto` from the live flag in `serviceStatus()`, beside the other
four. Then no retained value can outlive a boot, because the truth is restated
every second.

While there: audit every retained topic the locomotive owns and make sure each
one is either restated periodically from live state, or cannot be stale. The
current set that is NOT restated includes `state/auto`, `state/session_direction`,
`state/start_mm`, `state/start_interval` and `state/brake` — all of which
describe a session and all of which currently survive the session that made them.

`state/session_direction` and `state/start_mm` are the same hazard wearing a
different hat: after a reboot they will still name this morning's orientation
and marker, while the navigator is `UNSET`. The console showed
"SESSION DIRECTION NOT CONFIRMED THIS SESSION" and "NO LOCATION CONFIRMED THIS
SESSION", so it is tracking confirmation separately and was not fooled — but the
locomotive should not be relying on the console to be more careful than it is.

## Why this matters beyond the inconvenience

Decision 0011 rejects by name a retained topic that asserts a state the
locomotive does not have — "a labelled lie is still a lie". This is that,
exactly, and it was introduced by omission rather than intent. 0.3 tightened the
command path considerably and did not look at what it retains.

## References

- `firmware/test-programs/NAVI_ONE/NAVI_ONE.ino` — `serviceStatus()`, `handleCommand()`
- decision 0011
