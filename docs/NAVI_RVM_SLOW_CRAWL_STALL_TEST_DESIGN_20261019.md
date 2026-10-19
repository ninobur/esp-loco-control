# Track test design: Otto's slowest Hall development rate (slow crawls and stalls through magnets)

Claude (Opus 5.5), 2026-10-19. **Design only, for David's review.** Nothing in
it has been run, and it changes no firmware, Pi service or configuration.

## Purpose

`docs/NAVI_RVM_HALL_CONTINUITY_QUESTION_20261019.md` found that, in the recorded
data, the Hall tracing separates "stationary inside a magnet" from "moving
inside a magnet" over a span of about 1 s:

| Class | Change over 1 s |
|---|---|
| Stationary in a magnet | at most 14.4 ADC |
| Slowest recognized moving passage (PWM about 30) | at least 46 ADC |

Its gap was that **no recording with independent motion truth shows Otto
crawling through a magnet slower than that**, nor the creep into a stall. The
MM109 stall shows the Hall slowing from about 10 to 1 ADC/s; whether Otto was
still moving there is not known.

This test measures, with independent motion truth:

- **M1:** the Hall development rate (range over τ = 0.5, 1, 2, 4 s) of passages
  through a magnet at the slowest speeds at which Otto keeps moving;
- **M2:** the creep into a stall: the Hall rate at Otto's last real movement;
- **M3:** the stationary drift over a magnet for long dwells, at the extremum
  and on a flank, for both polarities;
- **M4:** the outcome of the current recognizer on each trial, by offline replay
  only.

**The test decides nothing.** It reports whether, and over what span, Hall-only
development separates from standstill. Any rule is David's decision afterwards.

## Configuration (unchanged)

| Item | State |
|---|---|
| Otto firmware | NAVI_RVM_SHAPE_MANUAL `3a8953c` as flashed; observation only, manual driving. It already sends native 5 kHz Hall, applied PWM and commanded PWM to the Pi NAVI sync recorder. |
| Motion truth | The IR test car (separate ESP32, wheel sensor, nominal 9.652 mm per pulse), coupled to Otto, transmitting on ESP-NOW to the Pi receiver (`/home/david/NGR/ir_espnow/ir_espnow_raw_YYYYMMDD.log`). It was transmitting during run 8 (33,870 frames in 10:29–10:44). |
| Pi loggers | As running: NAVI sync recorder (`.nsr`), IR ESP-NOW raw log, MQTT run logger. Nothing restarted or reconfigured. |
| Clock alignment | NSR frames and IR lines both carry the Pi receive time. Otto's clock maps to it through the NSR status records. The expected error (tens of ms) is small compared with the 0.5–4 s spans measured. |

## Known limitation of the motion truth (must be resolved first)

The test car's onboard completed-pulse detector (TX 1.5) **counts nothing below
about 2 pulses/s** (about 19 mm/s), per the repo's own sweep
(`docs/IR_SLOW_DEPARTURE_LIMITATION_20260919.md`). That is exactly the crawl
range this test is about.

The car also transmits its **raw 1 kHz IR waveform**, which the receiver logs.
Ground truth for slow motion must therefore come from that waveform, analysed
offline (stripe transitions, with no rolling-envelope detector), not from the
onboard counts.

## Step A0: desk check before any track time (analysis only)

Using **run 8's existing IR log**, which covers the MM109 stall:

1. Decode the raw IR waveform (`tools/ir_movement_decode.py`, plus an offline
   transition extraction) for 600–670 s of Otto time.
2. Establish whether the raw waveform resolves wheel-stripe transitions at Otto's
   slow running (PWM 30–32) and shows **when the wheel actually stopped** at
   MM109. In particular: was Otto creeping during 606–612 s, while the Hall rose
   2017 → 2034?
3. Confirm the clock alignment on a sharp event (the departure at about 665 s:
   Hall 2,020 → 2,069 within 1 s).

**Outcomes:**
- If A0 resolves slow motion, the track test can proceed, and **A0 alone may
  already answer M2 for MM109.**
- If the raw waveform cannot resolve motion below about 19 mm/s, the test has
  **no motion truth for the case it exists to measure**. Stop and report.
  Alternatives (for example, timestamped video of the wheel) would be David's
  choice.

## Preconditions on the day

