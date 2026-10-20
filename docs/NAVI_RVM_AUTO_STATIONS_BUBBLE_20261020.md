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

### Toby (9950012)

| | |
|---|---|
| Port | `/dev/cu.wchusbserial110`. MAC read first: `ec:e3:34:78:a2:60` (Toby) |
| Image | `119ad9b`, Toby build (`-DNAVI_LOCO=9950012`); app `.bin` sha256 starts `b9678aef1d6dea02` |
| Upload | arduino-cli at 115200 baud; all four regions "Hash of data verified" |
| MQTT `state/bootid` | `NAVI_RVM_AUTO_STATIONS_BUBBLE`, boot `91CB945C5997633C`, model `330cdc007da3445b`, **continuity `OFF`**, `auto PRESENT`, `bubble_hold_mm 5` |
| `state/loopstat` | `heap_free` 34.0 KB, `heap_min` 22,560 B, `block_q_drop` 0, `pub_drop` 0 |
| Peer link | **Both locomotives receive each other.** Toby sees Otto (9950011, reliable, CW, MM040); Otto sees Toby (undeclared, reliable 0) |

**Observations after both flashes**

- **Otto is in STOP_NOW** with occupancy UNKNOWN, by design, because Toby is
  powered but not yet declared. Declaring Toby clears it. At that moment Otto
  was declared at MM040 CW, with AUTO enlisted but not running.
- **`peer_rx_rejected`** reached 6,836 on Otto and 87 on Toby. These are
  ESP-NOW frames of the wrong length or magic from other devices on the
  channel. They are rejected and harmless, but they indicate other ESP-NOW
  traffic.
- **Toby's `state/warning`** read "THROTTLE REFUSED: e-stop is set". Toby's
  `estop` is 0, so this looks like an earlier retained message.
- **Neither locomotive has battery protection** until it reboots on track power
  (INA219 not found after a USB boot). This was confirmed on Otto.

---

## Revision 2, 2026-10-20: dashboard GO, pairing, dynamic roles, silence rule

**Field finding.** Otto (declared MM040 CW) and Toby (MM050 CW) were both
enlisted in AUTO but would not start. Each reported `auto_running 0` and
occupancy FREE. The dashboard's GO publishes `ngr/dispatcher/cmd/go/<loco>`,
which is what NAVI_EWO_0_6 and QUORUM subscribe to. Revision 1 listened only
on `ngr/loco/<id>/cmd/go`. **That was a defect in revision 1.**

Earlier, a silent Toby that Otto had heard once held Otto in STOP_NOW for the
rest of the boot.

**Operator rulings:**

- "The locos have to be able to run singly."
- "If the locos are paired, then silence of the other loco means stop: at the
  next station if you are the leader, initiate rainbow carpet stop if
  follower."
- "Dynamic roles is correct."
- On the question of whether the locomotives negotiate their roles: build the
  role echo.

**Changes:**

1. **Dashboard GO and STOP.** The sketch subscribes to
   `ngr/dispatcher/cmd/go/<LOCO_NAME>` and `…/stop/<LOCO_NAME>`, handled as in
   EWO_0_6.
2. **Pairing.** A locomotive pairs once it hears the other fresh and TRACKING
   while it is itself TRACKING. If it has never paired, it runs **singly**,
   whatever else is on the radio. A **new declaration ends pairing**.
3. **Dynamic roles.** Each locomotive derives its role from its own committed
   MM and the other's broadcast MM, by the shorter way round the loop. ROUTE_N
   is odd, so there is no tie. The same MM gives no role.
4. **Role echo.** Each broadcasts its derived role (PeerPacket v2, 18 B). A
   role is confirmed only while the other broadcasts the opposite role.
5. **Link LOST while paired.** LOST means silent for 2 s, not tracking, a
   different direction, the same MM, or unconfirmed or conflicting roles:
   - **Last confirmed role LEADER:** run on to the next station, make the
     platform stop, and **stay** there. The dwell does not release it.
   - **FOLLOWER, or never confirmed:** start a **rainbow-carpet stop at the next
     MM** (`LOST_STOP`) and stay there.
   - A stop already under way is completed and held.
   - Both resume when the link is KNOWN again.
   - This replaces revision 1's stop-where-you-are on an unknown peer.
6. **Telemetry.** `state/station` adds `paired`, `link`, `role`,
   `confirmed_role`, `peer_role` and `platform_held`.

**Host tests (10, all pass).**

- **T1–T3 as before:**
  - carpet values;
  - solo laps with braking at the station MM;
  - bubble hold at 58 (CW) and 68 (CCW), minimum gap 5.0 MM.
