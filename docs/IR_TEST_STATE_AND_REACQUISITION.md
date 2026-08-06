# IR_TEST — honest state classification and reliable reacquisition

**Date:** 2026-08-05
**Sketch:** `firmware/test-programs/IR_TEST/IR_TEST.ino`, node `IR_SPEED_SENSOR` (unchanged)
**Commits:** `e0f7903` (1: interval median) · `58f0b0f` (2: sensor state) ·
`eaae130` (3: latch timeout) · `e1ddd53` (4: decay/quality/NVS) ·
`61abaf3` (raw ring)
**Status:** built, simulated, **not flashed** — Sam reviews before hardware.

One commit per change, revertible independently. The ring buffer ("Also add")
is a fifth commit on the same terms.

Goal, from the task: *the firmware must be a credible judge of when the sensor
is speaking reliably.* The 2026-08-05 failure was not that the sensor stopped
working — it stopped working and said it was fine, for 479 s of a 30-minute
run.

---

## Change 1 — `e0f7903` — interval median replaces revolution-time median

- `SPEED_MEDIAN_N = 5` raw intervals; speed =
  `WHEEL_CIRCUMFERENCE_MM / ((median/1000) * SPOKES_PER_WHEEL)`.
- `revBuffer`, `revIndex`, `revsFilled`, `medianRevMs()` and the summation
  deleted. `intervalRing` is now a plain 5-deep ring.
- Admission stays **unconditional** — the zero sentinel (no prior rise) is the
  only non-insertion, and it is the absence of a measurement, not a judgement.
- Contamination window: exactly 1 interval. At ten spokes the revolution
  summing spread one glitch across 14 output pulses; measured cost after every
  dwell was 8 zeros + 2 garbage values.

Also corrected in passing (invalidated comments): header spoke-count paragraph
(said two tape flags; now describes the factory 10-spoke wheel), `DEBOUNCE_US`
comment (said 15 ms against the committed 2500 µs; now explains the
rise-to-rise bound and why 15 ms would be 1.5× under the fastest genuine
interval).

## Change 2 — `58f0b0f` — UNAVAILABLE / REACQUIRING / VALID / STOPPED

- `state` on every pulse, every status beat, and the timeout event.
  `speed_valid: 1` only in VALID.
- `speed_mmps`/`pkph` are JSON **null** until `SPEED_PROVISIONAL_N = 3`
  intervals are in; provisional (valid 0) figures from 3; VALID at
  `SPEED_MEDIAN_N = 5`.
- **Timeout ≠ stopped.** On edge silence the state is UNAVAILABLE. STOPPED
  requires `motionWitnessSaysStopped()`, which returns false on the survey car
  (no witness exists) — the state is unreachable by construction, and the
  production locomotive's PWM/Hall evidence plugs into that one function.
  The `"event":"STOPPED"` payload is replaced by `"event":"TIMEOUT"`.
- **Timeout detection is sampler-owned** (`lastRiseMs`, written at the edge),
  replacing loop-consumption time which stalls under network backpressure —
  closing Sam finding 2 (an MQTT outage could fabricate a stop mid-motion).
- On timeout: filter history cleared; the next interval arrives
  sampler-flagged `intervalValid = 0` when the gap exceeds `SPEED_TIMEOUT_MS`.
  Equivalent to clearing `prevRiseMs` (task wording), implemented as a flag at
  measurement time so `interval_ms` still reaches the log — the dwell length
  is diagnostic data. **Nothing fabricated is ever inserted into the FIFO.**
- `SPEED_TIMEOUT_MS`: 4000 → **2500**. Derivation: 115 mm / 10 spokes =
  11.5 mm per interval; 575 ms at the 20 mm/s creep bound; tolerate three
  consecutive missed spokes at the measured 30% low-speed miss rate
  (4 × 575 = 2300); round to 2500.
- `PubMsg` payload 256 → 384 (state fields), → 512 in Change 4.

## Change 3 — `eaae130` — latch timeout

- `LATCH_TIMEOUT_MS = SPEED_TIMEOUT_MS` (2500). Same physics: at ~50% duty a
  pulse lasts half a spoke period; above the creep bound no genuine pulse can
  be even half this long.
