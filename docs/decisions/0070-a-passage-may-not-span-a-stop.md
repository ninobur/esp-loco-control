# 0070 — A passage may not span a stop

**Status:** PROPOSED. Not ratified. **No firmware has been modified.**
**Date:** 2026-09-01, rewritten 2026-09-02 on the operator's ruling
**Touches:** 0054 (four conjunctive characteristics), 0057 (no duration ceiling,
no motion gate on the guard), 0059 (the six-marker window), 0064, 0065.
**Evidence:** findings 09, 11, 13; `tools/interrupted_replay/`.
**Would ship as:** NAVI_ONE 1.0.

---

## The governing rule

> A magnetic level is not a magnet event. A partial arc is not a magnet event.
> Only a completed, coherent rise-and-fall waveform establishes a MOVING MAGNET.
>
> A controlled stop should behave like a timeout in a game. The measurement is
> paused while Toby is stationary, then resumed when moving-magnet morphology
> returns.
>
> PWM transitions arm observation. Hall morphology determines whether
> measurement pauses or resumes. Only the completed moving rise-and-fall pattern
> establishes a magnet.
>
> — the operator, 2026-09-02

**This replaces the proposal of 2026-09-01 in full.** That one added a second,
weaker way to establish a magnet: it judged the pre-stop or the departure
fragment on its own, without the shape test. It is withdrawn. Its own gate found
four things it would have counted as magnets — a gradual DC ramp, a slow
electrical step, a shoulder, and a double-lobed artifact — and the operator's
answer is the right one: incomplete evidence does not establish anything.

**In this design the recognizer is not touched at all.** `MagnetRecognizer.h`
and `WaveformWindow.h` are byte-for-byte the firmware's, and the harness
compiles them unmodified. There is no second recognizer, no `shapeTested: 0`
exemption, and no threshold that moves. What changes is when the acquisition
layer is *recording* — and nothing else.

---

## The problem

A passage runs from a threshold crossing to a threshold return. That model
assumes the sensor is *moving past* the magnet. When the locomotive stops
mid-field the model has no way to end, and everything before the stop merges
with everything after it into one object, which is then fitted to a Gaussian and
thrown away.

Twice on 2026-09-01, twenty-one minutes apart, at two platforms:

| | Grillers CCW (finding 11) | Arches CCW (finding 13) |
|---|---|---|
| the real magnet is | at the HEAD, on arrival | at the TAIL, on departure |
| plateau | 163 counts, on the magnet | 41 counts, a fringe |
| open for | 37,733 ms | 36,689 ms |
| residual | 0.2773 | 0.3825 |
| outcome | MM59 lost, strike | MM106 lost, strike |

Stop-offset tuning relocates the landing; it cannot guarantee the landing is
clear of every fringe field, and both incidents happened with the offsets doing
exactly what they were set to do. Gate 12 H puts a number on it: sweeping the
coast across a whole marker spacing, **two landings in nine leave the sensor in
a field strong enough to hold a passage open.**

---

## The unintended-consequence risk, stated here rather than later

Per the operator's ruling of 2026-08-30, a principle gets its risk written down
the first time it is recorded, and again each time a later decision leans on it.

**The risk is that "a stop was requested" becomes evidence.** It is not. PWM
arms observation and decides nothing: it cannot pause a measurement, resume one,
decide a polarity, contribute a sample, or advance anything. The pressure to let
it do more will be there at every future change — and the moment it does, this
stops being an instrument and becomes a program that tells itself what it
expected to see. Findings 05 and 06 are what that looks like from the inside.

**The second risk is that a paused measurement is a piece of retained state
that can outlive its frame.** It is cleared by `reset()` on every declaration and
direction change, and it is ended by two wall-clock watchdogs that keep running
while the measurement clock does not. If either of those is ever weakened, a
stale half-waveform can be stitched onto a frame that never saw the stop.

---

## The design

