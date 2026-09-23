# IR health foundation: first implementation step

2026-09-23. Implemented by Codex following David's "Please take the first step."
This continues Sam's architecture 0.3 and Claude's review, under David's current
cyclometer/rider instruction. No new navigation policy was introduced.

## Delivered

`firmware/reference/NAVI_COHERENCE/IR_ARCHITECTURE_0_4/` contains:

- `IrInstrument.h`: health/readiness classification, explicit diagnostic names,
  fixed wire-reason type handling, and nonzero calibration/boot/pitch checks.
- `IrOdometryEpoch.h`: irreversible epoch termination, immediately available
  fresh odometry on READY recovery, fault-counter continuity, explicit adapter
  termination hook, identity-safe reset/source change and timestamped points.
- `MmDistanceReference.h`: NAVI-owned MM association to an explicitly supplied
  Hall-aligned point; epoch/owner checks; MM and timestamp accessors; explicit
  availability with every distance result.
- `tests/test_ir_architecture.cpp`: executable regression cases replacing the
  prior trace-only checklist, plus reset, source, order and alignment cases.
- `tests/esp32_compile_check/`: non-operational compile fixture, not a loco sketch.
- `README.md`: interface contracts, original nine-case coverage mapping,
  reproduction commands, provenance and remaining limits.

The preserved input ZIP is
`firmware/reference/NAVI_COHERENCE/NAVI_IR_ARCHITECTURE_0_3.zip`, SHA-256
`16a8ee0b4195f1e42913b6ff19174772ba115e8296d0e3d093c1f35e6290dc0b`.
Architecture-package 0.4 is not NAVI_COHERENCE locomotive version 0.4.

## Engineering choices and reasons

These are Codex's implementation choices for the supplied invariants, not new
operator rulings:

1. Keep the wire reason as a raw byte with explicit classification, rather than
   silently converting an unchecked byte to an enum or enabling permissive
   compilation. Unknown values are visible and not measurement-ready.
2. Classify inside `ingest`, preventing mismatched snapshot/classification pairs.
   This is instrument interpretation only: no map, PWM, station or motor inputs.
3. Preserve the original epoch-ending reason until READY recovery. This addresses
   Claude's observability concern without adding a trust-earning delay.
4. Keep epoch IDs monotonic through reset/source change; use 64-bit IDs with an
   exhaustion guard, noncopyable owners and owner-bound references. The original
   `reset()` could reuse Epoch 1 while a reference to it survived. Reference
   lifetime must remain within owner lifetime; replacing an object in place
   while keeping its old references is outside this contract.
5. Supply an explicit no-packet termination hook; do not depend on receiving a
   packet to discover a silent link. Adapter integration must invoke this hook
   at the actual existing freshness deadline and for relevant integrity loss.
6. Reject same-boot duplicate/backward timestamps without adopting them. Other
   observed continuity breaks establish a new origin at the ready endpoint,
   matching the supplied design. Upstream order/source admission still applies.
7. MM synchronization accepts a retained timestamped point, not implicitly the
   newest count. Hall judgment can occur 400 ms after detection; that delay must
   not silently enter the distance anchor. Clock conversion/alignment is not
   implemented here. Point membership checks reject previous-epoch associations.
8. Return distance as `{available, mm}`. Unavailable is not measured zero. Do not
   reintroduce context-free reference validity; use `validFor(current)`.

## Verification results

Native C++17 with `-Wall -Wextra -Werror`, AddressSanitizer and
UndefinedBehaviorSanitizer: **PASS**, compiled and executed from the repo path.

Coverage includes startup/not-ready, a healthy 30-second zero-count dwell and
restart, rising `unreliableSamples` without invalidation, five optical/readiness
outage types, eleven hidden continuity changes, explicit link/queue-gap endings,
repeated reset/source replacement, cross-owner rejection, time order, and delayed
Hall synchronization. Old references stay invalid after every tested epoch change
without calling `reference.invalidate()`.

ESP32 core 3.3.11, `esp32:esp32:esp32`, `--warnings all` compile-only fixture:
**PASS**, no reported warnings, 270,412 bytes flash and 22,316 bytes static RAM.
Those sizes describe the minimal fixture, not locomotive firmware.

`git diff --exit-code -- firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_5_AUTO_ENABLED firmware/common`
returned success: the running-code baseline and shared detector were unchanged.
No hardware access, source pairing, motor commands or flashing occurred.

## Important measured limitation

The host test also drives the actual shared detector with a synthetic healthy
square wave, then four seconds of constant illumination. It goes from TRACKING
to INADEQUATE_CONTRAST. This is executable evidence of a detector limitation,
not merely a speculative caution and not a new field result.

The new layer correctly preserves an epoch during fresh, **measurement-ready**
zero movement. It correctly ends an epoch when the supplied detector says it
cannot measure. Consequently, accepting SIGNAL_STALE and excluding
`unreliableSamples` does not by itself guarantee continuous odometry through
real stops with the current TX. No threshold was lowered and inadequate contrast
was not relabeled healthy to make this test appear successful.

This acquisition distinction must be resolved/verified before claiming the
stop/dwell requirement is delivered end to end. A suitable next investigation
compares real stop traces and the detector's threshold/phase retention behavior;
it does not ask IR to diagnose whether the locomotive intended to stop.

## What is deliberately not implemented yet

- Live packet validation, pairing, watchdog scheduling and epoch-tagged history
  integration. The new API consumes decoded integrity-checked snapshots, not raw
  bytes. Its link-loss tests invoke the adapter hook explicitly; they do not
  establish a working live watchdog.
- Live epoch/health/reference telemetry. Stable diagnostic names are ready.
- Calibration transition admission in the existing transport; it currently
  rejects same-boot calibration/pitch changes as Old.
- Automatic MM identification or reassignment, the 70-count detector setting,
  650 ms fallback, revised window/missed-landmark logic or local correction.
- Station/AUTO behavior changes, new production promotion or a flashable build.

The first independent implementation is complete and tested. Measurement health
is still the priority: acquisition and adapter continuity must be demonstrated
before the later twenty-question navigation changes use this layer.
