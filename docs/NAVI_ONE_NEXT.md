# NAVI_ONE — open items

## 1. END AO deserves a gentle stop (operator, 2026-08-29)

> "Absent EStop, END AO should have a gentle deceleration ramp like at the
> stations."

Today every automatic deceleration uses `AUTO_STEP_DOWN_MS` = 31 ms/step,
~2.8 s from PWM 90. That is right for a fault and wrong for an operator ending
a session.

**Ruled by the operator, 2026-08-29:** the steep ramp stays for one-strike and
low voltage, and the reason is not safety — it is *communication*.

> "Steep ramp is informative. It signals that something is wrong."

The way a locomotive stops is a channel. A gentle stop says the session ended;
a hard one says go and look. Two stops that felt the same would throw that away.

| stop | rate | why |
|---|---|---|
| E-stop | instant, no ramp | unchanged |
| **END AO / dispatcher release / `cmd/auto 0`** | **~200 ms/step** (~9 s from 45) | an operator ending a session, not an emergency. Matches the passenger-gentle station pacing of QUORUM v1.12B |
| one-strike (WRONG_MAGNET) | 31 ms/step | **informative**: the manner of the stop tells the operator something is wrong |
| low voltage | 31 ms/step | **informative**, same reason |

Not built. `requestPwm(target, up, down)` already takes a per-step down rate, so
this is one constant and three call sites.

## 1b. The 0.1 review — closed

All thirteen findings disposed of in `NAVI_ONE_0_3`; see
`docs/reviews/NAVI_ONE_0_1_REVIEW_CLOSEOUT_20260830.md`. Decisions 0058-0062.
Two things are deliberately open:

* **The Gaussian fit runs on the sampling task** (finding 13.9). Wants one
  bench measurement of tick jitter at passage close — not a redesign.
* **Two display-only cross-thread reads** stay uncontracted, on purpose.
* **A comms watchdog** — ruled out by the operator; see item 1d.

## 1d. In AUTO, nothing watches the radio — RULED, and closed

If MQTT drops while AUTO is running at cruise, the locomotive keeps going and
the dashboard's stop button reaches nothing. Raised 2026-08-30 as a possible
new stop condition. The operator ruled it out the same day:

> No. under most conditions, I could flip the switch if coms stopped working.

There is a human with a switch, standing at the railway. That is the watchdog,
and it is a better one than a radio timeout that would stop the locomotive
every time the broker blinked. Not to be re-raised without new evidence that
the switch is not reachable.

## 1c. Six markers is the real bound

Decision 0059. A missed magnet advances silently about half the time and can
stay hidden for up to six markers, ~1.8 m, before the polarity chain refuses it.
The ten-magnet witness does not close this and cannot — it is a tautology.
Closing it needs distance, which is the IR thread's problem (item 4).

## 2. Stations are absent from AUTO

Deliberate, per the operator: prove the circuit first. AUTO currently cruises at
a fixed PWM and stops only on fault or release — there is no completion
condition in the code, contrary to what this document said before.

## 3. Which navigator governs

Decisions 0053/0055 (NAVI_2, two tests) and 0054 (NAVI_CL2, four tests) both
read as current and neither mentions the other. NAVI_ONE now supersedes both in
practice, on evidence, but nothing says so on the record. One short decision
would fix it — pending the operator's ruling on which lineage is current.

## 4. The IR observer

Branched to its own thread. See `docs/IR_DEV_REC/2026-08-29_IR_HANDOFF.md`.
Note that presence must be DECLARED, not probed — the probe reasoning in the
current sketch is backwards and will call a fitted-but-stationary sensor absent.

## 5. Watch `floor_rej`

It reached zero once the floating pin 34 stopped being sampled. If it climbs
again, either the ADC gating regressed or the probe wrongly decided IR was
fitted. Nothing reaches the recognizer either way; it is margin, not safety.

## Where it stands, 2026-08-29

**528 accepted, 0 refused, 0 non-magnets — 3.09 circuits of the Lowline,
`trust: PROVEN` throughout.** Gates: survey 195/195 + 156/156, contract 87/0,
real-lap replay 172 advances closing at MM040.

## Where it stands, 2026-08-30

`NAVI_ONE_0_3`, **not flashed**. The strike latches, the whole command layer is
parsed strictly and gated on standing still, the publish queue holds through a
broker blink, battery policy comes from Toby's profile, and IR presence is
declared rather than probed.

**FOUR gates now, all green:** survey 195/195 + 156/156 misclassified 0;
contract 176 checks 0 failures; **ops 72 checks 0 failures** (the .ino layer and
`HallCapture`, neither of which had ever been driven by a test); real-lap replay
172 advances closing at MM040 with every record in the file accounted for.

Compiles at 959,291 bytes (73%), 51,748 globals; banner `NAVI_ONE_0_3 —
9950012`, verified in the compiled image. Selecting Otto now stops the build,
by design.

Every flash deserves a test run, and this one has not had it. Two behaviours to
watch on the first run: the battery band is now 13.25/14.0 from the profile
rather than a single 14.4, and a low-voltage stop ramps steeply instead of
cutting. The console field `moving` is now `powered`.