```
   TRAVERSING ──────────────────────────────────────────────────────────┐
        │  the field stops moving          the arc continues            │
        │  AND a controlled stop is        AND a controlled departure   │
        │  in progress                     is in progress               │
        ▼                                                               │
     PAUSED ─────────────── resumed, stitched ──────────────────────────┘
        │                                                     │
        │  the field returns to baseline                      │  neither, for
        │  (requirement 8, even while paused)                 │  90 s, or 30 s
        ▼                                                     ▼  after departure
     CLOSED ── the UNCHANGED recognizer ── one advance    ABANDONED ── zero, and
               amplitude, whole-wave polarity, Gaussian              a controlled
               morphology, clipping, rebound guard                   shutdown
```

### 1. The sentinels arm observation

`stopArming` is raised on the loop thread by `stationService()` and read on the
Hall task.

| authority | arms | why |
|---|---|---|
| station zero-ramp, station dwell | `Decelerating` | the only ones today |
| station departure | `Departing` | |
| a CTO stop | — | **not implemented in NAVI_ONE 1.0.** When it arrives it arms here, explicitly, in one line |
| MANUAL | — | `stationService()` returns early unless `autoRunning` |
| e-stop, low voltage, dispatcher release, a strike, a contradiction | — | every one clears `autoRunning` |
| `MISSED`, `PHASE_TIMEOUT` | — | the station has stood down |

No existing semantics change. The flag is additive and read in one place.

### 2. Loss of progression pauses the measurement

A moving magnet moves the field. When the field stops moving, the recording
stops. The test is a 400 ms window of samples at the baseline sampler's own
cadence, **with the single highest and single lowest discarded**, spanning no
more than `settleSpan` counts.

**The constant is measured, and the first proposal failed the measurement.** A
raw ±8 count band was proposed on 2026-09-01 and is arithmetically incapable of
firing on this sensor. Four stationary plateaus from the field captures — 261 to
386 real 1 kHz readings each, taken while Toby was demonstrably parked:

| plateau | n | sd | 16-sample span, raw p95 / max | trimmed p95 / max |
|---|---:|---:|---:|---:|
| finding 11, parked on MM59 | 261 | 4.22 | 48 / 50 | 11 / 12 |
| finding 13, parked in a fringe | 271 | 3.62 | 46 / 46 | 8 / 9 |
| finding 09 A, latched offset | 386 | 3.02 | 21 / 30 | 11 / 13 |
| finding 09 B, latched offset | 261 | 3.81 | 22 / 24 | 11 / 13 |

The raw span is dominated by single-sample outliers — decision 0065's
population, present at rest as well as in motion. Trimming collapses it to a
**maximum of 13 counts across every plateau on record**; the ceiling is 20.

On pause: recording stops, the measurement clock stops, the samples taken while
the field was already flat are dropped, and the samples, the entry baseline, the
polarity evidence and the continuation state are all kept. The wall-clock
watchdogs keep running.

**Falling PWM alone pauses nothing.** Gate 12 F1 arms `Decelerating` for a whole
crossing with the throttle at zero: the field keeps moving, so the recording
does, and the passage completes normally.

### 3. While paused, nothing is evidence

Plateau readings, isolated spikes, reversals, steps and disorganised variation
neither confirm nor veto the pending passage. Gate 12 F3 puts all of them on the
line at once — a spinning departure with motor hash, spikes, a reversal and a
step that arrives and stays — and the measurement neither resumes nor is
corrupted.

One thing does end a pause besides resumption: **the field returning to
baseline**, which is requirement 8 and does not stop being true because the
measurement is paused. Without it, coming to rest on a magnet's *tail* — inside
the entry margin, with only a few counts of arc left — strands the measurement
for ever, because there is no longer enough field left to satisfy the resumption
test. Gate 12 D found exactly that, and it needs a settle window of quiet rather
than the 8 ms an ordinary close takes.

### 4. Resumption is decided by morphology

Nothing resumes because the throttle came up. The field must show:

- a **monotone run** across the resume window, allowing one step to disagree, so
  a single sample can neither cause nor block it;
- at least `resumeMove` (25, = `exitMargin`) counts of travel — which a drift
  cannot make in 200 ms and a step makes in one sample and so fails the run;
- **no reversal through the baseline** — the arc has one sign;
- a **direction the arc could take from where it stopped**: below its own peak,
  Toby is past the top and only a continued fall is plausible; at its own peak
  he may be on the way up or at the apex, and either is.

### 5. Stitching

