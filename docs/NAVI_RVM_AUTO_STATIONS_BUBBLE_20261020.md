# NAVI_RVM_AUTO_STATIONS_BUBBLE: AUTO stations and the two-train bubble on NAVI_RVM

2026-10-20 · Claude (Opus 5.5) · Branch `agent/navi-rvm-gated-peer-20261020`

**EXPERIMENTAL.** It is built, host-tested and compiled. **It is not flashed and
not field accepted.** Flashing needs David's separate authorization.

## Operator instructions (2026-10-20)

- **Validate the model.** Add back the 70-count requirement, the polarity map
  and any data source that makes it run reliably.
- **The test.** Four station stops and two-train operation with minimal NAVI
  peer spacing. If that fails, the project is shut down.
- **No IR** ("IR signal processing is not reliable").
- **Spacing at stations only.** If the leader is in the station, the follower
  performs the same stop 5 mile markers earlier (one firmware constant). When
  the leader leaves the platform, the follower advances to the platform.
- **The rainbow carpet model** for the stops.

## What it is

Sketch: `firmware/test-programs/NAVI_RVM_AUTO_STATIONS_BUBBLE/`. One sketch
serves both locomotives:

- **Otto (9950011):** the default build;
- **Toby (9950012):** built with `-DNAVI_LOCO=9950012`.

