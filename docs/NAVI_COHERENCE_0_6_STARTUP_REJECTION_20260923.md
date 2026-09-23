# NAVI COHERENCE 0.6 startup rejection

Status: field test held before motion; diagnosis confirmed, correction proposed.

## Evidence

David supplied a parallel-chat stationary-test report: Toby stopped at MM41,
PWM zero, 119 IR pulses, legacy link accepting packets but shadow health
rejecting every packet. No moving-test acceptance is claimed.

Codex read the Pi recorders on 2026-09-23. Both ngr-runlog.service and
ngr-ir-espnow-record.service were active. No commands to the train, recorder
changes, firmware changes, or Pi decoder changes were made.

Raw MQTT at 13:42:35 showed ir_link accepted=10037, rejected=0, pulses=119,
reason=1, fresh=1. The loco's raw diag/ir_health JSON itself contained
PACKET_INVALID, accepted=0, rejected=10037, epoch_active=0. This is not an
error introduced by the Pi Feed JSON decoder.

An independently decoded 110-byte type-5 snapshot from the serial recorder
at epoch 1790196183.091549, sequence 12553, contained:

- completedPulses: 119
- calibrationId: 0
- pitchUm: 9652
- nominalUm: 1148588 (119 * 9652)
- opticalReason: 1 (INADEQUATE_CONTRAST)
- span: 15

Raw snapshot:

```text
5249010509310000b894e7bf324232888759e64a000000007d00000000000000770000000000000000000000000000000000000000000000700613000000000000000000000000000100000000000000000000000000000000000000b4250000ac8611000000000001000f001e96
```

## Diagnosis

IR_SCOPE_ESPNOW_TX constructs Measurement(movementBoot,0,9.652).
Its zero calibration ID is therefore not a malformed packet. The legacy
MovementEvidence adapter accepts it. IrHealthMonitor rejects it through
!w.calibrationId; IrInstrument also treats zero as CalibrationFault.
The new monitor's compatibility assumption was wrong. Synthetic tests using
nonzero calibration IDs did not exercise the deployed sender's contract.
Independent compilation and observation-only verification did not establish
wire compatibility with this sender.

The TX also currently reports inadequate contrast. The rejection masks that
report. This does not establish missed pulses, false pulses, or distance
accuracy, nor establish the cause of the low contrast. Fresh radio reception
must not be described as healthy optical measurement.

## Proposed Next Step

Keep the moving test on hold. Separate packet validity and optical health
from calibration/validated-distance status, preserving all existing navigation
behavior and unvalidated-distance restrictions. Do not invent a calibration
ID or suppress INADEQUATE_CONTRAST. Add a real-wire regression fixture before
rebuilding and rechecking stationary telemetry. Proposal is not implemented.

## Repository Observation

The intervening firmware reorganization moved 0.6 into
firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH.
The familiar NGR-Files/NAVI_COHERENCE_0_6_IR_HEALTH symlink still points to the
removed firmware/test-programs location. It needs repair before the next
Arduino IDE upload. No paths were changed during this diagnosis.
