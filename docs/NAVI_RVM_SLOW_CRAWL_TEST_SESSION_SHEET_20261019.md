# Slow-crawl and stall track test: session sheet

For David, to use at the track. Design:
`docs/NAVI_RVM_SLOW_CRAWL_STALL_TEST_DESIGN_20261019.md`, step A0 included.
Prepared by Claude (Opus 5.5), 2026-10-19.

**Nothing on Otto or the Pi changes for this session.**
- Otto runs `3a8953c` (NAVI_RVM_SHAPE_MANUAL, manual, observation only).
- The unified rule is **not** on Otto. It will be evaluated **offline** by
  replaying this session's recording.

## Before the session

| ✓ | Check |
|---|---|
| ☐ | `state/bootid` shows `NAVI_RVM_SHAPE_MANUAL`, model `330cdc007da3445b` |
| ☐ | IR test car coupled to Otto. Note **which side** (ahead / behind) and the **coupling slack** (mm, approximate): ______ |
| ☐ | IR car banner / version: ______. **2-minute stationary check**: completed counts do not rise |
| ☐ | Pi: the NAVI sync recorder, the IR ESP-NOW receiver and the MQTT run logger are all running (no restarts) |
| ☐ | Battery voltage (dashboard INA219): ______ V. `heap_min`: ______ |
| ☐ | Declare **CW** on a **baseline** (between two magnets) at a known interval: ______ |

## Rules for every trial

- One action per trial. E-stop is always within reach.
- Stall dwell is **at most 60 s**; watch the current on the dashboard.
- **Stop the session** on:
  - low voltage;
  - any `online` flap or reboot;
  - the IR car failing its stationary check.
- Note the **wall-clock start and end** of each trial to within a few seconds.
  This is the only way to find the trials in the recording.
- For every standstill, note **where the Hall sensor rests**: the magnet numbers
  either side, or "over MMxxx" and roughly where (peak, flank, shoulder).

## Trials

Sites:
- **A:** the MM109 area (where Otto stalled at PWM 32);
- **B:** your choice of a level straight with one **N** and one **S** magnet
  (note their numbers);
- **C:** optional, any other place Otto loses power.

| ID | Site / magnet | Action | Repeat | Start | End | Lowest PWM that kept moving | Rest position / notes |
|---|---|---|---|---|---|---|---|
| T0-a | baseline | Stationary, PWM 0, 120 s (drift control) | 1 | | | — | |
| T1-N1 | B, N: ____ | **Slowest crawl through the magnet.** On the baseline before it, step the throttle down 2 at a time to the lowest PWM that keeps Otto moving. Hold it through the magnet and onto the baseline beyond. | 1/3 | | | | |
| T1-N2 | B, N | same | 2/3 | | | | |
| T1-N3 | B, N | same | 3/3 | | | | |
| T1-S1 | B, S: ____ | same | 1/3 | | | | |
| T1-S2 | B, S | same | 2/3 | | | | |
| T1-S3 | B, S | same | 3/3 | | | | |
| T1-A1 | A, MM109 | same | 1/3 | | | | |
| T1-A2 | A, MM109 | same | 2/3 | | | | |
| T1-A3 | A, MM109 | same | 3/3 | | | | |
| T2a-N1 | B, N | **Stall at the peak.** Crawl in, then reduce the throttle until Otto stops near the peak. Keep that PWM (do **not** go to 0) for 60 s, then raise the PWM until it moves again. | 1/2 | | | | |
| T2a-N2 | B, N | same | 2/2 | | | | |
| T2a-S1 | B, S | same | 1/2 | | | | |
| T2a-S2 | B, S | same | 2/2 | | | | |
| T2b-N1 | B, N | **Stall on the rising flank** (about halfway to the peak), 60 s, then resume | 1/2 | | | | |
| T2b-N2 | B, N | same | 2/2 | | | | |
| T2b-S1 | B, S | same | 1/2 | | | | |
| T2b-S2 | B, S | same | 2/2 | | | | |
| T2c-1 | B, N or S | **Stall on the outer shoulder** (just as the Hall starts to leave the baseline), 60 s, then resume | 1 | | | | |
| T3-N | B, N | **Commanded STOP at the peak** (throttle 0), 60 s, then GO | 1 | | | — | |
| T3-S | B, S | same | 1 | | | — | |
| T4-1 | B or A | **Creep.** Inside a magnet, hold the PWM just above the stall point for up to 30 s | 1/2 | | | | |
| T4-2 | B or A | same | 2/2 | | | | |
| T5-1 | B | **Short stop in a magnet: under 1 s** (a brief throttle dip to stall, then straight back up) | 1/2 | | | | |
| T5-2 | B | same | 2/2 | | | | |
| T0-b | baseline | Stationary, PWM 0, 120 s (drift control) | 1 | | | — | |

