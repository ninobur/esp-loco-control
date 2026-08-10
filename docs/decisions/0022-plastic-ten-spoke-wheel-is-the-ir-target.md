# 0022 — The LGB plastic 10-spoke wheel is the IR speed target

Status: Accepted (operator, 2026-08-10)

## Decision

The current IR speed prototype targets the molded-plastic LGB 10-spoke wheel.
Its measured rolling circumference is 3.8 in = 96.52 mm, producing 9.652 mm
per completed spoke pulse. This supersedes decision 0008's 7-spoke finescale
steel-wheel target.

## Context

Decision 0008 relied on earlier event telemetry that appeared to show a blind
arc on the plastic wheel. Synchronized Hall/IR capture later proved that the
dominant missing-event signature was telemetry loss, not missed optical
events. Night waveform work showed smooth, repeatable plastic-wheel pulses;
the daylight out-and-return test found no saturation and local pulse/Hall
ratios agreeing within approximately 1.2% after reversing the car.

The operator measured ten loaded rolling revolutions three times: 38 in,
37 7/8 in, and 38 1/8 in. Their mean is 38 in per ten revolutions, or 3.8 in
per revolution. This direct measurement controls over nominal catalog
diameter and assumed compression factors.

## Alternatives considered

- **Retain the 7-spoke steel wheel.** Rejected as the current target: its
  apparent advantage was partly created by defective telemetry and a stale
  15 ms diagnostic debounce that deleted valid events at speed.
- **Use nominal 30 mm diameter / 94.25 mm circumference.** Rejected for
  calibration: direct loaded rolling measurement is available.
- **Apply an assumed 0.5% plastic compression correction.** Rejected: it was
  not measured on this car and conflicts with the rolling result.

## Consequences

- Spoke count is 10 and rolling circumference is 96.52 mm.
- Distance per pulse is derived from those inputs, never duplicated manually.
- Earlier 7-spoke speed figures and phase bins do not calibrate this wheel.
- The target remains subject to the written daylight gate before any QUORUM
  integration or field-accepted claim.

## References

- decision 0008 (superseded)
- `docs/IR_DEV_REC/2026-08-09_SYNCHRONIZED_HALL_IR_LAP.md`
- `docs/IR_SPEED_LOCAL_QC_REVIEW.md`
- `firmware/test-programs/IR_SPEED_LOCAL/README.md`
