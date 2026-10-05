# 0122 — Type-5 pitch is configuration and calibration metadata has no runtime authority

Status: Accepted for the EWO candidate (2026-10-05); not flashed or field accepted
Decided by: David's explicit Document B implementation instruction; implementation findings by Codex

## Decision

David instructed: “Pitch is configuration, not runtime state” and “Remove the
fictional lifecycle, not measurement validation.” The installed pitch is
9652 µm per completed pulse. EWO checks that configured value and the
overflow-safe pulse/distance product. A different pitch is malformed evidence,
not a supported configuration transition.

The Type-5 calibration ID remains raw wire/recorder/telemetry metadata. It
neither changes the measurement epoch nor exempts a report from ordering
checks. Source and boot identify the stream. Genuine sequence, capture-time,
pulse rollback and local receive-time discontinuities retain their existing
protection. Invalid measurements use `PACKET_INVALID`, not a calibration state.

## Context and consequences

Codex traced the current TX's measurement construction to fixed ID 0 and
9.652 mm/pulse, with no runtime setters. EWO's inherited comparison nevertheless
treated either field changing as a new frame, reset evidence/speed and bypassed
same-frame ordering. That behavior is removed without a replacement lifecycle.

The historical `IrOdometryEpoch` and older NAVI_COHERENCE admission machinery
are not in EWO's firmware include path. They remain prior art. EWO's runner now
tests its actual continuity model instead of adopting that reference engine's
calibration/pitch transitions and diagnostic epoch breaks. The shared optical
classifier remains in use; existing binary diagnostic numbers are preserved.

Cost: a future genuinely different wheel installation needs an explicit
configuration/architecture change. Raw calibration metadata will not signal
such a change on its own. This pass does not resolve EWO's existing source
selection limitation, redesign station/adaptive braking, implement Document C,
or change Type-5/CTO2 wire contracts.

## References

- [Document B](../NAVI_VESTIGIAL_CODE_CLEANUP_ARCHITECTURE_20261005.md)
- [Current EWO implementation](../../firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/README.md)
- Starting/rollback commit: `b42e0d83ee98253ac72427198fd37953b11ac526`
