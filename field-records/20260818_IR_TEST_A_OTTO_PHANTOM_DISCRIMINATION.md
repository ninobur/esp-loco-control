# 2026-08-18 (evening) — IR Test A on Otto: the first phantom IR would have vetoed

**Session:** the IR Test Car handed off from Toby to Otto, then ~3 hours of
running — AUTO, one manual interlude, counter-clockwise. Observation only
throughout: `authority:"OBSERVE_ONLY"` on every `telem/speed` frame, zero
navigation/motor/CTO/E-stop authority. Firmware: Otto
`QUORUM_1_16R_IR_TEST_A` (unchanged sketch, Otto profile newly enabled);
sender `IR_TEST_CAR_ESPNOW_1_0`, rebuilt mid-session (see §4).

Otto was chosen as the mule because the low-speed phantom markers this layer
exists to discriminate are his: the 2026-08-17 night session logged 21
`LOW_PWM`-gated acceptances, about half carrying the same weak signature that
gets `QUARANTINED` at working PWM.

---

## 1. Verdict: IR's value is vetoing markers, not measuring speed

The single most important event of the session, at `23:18:14.370`, mm 61→60,
departing the Grillers stand:

```
gate=RAMP   acc=1   dt=830ms   marker speed = 379.5 mm/s
IR: valid=1   deltaPulses=6   distance_mm=60.1
hypotheses k0..k4 = [0, 315, 615, 935, 1230]   bestk=0   resid=+60.1
```

IR independently measured **6 pulses — 60.1 mm of travel — across an interval
where a genuine adjacent marker requires ~315 mm**. The route hypothesis
resolved to `bestk=0`, which is the spec's phantom verdict: too little travel
for another physical marker. Its neighbours in the same departure all resolve
cleanly to `bestk=1` with small residuals (−12.1, −38.2, −0.6). This event
stands alone at k=0.

The marker-only view could not see it. It computed 379.5 mm/s and accepted the
event, because **`gate=RAMP` suspends the conservation test exactly as
`LOW_PWM` does**. The marker's peak was 38 — the classic phantom signature —
and it was accepted anyway.

**The phantom did real damage and the railway cleaned it up itself:**

- four consecutive `DISAGREE`s followed (mm 59, 58, 56, 55) with inverted
  polarity expectations — the signature of an odometer one marker ahead of
  the world
- quorum opened and adopted **offset −1** at mm 48, undoing precisely the
  one-marker over-advance the phantom caused
- `QUORUM_CLOSED` at mm 47; total excursion about 15 seconds

So the navigator absorbed a phantom, drifted a marker, and spent a quorum
cycle recovering — while the IR layer had already computed the refutation and
published it, labelled `OBSERVE_ONLY`.

**This reframes what IR is for.** Every `QUARANTINED` catch this session (mm
63, 66, 80, 88, 89, 90, 152, 153, 155) was made by the existing
`dt < Q_FLOOR_MS` physical-impossibility test with no help from IR; IR only
corroborated them after the fact. IR's unique contribution is the events that
arrive while the timing gate is **suspended** — `RAMP`, `LOW_PWM`, `NO_PREV` —
where it is the only independent distance witness on the train.

That argues the first useful grant of IR authority is **not speed control**.
It is a veto on marker acceptance when the timing gate cannot rule.

---

## 2. Recurring phantom sites (Otto, CCW)

Confirmed repeaters, each with the same signature on every hit (peak 38–51,
dt 185–413 ms, correctly `QUARANTINED`, IR showing real motion throughout):

| Marker | Hits | Notes |
|---|---|---|
| mm 63 | 3+ | Grillers centre; matched 2026-08-17's two hits at `peak=39, pwm=63` |
| mm 80 | 3 | most reliable repeater; `peak` 38–44, `dt` 205–250 ms, always `pwm=90` |
| mm 88/89/90 | 4 | almost certainly one physical site read at different approach phases |
| mm 152/153/155 | 3 | Bamboo approach |
| mm 66 | 1 | near Grillers; may share a cause with mm 63 |

None of these reached navigation — the gate held every time.

---

## 3. Corrections to claims made earlier in this session

Recorded because they were stated confidently and were wrong:

