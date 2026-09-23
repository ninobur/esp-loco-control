# NAVI_COHERENCE 0.6: IR health field instrumentation

2026-09-23, Codex. David requested IR health indicators in the sketch before
a few field-test laps. This follows the tested independent architecture 0.4,
Sam's Epoch design and Claude's ordinary-outage recovery review.

## Scope and choices

Delivered `firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/`, based on the
complete 0.5 AUTO_ENABLED build. The original 0.5 and common TX are unchanged.
This is observation-only integration, not replacement of navigation's existing
IR adapter. AUTO remains enabled under its existing rules.

The cyclometer supplies measurements and health. NAVI supplies accepted MM
identity to a shadow reference. Instrument faults neither relocate Toby nor
issue motor commands. The monitor has no return path into navigation evidence.
`ref_basis=NAV05_ACCEPTED` explicitly prevents mistaking agreement with the
existing navigator for independent landmark validation.

Engineering choices (Codex implementation, not additional operator rulings):

- Separate raw-packet admission makes calibration changes, health and continuity
  observable even when the old adapter rejects them. Existing adapter calls
  and all eight baseline headers remain unchanged.
- A one-second watchdog runs without packets and also checks between arrivals
  while draining the queue. Duplicate/old packets cannot refresh it. Queue loss
  ends the epoch and fences out packets queued before the loss was noticed.
- Keep 64 timestamped observations, including unavailable points, for Hall
  alignment. Use the existing minimum-offset clock mapping and 150 ms nearest
  gate. Reject invalid nearest points instead of searching for a more convenient
  healthy one. These tolerances still need field evidence.
- The accepted Hall identity is associated with its opening timestamp after
  any existing sequence correction. Declaration/reversal clears the reference
  without resetting instrument continuity. Epoch changes invalidate old
  references automatically, independently of external invalidate calls.
- JSON separates unavailable (`null`) from measured zero. Serial gives compact
  health/readiness/epoch/reference indicators. MQTT adds nonretained
  `diag/ir_health` under the existing loco prefix. One-second heartbeats and
  200 ms change coalescing limit load; counters expose totals but do not preserve
  every transition's timestamp. No server/UI contract was changed.

## Verification and limits

ESP32 core 3.3.11, `esp32:esp32:esp32`, warnings enabled: PASS, 1,012,715 bytes
flash (77%), 72,204 bytes static RAM (22%). Three existing Adafruit_INA219 enum
warnings, no sketch warnings. Host C++17 with warnings-as-errors, ASan and UBSan:
monitor PASS; existing navigator PASS (2,052 offset corrections, 6,840 holds);
station correction PASS (32 approaches). Python parsed 11 real monitor JSON
outputs, maximum 546 bytes. Baseline header/test comparison confirms unchanged
navigation implementation. The sketch entry-point diff contains only diagnostic
hooks, version/banner changes and the additional topic.

Monitor tests cover healthy zero dwell, readiness, silent timeout, repeated
duplicates, ordinary recovery, delayed Hall alignment, unavailable nearest
samples, epoch boundaries, queued-loss fencing, foreign source, CRC, hidden
fault counters, calibration change, reboot/retired boots, source replacement,
old reference invalidation and payload sizing. Host tests are not field proof.

Current TX can report inadequate contrast after constant illumination at a
stop. That limitation remains visible, not suppressed. Healthy zero handling
does not make an unhealthy detector ready. Legacy navigation still has its
old admission/trust behavior; this build does not claim to have replaced it.
The later twenty-question navigation changes remain pending.

Read-only Pi inspection found both `ngr-runlog.service` and
`ngr-ir-espnow-record.service` active/running. This verifies recorder processes,
not reception of the new topic before flashing. The deployed run logger's
subscription is `ngr/#`, which includes the new diagnostic topic. Confirm actual 0.6 boot and
health traffic before the first lap. No firmware flash, motor command, Pi
service modification or field run was performed during preparation.

## Field handoff

Use the sketch README for upload at 115200 and the short dwell/manual-lap/
restart/subsequent-lap sequence. Preserve start interval, direction and approximate
stop/lighting-transition times. First questions: does healthy zero stay zero;
when and why does continuity end; does recovery start a new epoch; does a later
accepted Hall establish a fresh reference; do new diagnostics affect timing or
queue drops? Do not infer trustworthy position merely from `ref_valid=1`.
