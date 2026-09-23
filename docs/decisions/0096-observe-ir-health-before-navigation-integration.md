# 0096: Observe IR health in 0.6 before changing navigation

Date: 2026-09-23. Status: implemented field candidate; not field accepted.

## Authority and scope

David requested: "Would you like to incorporate IR health indicators into the
Sketch as the next step. Then, I would like to run a few laps for a field test."
This continues his cyclometer/rider boundary and health-first sequencing in
0094 and 0095. It does not supersede the remaining twenty-question decisions.

## Implementation choice

Codex chose observation-only integration in NAVI_COHERENCE 0.6, retaining the
complete 0.5 navigation/AUTO baseline. A parallel monitor validates paired IR
packets and exposes health, readiness, irreversible Epochs and a NAVI-owned
shadow MM reference through Serial and a new diagnostic MQTT topic.

The monitor cannot supply navigation rulings or issue motor commands. It uses
the original Hall event timestamp for its reference. Reference identity is
labeled NAV05_ACCEPTED, not independently verified truth. Zero and unavailable
are distinct. New READY data starts an epoch immediately after continuity loss;
old references cannot revive. Resetting NAVI's frame clears the reference, not
the cyclometer's measurement history.

## Consequences

These laps can assess health and continuity without simultaneously changing
navigation algorithms. Existing legacy IR behavior and the TX stationary
contrast limitation remain, explicitly documented. Additional diagnostic load
needs observation. No claim of field acceptance follows from a successful build.

Verification, limitations and the short test sequence are recorded in
`docs/NAVI_COHERENCE_0_6_IR_HEALTH_20260923.md` and the sketch README. No flash or
motor command was part of this implementation.
