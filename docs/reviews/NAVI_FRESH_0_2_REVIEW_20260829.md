# Review — NAVI_FRESH_0_2

**Reviewer:** Claude (session `agent/toby-1-13-flash`, 2026-08-29 evening)
**Subject:** `/Users/davidbrown/NGR/NGR-Files/NAVI_FRESH/` — `NAVI_FRESH_0_2`
**Status of subject:** development, host-tested, not flashed, not field accepted
**Verification performed:** ran `run_tests.sh` on this machine — both gates pass

```
PASS: 0 failures                                        (test_core.cpp)
survey labels:       rej=0: 192   rej=1: 3   rej=2: 150
recognizer outcomes: MAGNET: 192  NOT_MAGNET: 3  REBOUND_SUPPRESSED: 150
PASS: every labelled primary accepted; no non-primary counted
```

## Summary

This is the better design. Three things in particular, said plainly because the
reviewer's own sketch (`NAVI_2`) does them worse:

1. **The recognizer is position-free.** `MagnetRecognizer::examine()` answers
   "is this a magnet" before anything knows where the locomotive is. `NAVI_2`
   fused recognition and identity in one function, which means a corrupted
   position can corrupt the shape judgement. The separation is correct and
   should be preserved in anything that follows.
2. **The relative peak floor is placed correctly.** 0.40 of a 31-sample running
   median sits between the measured populations of decision 0054 — false events
   top out at 0.26, real magnets' 1st percentile is 0.63. `NAVI_2` has no
   amplitude test at all, and paid for it in the field the same evening (below).
3. **The rebound guard is measured close-to-open.** A real gap between passages,
   rather than rise-to-rise. Simpler and more physical than `NAVI_2`'s
   motion-gated clock, which grew a defect the operator then had to rule on
   (decision 0055).

925 lines against 5,922, with host gates and a Python reference model.

## Field evidence from the evening of 2026-08-29

This post-dates the README and is offered as data, not as criticism.

### The recognizer would have prevented the drift that `NAVI_2` suffered

Toby ran on `NAVI_2_0` and lost about five markers during recovery from a
derailment. Two events were the direct cause:

| time | peak | duration | running median | ratio |
|---|---:|---:|---:|---:|
| 18:17:04 | 40 | 127 ms | ~205 | **0.195** |
| 18:17:04 | 50 | 47 ms | ~205 | **0.244** |

Both are below `relativePeakFloor = 0.40`. `NAVI_FRESH` refuses both as
`TooSmall`. This is a concrete, out-of-sample point in the recognizer's favour.

### The IR bounds cannot be set from tonight's mount, but the failure mode is now measured

The README correctly lists these as placeholders. The operator fitted an IR
sensor to Toby's power car (GPIO 34, trailing unpowered wheel) this morning,
using a wrong-side housing he describes as jerry-rigged. Over 1,269 s of
one-second telemetry:

- 1,111 s `NO_CONTRAST`, 82 `MARGINAL`, 46 `GOOD`; spans reached 4095
- while pulsing, rise-to-rise intervals: **p05 = 2 ms, p25 = 6 ms**, median 42 ms
- bursts to **239 pulses/second** — 2.3 m/s at 9.652 mm/pulse, on a railway
  whose auto ceiling is about 0.33 m/s
- every health field read normal throughout: `open_aborts = 0`, `sat = 17`

At that rate `intervalMaxPulses = 80` trips in under a second, and
`IR_MISSING_MAGNET` would stop the locomotive repeatedly. The 15 ms debounce is
also defeated by 2–6 ms intervals.

The mount is temporary, so these are not the installed numbers. What they do
establish is that **IR fails silently in both directions**: this overcount, and
the 1%–39% silent undercount from spoke merging recorded in
`docs/IR_DEV_REC/2026-08-26_IR_FOUR_LAP_DISTANCE_TRUTH.md`, where laps reporting
96–97% contrast-valid were off by 19% and 39%. Any bound derived from IR should
assume both directions of error, and the installation check in the README §
"Short Toby installation check" should record the overcount case explicitly.

### A clean circuit was achieved and is available as out-of-sample data

Earlier the same evening, with no position declared, Toby ran a full circuit
producing 172 detections. Matching the observed polarity word against `NGR_DNA1`
over every rotation and both directions gives **172/172 starting at MM41 CW** —
one exact lap. Peaks min 146, median 205, max 323; intervals min 898 ms.

