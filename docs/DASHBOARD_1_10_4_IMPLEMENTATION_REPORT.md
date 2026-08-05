# Dashboard v1.10.4 — implementation report

**Describes commit e85aded (`server/ngr_app_v1_10_4.py`).** Four bugs from
v1.10.3 field testing. Firmware untouched.

---

## Bug 1 (safety) — E-STOP did not zero the throttle

The slider kept its old value through an E-STOP, so clearing E-STOP could
resume at speed. The firmware zeroes PWM itself on estop; what was wrong was
that the **operator's control disagreed with the locomotive**, and the control
is what the operator acts on next.

Fixed in `toggleEstop()`: the estop publish is still the first statement and
is not gated by anything. The throttle-zero is issued *behind* it — slider to
0, display to 0, `cmd/throttle 0` published — on **both engage and clear**, so
the control reads 0 whenever E-STOP has been touched. The throttle-zero is
itself ungated (a direct `fetch`, not via `sendCmd`, which would have
suppressed it under AUTO).

**Verified at the broker**, in order:

```
ngr/loco/9950011/cmd/throttle 150     <- operator drives up
ngr/loco/9950011/cmd/estop 1          <- E-STOP, first
ngr/loco/9950011/cmd/throttle 0       <- zero, behind it
```

Slider went 150 → 0 visibly; display "150 / 255" → "0 / 255". Clear path
tested separately: 90 → 0.

---

## Bug 2 — direction bounced back to Neutral

Two independent mechanisms. Only the first is a dashboard defect.

### (a) Ours — the optimistic light was wiped

`updateDirButtons(d, confirmed)` cleared **all** button classes and re-added
one only `if (confirmed)`. `confirmed` was `ageOf(s,'direction') !== null` —
false until the locomotive reports a direction. So the optimistic light from
the tap survived only until the next poll, one second later, and then went
out. Compounding it, `_fresh_state()` defaults `direction` to `"1"` —
**Neutral** — so the latent value the display fell back toward was Neutral.

Fixed: `lastCommandedDir` remembers what the operator tapped. Precedence is
now (1) the locomotive's reported direction if it has ever reported one,
(2) otherwise the operator's last command, (3) otherwise nothing. Never a
default, never blank.

**Verified**: with `ages.direction === null` (locomotive never reported), tap
Forward → lit immediately → still lit after repeated poll cycles. v1.10.3
blanked it on the first poll.

### (b) The locomotive, correctly — and it was never explained

QUORUM refuses `cmd/direction` outright:

```c
if(autoRunning){ stationPublish("DIR_REFUSED",0,"AUTO_IN_CONTROL"); return; }
if(motorIsMoving()){ stationPublish("DIR_REFUSED",0,"WAIT_FOR_STOP"); return; }
```

with `motorIsMoving()` being `actualPwm>0 || commandedPwm>0`. On refusal it
keeps reporting its previous direction — and after **any** E-stop that is
NEUTRAL, because the firmware forces `motorDirection=DIRECTION_NEUTRAL` on
both engage and clear.

So the likely field sequence is: E-stop (or fresh session) leaves the
locomotive in Neutral → operator raises the throttle → operator taps Forward
→ **the locomotive refuses because it is already moving** → it goes on
reporting Neutral → the dashboard faithfully mirrors Neutral. The doctrine
says mirror what the locomotive reports, and v1.10.3 did exactly that; it
simply never said why, so a correct refusal read as a glitch.

The disagreement is now named:

> FORWARD NOT ACCEPTED — OTTO REPORTS NEUTRAL (STOP FIRST: DIRECTION IS
> REFUSED WHILE MOVING)

Display only. Buttons stay live, the command path is untouched, and nothing
is gated on it. **Verified**: with the locomotive reporting Neutral after
Forward was commanded, Neutral lights, Forward clears, the explanation
appears, and the buttons remain enabled.

> Operational note: the order that works is **stop, select direction, then
> throttle**. Selecting direction while rolling will always be refused by the
> firmware, and that is deliberate — it is the guard that stops a loco being
> thrown into reverse under power.

---

## Bug 3 — INA219 root cause

**No locomotive publishes voltage, current or power. The dashboard binding is
not wrong; the data source does not exist.**

`buildTopics()` in QUORUM builds 28 topics. Not one is a `telem/` topic —
there is no `telem/voltage`, `telem/current` or `telem/power`, and the string
"INA219" does not appear anywhere in the sketch.

Tracing the lineage:

| firmware | INA219 references |
|---|---|
| `NGR_LL_DNA_CTO2_r12_CONTINUITY_FIRST` | 7 — publishes `telem/voltage` |
| `SOLONAV_1_0` | 7 |
| `SOLONAV_1_2_MEDIAN_BASELINE` | 7 |
| `SOLONAV_1_3_MEDIAN_GUARD` | 7 |
| **`SOLONAV_2_1`** | **0 — dropped here** |
| `SOLONAV_2_2` … `SOLONAV_2_14` | 0 |
| `QUORUM_1_4` | 0 |

Dropped at **SOLONAV 2.1**, the single-locomotive rewrite that also removed
multi-train awareness. QUORUM inherits the gap — the same shape as the
missing `cmd/brake` channel found in v1.10.3.

**So the "—" is correct behaviour, not a bug.** It is v1.10.0's governing rule
working as designed: show nothing rather than a remembered number. The
15.4 V / 1.8 W figures that used to appear on these tiles were **retained MQTT
ghosts** from the r12/SOLONAV 1.x era — precisely the failure v1.10.0 was
built to stop, and the reason those three tiles were among the documented
stale-data casualties.

No values are fabricated. A footnote now appears beneath the tiles once the
locomotive is otherwise talking:

> VOLTAGE / CURRENT / POWER NOT PUBLISHED BY THIS FIRMWARE — INA219 telemetry
> was removed at SOLONAV 2.1. Not a sensor fault.

It is suppressed when nothing at all has been heard, because the status line
already covers total silence and the note would be noise on top of it.

Restoring these readings is a firmware change: an INA219 read and three
`telem/` publishes. The hardware is still on the locomotive (`CLAUDE.md`
records the INA219 on I²C, SDA 21 / SCL 22) — only the firmware support went.

---

## Bug 4 — command path verified intact

Not a regression. The v1.10.3 fix is present and unmodified:

| control | handler | between input and `fetch` |
|---|---|---|
| throttle | `oninput="sendCmd('throttle',…)"` | `if (isCto) return` only |
| brake | `oninput="sendCmd('brake',…)"` | `if (isCto) return` only |
| direction | `onclick="sendDir(d)"` | `if (isCto) return` only |

Server `_cmd()` still returns for estop before any check, and the manual path
still takes **no lock**.

Measured request-to-broker on v1.10.4: E-STOP 0.90 ms, throttle 0.58 ms,
brake 0.58 ms, direction 1.19 ms.

The field impression that the display "waits on confirmation" was **Bug 2(a)**
— the optimistic light being wiped one second later by the poll — not a
command-path defect. The command went out immediately; only the light went
away.

---

## Two firmware gaps now on the record

Both found by tracing a dashboard symptom to its source, and both the same
shape — a channel that existed in r12 and was dropped in a rewrite while the
console kept a control for it:

1. **`cmd/brake`** — no topic, no subscription, no handler (v1.10.3 report).
2. **INA219 `telem/voltage|current|power`** — dropped at SOLONAV 2.1 (this
   report).

Neither is fixable from the dashboard. Both need an operator decision about
whether the feature is wanted back.
