# IR wheel sensor — findings and design

Status as of 2026-07-27. The sensor exists as a bench and tow-test rig; it is
not integrated into locomotive firmware. This records what has been
established so it does not have to be rediscovered.

---

## Why it matters

**It is the only independent witness on the locomotive.** Position and speed
currently both derive from the same Hall sensor, so a bad magnet read corrupts
both at once and nothing in the system can notice the contradiction. An
independent speed source lets the locomotive say *"DNA claims I reversed
direction, but the wheel says I have been rolling forward the whole time"* —
a check it presently cannot perform at all.

It is also what closes the loop on stopping. Station speeds and stop offsets
are hand-tuned per station and per consist; adding two coaches required
re-tuning and caused a stall on the Grillers grade. With wheel speed a stop
becomes a **distance** rather than a ramp duration, and grade and load stop
mattering.

Milestones M2 and M3 in `ROAD_TO_CTO.md`.

---

## The central finding: it was never mainly a sunlight problem

The project summary attributed the failures to sunlight and prescribed hoods
and shielding. Analysis of `Gaffer_Tape_shielded.txt` showed otherwise.

The three largest "lockouts" aligned **exactly** with MQTT dropouts:

| event | `online` LWT | published width |
|---|---|---|
| 16:30:06 → 16:30:07 | 0 → 1 | 21,274 ms |
| 16:30:52 → 16:30:54 | 0 → 1 | 24,083 ms |
| 16:32:08 → 16:32:19 | 0 → 1 | 34.5 s gap |

And `pulse_count` advanced **+10, +9, +10** across those windows. The sensor
never went blind. It kept seeing wedges the whole time. What stopped was
publishing.

The mechanism: `connectWiFi()` blocks in a `while` loop for up to 30 seconds,
called from `loop()`. Sampling is polled in that same loop, so while the loop
is frozen no ADC read happens and `sensorState` stays stuck true. When the loop
resumes and finally reads below threshold, it computes
`micros() - pulseStartMicros` and emits a 21-second "pulse width." That value
measures how long the CPU was busy, not how long the sensor saw something.

**The ADC never came near saturation.** Across 290 samples: median 1762,
maximum 2511, out of 4095. Not one reading above 4000.

The gaffer-tape hood *did* visibly help, and that observation stands. But the
logged evidence attributed to sunlight was network artefact, which means the
sun problem was never actually measured — only assumed — and the fix for it was
never tested against clean data.

**The pattern worth remembering:** environmental explanations are easy to
generate and nearly impossible to disprove. When one hood does not work you
build a better hood, so the work never terminates. Capture data before
accepting a story about weather.

---

## Target: solid face, not spokes

Settled by field test. A solid wheel face with a defined tape band gives one
optical transition per boundary. Bare spokes produced 16 pulses where 10 were
expected.

Two mechanisms, both real:

**Edge doubling.** Each spoke presents two optical transitions in quick
succession — leading edge entering the field, trailing edge leaving. With a
finite spot size the sensor resolves the edge itself as a feature separate from
the spoke face.

**Aperture effect.** During the gap between spokes the optical path passes
straight through and terminates on whatever is beyond — sky, ballast, garden.
Each gap is an aperture aimed at the ambient IR environment. On a solid face
the wheel itself is the shield and the dark state is a real dark state. This
predicts that bare-wheel performance looks excellent on the bench and degrades
in the garden, which is the worst way to discover a problem.

Black foam wedges behind the tape improved contrast substantially.

---

## Ambient rejection: differential sampling

The QRE1113 is a plain DC reflective sensor. It cannot distinguish its own
emitter's light from the sun's. But that discrimination can be added in
software:

1. Wire the emitter LED to a **GPIO** rather than straight to VCC
2. Read the ADC with the emitter **off**
3. Turn the emitter **on**, allow settling time
4. Read again
5. Take the **difference**

The ambient appears in both samples and cancels. What remains is only the light
the emitter put out and the wheel reflected back. This is what a 38 kHz TSOP
receiver does in hardware, in about fifteen lines of code. Sample the pair at
500 Hz–1 kHz — enormous headroom against the observed 134–205 ms pulse
intervals.

**The one case it cannot save:** if direct sun drives the phototransistor to
the top of its range, both samples read maximum, the difference is zero, and
the sensor is blind regardless. That is where a hood still earns its place —
but it no longer needs to be light-tight, only to keep the sensor out of
saturation so the subtraction has room to work. A modest hood plus differential
sampling should outperform a perfect tunnel alone.