About 26 trials, 60–75 minutes.

- **T2c and T5 were added after review.** They target the parameter-choice
  issues below.
- At the end, message me the session's start and end times. I copy the Pi files
  read-only.

## On the lookout: issues the three parameters could cause

These three numbers decide **which 20 ms observations the recognizer receives**.
They do not qualify magnets.

| Parameter | Value |
|---|---|
| **k** | 50 blocks (1 s) |
| **band** | 24 ADC (largest change still counted as "unchanged" within those k blocks) |
| **m** | 16 × baseline noise (about 8–10 ADC; how far from the baseline the tracing must be before flat stretches are withheld) |

**What each parameter could get wrong:**

| Parameter | Could go wrong if… | What it would do | Where the session shows it | What to note at the track |
|---|---|---|---|---|
| k = 1 s | **too short**: a slow but moving peak or shoulder stays flat for over 1 s | withholds real development; the shape is compressed (already seen: 124 s of flat shoulders withheld while moving, no loss so far) | T1 slowest crawls, T4 creep | the lowest PWM that kept moving; whether Otto ever paused visibly |
| | **too long**: a stop shorter than 1 s is not seen as stationary | the stationary blocks of a brief stop stay in the waveform | T5 short stops | that the dip was brief, and where |
| band = 24 ADC | **too wide**: genuine slow development of under 24 ADC per second is treated as unchanged | withholds a slow crawl through a peak or flank | T1, T4 (the IR gives the true speed) | crawl PWM |
| | **too narrow**: the stationary Hall drifts by more than 24 ADC within 1 s (MM109 drifted 15 in 6 s) | part of a stall plateau leaks into the waveform and can distort the ensemble or the baseline | T2a/T2b 60 s stalls; T0 drift | any visible shudder or creep during a stall; the stall current |
| m = 16σ | **too low**: gently curved baseline near a magnet counts as "off baseline" | flat moving baseline near magnets is withheld | T1 (shoulders), cruise segments between T trials | — |
| | **too high**: a stall on a magnet's outer shoulder sits within 16σ of the baseline | that stall plateau is **kept** and can stretch the ensemble | **T2c** (outer shoulder), T2b (flank) | exactly where it stalled relative to the magnet |
| | **noise-dependent**: the threshold scales with the measured baseline noise (floor 0.5) | if the noise changes (motor, PWM, temperature), the threshold moves with it | all; compare T0-a with T0-b | battery voltage start and end |
| (prototype) baseline tracker | the offline model uses a simple level tracker in place of the recognizer's own baseline line | results can differ slightly from a firmware implementation | all | — |

**Offline, after the session,** each trial is checked under the pre-chosen
settings **and** the full 36-setting grid, against the IR truth. Reported:
- **blocks withheld while moving:** count and where (peak, flank, shoulder);
- **stationary blocks kept:** where;
- each magnet recognized once, missed or split;
- LLR changes;
- for every T1/T4 passage, its **development rate against IR speed**: the
  slowest moving change over 1 s, compared with the 24 ADC band.

A failed or surprising result is reported as found. Thresholds are not adjusted
to fit it; any change is your decision.