- **mm 156 is not a "marginal speed-floor site."** It is the **Bamboo station
  stop** (centre mm 157, CCW landing one marker back). The four `STOPPED`
  readings there carried marker durations of ~26 s — Otto standing at the
  platform. IR was correctly reporting zero on a stationary wheel every time.
  Those are not anomalies and must not be read as sensor flicker.
- **A "provisional speed under `REACQUIRING`" was proposed and is forbidden by
  design.** `IRSpeedWire.h` freezes the canonical encoding so that any state
  other than `VALID`/`STOPPED` carries the invalid sentinel, precisely so a
  low-confidence moving measurement can never be mistaken for a measured stop
  (decision 0005). If that number is ever wanted, it needs a **separate field
  in a V2 packet**, not an overload of `speedMmpsX100`.
- **The "governor adds power into a stall" hazard was already ruled on**, by
  decision 0036: a reported zero never authorises an upward PWM correction;
  under a positive motion request the governor holds preset and reports
  `MOTION_UNCONFIRMED`.
- **Decision 0036's summary is broader than its parent, 0014.** 0014's actual
  law forbids *escalation toward maximum* from an already-high PWM (the
  "U-Haul in the median" rule) and explicitly **retains bounded trim as
  actuator behaviour**. Read narrowly and correctly, 0014 does not block
  closed-loop speed control; it defines the bootstrap — `PWM_ESTIMATE` covers
  starting and crawling, measured speed takes over when trustworthy.

---

## 4. Sensor car changes, and an honest null result

Three changes were made to `IR_ESPNOW_SENDER.ino` mid-session and flashed at
~23:00. The wire contract was **not** touched (`IRSpeedWire.h` unchanged, 72
bytes, offsets pinned).

**(a) The edge-silence reset is now unconditional on contrast.** The reset
whose own comment reads *"no interval may span an edge-silence episode"* was
gated behind `!contrastMarginal`, so it withheld itself in exactly the case it
was written for: a stopped wheel settles the optical signal onto one plateau,
span narrows into the marginal band, and the guard stops running.
`previousRiseMs` then survived the whole dwell, so the first interval measured
after departure was the length of the *stop*. It never corrupted output only
because median-of-5 outvoted it — which made `SPEED_WINDOW_N` load-bearing for
correctness by accident rather than for smoothing by design. This is a real
latent defect and the fix stands on its own merits.

**(b) `SPEED_WINDOW_MIN = 3`** — first `VALID` after 4 pulses instead of 6
(~29 mm of travel instead of ~48 mm). Three is a floor, not a preference:
median-of-3 rejects one outlier, median-of-2 returns the larger of two and
would report a single bad interval as the speed.

**(c) `spokeParks` split out of `openAborts`**, so that counter means "a fault
happened" again. Local only, on the 5 s serial line as `parks=`. The
straddling spoke stays uncounted — a deliberate ~9.652 mm under-count, because
at the moment the pulse hangs that spoke has not finished passing and this
sensor counts edges without direction; a wheel creeping back off retreats over
the same edge it arrived by.

### The measurement does not show (b) working

| | before (5-window) | after (3-window) |
|---|---|---|
| samples | 6969 | 904 |
| stop→VALID episodes | 55 | 9 |
| `VALID` share of moving time | 98.0% | 97.8% |
| recovery mean | 0.73 s | 0.89 s |
| recovery max | 1 s | 1 s |
| slowest `VALID` | 26.01 mm/s | 27.49 mm/s |

**No improvement is visible, and the after-figures are marginally worse.**
Two reasons to treat this as inconclusive rather than as a refutation:
the after-sample is 9 episodes against 55, and — more fundamentally — the
instrument cannot resolve the effect. Telemetry is 1 Hz; the predicted change
is two pulses, roughly 0.3 s at typical departure speeds. A 1 Hz sampler
cannot see it.

**This should not be recorded as a win.** The honest statement is that IR was
already healthy before the change (98% `VALID` while moving, every stop
recovered within one sample, readings down to 26 mm/s), and (b) has not been
shown to improve anything. (a) remains justified as a latent-defect fix
independent of any measured speedup.

To settle (b), a higher-resolution measurement is needed — sender-side serial
timestamps at pulse granularity, not the 1 Hz MQTT view.

---

## 5. Bench behaviour does not match track behaviour

