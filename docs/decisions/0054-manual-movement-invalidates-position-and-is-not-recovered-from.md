# 0054 — Manual movement of the locomotive invalidates position, and the navigator does not recover from it

Status: Accepted (operator, 2026-08-29)

## Decision

Moving a locomotive by hand destroys its position. The navigator does not
detect, compensate for, or recover from it. The operator re-declares.

No mechanism will be added to distinguish "being pushed" from "standing
still". The motion clock stays as it is: it accrues only while
`actualPwm > NAVI_MOVE_PWM_FLOOR`, so magnets passing under hand power are
refused, and those refusals are **correct** — not a fault to be fixed.

Operator: *"Recovery from manual movement is not expected. It destroys Toby's
truth."*

## Context

On the evening of 2026-08-29, during recovery from a derailment, four genuine
magnets (peaks 210–224, polarity matching the expected target) were refused as
`REBOUND` with `moved_ms = 0` while the operator moved Toby by hand. Position
drifted about five markers across that episode, from a combination of these
refusals and two low-amplitude false accepts.

I characterised this as a defect of my own making and proposed two fixes: give
the motion clock to the IR wheel sensor, or let a longer wall-clock bound
release the debounce. Both were attempts to make the navigator survive being
picked up.

The operator's ruling is that this is the wrong goal. A locomotive that has
been lifted, pushed, or re-railed has no position that any magnet can confirm,
and machinery that manufactures one is the same error as the offsets: admitting
a situation the model cannot honestly represent, then compensating.

## Evidence that the exposure is narrow

The same evening, immediately after, Toby ran **manual under power** for 32
consecutive markers with zero refusals — through a power cycle, a
re-declaration, two stops and a full reversal, ending at the operator's visually
confirmed 40-41 with `trust: PROVEN`.

Manual mode is not the problem and never was. Detection and identity are
bit-identical across modes; `naviHalt()` contains the only line in the entire
path that reads the mode. The failing case is specifically *magnets passing
while the motor is not providing the motion*, which happens only while a person
is handling the locomotive.

## Alternatives considered

- **IR wheel pulses as the motion clock.** Rejected. It couples the IR sensor —
  which has now demonstrated silent failure in both directions, undercounting up
  to 39% and overcounting 3–8× — to the refusal path, and it does so in order to
  recover from something that should not be recovered from.
- **A longer wall-clock bound releasing the debounce.** Rejected. It reopens the
  stop-on-the-magnet rebound the operator identified on 2026-08-28, which
  polarity then catches only about half the time.
- **Treating the refusals as a bug and suppressing them.** Rejected: they are the
  navigator correctly reporting that it cannot name the magnet it just saw.

## Consequences

- Handling the locomotive requires a re-declaration. This is rule 1 of decision
  0053 doing its job, not an additional burden.
- Refusals during handling are expected output, not faults. They should not be
  counted against the navigator in any field record.
- With rule 5 literal (`5bd37aa`), handling a locomotive in AUTO will stop it.
  That is intended.
- The IR observer remains an observer. This decision removes the only proposed
  reason to give it any authority.

## References

- decision 0053; `firmware/test-programs/NAVI_2/NAVI_2.ino`
- `docs/research/20260829_A_CLEAN_LAP.md`
- field evidence: `~/NGR/telemetry/all_20260829.log`, 18:17–18:19 (the refusals)
  and 18:36–18:40 (32/32 driven manual)
