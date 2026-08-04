# Dashboard v1.10.3 — implementation report

**Describes commit 427cfc7 (`server/ngr_app_v1_10_3.py`).** Operator ruling:
manual control is sovereign and DUMB. In MANUAL every control publishes its
command immediately on input — tap to publish, nothing between. The bicameral
doctrine applies to the dashboard command path, not only to the firmware.

Operator evidence: E-STOP, which bypasses all logic, was more responsive than
throttle decrease, which did not. Signal strength was surveyed for five hours
with zero dropout, so coverage was excluded as a cause. That reasoning was
correct — every delay found was added dashboard logic.

---

## Change 3 — command-path audit

`input event → broker`, step by step, for each manual control. Anything that
is not "read the value and publish it" is listed with its disposition.

### E-STOP — the reference path

| # | step |
|---|---|
| 1 | `onclick` → `toggleEstop()` |
| 2 | optimistic label/class flip (display only) |
| 3 | `fetch('/otto/cmd/estop/N')` |
| 4 | server `_cmd()` → **returns before any lock or check** |
| 5 | `pub_loco()` → `mqtt_conn.publish()` |

Five steps, nothing between input and publish. This is the model.

### THROTTLE — before

| # | step | disposition |
|---|---|---|
| 1 | `oninput` → **display only, no publish** | — |
| 2 | `onchange` → `sendCmd('throttle', …)` | **REMOVED.** On a range input `change` fires when the finger LIFTS. The entire drag published nothing. **This is the reported latency.** |
| 3 | `sendCmd`: `if (isCto) return` | KEPT — chamber boundary, false in MANUAL |
| 4 | `fetch(...)` | — |
| 5 | server: `with mqtt_lock:` to read `auto` | **REMOVED** — see below |
| 6 | `pub_loco()` | — |

### THROTTLE / BRAKE — after

`oninput` → `sendCmd()` → `fetch` → server (no lock) → publish. Display update
follows the publish rather than replacing it.

### DIRECTION — before

| # | step | disposition |
|---|---|---|
| 1 | `onclick` → `sendDir(d)` | — |
| 2 | `if (isCto) return` | KEPT — chamber boundary |
| 3 | 1-second **same-value bounce guard**: `if (pendingDir && pendingDir.value === d && now - sentAt < 1000) return` | **REMOVED.** Silently swallowed a deliberate re-press inside one second. |
| 4 | `pendingDir = {value, sentAt}` | **REMOVED** |
| 5 | `dirNoConfirmUntil = 0` | **REMOVED** |
| 6 | loop over three buttons setting `waiting` class | **REMOVED** from the pre-publish path |
| 7 | write "WAITING FOR CONFIRMATION…" to the DOM | **REMOVED** |
| 8 | `fetch(...)` | — |
| 9 | server: `with mqtt_lock:` | **REMOVED** |

Seven steps between tap and publish, five of them DOM or state bookkeeping.

### DIRECTION — after

`onclick` → `if (isCto) return` → `fetch` → optimistic button light **after**
the publish is issued. Two steps, and the visual is no longer ahead of the
command.

### Server — the lock

```python
with mqtt_lock:
    if loco_state[lid]["auto"] == "1":
        return "", 423
```

`on_mqtt_message()` holds that same lock for its **entire body** — every JSON
parse, every state write, the packet-log append — for every inbound message.
At 1 Hz loopstat plus alerts plus marker bursts, a manual command could block
behind telemetry parsing. **E-STOP returns before the lock and never waits.**
That is precisely the asymmetry the operator measured, and it is structural
rather than incidental.

Removed. A bare dict read of a `str` is atomic under the GIL, and the value
can change immediately after the lock is released anyway, so the lock bought
no correctness — only contention with the receive thread.

### Measured result

Server-side, request issued → message observed at the broker:

| control | latency |
|---|---|
| E-STOP | 0.53 ms |
| throttle | 0.63 ms |
| brake | 0.54 ms |
| direction | 0.50 ms |

Indistinguishable, which was the target.

---

## Change 2 — brake root cause

**The root cause is not in the dashboard, and this commit does not make the
brake work.**

The dashboard's publish path was inert for the same `onchange` reason as the
throttle, and that is fixed — `cmd/brake` now reaches the broker on every
input, verified. But the locomotive never hears it:

| layer | state |
|---|---|
| dashboard client | published on `onchange` only → **fixed**, now `oninput` |
| server `_cmd()` | `brake` is in the allowed set → was always fine |
| broker | message arrives → **verified** at `ngr/loco/9950011/cmd/brake` |
| **firmware** | **no `cmd/brake` channel exists** |

In `firmware/QUORUM/QUORUM.ino`:

- `T_ST_BRAKE` exists — a **state** topic, published once per connect as a
  retained inert `"0"`, commented *"no brake channel in SOLONAV"*.
- There is **no `T_CMD_BRAKE`** anywhere in the file.
- `subscribeAll()` subscribes to eleven command topics; `cmd/brake` is not
  among them.
- `handleCommand()` has no brake branch.

The channel existed in the pre-SOLONAV lineage — `archive/NGR_LL_DNA_CTO2_r12
_CONTINUITY_FIRST.ino:1553` defines `TOPIC_CMD_BRAKE`. SOLONAV dropped it
during the rewrite and QUORUM inherited the gap. The firmware header comment
is explicit that brake is published as an inert constant "so the console's
parsing does not break" — the console was kept compatible with a channel that
had been deleted.

**So the dashboard has been publishing into the void since the SOLONAV
rewrite.** Restoring the brake requires a firmware change: a `T_CMD_BRAKE`
topic, a `subscribeAll()` entry, and a `handleCommand()` branch deciding what
brake means for a locomotive whose only actuator is PWM. Firmware was out of
scope for this change, so it is reported rather than done.

---

## Verification

All against a live broker with `mosquitto_sub -t 'ngr/loco/+/cmd/#'`:

- **Throttle drag** 120 → 60 in six steps published **all six** intermediate
  values. Under v1.10.2 the same drag published **nothing** until release.
- **Brake drag** 0 → 64 → 128 published all three.
- **Three rapid same-value direction presses** published **3 of 3**. v1.10.2
  published 1 of 3 — the operator's deliberate re-press was swallowed.
- **Latency** for all four controls sub-millisecond and equal (table above).
- **E-STOP** unchanged: still the first branch out of `_cmd()`, before any
  check, in every state.
- AUTO-chamber behaviour untouched: `isCto` still suppresses manual sends
  while the dispatcher is in control, and the server still returns 423.

## A note on publish rate

`oninput` on a range control fires per pixel of travel, so a full-range drag
now emits tens of commands where v1.10.2 emitted one. This was checked rather
than assumed: the firmware's `cmdQueue` holds 16 entries and `serviceCommands()`
drains **all** of it every `loop()` pass at roughly 30 Hz, so even a 50 msg/s
drag leaves the queue at a depth of one or two. `cmd_drops` in `loopstat` is
the counter to watch if that assumption is ever wrong. No rate limit was
added, deliberately — the ruling is that nothing sits between input and
publish, and the measurement supports it.