- **T4, silent leader:** the follower made a `LOST_STOP` carpet stop, then
  resumed and served 5 stations once heard again.
- **T6, leader loses TRACKING** on the main line: the follower carpet-stopped
  **3.8 MM** short. That margin is smaller than revision 1's fast stop
  (6.4 MM), because the carpet stop is gentler, as ruled.
- **T7, silent follower:** the leader stopped at its next platform and stayed
  (`platform_held`).
- **T8:** an undeclared peer on the radio leaves the locomotive running singly.
- **T9:** after a redeclaration, the locomotive runs singly.
- **T10:** both claiming leader gives LOST.
- **T5:** occupancy geometry and role complementarity over the whole loop.

**Compile:** esp32 3.3.12, real libraries. The only warnings are in the
Adafruit INA219 library, unchanged from earlier builds.

| Locomotive | Flash | Globals | app `.bin` sha256 (first 16) |
|---|---|---|---|
| Otto | 996,315 B | 121,532 B | `aa14d81be5995880` |
| Toby | 996,159 B | 121,532 B | `cb0b131ba39c1f9e` |

**Navigation is unchanged.** This revision touches only `RvmStations.h` and
the sketch.

**Not flashed.**

**Operating notes:**

- To run one train singly after a paired session, switch the other off and
  **redeclare** the one you want to run.
- Both trains must be declared in the same direction. Opposite directions,
  once paired, read as LOST.

### Flash of revision 2 (`1bf6454`)

David authorized it: "flash both. Toby first. No backup." No backup was taken.

| Locomotive | Port / MAC | Upload | MQTT verification |
|---|---|---|---|
| Toby | `/dev/cu.wchusbserial110`, `ec:e3:34:78:a2:60` | 115200, all four regions verified; app sha256 `cb0b131ba39c1f9e…` | boot `09BB68BE14E87D8F`, continuity OFF, `auto PRESENT`; `state/station` shows `paired`/`link`/`role` fields, `espnow 1`, link SOLO; `heap_free` 35.5 KB, `heap_min` 16,348 B |
| Otto | `/dev/cu.wchusbserial110`, `68:09:47:ac:4e:b4` | 115200, all four regions verified; app sha256 `aa14d81be5995880…` | boot `CF896167044E6813`, continuity `HALL_ONLY_K50_B24_M16`, `auto PRESENT`; `espnow 1`, link SOLO; `heap_free` 32.9 KB, `heap_min` 17,784 B; drops 0 |

**The peer link on revision 2 is not yet verified.** Toby was `online 0`
(unplugged from USB) when Otto came up. Check `state/station` `peer_rx`, `link`
and `role` once both are powered on the track.

## Field, 2026-10-20: first run on revision 2

David's report: "running. 1st stops successful." This is the operator's
observation. No log has been analysed yet.

### Toby's CONTRADICTION at MM064 (13:29:46), from the Pi run logs

**David's report:** "toby restarted from grillers then stopped at 066-067. Otto
the follower maintained spacing and stopped also. Toby reports Contradiction."

**Source:** `9950012_20261020_132846.log`, Toby boot `E504D063D480E59C`.

| seq | result | MM | polarity (got / map) | LLR | amplitude | baseline level |
|---|---|---|---|---|---|---|
| 1–13 | MM | 051–063 | all agree | 38.5–42.9 | 213–264 | 1822–1832 |
| **14** | **CONTRADICTION** | expected 064 | **N / S** | **8.7** | 344 | **1657.3** (from 279.9 s) |
| 15 | RECOGNIZED_NO_CLAIM | — | N | 39.5 | 188 | 1826.9 |

**Interpretation** (consistent with the log; not observed on the track):

1. **Toby landed with his sensor over MM064.** Braking for Grillers began when
   MM063 was committed, about 1 s after the magnet. With the 300 ms-per-step
   final brake he stood with his sensor over **MM064's field**, an S magnet
   below the baseline.
2. **The dwell became baseline.** Toby runs **without continuity**, so the
   dwell's flat magnet field entered the recognizer as baseline. The reference
   level fell to 1657, against 1822–1832 before.
3. **The departure looked like an N ensemble.** On leaving, the return to the
   true baseline measured against 1657 looked like a +344 N ensemble, where the
   map has MM064 = S. That is a CONTRADICTION, and AUTO was withdrawn: a
   controlled stop at about MM066–067.

This is risk 3 of this record ("Toby over a magnet, without continuity").

**Otto, the follower**, did what the rules require (Otto log `…_132853`):