On the bench after flashing, a hand spin reached `VALID` normally, then span
collapsed 383 → 95 → 63 within ~5 s of the wheel stopping, crossing into
`INVALID_CONTRAST`.

On the track the same firmware and car logged `cinv=0` across 55 stops — the
contrast-invalid path never fired once, including 26-second station stands.

Same hardware, opposite behaviour. Something about sitting on the rails behind
Otto — residual vibration, ambient IR, or the surface the sensor faces — keeps
span above threshold in a way a desk does not. **Envelope constants must not
be tuned against bench observations.**

Related and unexplained: `abrt=0` for the whole session. Every stop parked
with the beam clear, or the abort path is pre-empted by something not yet
identified. `parks=` was added partly to observe this and had not incremented
by end of session.

---

## 6. Transport (Gate 2's open item, now closed)

Measured across ~1550 markers and 144,009 accepted packets:

- inter-arrival: **median 50 ms, p99 56 ms, max 87 ms**
- `regr=0`, `boot=1`, `gaps=54` of 144,009 (0.04%)
- `txe=0` throughout; `txf=54`

The provisional `IR_LINK_STALE_MS` (500 ms) has roughly 6× headroom over
observed worst-case jitter, and `IR_PROJECT_MAX_MS` (150 ms) about 1.7×.
Neither provisional value is wrong. Both can now be justified from data
rather than guessed.

---

## 7. Other observations

- **Quorum self-resolution ran four times.** Three adopted cleanly (margin 2,
  single viable candidate, 5–30 s). One failed to `NO_QUORUM` with
  `reason:HARD_BOUND` at mm 77 after **90+ seconds of confirmed continuous
  motion**, `viable=[-1,3]` never separating. Operator re-declared. This is
  the slowest non-resolution observed to date; 2026-08-17 had two `HARD_BOUND`
  failures that gave up far sooner.
- **Weak-but-real magnets are a distinct population from phantoms.** mm
  137–148 and 159–162 read `peak` 108–129 and pass `ACTIVE` with IR confirming
  motion every time. mm 91–97 read `peak` 39–47 — phantom-range amplitude —
  and also pass `ACTIVE` legitimately. Peak alone does not separate phantom
  from real; the timing gate and IR distance do.
- **Manual mode does not disable the discrimination logic.** `navOnMarker()`
  runs unconditionally; `autoRunning` gates only whether `requestPwm()` is
  called. The apparent "phantoms only in AUTO" pattern from 2026-08-17 is a
  route-coverage artifact: that night's manual window was 80 seconds covering
  mm 10–32 only, and never passed a phantom site.

---

## 8. State at end of session

**Flashed and verified:**

- Otto (9950011): `QUORUM_1_16R_IR_TEST_A`, IR block newly added to his
  profile, sensor MAC `EC:E3:34:78:A2:60 (usable)`.
- IR Test Car: retargeted to Otto (`B0:CB:D8:D0:FF:4C`, `target_loco=9950011`),
  then reflashed again at ~23:00 with the three changes in §4.
- Toby (9950012): IR disabled — commented out, not deleted, with the
  bench-paired MAC preserved in the comment. The sender registers exactly one
  peer, so this is a hand-off, not an addition.

**Repo hygiene found along the way:**

- `LocoConfig.h` listed Otto's include **twice**, once above the active line
  and once below. Either could be uncommented while the other looked
  untouched. Reduced to one line per locomotive.
- The QUORUM replay suite was failing **33 checks before this session began**,
  from two unrelated causes: `LocoConfig.h` pointed at Toby while the suite is
  built on the Otto-tow capture (16 CTO failures), and the morning's MAC
  pairing commit silently overrode the harness's own sensor MAC (17 IR
  failures, all reporting `NO_SENSOR`). Both fixed; suite green. Neither would
  have been visible without running it.

**Open:**

- Higher-resolution measurement to settle §4(b).
- `abrt=0` / bench-vs-track contrast discrepancy unexplained (§5).
- Decision record superseding part of 0021 (two ESP32s joined by ESP-NOW)
  still unwritten — owed since the Test A phase began.
- The `TOBY_STA_MAC` constant in `ir_espnow_config.h` now holds Otto's
  address. Left renamed-in-comment only, to avoid a rename mid-swap.
