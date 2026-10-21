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

---

## Revision 4, 2026-10-20: follow the leader, random starts, open-ensemble continuity

**Field findings on revision 3** (the revision-2 build was still flashed):

1. "I paused OTTO just now. Toby, the follower continued and ran into him. The
   locomotives must know where the other locomotive is located." Spacing acted
   at stations only.
2. **Otto's CONTRADICTION at MM141** (13:36:40, boot `DD20D550…`). At PWM 90
   his moving baseline rose about 10 ADC between MM137 and MM141 (1939 to
   about 1950). Hall-only continuity, with a margin of 16 × 0.5 = 8 ADC,
   withheld normal moving track in runs of 26, 48 and 32 blocks. The spliced
   tracing degraded MM139 and MM140 (LLR 34.7 and 25.0) and then produced an N
   ensemble at MM141, with baseline noise **10.1**; the map has S. The later
   CONTRADICTIONs at 13:40 (MM156) and 13:42 (MM147) followed David's
   declarations, which he reported as wrong.

**Operator rulings:**

- "Ideally, the move up delay would vary randomly between 2 and 5 seconds.
  Also the leader start time will vary by 0-3 seconds after follower PWM=0."
- Stops over a magnet: "On baseline: a flat tracing is simply baseline.
  Nothing needs to happen. Inside a magnetic ensemble: if the tracing stops
  changing, the locomotive may have stopped over the magnet. The encounter
  remains open until the tracing resumes progressing and the ensemble is
  completed … without relying on the throttle command."
- On Claude's proposal (a leader-anchored carpet plus open-ensemble
  continuity): "Yes. Right answer. Please build both."

**Changes:**

- **Follow the leader, anywhere** (`RvmStations.h`). The rainbow carpet,
  anchored `BUBBLE_HOLD_MM` (5) behind the leader's committed MM, caps a
  confirmed follower's PWM. At the end of the carpet the follower stops and
  stays. Once the leader is under power it moves up after a random 2–5 s,
  creeping at 43 until the leader's committed MM moves on. The slower of the
  station carpet and the follow carpet wins.
- **PeerPacket v3** (19 B) carries the applied PWM, so the follower sees the
  leader start.
- **Random delays** (xorshift, seeded from `esp_random()` at boot):
  - the follower moves up 2–5 s after the leader starts, at stations and on
    the line;
  - a convoy leader starts 0–3 s after the follower stands at PWM 0.
- **Continuity (`RvmContinuity.h`)** withholds only while an ensemble is
  **OPEN**:
  - it opens when a block departs at least 70 ADC from the reference;
  - it closes when the recognizer accepts an ensemble, which also takes that
    ensemble's baseline level and noise as the reference;
  - while it is closed, the reference follows the moving baseline (±35 ADC,
    τ = 1 s) and nothing is withheld;
  - "off baseline" also needs at least **35 ADC**. In Toby's Run 12 the
    baseline after a magnet sat about 15 ADC from the one before it, and
    withholding it starved the recognizer.
  - **Toby has continuity again.**
  - **Known limit:** a stall on a shoulder within 35 ADC of the baseline is
    not protected.

**Host tests (14, all pass)** add the following:

- **T13:** the leader is paused on the open line at three points, in both
  directions. The follower stopped behind it every time and moved after it.
  Minimum gap in the model: **2.2 MM**, from an abrupt pause at cruise.
- **T14:** the convoy leader's start delay ranged 0.17–2.90 s.

**Replay of every recorded boot** (`u12_open_continuity_replay.py`, results in
`analysis/…/results/open_continuity_20261020/`), compared with the field
build's continuity:

| Recording | Result |
|---|---|
| Otto, 11 boots (Oct 15; Oct 18–19, including stall Run 9 and Runs 10–12) | **identical** claims, misses and contradictions; less withheld (none on clean runs) |
| Toby, Run 12 | **203/203 correct, 0 CONTRADICTIONs, 0 misses**, against 10 correct and 8 wrong before |
| Toby, today (`E504D063…`) | **MM064 claimed at 309.4 s**, where the field run contradicted. A later replay CONTRADICTION at MM157 follows Pi recording gaps (blocks 34662–34663) where the field run had none: a recording artifact |
| Otto, today (`DD20D550…`) | The replay derails at MM113 with **both** rules, a place the field run passed cleanly, because the recording has 213 gap blocks. From a declaration after MM136, both rules pass MM137–147. The MM141 field failure needed the stale reference, so replay cannot test it. **From logged values:** every block withheld there was within 14 ADC of the level, and the new rule withholds nothing within 35 ADC or outside an open ensemble |