- On expiry, in the sampler: pulse **discarded** (never becomes an event),
  `sensorState` cleared, `prevRiseMs` zeroed (the aborted rise anchors
  nothing), `latch_timeouts` incremented. The artificial fall that eventually
  arrives emits nothing — `sensorState` is already false.
- In loop(): a counter change publishes a `LATCH_TIMEOUT` event (null speeds),
  resets the filter, sets UNAVAILABLE. No NVS write — a latch proves nothing
  about the envelope.
- If the signal is still above threshold the machine re-arms and re-latches;
  each increment is another 2.5 s of confirmed blindness. **Log note:** each
  latch consumes a pulse number without an event, so a `seq` gap with
  `latch_timeouts` advancing is a latch, not a lost message.

## Change 4 — `e1ddd53` — activity-gated decay, honest quality, provenance, atomic NVS

**Decay gates on signal activity** — rolling min/max range over
`ACTIVITY_WINDOW_MS` (250) of raw samples, threshold `ACTIVITY_MIN_RANGE`
(100 counts; stationary noise is tens, a moving spoke signal is
hundreds-to-thousands):

| trace | meaning | decay |
|---|---|---|
| flat | stationary | **hold**, phase included (no catch-up burst) |
| varying, edges flowing | normal motion | every 250 ms (8 counts/s/side) |
| varying, no rise for a timeout | **blind-while-moving** | every 50 ms (40 counts/s/side) — decay is the only path back to sight |

Expansion stays unconditional and above every gate. The rate is bounded steps,
never proportional — nothing chases a transient.

**Quality** is now `assessQuality(span, active, sinceRise, latchMs)`:
UNAVAILABLE on span collapse **or** blind-while-moving **or** an expired
latch; MARGINAL on thin span **or** a latch past 1 s. Deliberately **no
max-span ceiling** (task correction honoured): wide span with edges flowing is
excellent contrast; the fault is width the current signal cannot cross, and
active-but-edgeless measures exactly that. The sampler's halt is now
explicitly on `MIN_USABLE_SPAN`, not on quality — the blind verdict must not
stop edge detection during the recovery the decay is driving.

**Provenance and diagnostics** on every status beat: `run_min`, `run_max`,
`th_hi`, `th_lo`, `local_range`, `env_src`
(COLD_PRIME / NVS_RESTORE / LIVE_ADAPTED), `env_age_ms`, `latched`,
`latch_ms`.

**Atomic NVS**: one checksummed, versioned, magic-tagged blob
(`EnvBlob`, key `env`, namespace `irsense`) — NVS commits an entry atomically,
so power loss mid-write yields the previous complete pair, never the old/new
mix two `putInt`s allowed. `putBytes` result checked; no "saved" log on a
failed write. Pair snapshot under `portMUX` on both sides (two int copies at
1 kHz). Legacy `envmin`/`envmax` keys remain a **read-only migration path**
for the field device; the next save writes the blob.

## Raw ring — `61abaf3`

2048 samples (4 KB static, 2.048 s at 1 kHz ≈ 3.5 spoke periods at creep),
written every tick. Frozen on latch timeout or edge-silence entry to
UNAVAILABLE; streamed oldest-first as 22 hex chunks of 96 samples on
`telem/rawdump` (own topic — `telem/pulse` untouched), one chunk per loop
pass and only when the outbound queue has room; then thawed. A genuine stop
dumps too (~7.7 KB/stop): run-down on a stop, trapped signal on a blind
episode, `reason` field says which.

---

## Verification

### 1. Balance
Braces 86/86. Preprocessor 12 `#if*` / 12 `#endif`.

### 2. Builds (esp32:esp32:esp32, `--warnings all`)

| commit | WiFi flash | RAM |
|---|---|---|
| 1 | 909,456 (69%) | 47,232 (14%) |
| 2 | 909,908 (69%) | 47,240 (14%) |
| 3 | 910,344 (69%) | 47,256 (14%) |
| 4 | 911,404 (69%) | 47,280 (14%) |
| ring | **912,256 (69%)** | **51,464 (15%)** |

USB-only at head: 293,544 (22%) / 26,516 (8%).

Warnings — all three pre-existing, none introduced: `pulseCount++`,
`eventDrops++` (`-Wvolatile`, C++20, untouched code); `lastRssiPublish`
unused (USB build only).

### 3. Identifier differences vs the task prompt

