# IR Architecture 0.4: independent measurement foundation

2026-09-23. Implementation by Codex, continuing Sam's
`../NAVI_IR_ARCHITECTURE_0_3.zip` and Claude's review. The original ZIP is
unchanged. This is **architecture package 0.4**, not a new locomotive sketch.
NAVI_COHERENCE 0.5, AUTO, Hall thresholds, transport and TX are unchanged.

## Architectural test

Would a cyclometer know this, or would the rider know it?

IR owns health/readiness classification and measured travel continuity.
NAVI owns the MM association and all operational interpretation. These headers
do not read PWM, station state, route maps or expected magnets and issue no
motor/navigation commands. `MmDistanceReference` is NAVI-owned bookkeeping,
not an instrument claim about position.

## Interfaces and changes from 0.3

- `classifyIrInstrument(decoded)` separates healthy/not-ready PRIMING and
  REACQUIRING from READY TRACKING and SIGNAL_STALE. Unknown wire reason values
  remain visible as raw `uint8_t` and classify as invalid; no unsafe enum
  conversion. Nonzero boot/calibration/pitch are required.
- `IrOdometryEpoch::ingest(decoded)` performs classification internally so a
  caller cannot accidentally supply a healthy classification for another packet.
  Every ready snapshot after an ended epoch starts a new one. Ordinary recovery
  retains the reason that ended the previous epoch. First READY is Epoch 1.
- Fresh, healthy zero counts keep the epoch and distance reference. Neither
  `unreliableSamples` nor time since the last pulse invalidates odometry.
- Changes in sample-gap, saturation, pulse-abort and inferred-pulse counters,
  boot, calibration or pitch end continuity. A valid new endpoint starts fresh
  odometry, never reconstructs travel across the discontinuity. Counter rollback
  also breaks continuity. Same-boot nonincreasing capture times are rejected,
  end the active epoch and cannot be adopted as a fresh origin.
- `endEpoch(reason)` is an explicit no-packet/queue-gap hook. Only the first
  termination reports `epochEnded`; later failures cannot reopen it. The
  adapter must invoke it on its actual freshness deadline, even if no packets
  or Hall events arrive. This package does not add or change a timeout policy.
- `reset()` and `sourceChanged()` preserve the monotonically increasing 64-bit
  epoch identity. Identity exhaustion fails closed. Objects are noncopyable;
  references also check owner identity. Keep the odometry object alive for the
  entire lifetime of its points/references; references cannot survive destroying
  and reconstructing their owner at the same memory address.
- `point()` captures an epoch-tagged, timestamped measurement for the existing
  transport history. `synchronize(mm,hallMs,alignedPoint,current)` explicitly
  accepts the Hall-aligned point, rather than implicitly using today's latest
  count after a delayed Hall judgment. A point from an ended epoch cannot sync.
  Clock alignment itself remains the adapter's responsibility, not implemented
  or certified by these classes.
- MM and Hall-time accessors are restored. `validFor(current)` is the validity
  test; there is deliberately no context-free `valid()` that could become stale.
  `epochDistance()` and `distanceFromMm()` return `{available, mm}`. Consumers
  must not read an unavailable result's placeholder value as measured zero.
- Stable names for health, readiness and epoch-break reasons support subsequent
  telemetry. No live telemetry publisher is installed by this package.

## Transport boundary: not yet integrated

`ingest` accepts **decoded, source-admitted, integrity-checked snapshots**.
Pairing, length/magic/version/type/CRC, receipt freshness, sequence order and
retired-boot rejection remain adapter responsibilities. The classes do not
silently replace those checks. The next integration must call `endEpoch` on
measurement-affecting transport failures and must not feed rejected bytes here.
It must also expose a valid calibration/pitch transition instead of indefinitely
rejecting it as `Old`, as the existing 0.5 transport currently does.

Navigation declarations and direction changes are not automatically instrument
failures. How NAVI handles its directional MM reference remains a separate
integration matter; do not call instrument reset just to clear navigation state.

## Executable verification

From the repository root:

```sh
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -I firmware/reference/NAVI_COHERENCE/IR_ARCHITECTURE_0_4 firmware/reference/NAVI_COHERENCE/IR_ARCHITECTURE_0_4/tests/test_ir_architecture.cpp -o /private/tmp/test_ir_architecture
/private/tmp/test_ir_architecture
arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all firmware/reference/NAVI_COHERENCE/IR_ARCHITECTURE_0_4/tests/esp32_compile_check
```

The nested Arduino fixture checks compilation only; it is not a locomotive
firmware and must not replace Toby's sketch. Tests remain below `tests/`, not
beside a production `.ino` where Arduino would link a host `main()`.

### Mapping Sam's nine review cases

| Original case | Executable coverage |
|---|---|
| 1 Startup PRIMING, first READY | `startup` |
| 2 Fresh unchanged count beyond 2.5 seconds | `stopDwellRestart`, 30-second dwell with rising unreliable counter |
| 3 Contrast outage and ordinary recovery | `outage(INADEQUATE_CONTRAST)`, no reboot or fault-counter change |
| 4 REACQUIRING ends active epoch | `outage(REACQUIRING)`; PRIMING also exercised |
| 5 Hidden sample-gap increase | `discontinuities`, plus saturation, abort, both inference counters and rollback |
| 6 New boot | `discontinuities`, including shorter sender uptime and zero count |
| 7 New MM synchronization | `outage`, new reference after fresh odometry resumed |
| 8 Zero versus unavailable communications | `stopDwellRestart` versus explicit `linkAndReset` termination hook; no automatic live watchdog claimed |
| 9 Old reference never revived on epoch changes | outage/discontinuity/reset/source tests, without calling reference.invalidate |

Additional coverage: all detector classifications, invalid metadata and unknown
reason; delayed-Hall reference; rejected stale points; cross-object identities;
500 repeated resets across five failure scenarios; duplicate/backward times.

## Known detector limitation, deliberately not hidden

The test `actualDetectorStop` runs the **existing shared detector** with a
synthetic healthy square wave and then constant illumination for four seconds.
It transitions from TRACKING to INADEQUATE_CONTRAST, not a measurement-ready
stationary state. This confirms that accepting SIGNAL_STALE and ignoring
`unreliableSamples` alone does not establish real stop/dwell continuity with
the present TX. A receiver cannot recover movement the instrument did not measure.

This step does not change the detector or declare its inadequate-contrast state
healthy. Healthy-zero semantics pass; actual stationary measurement readiness
remains a separate, explicit acquisition/integration gap to resolve with evidence.
