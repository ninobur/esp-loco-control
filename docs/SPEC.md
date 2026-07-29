# Highline Auto Mode — Specification

**Project:** esp-loco-control
**Target firmware:** `NGR_LL_DNA_CTO2_r12_CONTINUITY_FIRST.ino` (ESPNOW_VERSION 14, CTO2_VERSION 3)
**Status:** DRAFT v3 — written against the current r12 sketch
**Locomotives:** Otto (9950011), Toby (9950012)

---

## 1. Purpose

Add **HL-Auto**, a Highline automatic mode, to the current locomotive
firmware as a **distinct third operating mode** alongside the existing
Manual and LL-Auto (DNA dispatcher) modes.

The locomotive must switch between the Lowline and the Highline **by
operator selection at session start — no reflashing**. Because HL-Auto lives
inside the same firmware, every future update to the main sketch carries
Highline with it automatically.

---

## 2. The three modes

| Mode | Track | Hall interpretation | Coordination |
|---|---|---|---|
| **Manual** | any | ignored | none |
| **LL-Auto** | Lowline loop | DNA 12-marker continuity navigation | CTO2 peer / dispatcher |
| **HL-Auto** | Highline | two-magnet polarity stations | none — single loco only |

HL-Auto is deliberately the simplest of the three. It is not a variation of
DNA navigation; it stands completely apart from it.

---

## 3. Design principles

**Distinct mode, hard wall.** When HL-Auto is active, the entire DNA stack
is stood down: DNA marker matching, the measured-speed governor, adaptive
baseline, station logic, traffic logic, and CTO2 peer coordination take no
part. HL-Auto does not read, write, or depend on DNA navigation state.

**Why one firmware, not two (decision record).** Building the Highline
controller on the older, simpler GoldCore firmware was considered — it would
keep the Highline code free of DNA/CTO2/governor weight. It was **rejected**
because it breaks the primary requirement: Otto and Toby must run on both the
Lowline and the Highline **without reflashing**. Two firmwares means
reflashing a loco every time it moves between lines. Therefore Highline must
be a mode *inside* the current Continuity firmware, even though that firmware
is heavier than Highline strictly needs. Do not revisit the GoldCore split
unless the no-reflash requirement itself is dropped.

**Isolation via the existing autoMode flag.** The sketch already broadcasts
`dispatcherAuto` as `pkt.autoMode` in the CTO2 packet (line ~837), and peers
key their coordination off it. HL-Auto therefore **must not set
`dispatcherAuto`.** It uses its own separate state flag (proposed
`highlineAuto`). Consequently, to the DNA world and to every peer
locomotive, a Highline loco looks exactly like a loco in Manual: not
enrolled, not participating in CTO2, invisible to coordination. This gives
the required isolation without modifying the DNA or CTO2 code paths.

**Geometry, not timing.** Station stopping is set by the physical spacing
between two magnets, so braking distance does not depend on loco mass,
motor, train length, grade, or battery voltage. Same magnet placement, same
stop, for any locomotive, with no per-loco ramp tuning.

---

## 4. Magnet coding

Magnets are identified by **polarity**, using the enum that already exists
in the sketch (`HallState { HALL_NONE, HALL_NORTH, HALL_SOUTH }`, line 111):

| Magnet | Meaning |
|---|---|
| `HALL_NORTH` | Station approach — slow to station speed |
| `HALL_SOUTH` | Station stop — ramp to zero |

Polarity coding makes each magnet self-describing, so a missed magnet costs
at most one station and never puts a sequence permanently out of phase.
`HALL_POLARITY_INVERTED` in the per-loco config already accounts for sensors
mounted in opposite orientations on Otto vs Toby.

---

## 5. HL-Auto behaviour

### Normal cycle

1. On dispatcher GO, ramp from 0 to **cruise PWM 70**.
2. Run at cruise until a `HALL_NORTH` magnet is detected.
3. Ramp down to **station speed 40 PWM**. The approach is now **armed**.
4. Run at 40 until a `HALL_SOUTH` magnet is detected.
5. Ramp to **0**.
6. Dwell **10 seconds**.
7. Ramp back to cruise 70. Disarm. Repeat from step 2.

### Out-of-order and missed magnets (single loco — a missed station is harmless)

- **`HALL_SOUTH` while unarmed** (North missed or never seen): **ignore
  completely.** Hold cruise, run through, try the next station. Do not stop.
- **`HALL_NORTH` while already armed:** ignore; stay armed at station speed.
- **`HALL_NORTH` with no South following:** after `HIGHLINE_ARM_TIMEOUT_MS`
  (proposed 30 s) disarm and return to cruise, so a failed stop magnet does
  not leave the loco crawling at 40 forever.

### Departure

No lockout required. With a ramp-down setting of 300, the loco overruns far
enough that the Hall sensor does not rest near the South magnet, so
departure cannot re-trigger it.

---

## 6. Mode entry, exit, and setup

### Selection
The operator selects Manual / LL-Auto / HL-Auto at **session start on the
loco dashboard**, alongside the existing setup fields. This is not a
mid-run MQTT toggle and not a compile-time flag.

### Setup fields
- **Direction FOR / NEU / REV:** Forward is required for HL-Auto, exactly as
  for LL-Auto.
