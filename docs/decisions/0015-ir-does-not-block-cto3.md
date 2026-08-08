# 0015 — IR speed sensing does not block CTO3 development

Status: Accepted (2026-08-06). Sequencing decision.

## Decision

CTO3 development proceeds now on its IR-independent core. The IR wheel-speed
sensor matures on a parallel bench track and is **not** a prerequisite for
building CTO3. Only the §9 calibration campaign — the learned PWM→speed fallback
model — is deliberately sequenced to come *after* IR evidence is available.

Speed enters the architecture through a **source-tagged interface** (measured
speed + source + freshness), so IR slots in as a speed source without changing
the mission layer, the traffic layer, or the spec's structure.

## Context

The question was raised whether the CTO3 spec assumes PWM speed control and
whether to wait for IR before proceeding. It does not assume PWM control — §8
puts measured speed first and PWM last (see 0014). IR is expected to become an
implementation-ready second source of navigational and speed truth soon,
particularly valuable at low speed where Hall's 300 mm gap blinds measurement.

## Alternatives considered

- **Wait for IR before building CTO3** — rejected: most of CTO3 (the four doors,
  self-truth broadcast, peer registry, physical-envelope math, travel-direction,
  mission engine) is speed-source-independent. Waiting would block the whole
  architecture on a dependency that touches only one part of it.
- **Build the full PWM calibration campaign now** — rejected: if IR provides
  continuous low-speed measurement, the PWM fallback model becomes far less
  important, possibly vestigial. Running ~16 PWM levels × 2 directions × full
  loops before knowing that risks wasted laps. The calibration is therefore
  step 4+ in the sequence, after the route map is proven and IR's capability is
  known.

## Consequences

- IR changes speed *quality*, not speed *architecture*. If IR fully replaces
  Hall speed at all velocities, CTO3 spec §8/§9 *simplify* (PWM fallback shrinks
  toward vestigial) — a simplification, not a contradiction, which indicates the
  architecture is pointed correctly.
- The IR/QUORUM fusion contract remains explicitly unresolved (CTO3 spec §13)
  until IR is real; it must not be guessed from CTO2 constants.
- Development order (CTO3 spec §12): travel-direction verification and station-
  stop v1 first (IR-independent); calibration and fusion detail decided once IR
  evidence exists.

## References

- CTO3 spec §8, §9, §12, §13
- Decision 0014 (speed-hold); resources/SPEED_CONTROL_DISCUSSION.txt
