# 0014 — Speed is the controlled variable; PWM is only the actuator

Status: Accepted (2026-08-05). Design intent; not yet implemented in firmware.

## Decision

Automatic speed control governs **actual measured speed (pKPH)**, not PWM. The
commanded mode is **SPEED_HOLD**, not THROTTLE_HOLD: the operator or a mission
sets a target speed, and the ESP32 varies PWM within safe limits to achieve it.

Speed evidence follows a source hierarchy:

1. **HALL_VALID** — recent magnet intervals fresh → measured speed is truth and
   governs PWM.
2. **HALL_AGING** — last measurement getting stale → blend with model, cautious.
3. **PWM_ESTIMATE** — starting / crawling / post-reset → use the learned
   PWM→speed model (see CTO3 spec §9) as best estimate. A source of truth, not the
   truth.
4. **MOTION_UNCONFIRMED** — PWM commands motion but no magnet arrives in the
   expected window.

**Hard safety law (the "U-Haul in the median" rule):** the controller must never
respond to "speed reads zero / not moving" by escalating PWM toward maximum. If
PWM is high and no magnet arrives, declare MOTION_UNCONFIRMED / STALL_SUSPECT and
do not increase PWM. If the speed source is unreliable, do not act as cruise
control; fall back to a cautious, bounded mode. When measurement is untrustworthy
the train becomes **more conservative, not more aggressive**.

Navigation is limited by **speed, not throttle**: empirical ceiling ~70 pKPH
sustained (caution), map updates untrusted above ~75, navigation unreliable
above ~80. These are speed thresholds, applied regardless of PWM.

## Context

PWM had been used as a speed surrogate only because no true speed gauge existed.
The operator rejected continuing to govern by PWM once speed data is available,
with the analogy: you do not tell a new driver "press the pedal halfway and back
off 5% if you feel fast" — you say "hold 55." The same PWM produces very
different speeds depending on grade, curve, load, and battery voltage, so PWM is
effort, not result. A concrete hazard motivated the safety law: a cruise-control
car that responds to "not moving" (asleep driver, stall) by flooring itself ends
up doing donuts in the median.

A field boundary made the ceiling concrete: clean navigation at 50–69 pKPH,
cascade failures (MM_OVERSPEED, MISSED_MM, polarity mismatch) at 78–82 pKPH.

## Alternatives considered

- **Governor trims PWM around a setpoint ("increase by 5 if slow")** — rejected
  as the *governing* principle: it keeps PWM as the authority with speed as a
  nag, and the crude ±5 step risks either sluggishness or, at the "not moving"
  edge, dangerous escalation. Retained only as a bounded, smooth actuator
  behavior, never as the control philosophy.
- **Fixed maximum PWM as the safety limit** — rejected: a fixed PWM ceiling is
  simultaneously too fast downhill and too weak uphill. This is why CTO2/CE ran
  CCW-only (to avoid Viaduct Hill clockwise). Speed-based governing removes that
  constraint and makes clockwise automation viable.

## Consequences

- Automatic operation must not depend on one fixed PWM setting; it uses target
  speed plus segment-aware PWM authority.
- Requires a trustworthy speedometer, which drives the learned-model and
  (eventually) IR work (0005, 0015).
- A hard speed floor applies: below ~2.5 s/marker, magnets can't measure speed
  and pull the baseline; no behavior may command a sustained crawl (see also
  the navigation speed-floor constraint).
- Implemented as CTO3 spec §8; firmware pending (development sequence step 5).

## References

- CTO3 spec §8; resources/SPEED_CONTROL_DISCUSSION.txt
- CTO3 intent baseline §"Speed-control direction"
