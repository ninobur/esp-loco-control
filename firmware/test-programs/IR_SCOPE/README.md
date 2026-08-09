# IR_SCOPE — IR wheel-sensor virtual oscilloscope

**Role:** diagnostic instrument (data car). Not locomotive firmware.
**Question it answers (2026-08-09):** why does the detector sometimes merge
adjacent spokes? Event telemetry shows a repeating mixture of normal and
~doubled pulse widths/intervals on the seven-spoke finescale wheel. The
hypothesis under test: the signal enters a spoke normally but the inter-spoke
trough never crosses `thrLow`, so the detector never rearms and swallows the
next spoke.

IR_SCOPE streams the **raw 1 kHz waveform together with the exact thresholds
and detector state that interpreted it**. The detector is IR_DIAG's, verbatim
(decision 0009: diagnostic thresholds match production): rolling 5/95
percentile envelope, `thrHigh = runMin + 2·span/3`, `thrLow = runMin +
span/3`, 15 ms rise-to-rise debounce, 2500 ms latch discard, contrast gate.
One ADC acquisition per sample feeds both the stream and the detector — the
plotted value IS the judged value.

## Pieces

| file | job |
|---|---|
| `IR_SCOPE.ino` | ESP32 firmware: 1 kHz capture + IR_DIAG detector + batched MQTT streaming |
| `IR_SCOPE_Plotter.py` | live scope display + CSV logger (HallProbe Plotter 1.1 interaction model) |
| `IR_SCOPE_Replay.py` | offline falling-threshold replay over a recorded CSV |

## Hardware / build

- ESP32 dev board on the data car; QRE1113 IR sensor on **GPIO 34** (ADC1 —
  pin 33 is the Hall sensor; confusing them produces a convincing dead-sensor
  reading). BOOT button (GPIO 0) publishes a marker.
- Copy `firmware/config/credentials_template.h` here as `credentials.h`
  (git-ignored) and fill in `WIFI_SSID` / `WIFI_PASS`.

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all firmware/test-programs/IR_SCOPE
```

Do not flash without the operator's go-ahead (standing rule).

## Running a capture

```bash
pip install paho-mqtt matplotlib
```

```bash
python3 firmware/test-programs/IR_SCOPE/IR_SCOPE_Plotter.py --broker 192.168.68.142 --window 10
```

- CSV auto-logs to `~/NGR/irscope_logs/irscope_YYYYMMDD_HHMMSS.csv`
  (`--outdir` to change), flushed after every batch.
- **space** freezes the display to inspect a merged pulse; acquisition and
  logging continue underneath.
- Markers: press BOOT on the car, or publish text to
  `ngr/diag/irscope01/marker/set` then `marker` to `.../control`. Markers
  carry the firmware sample number and land as self-contained CSV rows.
- What the merged-pulse condition looks like on screen: raw crosses the
  orange `thrHigh` line (gold ▲, blue band starts), the following trough
  bottoms out **above** the green `thrLow` line, no red ▼ appears, and the
  band runs on through the next spoke.

## Replay: would a different falling threshold fix it?

```bash
python3 firmware/test-programs/IR_SCOPE/IR_SCOPE_Replay.py ~/NGR/irscope_logs/irscope_20260809_1.csv
```

Replays the real detector semantics (contrast gate, debounce, latch
timeout — not a crossing counter) over the recorded waveform at falling
fractions **1/3 (current), 0.40, 0.50, 0.60** (`--frac` adds more), with the
rising rule held at production. The 1/3 replay is validated against the
firmware's own recorded edges first; if that check warns, distrust the rest.

Every replayed pulse is an **episode with an explicit outcome** — completed,
latch-discarded, contrast-discarded, open at a gap, open at record end,
post-gap state-unknown — so a candidate that stops producing fall edges is
reported, not hidden. Merged/short classification is judged against a
**local** base (p25 of the ±10 neighbouring intervals in the same session),
because a hand-pushed wheel changes speed and a pooled base would misread
slow stretches as merges; intervals with too few neighbours are reported as
`uncls`, never guessed.

Interval ratios are scale-free and blind to **uniform** distortion (every
pulse merging two spokes shifts all intervals identically), so the report
also runs a threshold-independent **waveform-structure pass**: a
prominence-based peak counter over the raw trace (`--prom`, default 0.25 ×
span) yields physical spoke passages, `p/rev-phys` = 7 × pulses / peaks
beside the interval-based `p/rev-int`, `mpk` (pulses containing ≥ 2
physical peaks — direct merged-spoke evidence), per-pulse `duty%` (> 100 %
is itself a merge signal), and an automatic NOTE when the two revolution
figures disagree. Limit: peaks/7 assumes one optical peak per spoke —
confirm absolute revolutions with the hand-turn marker protocol; a
consistent 2× disagreement with hand-counted turns means duplicated
optical features per spoke.

Overlay a candidate on the waveform to see exactly which troughs would emit
a fall edge:

```bash
python3 firmware/test-programs/IR_SCOPE/IR_SCOPE_Replay.py capture.csv --plot 0.50 --start 12 --duration 4
```

## MQTT contract

Topics under `ngr/diag/irscope01/`:

| topic | direction | content |
|---|---|---|
| `status` | out | LWT retained `{"online":0}`; retained `{"online":1,...}` on connect; 5 s JSON health beat (includes wheel metadata: 7 spokes, 27.8 mm dia, 87.34 mm circ — metadata only, never used by the detector; **no speed is computed**) |
| `samples` | out | ~5/s sample batches, format below |
| `marker` | out | `{"sid","sample","text"}` |
| `marker/set` | in | replaces the marker text |
| `control` | in | `marker` = publish marker now, `status` = beat now |

**Batch format** (one JSON object per 200-sample batch):

```json
{"sid":"a1b2c3d4","seq":123,"first":24600,"n":200,"miss":0,
 "env":[runMin,runMax,thrHigh,thrLow],
 "envu":[[off,runMin,runMax,thrHigh,thrLow]],
 "late_us":40,"late_n":0,"miss_n":0,"envx":0,"bdrop":0,
 "latch":0,"closs":0,"sat":0,"pulses":42,
 "hex":"..."}