| prompt said | reality |
|---|---|
| "clear `prevRiseMs`" (Change 2 timeout) | implemented as sampler-set `intervalValid = 0` on the next pulse — identical filter behaviour, and `interval_ms` (the dwell length) survives into the log. `prevRiseMs` **is** cleared directly in the latch path (Change 3), where the rise itself is void. |
| `latch_timeouts`, `speed_valid`, `run_min`, `run_max` | did not exist; created as specified (variables `latchTimeouts` etc., JSON names as given) |
| everything else | matched the source exactly: `revBuffer`, `REV_MEDIAN_N`, `intervalRing`, `prevRiseMs`, `sensorState`, `lastPulseMs`, `qualityFromSpan()`, `MIN_USABLE_SPAN`, `SPEED_TIMEOUT_MS`, `ENVELOPE_DECAY_MOTION_WINDOW_MS` |

Deleted identifiers: `REV_MEDIAN_N`, `revBuffer`, `revIndex`, `revsFilled`,
`medianRevMs()`, `lastPulseMs`, `ENVELOPE_DECAY_MOTION_WINDOW_MS`,
`qualityFromSpan()`, `NVS` use of `putInt`.

### 4. Published schema, field by field

**`ngr/spoke/IR_SPEED_SENSOR/telem/pulse`** — one per completed pulse:

```json
{"seq":41,"t_ms":128340,"interval_ms":72,"width_ms":13,
 "state":"VALID","speed_valid":1,"speed_mmps":159.72,"pkph":29.72,
 "raw":1198,"span":3660,"quality":"OK","accepted":1,"rssi":-62,
 "ev_drops":0,"rej":0}
```

| field | type | notes vs previous schema |
|---|---|---|
| `seq` | u32 | unchanged. **Gaps now also arise from latches** (a latch consumes a number, emits nothing) |
| `t_ms` | u32 | unchanged |
| `interval_ms` | u32 | unchanged; after a dwell it carries the dwell length (flagged internally, not fed to the filter) |
| `width_ms` | u32 | unchanged; can no longer exceed `LATCH_TIMEOUT_MS` (2500) |
| `state` | string | **NEW** — `UNAVAILABLE\|REACQUIRING\|VALID\|STOPPED` |
| `speed_valid` | 0/1 | **NEW** — 1 only in VALID |
| `speed_mmps` | float 2dp **or null** | **CHANGED** — null until 3 intervals; provisional 3–4; never 0.00 outside a true measurement |
| `pkph` | float 2dp **or null** | same rule |
| `raw` | int | unchanged (falling-edge value) |
| `span` | int | unchanged |
| `quality` | string | same values, **new meaning** — now from `assessQuality()` (multi-input), not span floor alone |
| `accepted` | 0/1 | **CHANGED** — 0 = interval did not feed the filter (no prior rise, or gap > `SPEED_TIMEOUT_MS`) |
| `rssi` | int | unchanged (0 in USB build) |
| `ev_drops` | u32 | unchanged |
| `rej` | u32 | hard 0, retained for schema shape |

**Event variants on the same topic** (no `interval_ms` — parsers must not
assume its presence):

```json
{"seq":167,"event":"TIMEOUT","state":"UNAVAILABLE","speed_valid":0,"speed_mmps":null,"pkph":null,"rssi":-67}
{"seq":170,"event":"LATCH_TIMEOUT","state":"UNAVAILABLE","speed_valid":0,"speed_mmps":null,"pkph":null,"latch_timeouts":3,"rssi":-67}
```

`"event":"STOPPED"` is **no longer published** on this build — parsers
watching for it must watch for `TIMEOUT` (and later, on a witness-equipped
locomotive, `state:"STOPPED"`).

**`ngr/spoke/IR_SPEED_SENSOR/telem/status`** — every 5 s:

```json
{"pulses":167,"raw":1734,"span":3660,"quality":"OK",
 "state":"VALID","reacq":3,"latch_timeouts":0,
 "run_min":210,"run_max":3870,"th_hi":2650,"th_lo":1430,
 "local_range":2210,"env_src":"LIVE_ADAPTED","env_age_ms":740,
 "latched":0,"latch_ms":0,
 "ev_drops":0,"pub_drops":0,"rej":0,
 "task_max_gap_ms":2,"mqtt_attempts":1,"rssi":-62,"heap":205000}
```

