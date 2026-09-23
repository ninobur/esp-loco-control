# Otto's survey build — the Toby collector identified, and one correction

**Date:** 2026-09-05
**Branch:** agent/toby-1-13-flash
**Task:** find the sketch that collected Toby's NAVI_ONE survey data and run it
on Otto, changing only what is genuinely locomotive-specific.

**Result: found, and it runs on Otto with a one-line change.** But it is not the
sketch described in the instruction, and the difference matters enough to stop
and ask before flashing anything.

---

## The correction, first

The instruction says Toby's NAVI_ONE survey data was collected *"using the IR
sensor car towed behind the locomotive, with the IR sensor car ESP
communicating with the locomotive ESP over ESP-NOW."*

**The survey build contains no IR code and no ESP-NOW.** Three independent
checks:

1. `/usr/bin/grep -c "IR_TEST_A\|esp_now\|ESPNOW"` over
   `QUORUM_1_13X.ino` returns **0**. Neither profile in that tree defines any
   IR symbol.
2. `docs/research/20260828_WHAT_THE_HALL_SENSOR_SEES.md` — the survey's own
   research record, and the document Toby's profile cites as the source of every
   measured recognizer constant — mentions IR, ESP-NOW or a sensor car **zero
   times**.
3. `field-records/20260828_TOBY_QUORUM_1_13_downgrade.md:114` states it outright:
   *"CTO peer coordination entirely (1.14 and later). **1.13 predates it: no
   pairing, no leader/follower, no fleet stop by absence.**"*

The survey ran on a build that **cannot** talk to a sensor car. It is a
Hall-only waveform recorder.

### What the recollection probably attaches to

There is real IR/ESP-NOW work, and it is a parallel thread, not this one:

| thread | what | where |
|---|---|---|
| **IR Test A** | sensor car paired to a locomotive over ESP-NOW, `CtoPeerPacket` v3 | QUORUM 1.16R; Gate 1 and first live run 2026-08-18; car handed **from Toby to Otto** that day |
| **IR daylight tests** | `IR_SCOPE_ESPNOW_TX/RX`, car → USB dongle recorder | 2026-08-26, `field-records/logs/20260826_ir_daylight_0*.log` |
| **The Hall survey** | waveform capture, no radio | **QUORUM 1.13X, 2026-08-28** |