**Compile:** both locomotives build cleanly.

| Locomotive | Flash | app `.bin` sha256 (first 16) |
|---|---|---|
| Otto | 997,639 B | `ea51faef78974535` |
| Toby | 997,639 B | `35b1dbac047c34e5` |

### Flash of revision 4 (`97dcefb`)

David authorized it: "you may flash when you are ready. Both locos are
connected by USB." No backup was taken.

| Locomotive | Port / MAC | MQTT after boot |
|---|---|---|
| Otto | `/dev/cu.wchusbserial10`, `68:09:47:ac:4e:b4` | boot `516ECF85E3A28531`, continuity on, `auto PRESENT`, ESP-NOW 1, `peer_rx` 127 (v3 packets from Toby), link SOLO (undeclared); `heap_free` 35.6 KB, **`heap_min` 9,188 B** |
| Toby | `/dev/cu.wchusbserial110`, `ec:e3:34:78:a2:60` | boot `937A04FBA91AB044`, continuity on, `auto PRESENT`, ESP-NOW 1, `peer_rx` 129, link SOLO; `heap_free` 29.9 KB, **`heap_min` 8,856 B** |

Both applications were written with hash verification. The bootloader and
partitions were unchanged and verified.

**Watch item:** `heap_min` is back to about 9 KB, against about 17 KB on
revision 2 after boot. No failures so far.

---

## Revision 5 (draft), 2026-10-20: the Arches collision

**Field, about 17:21–17:22, on revision 4.** Otto led out of Arches. Toby
followed and struck Otto's consist from behind. David: "looks like Otto stopped
around 120".

**Chain of causes, from the Pi logs and the 16:50 capture slice** (Otto boot
`5CE2BC5F…`):

1. **Otto's navigator froze at MM108 while he moved on.** He had stood on the
   platform for about 10 minutes (an E-STOP). His motor-off Hall level crept
   from 1892 to 1940. Under power his moving baseline settled at about
   1880–1893. Revision 4's open-ensemble rule, armed by MM109 passing slowly
   on departure, treated that moving baseline as a stall plateau. It withheld
   it in runs of 57–172 blocks, so no further magnet was ever completed.
   Replaying revision 4's continuity reproduces the freeze (CONTRADICTION
   expecting MM109).
2. **The roles inverted.** Toby's broadcast MM109 was "ahead" of Otto's frozen
   108, so Otto became FOLLOWER and stopped at once (STOP_NOW) at about MM120
   physically.