New fields: `state`, `reacq`, `latch_timeouts`, `run_min`, `run_max`,
`th_hi`, `th_lo`, `local_range`, `env_src`, `env_age_ms` (ms since last decay
step; uptime if never), `latched`, `latch_ms`. All pre-existing fields keep
name, type, and relative order.

**`ngr/spoke/IR_SPEED_SENSOR/telem/rawdump`** — **NEW topic**, only on a
freeze:

```json
{"part":1,"of":22,"reason":"LATCH","t_ms":128340,"hex":"4b24ce..."}
```

`hex` = 96 samples × 3 hex digits (12-bit ADC), oldest first across parts;
final part short (32 samples). `reason` ∈ `LATCH|TIMEOUT`.

`telem/rssi` and `status/online` unchanged.

### 5. No 0.00 in REACQUIRING/UNAVAILABLE
By construction: the speed fields are null until 3 intervals are in, and only
genuine nonzero intervals enter, so any published number is
circumference/median > 0. Host replay of 66 pulses across six scenarios
(including both field traces): **0 forbidden zero-speed outputs**.
UNAVAILABLE publishes no pulse payloads at all (no edges) — only events,
which carry null.

### 6. No completed pulse from a latch
The latch branch emits nothing and clears `sensorState`; the eventual
artificial fall finds `sensorState` false and emits nothing. Verified by code
path — there is no emit site reachable from the latch.

### 7. `MIN_USABLE_SPAN` on NVS restore
Honoured in both restore paths (blob and legacy): magic/version/checksum →
range → inversion → `storedSpan < MIN_USABLE_SPAN` → cold prime. Save-side:
an envelope under the floor is never written.

### 8. Field-trace replay

Seq 1–19 verbatim (including the 261,632 ms dwell interval):
- the dwell gap is sampler-flagged invalid and **never enters the ring**;
- provisional estimate at pulse 5, VALID at pulse 7, 157–162 mm/s throughout
  (true ≈ 159.7);
- the trailing 271 ms glitch: zero effect (median absorbs it).

Old wedge seed (41 ms then genuine 232–620 ms): converges by pulse 4, VALID
at pulse 6, never sticks. Missed spoke (72→144 ms): **zero error**. Spurious
edge (72→36+36): **zero error** (two bad slots of five never outvote three
good). Dwell recovery: null·null·null, provisional at 4, VALID at 6 — against
the old 8 zeros + 2 garbage + correct-at-11.

Envelope simulation (1 ms resolution, transcribed code):
- **Blind-while-moving** (field envelope 210/3900, signal 1300–2400 trapped
  mid-band): quality honestly UNAVAILABLE from t = 0.2 s; fast decay narrows
  the band; **first edge at t = 18.7 s** — against 261 s paid in the field.
- **Stationary 10 min** (flat 1800): envelope byte-identical from t = 3 s on,
  quality OK, zero decay. B1's fix survives.
- **Normal running 30 s**: expansion/decay equilibrium, stable span, OK
  throughout.

---

## Expected behaviour changes in the next field log

1. `quality` will read UNAVAILABLE within ~2.5 s of any blind episode —
   the 479 s of false OK cannot recur.
2. Speed output after a dwell: 3 nulls then a provisional figure, no zeros.
3. Every station stop emits a TIMEOUT event (not STOPPED) plus a 22-part
   rawdump (~7.7 KB).
4. `seq` gaps with `latch_timeouts` advancing are latches, not losses.
5. Miss-rate prediction (IR_SENSOR_NOTES, "Prediction on record"): if the
   envelope hypothesis is right, the steady-fast-window miss rate falls well
   below 6.7%. The rawdump makes the alternative diagnosable either way.

## Not done, deliberately

- `WHEEL_CIRCUMFERENCE_MM` still 115.0 (task rule; ~109 mm derived from
  Toby's mile markers, pending clean data — all speeds ~5% high until then).
- Differential ambient sampling (hardware-gated: shared VCC rail).
- Per-pulse **peak raw** (named in IR_SENSOR_NOTES as the direct
  threshold-margin test; small addition, but outside this pass's four
  changes — flag for the next one).
- `qualityFromSpan()` deleted rather than kept dead — it had exactly one
  caller and its floor-only logic is the documented defect.