- **Orientation CW / CCW** and **mile-marker interval / position preload**
  are **Lowline-only**. HL-Auto ignores them; the dashboard should hide or
  disregard them when HL is selected.

### Entry guard
`enterHighlineAutoMode()` mirrors the guards in `enterDispatcherAutoMode()`
(line 1522): refuse if E-stop engaged, refuse if the loco is not stopped
(`rampCurrent > MOTOR_DEAD_ZONE_PWM || rampTarget != 0`). It must NOT set
`dispatcherAuto`.

### Start
After setup and entry, the **dispatcher GO/STOP** starts the run, same
signal as today. The dispatcher needs no knowledge of Highline.

### Release (both mirror existing LL behaviour)
- Dispatcher STOP / release → back to Manual, via a Highline analogue of
  `leaveDispatcherAutoMode()` / `stopAutoSequence()`.
- Power-cycle → back to Manual.

---

## 7. Constraints — must not break

- `ESPNOW_VERSION` (14), `CTO2_VERSION` (3), and both packet structs
  (`TrainPacket`/ESP-NOW and `CtoPeerPacket`) are unchanged. Compatibility
  with the dispatcher and peers is preserved.
- DNA continuity navigation, the measured-speed governor, adaptive baseline,
  station/traffic logic, and CTO2 are unmodified.
- `dispatcherAuto` semantics are unchanged; HL-Auto never sets it.
- Manual already ignores Hall navigation (r12 README point 1) — unchanged.
- E-stop, low-voltage cutoff, and Manual authority override HL-Auto at all
  times.
- Existing MQTT topics and payloads keep their meaning; add new ones only
  for mode selection and Highline state.

---

## 8. Structural change to existing code (flagged, not silent)

A distinct third mode requires representing three modes, where the sketch
currently has effectively two states (`dispatcherAuto` true/false).

- Add a separate `highlineAuto` flag (do not overload `dispatcherAuto`).
- Audit reads of `dispatcherAuto` that mean "are we under automatic motor
  authority?" vs "are we in a DNA/CTO2 session." Motor-authority and
  release paths may need to consider `highlineAuto` too; DNA/CTO2 paths must
  continue to key on `dispatcherAuto` alone so Highline stays invisible to
  them.
- `updateMotorAuthority()` must grant authority under HL-Auto as it does
  under dispatcherAuto.

This is the one intended modification to working code. Review before
implementing.

---

## 9. Configurable values (per-loco config headers)

| Name | Value | Meaning |
|---|---|---|
| `HIGHLINE_CRUISE_PWM` | 70 | Running speed between stations |
| `HIGHLINE_STATION_PWM` | 40 | Approach speed after North magnet |
| `HIGHLINE_DWELL_MS` | 10000 | Stop duration at the station |
| `HIGHLINE_RAMP_DOWN_MS` | 300 | Ramp-down rate |
| `HIGHLINE_ARM_TIMEOUT_MS` | 30000 | Disarm if no South follows North |
| `HIGHLINE_RAMP_UP_RATE_MS` | TBD | Acceleration rate |

40 PWM is a mid-run speed reduction from 70, never a start-from-rest, so
motor stiction at 40 is not a concern. Bench-confirm both locos hold 40
smoothly.

---

## 10. Dashboard (Flask) changes

Facts from the current app (`ngr_app_v1_9_3.py`):

- The mode selector already exists. `POST /<loco>/mode/1` publishes
  `auto = "1"` to `ngr/loco/<id>/cmd/auto`, which the loco handles by
  calling `enterDispatcherAutoMode()`. Today it is binary: MANUAL or AUTO.
- The mode badge is computed from the `auto` and `ce` state flags and shows
  MAN / CTO / CE (template lines ~315, ~433).
- The console uses behavioural language (Arrival Style, Departure Style,
  Trailing Creep), not firmware terms.

Changes needed for HL-Auto:

1. **Add a third mode selection.** Extend the mode route so the dashboard can
   command HL-Auto — e.g. `POST /<loco>/mode/2` publishing to a new
   `cmd/hlauto` topic (or a mode value on a `cmd/mode` topic). LL-Auto keeps
   using `cmd/auto` unchanged.
2. **Add an HL badge.** Extend the MAN/CTO/CE expression to also show **HL**
   when the loco reports Highline active. Requires the loco to publish a
   Highline state (see below).
3. **Conditional pre-auto gate — operator's requirement.** The dashboard
   currently blocks AUTO until orientation (CW/CCW) and mile-marker (MM) are
   set. These are DNA-navigation prerequisites and are **meaningless for
   Highline**. The gate must therefore be conditional:
   - **LL-Auto:** require CW/CCW and MM, as today.
   - **HL-Auto:** do **not** require them. Forward direction is still
     required (shared entry guard).

## 11. Open questions

1. **Highline state reporting.** Which new topic(s) report HL-Auto status to
   the dashboard (armed / cruising / dwelling), feeding the HL badge?
2. **Mode command shape.** New dedicated `cmd/hlauto` topic vs a unified
   `cmd/mode` value — pick one and keep `cmd/auto` (LL) untouched for
   compatibility.
3. **Ramp-up rate.** Value for `HIGHLINE_RAMP_UP_RATE_MS`.

---

## 12. Out of scope

- Multiple locomotives on the Highline / CTO on the Highline
- Dispatcher awareness of Highline
- Reverse-direction Highline running
- DNA navigation changes of any kind
