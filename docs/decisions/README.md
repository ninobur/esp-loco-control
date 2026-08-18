# Decision records

Short records of significant NGR design decisions: what was decided, why,
what was rejected, and what it depends on. They capture the reasoning behind
the specs and firmware, so a future reader (operator, Sam, CODEX, or a Claude
instance) can see not just what the system does but why — and what was
considered and rejected.

## Rules

- One record per significant decision. Numbered sequentially, never
  renumbered.
- A decision is never edited away. When something changes, add a NEW record
  that supersedes the old one, and mark the old one "Superseded by NNNN".
  The history of the thinking is preserved.
- Keep them short — half a page. Reasoning, not transcript.
- Write the record when the decision is made, not reconstructed later.

## Template

```
# NNNN — Title (the decision, stated as a sentence)

Status: Proposed | Accepted | Superseded by NNNN  (date)

## Decision
What was decided.

## Context
The situation and forces that made a decision necessary.

## Alternatives considered
What else was on the table, and why each was not chosen.

## Consequences
What follows — obligations, constraints, things now easier or harder.

## References
Specs, commits, reports, resource files.
```

## Standing practice

At the end of any session that makes an architectural or capability
decision, the decision is recorded here — including capability removals, so
a dropped brake/INA219/Blynk-style capability is a visible, argued record
rather than a silent drift discovered later.

## Index

| # | decision | status |
|---|---|---|
| [0001](0001-quorum-holds-position-on-disagreement.md) | QUORUM replaces tally navigation: position is held on disagreement, never discarded | Accepted |
| [0002](0002-locomotive-control-is-bicameral.md) | All NGR locomotive controllers are bicameral | Accepted (constitutional) |
| [0003](0003-one-control-sketch.md) | There is one control sketch; MANUAL is retired to reference | Accepted |
| [0004](0004-speed-filters-admit-unconditionally.md) | Speed filters admit every measurement; robustness lives in the output | Accepted |
| [0005](0005-timeout-means-blind-not-stopped.md) | An IR timeout means the sensor stopped seeing; STOPPED requires an independent witness | Superseded in part by 0036 |
| [0006](0006-envelope-decay-gates-on-signal-activity.md) | Envelope adaptation gates on signal activity, not wall clock or completed pulses | Accepted |
| [0007](0007-nvs-persists-only-the-proven-envelope.md) | NVS persists only a pulse-proven envelope snapshot | Accepted |
| [0008](0008-finescale-steel-wheel-is-the-target.md) | The 7-spoke finescale steel wheel is the production speed target | Superseded by 0022 |
| [0009](0009-diagnostic-thresholds-match-production.md) | The diagnostic uses production detection thresholds, with a written revert criterion | Accepted |
| [0010](0010-threshold-decisions-rest-on-headrooms.md) | Threshold decisions rest on headrooms, not crossing margins | Accepted |
| [0011](0011-restore-the-brake-channel.md) | The brake channel is restored to the control firmware | Accepted |
| [0012](0012-restore-ina219-telemetry.md) | INA219 telemetry is restored to the control firmware | Accepted |
| [0013](0013-bicameral-four-door-authority.md) | Bicameral locomotive authority has four crossing doors | Accepted |
| [0014](0014-speed-hold-not-throttle-hold.md) | Speed is the controlled variable; PWM is only the actuator | Accepted; not yet implemented |
| [0015](0015-ir-does-not-block-cto3.md) | IR speed sensing does not block CTO3 development | Accepted |
| [0016](0016-published-evidence-carries-its-own-provenance.md) | Published evidence carries its own provenance; unknown is never fabricated | Accepted |
| [0017](0017-hall-baseline-adapts-only-in-motion.md) | The Hall baseline adapts only while the locomotive is believed moving | Accepted |
| [0018](0018-firmware-role-and-evidence-status.md) | Firmware role and evidence status are tracked independently | Accepted |
| [0019](0019-merged-pulse-evidence-is-the-raw-waveform.md) | The merged-pulse question is answered by a raw-waveform scope, not threshold tuning on event telemetry | Accepted |
| [0020](0020-compensation-requires-observed-evidence.md) | No compensation mechanism is added without demonstrated need | Accepted |
| [0021](0021-ir-speed-is-local-and-telemetry-is-summary.md) | IR speed is computed locally and telemetry reports summaries | Accepted |
| [0022](0022-plastic-ten-spoke-wheel-is-the-ir-target.md) | The LGB plastic 10-spoke wheel is the IR speed target | Accepted |
| [0030](0030-train-extent-is-a-distance-and-the-producer-applies-it.md) | Train extent is +2 / -4 markers, in per-loco config, applied by the producer | Superseded in part by 0033 |
| [0031](0031-no-quorum-on-any-locomotive-stops-every-locomotive.md) | NO_QUORUM on any locomotive stops every locomotive, enforced by absence | Accepted |
| [0032](0032-roles-are-derived-coordination-state-latched-and-echoed.md) | Leader/follower is derived coordination state — latched, echoed, never negotiated | Accepted |
| [0033](0033-separation-is-the-bubble-plus-six-markers-clear.md) | Separation is the occupancy bubble plus six markers clear; Hall gaps are derived diagnostics | Accepted |
| [0034](0034-v1-14-cto-wire-and-membership-contracts.md) | v1.14 CTO wire and membership contracts — role-echo packet, first-seen-this-boot membership | Proposed |
| [0035](0035-quarantine-the-doubtful-and-never-stop-learning.md) | Quarantine the doubtful, and never stop learning — floor, suffix rescue, self-resolution | Accepted |
| [0036](0036-a-stationary-ir-wheel-reports-zero-without-upward-authority.md) | A stationary IR wheel reports zero, but zero never grants upward motor authority | Accepted |