| Layer | Source | Change |
|---|---|---|
| Recognition | NAVI_RVM_CONTINUITY_MANUAL 8986c5f (Otto's field build, Runs 9–12) | 70-count gate (`93cb9ee`) |
| Polarity map | `RvmNavigator.h` | none: a wrong-polarity ensemble is a CONTRADICTION |
| Continuity | `RvmContinuity.h` | per-locomotive: **Otto on, Toby off** (below) |
| AUTO / GO, admission | NAVI_COHERENCE 0.6 `Ops.h` | copied, including the reverse lockout and the enrolled-throttle rule |
| Station stops | NAVI_EWO_0_6 `stopGeography` and `NaviStopOverlay` (U10 rainbow carpet) | copied; position = committed MM only (EWO's "OPERATOR_MM" path) |
| Stations | EWO_0_6 | PATIO 15, GRILLERS 63, ARCHES 108, BAMBOO 157 |
| Cruise | `cruisePwmAt` (grade sections, operator 2026-09-01 and 10-09) | none |
| Bubble | new: `RvmStations.h` | ESP-NOW broadcast at 4 Hz |

**Carpet**, in whole MM before the stopping MM:

| MM before the stop | 11 | 10 | 9 | 8 | 7–2 | 1 | 0 |
|---|---|---|---|---|---|---|---|
| PWM | 90 | 80 | 70 | 60 | 50 | 43 | 0 |

At 0 it brakes at 300 ms per count, dwells 5 s at PWM 0, then cruises.

**AUTO is withdrawn with a controlled stop** on CONTRADICTION, on loss of
TRACKING, and on any direction change or declaration.

## The bubble

- **"Leader in the station"** means the peer is fresh, tracking and running in
  the same direction, and either:
  - it reports final braking or dwell for this station, anywhere ahead within
    11 MM beyond the platform; or
  - its committed MM lies between our holding point (exclusive) and the
    station (inclusive), and it has not released this station.
- **When the decision is taken.** It is decided from 16 MM out, so the
  follower runs the whole carpet shifted 5 MM.
- **While held.** Once stopped short, the follower stays at zero until the
  platform is free, even if it rolled past the holding MM.
- **Too close to the holding point.** If the station becomes occupied when the
  follower is already inside 5 MM, it stops where it is.
- **Unknown peer** means silent for 2 s, not tracking, or running the other
  way. The other train may be standing anywhere ahead, so the follower **stops
  where it is**, on the fast ramp, and resumes when the peer is known again.
  **Consequence: both locomotives must be declared before GO.** A powered,
  undeclared peer reads as Unknown. A peer never heard reads as no other train.
- **Leader departure** never depends on the follower.
- **Not covered:** a leader stopped by the operator between stations while
  still tracking is **not** protected. Spacing acts at stations only, as
  chosen. Stop both trains by hand in that case.

## Toby's continuity: off

- **Field (Run 12):** the continuity rule withheld 23,744 blocks of Toby's
  run, which left 18 claims and 2 CONTRADICTIONs.
- **Cause:** Toby's Hall level steps by about +23 ADC while running steadily
  at PWM 90 (block 5400), and moves by about 52 ADC with the motor. A Hall-only
  rule cannot tell that from a stall on a shoulder.
- **A fix that was tried and reverted:** making continuity follow the
  recognizer's moving baseline. It still missed MM050.
- **Continuity off, gate on:** **203 consecutive correct claims, 0
  CONTRADICTIONs** on the same run.
- **Cost:** a stop with Toby's sensor over a magnet may break that magnet's
  recognition. Carpet stops brake after the station magnet has been
  recognized, so they should stand on baseline.
- **Probable root cause:** the sensor mounting near Toby's motor, which is
  hardware and the operator's call.

## Evidence

- **Navigation, unchanged replay.** Built against this sketch's headers, Toby's
  Run 12 (continuity off) and Otto's Run 12 from MM102 give `nav.csv` files
  byte-identical to the validated replays: 203 and 79 consecutive correct MM.
- **Host tests:** `tests/navi_rvm_bubble/test_bubble.cpp`, all pass. A crude
  kinematic model (speed = PWM/90 MM/s, committed MM reported 1.5 s after
  passing) drives the real logic:
  - **T1, carpet values:** the PWM table above, CW and CCW.
  - **T2, solo, 3 laps each way:** braking began at the committed station MM
    every time, in route order: 63, 108, 157, 15 CW and 108, 63, 15, 157 CCW.
    The model's landing points (about 3 MM past the station) are not railway
    predictions.
  - **T3, two trains each way:** the follower held at MM058 (Grillers CW) or
    MM068 (CCW), then made its platform stop. Minimum gap: **5.0 MM**.
  - **T4, peer radio silent:** the follower stopped where it was. With the
    radio restored it resumed and served 5 stations.
  - **T5:** occupancy cases (peer behind, between, dwelling, released, other
    direction, never heard).
  - **T6, leader dead on the main line** (CONTRADICTION, 6.8 MM ahead): the
    follower stopped **6.4 MM** short of it.
- **Compile:** arduino-cli, esp32 core 3.3.12, real libraries, no warnings.

| Locomotive | Flash | Globals | `.bin` sha256 (first 16) |
|---|---|---|---|
| Otto | 995,655 B (75%) | 121,516 B | `b0929ae9cab7eedb` |
| Toby | 995,495 B (75%) | 121,516 B | `b9678aef1d6dea02` |

## Risks to watch on the first run

1. **Heap.** ESP-NOW is new on this sketch. 8986c5f ran with `heap_min` about
   9.4 KB. Check `state/loopstat` `heap_free` and `heap_min` on the bench
   before running.
2. **Landing position.** Braking starts when NAVI_RVM commits the station MM,
   about 1 s after the ensemble. Landings will sit past the station disc.
   Calibrate later, per station and direction (architecture §6), not now.
3. **Toby over a magnet**, without continuity (see above).
4. **ESP-NOW channel.** Both locomotives must be on the same access point.
   A roam to another channel makes the peer silent, and the follower stops.
5. **Both trains run the same direction.** Opposite directions are treated
   as Unknown, and both stop.

## Operating sequence (proposed)

1. Flash Otto and Toby. Check boot, heap and `state/station` over MQTT.
2. **Solo**, Otto only, Toby powered off: declare, `auto 1`, `go`. Watch the
   four stops on two laps.
3. **Two trains:** declare both in the same direction, at least about 20 MM
   apart, then `auto 1` and `go` on each. Watch the follower hold 5 MM short
   while the leader dwells.

Telemetry: `state/station` (target, carpet section, execution, bubble,
occupancy, peer), `state/auto`, `state/auto_running`, plus the existing
`state/nav`, `diag/rvm_ensemble` and `state/loopstat`.

## Rollback

The previous firmware is `8986c5f` (Otto) and `8f343b0` (Toby). Both
backups are recorded in their flash records.

---

## Flash record, 2026-10-20

David authorized it: "Yes, flash Otto and Toby with NAVI_RVM_AUTO_STATIONS_BUBBLE
(119ad9b) … Otto first." He then added: "No need to back up either." **No backup
was taken.** Rollback means rebuilding `8986c5f` (Otto) or `8f343b0` (Toby) from
git and flashing it.

### Otto (9950011)

| | |
|---|---|
| Port | `/dev/cu.wchusbserial110`. MAC read first: `68:09:47:ac:4e:b4` (Otto), ESP32-D0WD-V3 rev 3.1 |
| Image | `119ad9b`, Otto build; app `.bin` sha256 starts `b0929ae9cab7eedb` |
| Upload | arduino-cli at 115200 baud; bootloader, partitions, boot_app0 and application all "Hash of data verified" |
| MQTT `state/bootid` | `NAVI_RVM_AUTO_STATIONS_BUBBLE`, class `EXPERIMENTAL_AUTO_STATIONS_BUBBLE_GATE70_NOT_FIELD_ACCEPTED`, boot `16FC79DFB73B319C`, model `330cdc007da3445b`, continuity `HALL_ONLY_K50_B24_M16`, `auto PRESENT`, `bubble_hold_mm 5` |
| `state/station` | `espnow 1`, occupancy `NO_PEER` (Toby not yet flashed) |
| `state/loopstat` | `heap_free` 33.7–35.8 KB, **`heap_min` 12,052 B**; `block_q_drop` 0, `cmd_drop` 0; `pub_drop` 5 at boot; Pi recording `nsr_native_drop` 303 at boot |
| `state/warning` | **INA219 NOT FOUND.** Booted on USB, so there is no battery protection until Otto reboots on track power |
