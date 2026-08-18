# IR Test A implementation report — sender, wire, and observation receiver

**Status: built, deterministically tested, replay-invariant, both locomotive
profiles compiling clean. NOT flashed, NOT bench-paired.** Operator
authorization: 2026-08-17, "PROCEED WITH IR TESTING AND IMPLEMENTATION."
Specs implemented: `docs/IR_TEST_CAR_ESPNOW_FIRMWARE_SPEC.md` (Draft 1) and
`docs/QUORUM_1_16R_IR_TEST_A_FIRMWARE_SPEC.md` (Draft 1), under
`docs/QUORUM_IR_INTEGRATION_SPEC.md` and decision 0036.

## What was built

| artifact | identity | size |
|---|---|---|
| `firmware/common/IRSpeedWire.h` | `IrSpeedPacketV1`, magic 0xC6 v1 | 72 bytes, every offset static_asserted |
| `firmware/test-programs/IR_ESPNOW_SENDER/` | `IR_TEST_CAR_ESPNOW_1_0` | 895,956 bytes (68%) |
| `firmware/QUORUM/QUORUM.ino` | `QUORUM_1_16R_IR_TEST_A` | Toby (IR on) 1,006,451; Otto (IR inert) 1,000,491 |

The sender is IR_SPEED_LOCAL_1_2's acquisition **unchanged** — same pin,
envelope, hysteresis, five-interval median, spans, timeouts, validity
meanings, task pinning — with MQTT/credentials/association stripped and an
ESP-NOW unicast reporting layer added: latest cumulative snapshot every
50 ms, channel 11, no queue, no retransmission, no catch-up burst. `STOPPED`
is derived at packet production (pulses static for 2,500 ms → wire state 5,
speed exactly 0) per decision 0036; acquisition state is never touched.

The receiver: one ESP-NOW callback routes by exact length (72 → IR, 45/15 →
the untouched CTO path); eight validation steps in §5.1's order, each with
its own literal reject counter; a dedicated 32-entry queue; loop-owned
acceptance with epoch/sequence/regression discipline; freshness truth
(numeric only for fresh canonical VALID/STOPPED, `LINK_STALE` past 500 ms);
marker-speed and Hall/IR observation with `agreement:"OBSERVE_ONLY"`; four
telemetry streams. **Non-authority is enforced and audited**: no IR-reachable
path calls or assigns anything with motor or navigation effect, verified by
a comment-stripped static audit that runs in the suite gate.

## Deviations from the specs, declared for CODEX

1. **Base is 1.16Rb, not bare 1.16R.** The receiver spec says "no intervening
   behavior change." Between the accepted 1.16R and this build sit 1.16Ra
   (radio instrumentation, proven byte-identical on all 43 replays) and
   1.16Rb (decision 0037 pairing dissolution, whose changed paths require a
   CTO peer — structurally inert in Test A's solo field gate). Building on
   HEAD keeps one lineage; the alternative was orphaning two reviewed
   increments. Flagged, not silently decided.
2. **`mm/speed` uses short JSON keys.** §9.3's field names plus §9.5's
   512-byte PubMsg bound are mathematically incompatible: the long-name
   skeleton alone measures 438 bytes and the clamped worst case 571. §9.5 is
   the harder constraint, so the keys are shortened with a fixed, in-code
   documented mapping (`ev`,`rev`,`from`,`to`,`pol`,`dur`,`drift`,`gate`,
   `acc`,`disp`,`dist`,`dt`,`mmps`,`irv`,`irage`,`irdp`,`irmm`,`k0..k4`,
   `bestk`,`resid`,`agree`); the schema string is unchanged. Measured worst
   case after shortening and float clamping: **390 bytes**.
3. **Sequence-gap counters live receiver-side only in the suite gate**; live
   queue-pressure under broker outage remains a bench/field item, stated in
   the test file header rather than implied covered.

## Verification

1. **33 deterministic assertions green** (`tests/test_ir_test_a.py`, wired
   into `run_suite.py` §4d): all eight validation rejects with separate
   counters; duplicate/out-of-order/gap; sequence wrap and pulse-count wrap
   as forward motion; in-epoch regression rejected and anchor invalidated;
   reboot epoch; 500 ms LINK_STALE with null; STOPPED as the only non-VALID
   numeric (exactly 0.00); one mm/speed record per received event with
   revision-2 finalization for a held-then-discarded event; route hypotheses
   k=0..4 including the MM170/MM000 wrap; observer resets on declaration;
   and the comment-stripped no-authority audit.
2. **IR-off build byte-identical to the 1.16Rb baseline: 43/43 replays**,
   stdout+stderr. Otto's binary is provably the accepted navigator.
3. **IR-on build navigation-identical: 43/43 replays** with only the four
   new topics filtered — enabling the layer with no sensor present changes
   no navigational outcome, no decision, no PWM transition.
4. **Both-era suite green** on the IR-on build and on the frozen legacy
   binary (`--no-build`, always, against frozen baselines).
5. **Both locomotive profiles and the sender compile clean.**
6. **Adversarial verification pass** (the standing CODEX substitute) run
   before commit; its findings and fixes are recorded below.

The harness gained `ir`, `ir_raw`, `ir_dump` commands that drive the *real*
`irAcceptFrame()` validation chain and the shared `irObserveEventPre/Post()`
pair — the same code the radio callback and `drainMarkers()` run, not a
copy. Two of my own defects were caught during this work by existing
discipline: a bogus `hall_dt_ms` expression (caught on re-read), and a
392-byte-over mm/speed payload — **the third buffer-arithmetic failure this
week, caught this time by the structural truncation guard added after the
second one.** The guard published a valid diagnostic instead of broken JSON,
which is precisely why it exists.

## Bench pairing — what the operator does next (spec §11.3)

Both MAC placeholders ship all-zero, which **safely disables** the paired
direction until filled in:

1. Flash the sender; its boot line prints the Test Car's STA MAC.
2. Flash Toby (Toby profile); record Toby's STA MAC.
3. Put the Test Car MAC into `LL_LocoConfig_9950012.h`
   (`IR_SENSOR_MAC_BYTES`), Toby's MAC into
   `IR_ESPNOW_SENDER/ir_espnow_config.h` (`TOBY_STA_MAC`); reflash both.
4. Hand-spin the wheel → pulses and VALID speed on
   `ngr/loco/9950012/telem/ir_speed`; stop on a spoke → `STOPPED`, 0.00;
   power-cycle the sensor → new boot epoch, `irSensorReboots` increments.

## Field gate

Per spec §11.4: Toby solo, sensor car coupled, MANUAL and AUTO-preset laps,
observation only. The Gate 2 outputs this exists to produce: the ESP-NOW
inter-arrival distribution (to set the real `IR_LINK_STALE_MS`), the Hall/IR
residual distribution at ordinary events and at known phantom sites (to set
the §9.2 tolerance), and the projection-error bracket. **Nothing in this
build may control the locomotive, and the fixed
`authority:"OBSERVE_ONLY"` string in every speed view is the acceptance
check for that claim.**