The excised interval is the stationary one, **defined identically at both
ends** — the pre-stop side is cut at the start of the plateau window, and the
post-stop side is stitched back to the moment the field stopped being flat,
capped at 512 ms.

That symmetry was earned the hard way and it matters more than it looks:

- Cutting the pre-stop side with a *movement* test instead (the same one
  resumption uses) excises the top of every arc, because **at a magnet's apex
  the field is flat by nature**, whatever the speed. It put a stop at the peak
  from residual 0.098 to 0.149.
- Restarting the recording at the moment resumption is *confirmed* — 125 counts
  a second — drops everything between 50 and 125 counts a second that the
  approach side kept. It put a stop on the falling flank at 0.134, over the
  ceiling.
- A fixed 400 ms stitch was worse still (0.160): it drags stationary samples
  back in.

### 6. Closing and judging

The passage closes when the complete moving waveform has returned to baseline,
by the ordinary exit rule. The measurement clock excludes the pause; the wall
clock is still there in `openedAtMs` / `closedAtMs` for the rebound guard.

Then the **unchanged** recognizer: amplitude, whole-wave signed polarity,
Gaussian morphology, clipping, rebound guard. One advance if it passes. Zero
otherwise.

### 7. A pause that never resumes

Two wall-clock watchdogs — 90 s on the pause, 30 s after departure has been
commanded. The second is deliberately long: Toby spins, stalls and takes his
time on the Grillers grade, and none of that is a navigation fault.

On either, the passage is discarded, zero advances are made, and position is
withdrawn **at once** with a specific diagnostic. Not deferred to the next
polarity disagreement: decision 0059 measured that window at six markers, about
1.8 m, and findings 12 and 13 are both that failure.

---

## Evidence

`sh tools/interrupted_replay/build_and_run.sh`. Two build trees, the eleven
existing gates run against **both** with the same unmodified runner, their
output diffed byte for byte, then gate 12.

> **The eleven are IDENTICAL between the two trees.** They include the
> 2026-08-28 survey replay — 187 real passages with their residuals — the
> 2026-08-29 lap replay, the polarity survey and both baseline gates.

**Gate 12: 190 checks, 0 failures.** The sketch compiles for
`esp32:esp32:esp32` on core 3.3.11: **+2,116 bytes of flash, +1,168 bytes of
RAM.**

### The two station incidents

```
F11  parked ON MM59 .................. one stitched waveform, ONE advance
     resid 0.0990 clean / 0.0978 with noise    peak 207   ratio 1.015
     PAUSE at 19,951 ms -> 36,400 ms paused -> RESUME -> MAGNET
F13  parked in MM106's fringe ........ one stitched waveform, ONE advance
     resid 0.1271 clean                        peak 189   ratio 0.887
     PAUSE at 22,551 ms -> 35,650 ms paused -> RESUME -> MAGNET
F09A / F09B  latched DC offsets ...... no stop, so nothing arms and nothing
     pauses; both refused by the ordinary recognizer, zero advances
```

Both incidents now count their marker and keep running.

### Stops around the arc, with a control

Three approach speeds (PWM 45 / 60 / 75) and three departure speeds
(60 / 90 / 110), a real 200 ms-a-count ramp down, 30 s, and a real ramp up. The
right-hand pair is **the control**: the identical deceleration with the ramp
stopping one count above the tractive floor, so Toby creeps through at the same
speed and never stops.

```
stop point                  rest    field   paused    resid adv  |  control
rising edge, 15% of peak   -28.4mm    35       0ms   0.0965  1   |  0.0549  0
rising edge, 25% of peak   -24.8mm    53       0ms   0.1046  1   |  0.0549  0
rising edge, 40% of peak   -20.1mm    85   37,000ms  0.1041  1   |  0.2668  0
the peak                    +0.1mm   210   37,400ms  0.0982  1   |  0.2004  0
falling edge, 40% of peak  +20.3mm    84   38,125ms  0.1456  0   |  0.1881  0
falling edge, 15% of peak  +28.6mm    34   38,583ms  0.2681  0   |  0.1504  0
```

**12 of 18 stops accepted; 18 of 18 of the identical creeps, with no stop in
them at all, refused.** The stop is not what breaks the last two rows — crossing
a magnet at a crawl is, and NAVI_ONE 0.9 cannot recognise that either.