**Prerequisite:** determine whether the emitter is hard-tied to VCC on the
breakout. If so, one trace cut and one jumper to a GPIO. That is the gate on
the entire approach.

---

## Architecture

**Sensor and governor share one ESP32.** The IR sensor wires into the power
car's existing ESP32 — the one already running Toby's motor and the navigation
firmware. There is no link, no protocol, no latency budget. An earlier plan
involving ESP-NOW between cars was solving a problem that does not exist.

**Test rig is a separate minimal car** — ESP32, battery, sensor, nothing else.
Identical model to the power car but not the same unit.

What that means, precisely:

| transfers | does not transfer |
|---|---|
| mount geometry — same sideframe, bolt spacing, clearances | standoff distance |
| the printed carrier design | ADC thresholds |
| | `WHEEL_CIRCUMFERENCE_MM` |

Wheel diameter differs with wear, axle end float differs unit to unit, ride
height sits a millimetre off, and the tape goes on by hand each time. Two
consequences: **slotted mounting holes are required**, not a nicety, because
standoff must be re-tuned after the move. And the circumference constant must
be **measured on the production car** — roll it a known distance, count pulses,
divide. A 2% wheel-diameter difference is a 2% permanent speed bias that would
look exactly like a governor calibration fault.

**The power car's wheels are driven.** The test car's are not. Same geometry,
different job: a driven wheel can slip and an idler cannot, and slip is the one
error a wheel encoder cannot see. This is the advantage the encoder was
supposed to have over back-EMF, and it is reduced when watching a driven wheel.
If the power car has an undriven axle, that is where the sensor belongs. Slip
is least likely at creep speed, which is where the measurement matters most, so
this may be acceptable — but it should be a decision, not an oversight.

---

## Mechanical

**Mount to the truck, not the body.** On a bogie car the truck swivels under
the body on every curve, changing both standoff and radial position on the tape
band by several millimetres, continuously, through the curve. That is a moving
target on a sensor with a narrow sweet spot, and it produces intermittent
dropouts that look almost exactly like sunlight lockout in the logs.

**Wheel side-play was the original constraint and has been removed.** A bushing
fix eliminated axle end float, which is what makes a close-clearance shroud
possible at all. Before that, any tight tunnel would have rubbed intermittently.

**Black ASA is not opaque to IR.** Many black filaments transmit near-infrared
through thin walls, so a 1 mm shroud can leak enough to defeat the exercise.
2.5 mm minimum on shade surfaces, or line the inside with the same gaffer tape
already proven to work.

**Foam creep.** The present mount is a foam block. Closed-cell foam under
compression creeps, especially through summer heat cycles, so the standoff is
probably drifting slowly. Worth a periodic check against calibration values,
and worth replacing with a rigid printed carrier once the sensor question is
settled.

**Weep slot** at the bottom of any snout, for grit and water. The ground bounce
it admits is minor compared with letting the tunnel silt up in a garden.

---

## Firmware requirements

Established from analysis of `Spoke_pulse_timing_5wedge.ino`. None of these are
optional; each corresponds to an observed failure.

> **Status 2026-08-04.** All of the requirements below are implemented in
> `firmware/test-programs/IR_TEST/`. The v1 sketch is retained
> unchanged as the record of what was actually flown.
>
> Three of the requirements were themselves wrong and have been corrected in
> place below — the ISR/`adc1_get_raw()` recommendation, the
> `setSocketTimeout()` claim, and a missing hazard (the reconnect flush).
> **All three corrections came from reading the installed toolchain and this
> repository's own field-measured numbers, not from assumption**: the ADC
> headers under `packages/esp32/.../3.3.11/`, the locomotive firmware's
> `hallTask`/marker-queue implementation, and the 2026-07-31 outage test.
>
> The build is also split by a single `IR_TELEMETRY_WIFI` switch. Undefined,
> no radio is compiled in at all and pulses log over USB, so the capture path
> can be validated with nothing able to stall it; defined, it does the RSSI
> survey. Validating capture and network in one run leaves a bad result
> unattributable, which is why stage 1 of the staged plan asks for no WiFi.