1. Otto reports `3a8953c` / `NAVI_RVM_SHAPE_MANUAL` in `state/bootid`.
2. The IR car banner and version are identified and coupled to Otto. Run the
   runbook's **2-minute stationary check**: completed counts must not rise, and
   the raw waveform must be present in the receiver log.
3. Battery voltage noted (INA219 on the dashboard). The `heap_min` baseline is
   noted.
4. The Pi has free disk (the NSR file is 8.7 GB and growing).
5. A trial sheet, filled in by David: trial ID, magnet, polarity, intended
   action, and wall-clock start and end (to the nearest few seconds).
6. Declare CW at a known interval before the first trial (navigation is
   observation only and is not under test).

## Sites

- **Site A:** the MM109 area, where Otto stalled naturally at PWM 32 in run 8.
  MM109–MM113 are all N.
- **Site B:** a level, straight stretch with **one N and one S magnet**, chosen
  by David.
- If David knows of another location where Otto loses motive power, it can be
  added as Site C.

## Trials (manual, bounded)

Each trial is one action. E-stop is always available. Each stall dwell is
**limited to 60 s** (MM109 already reached 60 s at PWM 32), and current is
watched on the dashboard. Stop the session on low voltage, any `online` flap or
reboot, or an IR stationary-check failure.

| ID | Action | Repeats | Measures |
|---|---|---|---|
| **T0** | Stationary on baseline, PWM 0, 120 s | 1 at start, 1 at end | Drift control |
| **T1** | **Slowest crawl through a magnet.** On the baseline before the magnet, step the throttle down 2 at a time to the lowest PWM at which Otto keeps moving. Hold it through the whole magnet and onto the baseline beyond. | 3 per polarity per site | M1 |
| **T2a** | **Stall at the extremum.** Crawl in, then reduce the throttle until Otto stops with the sensor near the peak. Dwell 60 s at that PWM (do not go to 0). Then raise the PWM until the passage resumes. | 2 per polarity | M2, M3 |
| **T2b** | **Stall on a flank.** As T2a, but stop on the rising flank (Hall about halfway to its peak). | 2 per polarity | M2, M3 |
| **T3** | **Commanded stop at the extremum** (throttle 0, PWM < 15), 60 s, then GO. This is the case the current rule handles; reference only. | 1 per polarity | M3 reference, M4 |
| **T4** | **Creep.** Hold the PWM just above the stall point inside a magnet for up to 30 s, if Otto can creep at all. | 2 | M1/M2 boundary |

Estimated session: about 22–26 trials, 60–75 min of track time.

## Analysis plan (offline, after the session)

1. **Copy, read only:** the NSR tail covering the session, the IR raw log and
   the MQTT run logs. Record their sha256 values.
2. **Truth:** a per-sample wheel phase from the raw IR waveform, giving
   **moving / stationary** and **speed (mm/s)** per 20 ms block, aligned to
   Otto's clock (validated as in A0).
3. **Per trial**, with `stationarity_evidence.py` measures, labelled by IR
   truth:
   - moving inside a magnet: the flattest τ-span range against IR speed, giving
     the curve **"minimum development over τ against speed"** and its lowest
     point;
   - the stall entry: the Hall rate over the last 0.5, 1 and 2 s before IR
     standstill;
   - stationary: the range over τ (up to 60 s) at the extremum and on the flank,
     for N and S;
   - the T4 creep: the rate and its relation to drift.
4. **The decisive table:** for each τ, the stationary maximum (and p99) against
   the moving minimum (and p1), split by site and polarity. State whether they
   separate, and at what τ.
5. **M4:** replay each trial's native Hall through `tests/navi_rvm_shape/replay.cpp`
   as flashed. Record which magnets are recognized or missed and their LLRs.
   This shows the current behaviour, not a proposed one.
6. Report in `docs/`, preserving every observation including contradictory ones
   (policy §5). No rule, threshold or firmware change is proposed in the report
   without a separate request.

## What the result can and cannot show

**It can show:**
- whether Otto's slowest real movement through a magnet still changes the Hall
  faster than standstill does, and over what span;
- whether the creep into a stall is distinguishable from standstill or merges
  with drift.

**It cannot show:**
- Toby's behaviour;
- behaviour at other temperatures;
- behaviour on grades not visited;
- anything about magnets with unusual shapes (for example MM023's overshoot) unless
  they happen to be among the trial sites.