```

- `sid` — boot session id; traces from different sids are never joined.
- `seq` — batch sequence; a gap means dropped batch(es), also counted in
  `bdrop`. `first` — sample number of `hex[0]`; **sample N is exactly N ms
  after capture start** (deterministic 1 kHz via `vTaskDelayUntil`; per-batch
  worst residual lateness against that grid is `late_us`, cumulative samples
  >250 µs off-grid are `late_n`).
- `miss` — sample slots **skipped** immediately before `first` because the
  sampler stalled a full slot or more (cumulative in `miss_n`). Missed slots
  are never fabricated as data: the open batch is flushed short, the slot
  numbers are skipped, and the tick resynchronizes — so every sample in
  `hex` really was acquired within a slot of its nominal time and `first+i`
  reconstruction stays exact. The detector itself ran continuously through
  the stall; only acquisitions are absent.
- `env` applies from `first`; each `envu` entry applies from `first+off`
  (envelope recomputes every 250 ms; `envx` counts the never-expected case
  of more than 4 recomputes in one batch).
- `latch`/`closs` — cumulative latch-timeout and contrast-loss discards.
  These clear `inPulse` **without** a fall flag: absence of the fall edge is
  the honest record.
- `hex` — `n` × 4 hex chars, one uint16 per sample:
  bits 0–11 raw ADC; bit 12 `inPulse` (state after the sample); bit 13 rise
  edge; bit 14 fall edge; bit 15 contrast-valid.

Acquisition never waits on MQTT: sensorTask (core 0, prio 2) fills a bounded
25-batch queue (~5 s); the network task drains it peek-publish-remove with a
per-pass cap. On overflow the newest batch is dropped and counted.

## CSV contract

One row per reconstructed sample plus self-contained event rows:

```
wall_time, row_type, session, batch_seq, sample, t_s, raw, run_min, run_max,
thr_high, thr_low, contrast_valid, in_pulse, rise, fall, info
```

- `row_type` — `SAMPLE`, `MARKER` (text in `info`), `GAP` (batches lost in
  transport, extent in `info` — red span on the live plot), `MISSED`
  (sampler-stall slots declared by the firmware — amber span), `SESSION`
  (new boot), `STATUS` (raw status JSON in `info`).
- `t_s` = `sample`/1000, firmware time.
- The replay tool distinguishes the three discontinuities by what
  physically happened: `SESSION` fully resets the detector; `MISSED`
  carries **all** replay state (the firmware detector ran continuously
  through the stall); `GAP` closes the open pulse as `gap_interrupted`,
  clears the interval anchor, and then **resynchronizes each candidate
  independently from the waveform**: state is unknown until the first
  sample below *that candidate's* thrLow (or a contrast loss), which pins
  any detector variant to idle with certainty. The unknown stretch is
  reported (`unkn` column, `resync_unknown` episode), excluded from
  statistics, and drawn grey in the overlay — never seeded from the
  recorded 1/3 detector, whose state is exact only for the 1/3 candidate.
  An unmarked sample-number jump is treated as a GAP, conservatively.

## Scope boundaries

- Diagnostic equipment only: Quorum, production navigation, `IR_TEST` and
  `IR_DIAG` are untouched, and nothing here can command a locomotive.
- The production threshold is **not** changed by this instrument; the replay
  report informs that decision, it does not make it.
- Speed is neither computed nor trusted until a hand-turn test shows exactly
  seven detected pulses for one physical revolution.
- No smoothing anywhere in the pipeline — narrow troughs are the evidence.