**Capture must not live in the polled loop — use a task, not an ISR.** A
dedicated FreeRTOS task at priority 2, pinned to core 0 (above the network
task), sampling on a `vTaskDelay(1)` tick, feeding a queue that `loop()`
drains. GPIO34 is ADC1, which is required: ADC2 cannot be read while WiFi is
up.

> **Corrected 2026-08-04.** An earlier revision of this section specified a
> comparator ISR or a hardware timer ISR calling `adc1_get_raw()`. That was
> checked against the installed toolchain and is wrong on two counts:
> `adc1_get_raw()` is the **deprecated legacy driver** on core 3.3.11 (it
> lives under `driver/deprecated/`), and its replacement
> `adc_oneshot_read()` **takes a mutex and is not ISR-safe either**. So the
> ISR recommendation had no safe ADC call behind it.
>
> The task pattern avoids the question entirely — `analogRead()` is
> perfectly safe outside an ISR — and it is the shape this project has
> already proven twice in the field on the Hall sensor: measured task gaps
> of **1–4 ms while `loop()` stalled for 20 seconds**, and `loop_max_gap_ms`
> of 80 ms against 94,033 ms before the change. A 1 ms tick gives 20–60
> samples across a typical flag, which is ample. It also means the capture
> task ports onto the locomotive's own ESP32 beside `hallTask()` unchanged,
> which is where this sensor is going.
>
> A comparator front end plus `attachInterrupt()` remains sound — a digital
> edge is genuinely ISR-safe — but that hardware does not exist yet.

**Ring buffer with timestamps taken at detection.** Events captured in the
sampling task and stamped there, published from `loop()` whenever the link
allows. A 30-second outage then costs nothing — every pulse is still recorded
with a correct timestamp and flushed when the link returns. The timestamp must
be taken at detection, never at publication: stamping at publication is
precisely the v1 defect that produced 21-second pulse widths.

