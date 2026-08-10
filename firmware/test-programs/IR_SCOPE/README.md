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
span/3`, 2.5 ms rise-to-rise debounce (production IR_TEST's value — the inherited 15 ms guard was proven on 2026-08-09 to delete spokes at speed), 2500 ms latch discard, contrast gate.
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
span) yields `mpk` (pulses containing ≥ 2 physical peaks — direct
merged-spoke evidence), per-pulse `duty%` (> 100 % is itself a merge
signal), `p/7pk` (apparent pulses per seven peaks), and an automatic NOTE
when `p/7pk` and `p/rev-int` disagree. **`p/7pk` is structural evidence,
not a revolution measurement** — its denominator comes from the same
waveform, so per-spoke optical doubling still reads 7.00.

The only **absolute** pulses-per-revolution figure is `p/rev-mk`, from
operator revolution markers: set the marker text to `rev` (via
`marker/set`), hand-turn the wheel, and press BOOT once per completed
revolution. The replay consumes those MARKER rows (`--rev-marker` sets the
matching substring), excludes windows that span transport gaps, and also
reports **peaks per marked revolution** — the direct test of the
one-peak-per-spoke assumption (7.0 clean; ~14 means each spoke presents
two optical features). Without markers the report states that the absolute
figure is unavailable.

Overlay a candidate on the waveform to see exactly which troughs would emit
a fall edge:

```bash
python3 firmware/test-programs/IR_SCOPE/IR_SCOPE_Replay.py capture.csv --plot 0.50 --start 12 --duration 4
```

## MQTT contract

Topics under `ngr/diag/irscope01/`:

| topic | direction | content |
|---|---|---|
| `status` | out | LWT retained `{"online":0}`; retained `{"online":1,"ch","bssid","rssi",...}` on connect; 5 s JSON health beat: wheel metadata (7 spokes, 27.8 mm dia, 87.34 mm circ — metadata only; **no speed is computed**) plus the network diagnostic set: `wifi` (WL status), `ch`, `bssid`, `rssi`, `wifi_disc`/`wifi_reason` (disconnect events + last reason code), `wifi_kicks`, `wifi_restarts`, `mqtt_state`, `mqtt_att`/`mqtt_ok`, `pub_fail`/`pub_slow`/`pub_max_ms`, `q`/`q_hw` (batch-queue depth and high-water), `fdrops` (forced bench drops) |
| `samples` | out | ~5/s sample batches, format below |
| `marker` | out | `{"sid","sample","text"}` |
| `marker/set` | in | replaces the marker text |
| `control` | in | `marker` = publish marker now, `status` = beat now, `netdrop` = force-drop the MQTT socket, `wifidrop` = force-drop WiFi (bench recovery drills — recovery must be automatic) |

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
50-batch queue (~10 s); the network task drains it peek-publish-remove. On
overflow the newest batch is dropped and counted.

## Telemetry transport (2026-08-09 field dropout fix)

The first field capture flapped — repeated LWT `online 0/1`, ~17 s logger
gaps, `bdrop` racing, and at least one state only a reboot cleared, with
sampling continuing throughout (same `sid`). Cause, established from the
library sources and reproduced arithmetic, was in this sketch's own network
path, not in the sensor, the broker, or the AP:

1. **Stampede → keepalive starvation.** The drain published up to 4 ×
   ~1.35 KB batches per 5 ms pass. lwIP's TCP send buffer (~5.7 KB) holds
   ~4 such batches, so a full-queue flush after any reconnect filled it
   instantly, and every further `publish()` then *blocked* inside
   `NetworkClient::write` (up to 3 s each — bounded by the same `_timeout`
   that `setConnectionTimeout` sets) with `mqtt.loop()` never running.
   Keepalive is 15 s: the broker timed the client out, fired the LWT, the
   client reconnected, blasted the full queue again, and the flap sustained
   itself.
2. **Wi-Fi wedge, reboot-only.** With `WL_CONNECTED` false the task did
   nothing — recovery depended entirely on `setAutoReconnect`, which is
   known to stall after some deauth reasons. That is the state a reboot
   "fixed".

The fix, all inside the network task (the sampler and detector are
untouched):

- **Pacing:** at most one batch publish per pass (`mqtt.loop()` runs between
  every large write), minimum 50 ms between batch publishes (catch-up
  ceiling 20 msg/s ≈ 4× the live rate), and 200 ms spacing (live rate) for
  the first 10 s after every MQTT connect so a reconnect flush cannot
  outrun the link that just failed. Queue deepened to 50 batches (~10 s).
  Backlog beyond that still drops-and-counts (`bdrop`) and appears as seq
  gaps — evidence is never silently discarded.
- **Wi-Fi supervision:** disconnect-reason logging via Wi-Fi events; a kick
  (`disconnect`+`begin`) after 20 s down, escalating to a full radio
  off/on restart after 3 kicks; and a zombie-association kick when MQTT
  connects fail ~60 s straight on a "connected" Wi-Fi. Every action is
  counted (`wifi_kicks`, `wifi_restarts`) and printed on serial.
- **Diagnostics:** see the `status` topic row above — channel, BSSID,
  Wi-Fi status and disconnect reasons, MQTT state and connect counters,
  publish failure/slowness, queue depth/high-water, forced-drop count.

Limitations: catch-up is deliberately capped at 20 msg/s, so an outage
longer than ~13 s begins dropping oldest-first into `bdrop`; the stream
favours bounded, honest gaps over unbounded stampedes. Channel/BSSID are
reported, not pinned — if the diagnostics show roaming between APs, pinning
is a follow-up decision, not something this sketch imposes silently.

### Bench soak + recovery drill (run before the next field capture)

1. **Quiet soak:** bench, wheel stationary, plotter logging. Run **≥ 60
   min** (comfortably beyond the observed failure interval). Expect zero
   `online` flaps; any flap must come with its own evidence
   (`wifi_disc`/`wifi_reason`, `pub_*`, `q_hw`).
2. **Forced MQTT drops:** `mosquitto_pub -h 192.168.68.142 -t
   ngr/diag/irscope01/control -m netdrop` — three times, ≥ 2 min apart.
3. **Forced Wi-Fi drops:** same with `-m wifidrop`, three times.
4. **AP-level outage:** power off the AP (or block the device) for ~2 min,
   restore, wait.
5. **Judge:** `python3 tests/soak_report.py <the CSV>`. Pass requires:
   every gap attributable (forced drop or the AP outage), every recovery
   ≤ 30 s from transport restoration, no still-offline end state, no
   reboot at any point (single `sid` throughout), and coverage gaps
   consistent with `bdrop` accounting.

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
  independently from the waveform**: state stays unknown until a sample
  that is *both* below that candidate's thrLow (or a contrast loss, which
  pins any detector variant to idle) *and* more than the 15 ms debounce
  horizon past the latest possible rise — a rise inside the gap could
  otherwise make the replay emit an edge the real detector would
  debounce. Every unknown stretch, quiet or active, is reported (`unkn`
  column, `resync_unknown` episode with an `activity` flag), excluded
  from statistics, and drawn grey in the overlay — never seeded from the
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