Evidence: `field-records/logs/20260829_navi2_first_lap/`,
`docs/research/20260829_A_CLEAN_LAP.md`. This is a genuinely out-of-sample
circuit for the recognizer, since its thresholds came from the 08-28 survey. It
also ran over freshly replaced disk magnets — see the provenance note in that
record before pooling it with 08-28 amplitudes.

## Findings

### 1. MQTT shares the 1 kHz sampler loop, with no queue — the one unaddressed item

`loop()` runs the sampler inline and then calls `connectMqtt()` and
`mqtt.loop()`. `publish()` calls `mqtt.publish()` directly on the same thread.
`PubSubClient::connect()` blocks until its timeout.

The file's own comment at line 209 states the intent — *"Control never waits on
MQTT"* — but the code does not yet implement it. A broker stall or a slow
reconnect stalls Hall acquisition, and there is no event queue to absorb it.

QUORUM learned this expensively: on 2026-07-29 a main-loop stall overflowed a
32-slot queue and destroyed 67 marker events. Its answer was a pinned FreeRTOS
Hall task at 1 ms, a separate network task, and publishes that only enqueue —
no locomotive-state thread ever touches the radio. That architecture is worth
copying even though the rest of QUORUM is not.

### 2. Sequence-only identity is deliberate, and the reviewer notes its cost

Invariant 7 scopes this to one bounded CW lap from 040–041, and within that
scope counting is sufficient. Recorded only so the trade-off is explicit:
`navMm` advances on any recognized magnet, `NGR_DNA1` is never consulted, and
polarity is used solely to orient the waveform before being discarded.

Consequently a single missed or false passage puts position permanently off by
one with nothing able to detect it. The 172/172 result above was self-proving
*because* polarity was recorded and could be matched to the map; the same run
under sequence-only counting would have produced an identical-looking result
whether or not it was correct.

Polarity costs one bit per event and one comparison. It is what makes a lap
provable rather than merely unrefuted, and it is what caught the `NAVI_2` drift
within two markers on the evening described above. Suggested for a later scope,
not a defect at the current one.

### 3. Version string is inconsistent

`NAVI_FRESH.ino:2` and the boot banner at line 277 say `NAVI_FRESH_0_1`; the
MQTT ready alert (line 168) and `state/loopstat` (line 204) say
`NAVI_FRESH_0_2`. A retained `state/loopstat` naming a different build than the
serial banner is the stale-identity ghost that `T_BOOT` exists to kill.

### 4. `MOTION_ARMED = true`, and the build is flashable now

`NAVI_FRESH.ino:29` has motion armed, and `build/esp32.esp32.esp32/` contains
`.elf`, `.merged.bin`, `bootloader.bin`, `partitions.bin` and `flash_args`. The
README states 0.2 is not flashed and requires separate authorization. Noting
only that nothing in the tree now prevents a flash, and the README's own
installation check (steps 1–5) has not been performed.

### 5. `state/warning` and the enrollment state topics are not published

The console gates enrollment on `state/nav_ready`, `state/session_direction`,
`state/start_mm` and shows `state/warning`. None are published here. Manual and
estop controls will work; the LL enrollment path on the dashboard will not
reflect this sketch's state.

### 6. Relative include path to credentials

`#include "../../../esp-loco-control/firmware/QUORUM/credentials.h"` couples the
build to the two directories' relative placement. Correct in intent — no second
plaintext copy — but it breaks if either tree moves. A build flag or a symlink
would be more robust.

## What this reviewer is doing next

At the operator's direction, `NAVI_2` is abandoned and a clean-slate navigator
is being built that adopts this design's position-free recognizer, relative peak
floor and close-to-open rebound guard, adds map-and-polarity identity and a
declarable position per decisions 0053/0056, and uses QUORUM's non-blocking
transport architecture. The recognizer thresholds and the Python reference model
here are treated as the reference.

## References

- `docs/decisions/0053`, `0054`, `0055`, `0056`
- `docs/research/20260828_WHAT_THE_HALL_SENSOR_SEES.md`
- `docs/research/20260829_A_CLEAN_LAP.md`
- `docs/IR_DEV_REC/2026-08-26_IR_FOUR_LAP_DISTANCE_TRUTH.md`