**Cap the reconnect flush — peek, publish, remove.** A 30-second outage
buffers roughly 105 events. Flushing them as individual blocking publishes the
moment the link returns stampedes a link that has just proved marginal, and
recreates the congestion the buffer exists to survive. Drain a fixed number
per network pass (8, as the locomotive's marker queue does), and remove each
message from the queue **only after** `publish()` reports success, so a
locally detectable failure retries instead of vanishing. The locomotive
firmware removed first and ignored the result, and markers 24 and 23
disappeared in the 2026-07-31 outage test with the drop counter still reading
zero.

**Non-blocking network.** No `while` loops anywhere in the connect path.
`WiFi.setAutoReconnect(true)`, `WiFi.setSleep(false)` (modem sleep is on by
default in STA mode and causes exactly the latency spikes observed), and a
fixed backoff timer rather than retrying every loop iteration.

**Bound the TCP connect with `espClient.setConnectionTimeout(3000)`.**

> **Corrected 2026-08-04.** An earlier revision listed `mqtt.setSocketTimeout(2)`
> as the connect bound. It is not: that is PubSubClient's own **read**
> timeout. Three similarly-named calls are in play and only one does this
> job — `mqtt.setSocketTimeout(s)` (PubSubClient read timeout),
> `espClient.setTimeout(s)` (the inherited **Stream read** timeout, in
> seconds, no effect on `connect()`), and `espClient.setConnectionTimeout(ms)`
> (the actual TCP connect bound, in milliseconds). On this core `WiFiClient`
> is a typedef of `NetworkClient`, whose `connect()` timeout comes only from
> the last of those. This distinction has already cost this project
> debugging time on the locomotive firmware; `setSocketTimeout(2)` is still
> worth setting, but it is not the connect bound.

**One JSON message per event, not six topics.** The original published six
topics per wedge — roughly 38 TCP writes per second at the observed 156 ms
interval, each a blocking write, from a moving vehicle. That publish rate is
likely a *cause* of the dropouts rather than only a victim. A single atomic
payload also fixes the log artefact where 15 interval values were stale
republishes of the previous pulse.

**Publish `WiFi.RSSI()` with each event.** Lets dropouts be correlated against
position, which answers whether there is an RF dead zone and whether it
coincides with a station.

**Outlier rejection on the interval buffer.** A stall-length interval entering
the rolling average corrupts speed for the next five pulses. Reject anything
beyond ~3× the current median before it enters.

**Reset the buffer on timeout.** The original zeroed reported speed but left
`intervalsFilled` and the buffer intact, so the first pulse after a stop
averaged against pre-stop garbage.

**Adaptive threshold, not a fixed one.** Calibrating once at boot and never
adapting is the same defect that cost weeks on the Hall sensor. Track a slowly
decaying running minimum *and* maximum, and place thresholds at fixed fractions
of the gap. That adapts to rising ambient and to fading contrast at once —
which is what you want, since a dirty lens and a sunny afternoon both present
as a shrinking gap.

**A floor under the threshold.** A self-adjusting threshold will squeeze itself
into the noise if contrast collapses — peeling tape, dirty sensor — and emit
garbage at high rate. Below a minimum span, declare the sensor **unavailable**
rather than reporting nonsense.

**Report signal quality alongside the reading.** The gap between baseline and
threshold is a confidence measure: a wide margin means a clean read, a
narrowing margin means the read is marginal *before* it fails outright. That is
graded evidence the governor can weight, and it gives early warning instead of
a hard failure.

---

## Known errors in the record

Two things in the project documentation are wrong and will mislead:

**The 2× direction is backwards.** With `SPOKES_PER_WHEEL` at 10 on a 5-wedge
target, `revsPerSec = pulsesPerSec / 10` returned **half** the true value. Old
readings were 2× too *low*, not too high. Any ceiling adjusted on the belief
they read high was corrected in the wrong direction.

**The pKPH conversion does not correspond to any standard G-scale factor.** The
sketch uses `mm/s × 0.21`. For 1:22.5 the correct factor is 0.081; for 1:29 it
is 0.104. 0.21 implies roughly 1:58. If `pKPH` is a house unit calibrated
empirically that is fine — but it must match whatever the navigation system
uses, or cross-validation between the two is meaningless.

> **Settled 2026-08-04.** The navigation lineage divides by a single empirical
> constant:
> ```c
> static const float PKPH_MM_PER_SEC = 5.37325f;
> const float measuredPkph = speedMmS / PKPH_MM_PER_SEC;
> ```
> — `NGR_LL_DNA_CTO2_r12` lines 414 and 2100, and identically in SOLONAV 1.x.
> That is 0.18611 per mm/s, roughly **1:51.7**: a house unit calibrated
> empirically, not a geometric scale factor. It is the number the `mm/speed`
> topic has always carried, so it is the number the IR sensor must produce for
> the two to be comparable at all. `IR_TEST` now uses
> `PKPH_MM_PER_SEC` verbatim; v1's 0.21 matched nothing.
>
> **Still open, and a dashboard question rather than a firmware one:** the Pi
> dashboard converts measured speed with `3.6 × 45 / 1000` = 0.162 (1:45),
> which agrees with neither the firmware constant nor any G standard. Three
> numbers were in circulation; two are now reconciled. The dashboard is the
> remaining one and needs an operator decision, since changing it alters every
> KpH figure the console has ever displayed.
>
> Current QUORUM firmware publishes `est_mm_s` raw and does no conversion at
> all, which is the better arrangement: convert once, at the point of display.

---

## Staged plan

Each stage has a result you can see in one session. Stop at any stage that
fails; nothing downstream is wasted, because each stage is useful alone.

**1. Instrument honestly.** Emitter on a GPIO, differential sampling, capture
off the polled loop, **no WiFi at all**, log over USB.
*Pass:* one full loop through the sunny sections with no lockouts.

**2. Read-only comparison.** Logs IR speed alongside marker-derived speed.
Governs nothing.
*Pass:* the two agree within 10% on clean segments, and IR stays sane through
the marker glitches.

**3. Creep authority only.** IR governs below 20 pkph. Markers keep position.
*Pass:* station stops land within tolerance without a crawl spiral.

**4. Distance-based stopping.** Stop by measured distance rather than ramp
duration.
*Pass:* all four stations within ±150 mm, same numbers with 0, 1 and 3 coaches.

---

## Open questions

- Is the emitter hard-tied to VCC on the breakout, or already broken out?
- Does the power car have an undriven axle?
- Is the test car the same model as the power car, or a minimal rig?
- Is `0.21` the pKPH factor the navigation system uses, or a different one?
- Will adhesive tape on a rotating wheel survive a Southern California summer
  with ballast grit? A printed or machined encoder disc on the axle, inboard of
  the wheel and shaded by the car body, would sidestep both the sun problem and
  the durability problem — worth considering before committing the mount
  geometry to the wheel face.
