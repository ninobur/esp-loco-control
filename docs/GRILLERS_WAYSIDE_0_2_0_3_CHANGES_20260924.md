# GRILLERS_WAYSIDE 0.2 / 0.3 — detector changes and bench findings

Date: 2026-09-24
Sketches: `firmware/TRACKSIDE/GRILLERS_WAYSIDE_0_1/`, `_0_2/`, `_0_3/`
Board: ESP32 Dev Module, CW Hall on GPIO 32, CCW Hall on GPIO 35
Role: observation only — no locomotive commands, no NAV correction.

## Purpose

A Hall sensor on each side of Station 0 at Grillers. Each loco carries two
identity magnets 2–3 in (51–76 mm) apart: NN = OTTO, SS = TOBY, NS = HANS,
SN = FRANZ. A pass over both sensors is an independent position fix, with
direction given by which side saw the loco first. Three more stations are
planned.

## 0.1 boot problem (not a code fault)

Continuous `?` characters were a Serial Monitor baud mismatch. The sketch runs
at 115200. Resolved by setting the monitor to 115200.

## 0.1 bench capture: what was wrong

After a magnet was detected, many extra events followed. Three detector faults
caused this.

### 1. Events closed as soon as they opened

The 20 ms release timer (`quietSinceMs`) was not cleared when an event opened.
It still held the close time of the previous event, so the first
sub-threshold sample closed the new event at once.

Evidence: 1 ms events (`open_ms=29142 close_ms=29143`). One magnet was split
into two events 40–50 ms apart, and pairing reported those two halves as a
loco (`OTTO pair_gap_ms=46`).

### 2. The same threshold opened and closed an event

70 counts both opened and closed. Near the edge of a magnet's field the reading
passes slowly through 70, and a few counts of noise made it chatter.

Evidence: most openings were at `excursion=70…84`.

### 3. The baseline followed the magnet

The baseline adapted by 1/256 per 1 ms sample (about 0.25 s) whenever the
reading was under 70 counts. That included the approach and departure flanks
of every magnet.

Evidence: the baseline rose from 1933 to 1973 in 1.5 s during magnet
activity. Later the baseline had been dragged to 1939 while the quiet reading
was about 1855, so the return to quiet read as a south pole
(`excursion=-84`). That phantom S produced a false `signature=SN loco=FRANZ`.

## 0.2 changes

| Change | Detail |
|---|---|
| Release timer cleared on open | `s.quietSinceMs = 0` when an event opens |
| Hysteresis | Opens at `HALL_THRESHOLD_COUNTS = 70`, closes below `HALL_RELEASE_COUNTS = 35` (still after 20 ms) |
| Baseline gating | Adapts only when within `BASELINE_QUIET_COUNTS = 20` of baseline; frozen during events (as in 0.1) |
| Baseline rate | `BASELINE_SHIFT` 8 → 12 (1/4096 per sample, about 4 s). Safe because the loco Hall zero was measured moving ≤ 6 counts/s without trending |
| `[RAW]` diagnostic | Raw, baseline and active for both sensors every `RAW_REPORT_MS = 1000` |

### Release time and pairing window checked against speed

Toby at PWM 120 measured 380–400 mm/s; locos run much slower in the station.
At 400 mm/s the two magnets' centres are 128–190 ms apart. Assuming a field
width of about 25 mm (not yet measured), the quiet gap between the two fields
is about 65 ms. The 20 ms release wait therefore does not merge the two
magnets, and `ID_PAIR_MIN_MS = 40` is below the 128 ms minimum. Both are left
unchanged until field width is measured on the bench. Slow passes are not at
risk: an event stays open for as long as the magnet is over the sensor.

## 0.2 bench findings

- **CW: correct.** At rest it stayed within about 6 counts of baseline (1840).
  One S pass gave exactly one event (`open_ms=14877 close_ms=15509`,
  `peak_signed=-1041`) with no fragments and no phantom, and the baseline held
  through the pass.
- **CCW: intermittent connector.** The CCW calibration came out at 486 on one
  boot and about 90 counts high on another. With all magnets 30 cm away, the
  CCW reading wandered between 1879 and 2147. Moving the connector on the CCW
  sensor wire changed the signal. This one fault explains every CCW problem
  seen, including the stuck events.
- **Stuck-event hazard (design).** Because the baseline is frozen while an
  event is open, a bad calibration holds an event open forever (`active=1`
  indefinitely). The periodic `[RAW]` line is what exposed this.
- **Polarity.** A north magnet over CW reads `polarity=S`, the same as the
  other NGR sensors in this orientation. Operator decision: turn the sensors
  over so positive = N on the wayside, as on the locos, rather than change the
  firmware.
- **Wiring.** Operator will rewire both sensors with shielded cable, one
  continuous run with contacts only at the ends. Advice given: shield grounded
  at the ESP32 end only, and sensor GND on its own conductor.

## 0.3 changes (written, compiled, not yet flashed)

| Change | Detail |
|---|---|
| `[RAW]` off by default | `RAW_REPORT_MS = 0`; output only on threshold crossings (operator request) |
| Calibration check | `[FAULT]` if the baseline measured at boot is outside 1200–2600 |
| Stuck-event timeout | An event open longer than `MAX_EVENT_MS = 3000` is reported as `[FAULT]`, not paired, and the baseline is reset to the current reading. Locos do not stop over these sensors, so no real pass lasts that long |

Implication of the timeout: if a loco ever does stop over a wayside sensor for
more than 3 s, the baseline will be reset onto the magnet's field. When the
loco leaves, the return to quiet will look like the opposite pole. The
`[FAULT]` line makes that visible, but the next identity from that sensor
should not be trusted. Revisit if station stops over the sensors become
possible.

## Open items

- Rewire both sensors (shielded cable, continuous), flip them for positive = N,
  then confirm clean `[CAL]` lines and `polarity=N` for a north magnet on each.
- Flash 0.3.
- Bench: single N and S passes on each sensor, then an NN pair across CW → CCW
  (expect `[IDENTITY] OTTO` on each sensor and one `[PASSAGE]`), then a slow
  pass.
- Measure field width (`close_ms − open_ms` at a known speed) and the real
  `pair_gap_ms`, then set `ID_PAIR_MIN_MS` / `ID_PAIR_MAX_MS` from data.
- The `[PASSAGE]` logic pairs the latest CW and CCW identities with no time
  limit between them. Not yet exercised on the bench.
