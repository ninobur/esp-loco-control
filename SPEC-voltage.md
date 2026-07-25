# Voltage Protection — PARKED (revisit later)

**Project:** esp-loco-control
**Status:** NOT an active work item. Do not implement. Notes only.

---

## Why this is parked

An earlier draft of this spec described removing an elaborate multi-stage
voltage system (throttle limiter, `lowVoltageThrottleActive`, sample
counter, shutdown latch, `MIN_THROTTLE_PWM`, `RECOVERY_VOLTAGE`,
`DISCONNECTED_VOLTAGE_THRESHOLD`).

**That work is already done.** The current firmware
(`NGR_LL_DNA_CTO2_r12_CONTINUITY_FIRST.ino`) does not contain that
machinery. Its voltage handling is already the lean, warning-only design
that was wanted:

- `serviceInaTelemetry()` reads bus voltage, current, and power and
  publishes them on the telemetry topics.
- It sets a single low-voltage warning flag when
  `busVoltage < LOW_VOLTAGE_THRESHOLD_V` (with a `> 0.1 V` guard so an
  absent/USB-only board does not false-trigger) and publishes it to
  `ngr/loco/<id>/state/lowvolt`.
- Nothing in this path touches the motor. Warning only.

So there is nothing to simplify.

---

## The one open question, for later

r12 **warns** on low voltage but does not appear to **cut off** the motor to
protect the pack. With a 4S LiPo, sustained over-discharge damages cells
permanently, so a genuine emergency cutoff below the warning threshold may
be worth adding.

Before doing anything:

1. Confirm against the full sketch whether any hard cutoff exists elsewhere
   that isn't visible in `serviceInaTelemetry()` alone.
2. If none exists, decide a cutoff voltage (the old design used ~3.31 V/cell
   = 13.25 V on 4S — a conservative, pack-friendly floor).
3. Keep it warning-first, cutoff-second: warn early, stop only at the floor.

This is a possible future work item, kept separate from Highline. Revisit
after Highline is working and after bench testing shows whether the pack is
actually at risk in normal running.
