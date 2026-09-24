# IR TX 1.6 Stop Retention: Review Handoff

Historical handoff for `1de06ae`: subsequently rejected by independent review
`4de40df`. Do not use the pass results below as bench approval. The revised,
unflashed R2 response is `IR_TX_1_6_R2_REVIEW_RESPONSE_20260923.md`; the original
design and results below are retained as history.

Status: prepared; software tests pass; independent review and physical bench
acceptance pending. Not flashed as of this record. Connected IR-car ESP32 MAC
38:18:2b:30:8c:2c was read on /dev/cu.wchusbserial110. This is not Toby.

## Operator Evidence

2026-09-23 screenshots at 18:52:56 and 18:53:02: near MM063, IR measured
3.3 pKPH before stopping, then reported INADEQUATE_CONTRAST at PWM0. Hall
retained its previous 6.3 pKPH. Operator also saw recovery after increasing
speed and a brief WARMUP before MEASURED. These are observations, not a
calibration/accuracy certification. PWM0 is not proof of stopped wheels.

## Root Cause and Change

The common detector computes a 5th/95th-percentile envelope over 512 samples
at 1 kHz. Constant low/high levels eventually erase its contrast; an open
pulse also times out at 2.5 seconds. This makes previously valid stationary
signals unavailable and can impair very slow cycles.

An opt-in retainStationary mode is enabled only by IR_SCOPE_ESPNOW_TX 1.6.
Other users retain their existing default behavior. A completed cycle with
at least 300 ADC counts of current envelope span earns a retained reference.
Until earned, stationary noise/flat input remains unavailable, not zero.

While the reference is retained:

- Hold thresholds through a collapsed live window; preserve open/closed phase
  across a stop rather than timing out a legitimate high plateau.
- A completed cycle with sufficient live span refreshes the reference.
- Quiet compatible input reports existing SIGNAL_STALE, which Toby's new
  health/epoch contract already treats as measurement-ready, not a dead link.
- Saturation, sample gaps, incompatible amplitude or a flat mid-band signal
  revoke the reference. Restart must reacquire rather than reuse bad history.
- No invented pulses, no new wire fields, no change to wheel pitch or bounds.

The amplitude compatibility margin is one-quarter of the learned span; the
mid-band test uses the existing 120-count contrast gate and learned hysteresis
thresholds. These are engineering choices to review and test physically, not
measured/certified optical error bounds. A quiet window is not inherently
evidence of a fault, but neither is it universal proof of sensor integrity.

## Important Limits for Reviewer

1. A stuck electrical input or optical obstruction at a learned low/high level
   is indistinguishable from a stationary wheel with this one channel. Do not
   claim this patch detects that fault. Operator coupling and independent
   Hall/movement evidence remain separate qualifications.
2. Startup at extremely low speed may not establish both levels within the
   live window. This patch retains a proven reference; it does not invent one.
3. Mid-transition stops remain ambiguous and can report unavailable.
4. Bright-sun transitions and realistic waveform replay need review. Tests
   with idealized plateaus cannot certify field pulse accuracy.
5. The speed display consumes the newer health contract. Ordinary NAV Hall
   admission still uses older MovementEvidence checks that reject SIGNAL_STALE
   and changed unreliableSamples. This patch does not unify those paths or
   claim navigation/control behavior is unchanged merely because it is in TX.
6. Hall's held last-interval speed and the staged throttle-slider label seen
   in the screenshots are separate display issues, not fixed here.

## Verification

tools/test_ir_stationary.cpp: ASan/UBSan, 120-second low and high holds,
exact completed counts on restart, ten cycles with four-second half-periods,
startup/noise, middle level, shifted level, saturation and sampling gap.

tools/test_ir_stationary_pipeline.cpp: the real Measurement, type-5 encode/CRC,
IrHealthMonitor, IrOdometryEpoch, IrSpeedTelemetry and qualification/JSON:
both two-minute holds preserve the epoch and give at least 118 valid zero
samples, restart gives positive measured speed, saturation ends the epoch,
radio staleness is unavailable, and unproven startup never becomes valid zero.
Nine emitted JSON payloads parsed and checked for zero versus null.
Existing detector, phase-retention, movement-contract and wire tests pass.

ESP32 3.3.11 build: 901,264 bytes flash, 64,288 bytes static RAM. Existing
volatile-increment warnings remain in TX. Corrected the TX shared-header
relative include for its actual repository location; this was needed to build
the canonical sketch in place.

## Review Request and Bench Plan

Please review whether a single high-contrast completed cycle is sufficient
to earn retention, threshold refresh behavior, revocation conditions, pulse
accounting across a long high stop, and the explicit limits above. Do not
approve by merely repeating the compile/test results.

After review or explicit operator authorization for bench-only testing:
flash IR car only at 115200, confirm version 1.6, roll enough to acquire,
stop on low and high phases for 30-60 seconds, restart, then very slow rolls.
Check raw pulse count, health, epoch, displayed zero and restart speed.
Exercise a real optical disturbance and power-cycle. No AUTO test until
the bench results are reviewed. RX needs no firmware change.

The existing NGR-Files/IR_SCOPE_ESPNOW_TX copy is older than the canonical
repo TX and must be reconciled with backup before calling it ready in IDE.
No local sketchbook replacement has been performed by this change.