### The sentinels

```
falling PWM armed, throttle at ZERO, field still moving ..... no pause, 1 advance
rising PWM armed, Toby does not move at all ................. no resume, watchdog,
                                                              AUTO withdrawn
rising PWM armed, wheels spinning: spikes, a reversal, a
  step that stays, motor hash ............................... no resume, watchdog,
                                                              AUTO withdrawn
rising PWM armed, a 12 s stall, and then he moves ........... resumed on the Hall
```

### The artifacts of the withdrawn design

```
a gradual DC ramp into an 80-count plateau ......... REFUSED, 0 advances
an electrical step with a slow leading edge ........ REFUSED, 0 advances
a shoulder: two overlapping lobes .................. REFUSED, 0 advances
a double-lobed non-magnet, stopped in the notch .... REFUSED, 0 advances
```

All four are presented **during** a controlled stop, with the pause logic armed
throughout. All four were accepted by the withdrawn design. The operator's rule
is what removes them.

### End to end, with the real station machine

The coast swept 60 % to 140 % of the measured fit, which walks the landing
390 mm — more than the 300 mm between markers.

```
speed  at rest  pause resume stitch  abandon  AUTO      dwell
 60%      127     1      1      1       0     RUNNING    30s
 70-110%    0     0      0      0       0     RUNNING    30s
120%     -209     1      1      1       0     RUNNING    30s
130-140%    0     0      0      0       0     RUNNING    30s
```

At every landing: the approach armed exactly once, no `MISSED`, the dwell began
at most once and was never shortened, at most one stitched waveform, no strike.

---

## What this cannot do

- **Finding 13 sits on the shape ceiling.** Clean it fits at 0.1271; with three
  counts of added noise it fits at 0.1321, against a ceiling of 0.13. Which side
  of the line it falls on is decided by noise. Nothing is miscounted — the
  waveform is refused, the marker is lost, and the strike arrives later — but
  this is the design's principal open risk, and **it must not be answered by
  moving the ceiling.** What would answer it is a field capture of a stop-and-go
  at Arches under this firmware, undecimated, to see where the real residual
  lands.
- **A magnet crossed almost entirely at a crawl is not recognisable, stop or no
  stop.** Six of the eighteen stop geometries are refused; the eighteen matching
  creeps are all refused. This is a pre-existing limit of the Gaussian in index
  space, not something this decision introduces — and not something it fixes.
- **A long stall changes the arc's symmetry.** If Toby breaks free after twelve
  seconds, the throttle has ramped the whole way and the second half of the arc
  is travelled far faster than the first. The recognizer objects, correctly.
- **The reconstructions are decimated.** The two station records are 128:1; the
  readings between the stored ones were never transmitted and are interpolated.
  They sample the noise distribution but not its time structure.
- **The track model in sections D to H is a model** — a 30 mm magnet at 15 mm
  sigma, 210 counts, no grade. Only the speed fit and the marker spacings are
  measured.
- **The derailment capture of finding 12 does not exist.** `WaveformWindow` is
  six deep and the MM113 rejection was nine markers before the strike that
  dumped it. **Fourth** time the depth has cost a diagnosis; 6 → 10 is one line
  and about 4.3 KB, against 267,516 bytes free.

## Scope

Not changed: the recognizer (byte for byte), the waveform window (byte for
byte), all thresholds, baseline behaviour, station offsets, route mapping,
rebound duration, the waveform wire format, gain calibration, and the manual,
e-stop, low-voltage and position-invalidation semantics.

Changed: `HallCapture` pauses and resumes one measurement; `Navigator` gains
`Ruling::Unresolved` and `unresolved()`; the `.ino` raises the two sentinels,
routes an abandoned pause, and publishes `stitched` and `paused_ms`; a version
bump to NAVI_ONE 1.0.

## Ratification

Nothing here is authoritative. `git status --short firmware/test-programs/NAVI_ONE`
is empty and the harness prints it before it runs anything. The exact diff is
`tools/interrupted_replay/0070.diff` — 499 lines added, 18 removed, across four
files plus the gate runner and two new test files.

**No firmware will be modified until the operator ratifies this design.**