`IR_SCOPE_ESPNOW_TX.ino` is dated 27 August — the day before the survey — which
is very likely why the two sit together in memory. It is a *diagnostic
instrument for the IR wheel sensor* ("Analyze IR_SCOPE_ESPNOW daylight test logs
for false-motion vs contrast-loss"), it records to a USB dongle rather than to
the locomotive, and its CTO receiver is read-only. The car may well have been
riding behind Toby that week. It did not produce the survey dataset.

**Nothing here needs an IR car, and I have not built one.** If you want IR data
alongside Otto's Hall survey that is a separate instrument running in parallel,
exactly as it did in August — say so and I will treat it as its own build.

---

## 1. The exact sketch

**`firmware/programs/QUORUM/variants/QUORUM_1_13X/QUORUM_1_13X.ino`**, `SKETCH_NAME
"QUORUM_1_13X"` (line 367). Tracked, committed, with its own README, profiles
and test suite.

Its own README describes it as *"QUORUM 1.13 — the sketch Toby ran on 2026-08-28
for the calibration laps — plus capture of the raw Hall excursion for every
admitted marker. Nothing else."* One existing line differs from stock 1.13
(`SKETCH_NAME`); everything else is 130 added lines, and the replay suite output
is byte-identical to stock.

Published on `ngr/loco/<id>/mm/wave`:

```json
{"t":69,"mm":51,"pol":"N","pk":200,"dur":137,"pwm":90,
 "n":162,"sc":3,"pre":12,"tr":0,"clip":0,"rej":0,"drop":0,"d":"<base64 int8>"}
```

`rej` is the disposition — **0** admitted, **1** crossed the threshold but under
the 40 ms floor, **2** sub-threshold and never an event. That third class is why
this build is the right instrument: it records what the thresholds *rejected*,
which is exactly the population Otto's survey has to characterise.

## 2. The evidence that this produced Toby's accepted survey data

| evidence | says |
|---|---|
| `field-records/logs/20260828_survey/README.md` | lists `toby_1_13X_survey_waveforms.log.gz` — *"the environment survey alone, with `rej` dispositions"*; 345 captures over a full circuit |
| `docs/research/20260828_WHAT_THE_HALL_SENSOR_SEES.md` | the conclusions drawn from those files; 2,092 captures across twelve laps |
| `LL_LocoConfig_9950012.h` (NAVI_ONE tree) | every measured constant is *"the midpoint of a measured gap ... in the 2026-08-28 PWM-90 circuit survey (351 waveforms; see docs/research/...)"* |
| `docs/NAVI_ONE_AMPLITUDE_FLOOR_SWEEP_20260904.md` | source table row 1: *"2026-08-28 survey waveforms, `rej` labelled"*, 351 records |
| commit `03d2945` | **"NAVI_ONE: position-free recognizer, gated on the 2026-08-28 survey"** |

The chain is unbroken: 1.13X produced the waveforms, the waveforms produced the
research record, the research record produced the recognizer block.

## 3. Configuration differences required to run it on Otto

**One line.** In `QUORUM_1_13X/LocoConfig.h`, move the active `#include` from
Toby's profile to Otto's. Nothing else.

Otto's profile in that tree is already complete and already correct for a survey:

| | Otto | Toby | note |
|---|---|---|---|
| `HALL_DEADBAND_COUNTS` | 25 | 25 | identical |
| **`HALL_ENTRY_MARGIN_COUNTS`** | **13** | **13** | **identical — entry threshold 38** |
| `HALL_MIN_PEAK_DELTA` | 35 | 35 | identical |
| `HALL_DOMINANCE_PERCENT` | absent | 120U | **not read by this sketch** (comment only, line 345) |
| everything else | | | identical but identity: `LOCO_NAME`, `LOCO_ID`, `BLYNK_AUTH_TOKEN`, and the dead `HALL_POLARITY_INVERTED` |

32 defines identical, 4 differ, all four identity.

**The 38-count aperture the instruction asks for is already there.** This tree
predates the 2026-08-20 phantom gate, which was applied only to
`firmware/programs/QUORUM/` and inherited by the NAVI_ONE trees. Otto's entry margin is
13 here and 45 there.

Verified by building it:

```
arduino-cli compile --fqbn esp32:esp32:esp32 --build-path <scratchpad>
  Sketch uses 984607 bytes (75%). Global variables use 54564 bytes (16%).
  ELF identity strings: 9950011, QUORUM_1_13X
```

Clean, first time, no source changes. Scratchpad copy; the repository and your
build directories were not touched.

## 4. Anything hard-coded to Toby rather than profile-selected

**Nothing.** `9950012|Toby|TOBY` appears three times in 182,815 bytes of sketch,
all three in comments (lines 90, 594, 1808), none in code. Identity, thresholds
and topics all come from `LOCO_ID` / `LOCO_NAME` via the profile — the topic
string is built at line 2300 as `ngr/loco/%s/mm/wave` from the profile id, so
Otto's captures land on Otto's own topic with no further change.

The waveform capture is also **not gated on auto mode**: `captureWave()` (line
761) runs on the Hall task for every excursion, whatever is driving. Manual
driving records exactly as automatic driving does.

---

## 5. The one genuine Otto-specific difference in the procedure

**QUORUM 1.13X navigates.** It is the production navigator plus one publish, not
a passive recorder. That was harmless for Toby, who has no phantom population.
It is not obviously harmless for Otto at entry 38.

From Otto's own profile, measured 2026-08-20 after the sensor realignment: 585
genuine reads at peak ≥104, and **11 spurious reads at peaks 40–91, durations
40–58 ms**. At entry 38 those clear the threshold, and the 40 ms floor does not
catch nine of the eleven. That population is what forced his gate up to 70 in the
first place, and it is on record costing him a NO_QUORUM at 14:30:51 that
*"opened and never resolved"*.

So a survey at 38 will very likely reproduce that: Otto's navigator disagreeing,
adopting positive offsets, and possibly dropping out of navigation mid-lap.

**This is desired data and an unwanted interruption at the same time.** The
phantoms are precisely what the survey exists to characterise — §10 of the step 1
report is the argument for opening the aperture so they are visible. But a
navigator losing quorum halfway round is a spoiled run.

**Recommended procedure change, and the only one I propose:** run Otto's survey
**manually driven at PWM 90**, the way Toby's two calibration runs were
(`toby_calibration_CW.log.gz`, `toby_calibration_CCW.log.gz` — *"the manual CW
calibration run, PWM 90"*). Capture is unaffected. Navigation may disagree as
loudly as it likes; nothing depends on it, and its disagreements are themselves
data. Toby's environment survey was a full circuit at PWM 90, which is
reproducible manually.

---

## 6. What I have not done

No code changed, nothing flashed, no image handed over. Pending your ruling on:

1. **The IR car.** The record says the survey used no IR and no ESP-NOW. Do you
   want Otto's survey to reproduce that Hall-only method — or is there a session
   the repository does not record, in which case tell me where and I will look
   again?
2. **Manual driving** for the survey run, per §5.
3. **Confirmation to flip the selector** in `QUORUM_1_13X/LocoConfig.h` and hand
   you the Otto image.

`IR_FITTED` and Otto's tractive floor — questions 2 and 3 of the step 1 report —
are **withdrawn**. Both belong to the NAVI_ONE recorder path, and neither exists
in QUORUM 1.13X. Running the proven collector removes them from step 1 entirely,
which is the point of running the proven collector.

## 7. Noted, not acted on

Otto's profile is duplicated across eleven trees carrying **two different entry
margins** — 13 in `SENSORTEST`, `MANUAL`, `QUORUM_1_13D`, `QUORUM_1_13X`; 45 in
`firmware/config`, `firmware/programs/QUORUM`, `NAVI_CL2`, `NAVI_2`, `NAVI_ONE`,
`NAVI_ONE_STATION_CURVES`. Whichever tree gets compiled decides which Otto is
flashed. Flagged; not touched.