3. **Toby had already been released to creep** because Otto's PWM was up
   (revision 4's "leader under power" creep). With no check on the leader's
   actual position, he ran into Otto.

**Physical finding:** on both locomotives the Hall reads 40–52 ADC differently
with the motor off than on, even on plain baseline:

| Locomotive | Standing | Moving |
|---|---|---|
| Otto | about 1940 | about 1890 |
| Toby | about 1915 | about 1863 |

A long dwell creeps further. With the motor on but the train standing, at
arrival (PWM about 30) and departure (PWM 15–40), the level sits 30–70 off the
moving level. **No Hall-only rule separates a standing train from a stall on a
magnet's flank.** Each Hall-only variant fixed one recorded case and broke
another.

**Operator ruling:** "Do what you do to make it work! Can use PWM for standing.
Look for unintended consequences."

**Changes:**

1. **Motor off means paused** (`RvmContinuity.h`). A 20 ms block whose highest
   applied PWM is below 15 (David's 2026-10-18 STOPPED rule) never enters the
   delay line, the recognizer or continuity. Declarations take effect at the
   first admitted block at or after them. PWM is used only to know the motor is
   off, never as evidence of movement. `state/loopstat` now has
   `paused_blocks`.
2. **The continuity reference is the moving baseline only:** the recognizer's
   own baseline level and noise at each recognized ensemble. There is **no
   tracking between ensembles**, because a dwell creeps.
3. **Withholding:** a block is withheld if it lies in a flat 1 s span more than
   max(16 × noise, **25** ADC) off that reference, and only after the first
   recognized ensemble since the declaration. The open-ensemble and
   one-episode experiments are removed.
4. **The follower never moves on throttle** (`RvmStations.h`). It is held
   whenever it is within 5 MM of the leader's committed MM. It is released only
   once the leader's MM has moved on far enough and the random 2–5 s has
   elapsed since the leader started. The creep is removed.
   - **T15** (new): a leader under power that does not move. The follower
     moved 0.00 MM in 60 s.
   - **Timing consequence:** in the model the follower now moves 7–12 s after
     the leader starts, not 2–5 s, because it also waits for the leader's
     position to advance.

**Unintended consequences checked:**

- **Real magnets pass at low PWM.** In all recordings, 2,421 recognized
  ensembles: 141 had a median PWM below 35 and 17 below 30. The lowest moving
  passages were at PWM 25–29. The PWM-0 ones are the Run 3 stop-over-magnet
  tests. So the pause stays at 15; raising it would drop genuine slow passages.
- **25 ADC floor.** Moving drifts seen are 10–23 ADC (Otto +10–14 before MM141;
  Toby +23). **Toby's +23 is 2 ADC inside the floor.**
- **Still not protected:** a stall at PWM 15 or more on a flank within 25 ADC
  of the baseline; and a coast at PWM below 15 over a magnet. One of 35 Otto
  stops was still rolling below 15.

**Regression replay** (`u12_open_continuity_replay.py`, results in
`analysis/…/results/r5_20261020/all_boots.json`):

| Recording | Result |
|---|---|
| Otto, 11 recorded boots (Oct 15; Oct 18–19 including stall Run 9 and Runs 10–12) | **identical** to the field build: every claim, miss and CONTRADICTION |
| Toby, Run 12 | **203/203, 0 CONTRADICTIONs, 0 misses**, against 10 correct and 8 wrong |
| Toby, today (`E504D063…`) | 106/106. The one CONTRADICTION (MM157) follows Pi recording gaps; the field run had none there |
| Otto, the collision run (`5CE2BC5F…`) | **MM109–114 claimed through the Arches departure**, where revision 4 froze at 108. It contradicts at MM115 at 17:22:05, the collision and Otto's emergency stop, when he was physically shoved |
| Otto, the MM141 stretch (from a declaration after MM136) | MM137–146 claimed |

**Host tests:** 15, all pass. **Compile:** Otto app `1e4dec4b3ce69b03…`, Toby app
`266ef7c60ef82d34…`, 997,499 B each. **Not flashed.**

**Addition (field report, 2026-10-20):** "when i push END AUTO button, the
process does not release the locos for auto operations." The dashboard's END
AUTO publishes `ngr/loco/<id>/cmd/dispatcher_release`. Earlier revisions did
not subscribe to it, which was a porting gap. It is now subscribed and handled
as in NAVI_EWO_0_6: AUTO enlisted and running are both cleared, a controlled
stop is made, `state/auto 0` is published, and the warning reads "RELEASED by
dispatcher".

### Flash of revision 5 (`fc759b8`)

David authorized it: "Flash revision 5 (fc759b8) to Otto and Toby over USB, no
backup, and verify over MQTT." No backup was taken.

| Locomotive | Port / MAC | MQTT after boot |
|---|---|---|
| Otto | `/dev/cu.wchusbserial110`, `68:09:47:ac:4e:b4`; app `094cf61cbfd64191…` "Hash of data verified" | boot `E7526088BAFB4520`; continuity on; `paused_blocks` present (r5); ESP-NOW 1, link SOLO, `peer_rx` 0 (Toby not yet on r5); `heap_free` 35.5 KB, `heap_min` 9,088 B; `block_q_drop` 0 |
| Toby | `/dev/cu.wchusbserial110`, `ec:e3:34:78:a2:60`; app `ab1a9868421f83c5…` "Hash of data verified" | boot `2CD346F1614DAF44`; continuity on; `paused_blocks` present (r5); ESP-NOW 1; `heap_free` 25.7 KB, `heap_min` 9,240 B; `block_q_drop` 0 |

**The peer link on revision 5 is not yet verified.** Otto was `online 0`
(unplugged) when Toby came up, so `peer_rx` was 0 on both. Check `state/station`
`peer_rx`, `link` and `role` once both are powered on the track.

### Revision 5.1: declarations made standing (field defect in r5)

**David:** "both locos are refusing AUTO 'POSITION NOT DECLARED' but I declared
position on both."

**Cause.** This is an unintended consequence of the r5 motor-off pause, and the
defect is mine. Declarations took effect only at the first admitted block,
meaning the motor on. A declaration is made standing, and AUTO needs TRACKING
before the locomotive can move. That is a deadlock.

**Fix.** A declaration requested while paused takes effect at the next paused
block:

1. The delay line, which holds the moments before the stop, is decided first.
2. The pending declarations are then applied: navigator TRACKING, and the
   recognizer baseline starts with the next block that has the motor on.

**Checks:**

- `tests/navi_rvm_bubble/test_standing_declaration.cpp` passes (declared while
  standing: 1).
- The key replays are unchanged: Arches departure MM109–114, Toby Run 12
  203/203, Run 12 MM101 clean, MM137–146.

| Locomotive | App sha256 (first 16) |
|---|---|
| Otto | `5cfc216426e2377b` |
| Toby | `dc7b3d8777816c1d` |

**Flash of r5.1 (`5b7a518`).** David authorized it: "flash revision 5.1
(5b7a518) to Otto and Toby over USB, no backup, verify over MQTT". No backup
was taken.

| Locomotive | Port / MAC | MQTT after boot |
|---|---|---|
| Otto | `…serial110`, `68:09:47:ac:4e:b4` | boot `81A074309EC045EA`; `heap_min` 13,676 B; `block_q_drop` 0; ESP-NOW 1; `peer_rx` 117 |
| Toby | `…serial10`, `ec:e3:34:78:a2:60` | boot `39B85830DD0CFC91`; `heap_min` 9,740 B; `block_q_drop` 0; ESP-NOW 1; `peer_rx` 102 |

All applications were written with "Hash of data verified". **The two hear
each other (r5 packets).** The standing-declaration fix is not yet verified on
the locomotives, because it needs a declaration.

---

## 2026-10-21 morning (revision 5.1 on both)

**David:** "They accepted the location and auto. They performed well. Just not
Otto stopped for a CONTRADICTION at CW 046-047 actual."

- **Otto's CONTRADICTION** (08:54:21, boot `671EDD80…`): expected MM038 (S),
  recognized N, while physically at 046–047.
  - Claims MM153–037 were correct (LLR 36–44).
  - From about 08:52:30 (MM011), Otto's moving reference baseline climbed from
    about 1910 to 2040 over two minutes, at cruise. Yesterday the same stretch
    was flat at 1895–1903.
  - Continuity withheld flat cruising runs that sat 70 ADC above the reference
    (51–59 blocks). The tracing was spliced (noise 9.0 at MM036, 5.8 at
    MM037).
  - **After the stop, motor off**, Otto's Hall still wandered between 1933 and
    2148, with spikes to 2235, changing every few seconds. Toby, standing at
    the same time, read 1842 ± 2. Yesterday Otto standing held 1940 ± 1.
  - The INA219 bus reading also read 0.00 V intermittently (08:50:03–08:50:48,
    and from 08:54:03).
  - David checked the Hall and the INA219 visually: "Nothing appeared out of
    order. No disturbance. Otto stopped very near a magnet."
  - **Cause not established.** It is recorded as an open finding. The
    navigator counted correctly until the Hall itself became unstable.
- **Convoy at Grillers** (08:45:47): Otto, the follower, held at committed
  MM058 behind Toby at the MM063 platform. Committed gap 5.

## Revision 6, 2026-10-21: avoidance stop only

**David:** "we do not need a special stop sequence when the locos are at the
station. The follower can just use an avoidance stop." Convoy wait: "keep — it
is more interesting if they are interacting."

**Change.** The station-specific follower hold (HOLD 5 MM before the platform,
and STOP_NOW inside it) is disabled. At stations, as anywhere, the follower
stops by the follow-the-leader carpet, 5 MM behind the leader's committed MM.
The convoy wait, the silence rule and each train's own platform stop are
unchanged. The follower leaves its own platform after its 5 s dwell, capped
behind the leader.

**Consequence.** With the leader committed at the station MM, the follower
stands where the station hold put it (Grillers this morning: 63 − 5 = 58). When
the leader commits station+1, it stands 1 MM closer. The model's physical
minimum gap is about 2.0 MM, against 5.0 with the station hold, because of its
crude 3 MM coast. Adjust `BUBBLE_HOLD_MM` if the field shows it is too close.

**Host tests (15, all pass, updated):** T3 and T12 check the avoidance stop at
every station (minimum 2.0 MM in the model). T11: the follower moves after the
leader starts.

**Compile:**

| Locomotive | App sha256 (first 16) |
|---|---|
| Otto | `fd22fe68804454bb` |
| Toby | `29649155e2c980b5` |

**Not flashed.**