| Time | Otto |
|---|---|
| 13:29:17 | Toby on the Grillers platform: OCCUPIED, so HOLD at MM058 |
| 13:29:34 | Toby leaves the platform: FREE, advance toward the platform (PWM 50) |
| 13:29:46 | Toby not tracking: link LOST, last confirmed role FOLLOWER, so `LOST_STOP` at MM059, braked and held |
| 13:33:16 | Toby redeclared at 066: link KNOWN, resume |
| 13:33:51 | braking for the Grillers platform |

### Follower touches the leader at stations (13:40–13:51)

**David's report:** "Now TOBY is following. Toby touches the back of Otto at
stations with no 3 car consist, just the power cars."

**Source:** `state/station` in Toby `9950012_20261020_133938.log` and Otto
`9950011_20261020_134253.log`.

**Defect (revision 2): the release comes too early.** "Leader leaves the
platform" was implemented as the leader's overlay reaching RELEASED, which is
the end of its 5 s dwell. At that moment the leader is still standing on the
platform. Otto in particular needs many seconds to start, because he is
stationary until PWM rises well above 15. The follower, released at once,
runs its last 5 MM and stops at the same place.

Bamboo, 13:51:

| Time | Otto (leader) | Toby (follower) |
|---|---|---|
| 13:51:03.5 | RELEASED at committed **MM158**; it had landed 1 MM past 157 | released from HOLD at **MM154** |
| 13:51:15.3 | committed **MM160**, only 2 MM in 12 s | brakes for the platform at committed **MM157**, then lands about 1 MM past |

That leaves about 2 MM between Hall sensors, less both bodies, which is
consistent with contact.

**What worked:** every bubble hold before release kept about 5 MM. For
example, Toby held at 152 while Otto was dwelling at 158 (13:50:55).

**At rest, leaders commit station+1:** MM158 for Bamboo, 64 for Grillers and
109 for Arches. Toby once committed 159 for Bamboo.

**Proposed fix, not yet built:** the leader has left only when its committed
MM is at least **3 MM past the station centre**. Until then the platform
stays OCCUPIED and the follower keeps holding.

---

## Revision 3, 2026-10-20: convoy, follower start delay, holding geometry

**Field findings on revision 2:**

- Toby nudged Otto at stations, and tapped him at Arches as Otto was starting
  out. Revision 2 released the follower at the end of the leader's dwell.
- David: "the follower is stopping 2 mile markers too close", and later "by
  about 5 MM", at Arches.

**Operator rulings (2026-10-20):**

1. "Put a 2 second delay for the follower to start at the hold point (after the
   leader at the platform starts) to move up to the station."
2. "The braking for the follower needs to start 15 MM before station center.
   The Leader (or the train if solo operation) begins braking 10 MM before
   station center." On the EWO carpet the first reduction (90 → 80) is at
   10 MM, so the leader and solo trains were already right. The follower's
   carpet is the same, ending 5 MM earlier. **No extra braking lead**: a lead
   added briefly in between was removed.
3. "This rule applies to the follower only if the Leader is stopped at the
   station" ("braking or standing").
4. "However, the leader must wait at the platform for the follower to arrive
   before starting out" ("Always (convoy)").
5. "The leader departs when the follower stops (PWM = 0). The follower moves up
   2 seconds later."

**Changes (`RvmStations.h`):**

- **Occupied** means the leader is in final braking, dwell or convoy wait for
  this station, ahead within 11 MM beyond it. The "leader between the holding
  point and the platform" case is removed.
- **Convoy:** a confirmed leader does not leave the platform until the
  follower reports HOLDING for that station.
- **HOLDING** now means stopped (applied PWM 0) and waiting.
- **Follower start:** a follower stopped at the holding point moves up
  `FOLLOWER_START_DELAY_MS` (2000) after the platform frees, that is, after
  the leader starts.
- Constants: `BUBBLE_HOLD_MM` 5, `HOLD_BRAKE_LEAD_MM` 0,
  `FOLLOWER_START_DELAY_MS` 2000.

**Host tests (12, all pass):**

- **T3:** the follower holds 5 MM short at every station.
- **T11:** the follower moves 2.00 s after the leader starts.
- **T12, convoy, both directions, about 25 minutes of model time:** the
  leader waited at every platform until the follower stood at PWM 0, with
  16 waits and the longest 62.7 s. There was no deadlock. The minimum gap was
  5.0 MM.
- **T6:** a dead leader on the main line now leaves only 2.7 MM in the model.

**Compile:** both locomotives build cleanly. **Not flashed.**
