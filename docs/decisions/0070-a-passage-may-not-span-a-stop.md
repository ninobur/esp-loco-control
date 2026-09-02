# 0070 — A passage may not span a stop

**Status:** PROPOSED. Not ratified. **No firmware has been modified.**
**Date:** 2026-09-01, revised 2026-09-02 after review
**Touches:** 0054 (four conjunctive characteristics), 0057 (no duration ceiling,
no motion gate on the guard), 0059 (the six-marker window), 0062 (shape
abstains on a railed passage), 0064 (polarity is the signed sum), 0065 (the
median-of-three judgement copy).
**Evidence:** findings 09, 11, 13; `tools/interrupted_replay/`.
**Would ship as:** NAVI_ONE 1.0.

---

## What this is, said plainly

**This is a second way to establish a magnet.** Until now there has been exactly
one: a completed passage that satisfies amplitude, shape and the rebound guard
together (decision 0054). This adds a second, narrower one, and it does not
satisfy shape — because in the case it exists for, shape is unavailable.

The first draft of this record described the navigation contract as unchanged.
That was wrong and the review was right to strike it. The contract's recognition
requirement is what changes. Everything else — position must be declared,
identity is polarity against one target, one strike withdraws — is untouched, and
an interrupted acceptance goes through `Navigator::judge()` like any other.

It is closest in spirit to **decision 0062**, which had the shape test abstain on
a railed passage rather than refuse it, on the grounds that the instrument did
not see the whole thing and a fit to a cropped arc is not evidence either way.
The same sentence applies to half a traversal. The difference, and it is the
uncomfortable one, is that 0062 abstained on a population the survey showed was
rare and self-announcing, while this abstains on a population that a station stop
produces **on purpose, twice a lap**.

---

## The problem

A passage runs from a threshold crossing to a threshold return. That model
assumes the sensor is *moving past* the magnet. When the locomotive stops
mid-field the model has no way to end, and everything before the stop merges with
everything after it into one object, which is then fitted to a Gaussian and
thrown away.

Twice on 2026-09-01, twenty-one minutes apart, at two platforms:

| | Grillers CCW (finding 11) | Arches CCW (finding 13) |
|---|---|---|
| the real magnet is | at the HEAD, on arrival | at the TAIL, on departure |
| plateau | 163 counts, on the magnet | 41 counts, a fringe |
| open for | 37,733 ms | 36,689 ms |
| residual | 0.2773 | 0.3825 |
| outcome | MM59 lost, strike | MM106 lost, strike |

A marker can be lost at either end. Stop-offset tuning relocates the landing; it
cannot guarantee the landing is clear of every fringe field, and both incidents
happened with the offsets doing exactly what they were set to do. The end-to-end
sweep in gate 12 H puts a number on it: across a full 300 mm marker spacing,
**about one landing in five leaves the sensor in a field strong enough to hold a
passage open.**

---

## The unintended-consequence risk, stated here rather than later

Per the operator's ruling of 2026-08-30, a principle gets its risk written down
the first time it is recorded, and again each time a later decision leans on it.

**The risk is that "a stop was requested" becomes evidence.** It is not, and the
design below refuses it three times over — but the pressure will be there at
every future change. The moment identity, route expectation or commanded PWM is
allowed to *supply* anything rather than merely *select a mode*, this stops being
an instrument and starts being a program that tells itself what it expected to
see. Findings 05 and 06 are what that looks like from the inside.

**The second risk is scope creep by analogy.** "A traversal was interrupted, so
shape abstains" is a sentence that will fit a great many situations if it is
allowed to. It is confined here to one physically identified event — a controlled
stop that cut a passage in half — and the confinement is the decision, not an
implementation detail of it.

---

## Mode selection

Interruption handling is entered when **both** are true:

1. the firmware is executing an **identified controlled stop**; and
2. a Hall passage is open and **its field has settled**.

Settling alone must never select it. An uninterrupted slow passage, including one
whose apex looks flat for more than 400 ms, keeps the complete Gaussian
recognizer — gate 12 G asserts exactly that at six crossing speeds, each driven
at the PWM that physically produces it.

### Which authorities participate

| authority | participates | why |
|---|---|---|
| station zero-ramp (`StPhase::Ramp`) | **yes** | |
| station dwell (`StPhase::Dwell`) | **yes** | |
| a CTO stop | **not implemented** | there is no CTO in NAVI_ONE 1.0. When it arrives it opts in here, explicitly, in one line. |
| MANUAL | no | `stationService()` returns early unless `autoRunning`; a hand on the throttle never selects this |
| e-stop, low voltage, dispatcher release | no | each clears `autoRunning` |
| a strike or a contradiction | no | position is already withdrawn; a frame nobody is navigating gets no alternate recognizer |
| `MISSED`, `PHASE_TIMEOUT` | no | the station machine has stood down; there is no stop to speak of |

**No existing semantics change.** The flag is additive and is read in exactly one
place. Manual, e-stop, low-voltage and position-invalidation behaviour are
untouched.

---

## Settle detection — and the withdrawal of the first proposal

The first draft proposed **±8 raw counts held for 400 ms**. *That rule cannot
fire on this sensor.* The review asked for the stationary noise to be measured
before the constants were frozen; it has been, and the measurement kills it.

Four stationary plateaus, from the field records themselves — real 1 kHz ADC
readings taken while Toby was demonstrably parked:

| plateau | n | sd | 16-sample window span, raw p95 / max | trimmed p95 / max |
|---|---:|---:|---:|---:|
| finding 11, parked on MM59 | 261 | 4.22 | 48 / 50 | 11 / 12 |
| finding 13, parked in a fringe | 271 | 3.62 | 46 / 46 | 8 / 9 |
| finding 09 A, latched offset | 386 | 3.02 | 21 / 30 | 11 / 13 |
| finding 09 B, latched offset | 261 | 3.81 | 22 / 24 | 11 / 13 |

The raw span is dominated by single-sample outliers — the same population that
made decision 0065 necessary, present at rest as well as in motion. Discarding
the one highest and the one lowest sample of the window collapses it to a
**maximum of 13 counts across every plateau on record**.

**The measured candidate, therefore:**

- 16 samples at **25 ms** — the cadence the baseline sampler already runs at, so
  this costs one ring of 64 bytes and no new timebase;
- the window's span, **with the single highest and single lowest discarded**, at
  or below **20 counts**;
- the settled level is the **median** of that window;
- the evidence region ends at the **start** of the window, so the whole settle
  window is excluded from the pre-stop evidence;
- the settled raw level is retained, frozen, for the departure.

These are still measured *candidates*, not frozen constants: every plateau above
comes from a decimated record, so it samples the noise distribution but not its
time structure. A bench capture of an undecimated stationary minute would settle
the last of it. What the measurement does establish beyond doubt is that a raw
band of ±8 is not the right instrument.

### Liveness

**A passage may not stay an ordinary passage across a controlled stop.** If the
station machine authorises departure while a passage is open and no interruption
was ever established, the dwell is about to become waveform morphology. In that
case the departure order is **not obeyed**: the locomotive is held at zero, zero
advances are made, `INTERRUPTION_UNRESOLVED` is published, and AUTO and position
are withdrawn.

The review said "hold Toby stopped". In this firmware, holding an AUTO
locomotive stopped indefinitely *is* withdrawing AUTO — `withdraw()` is the one
choke point that stops it, publishes, and refuses `GO` until a new declaration.
Anything softer would leave AUTO nominally running with a station machine that
has been overruled.

---

## The pre-stop judgement

Judged over the samples collected **while the field was still changing** — from
the passage opening to the start of the settle window. Every stationary sample is
excluded by construction. All of the following are **required**; none abstains,
where the ordinary path lets the guard abstain:

| test | threshold | notes |
|---|---|---|
| adequate support in time | the changing stretch spans ≥ `floorMs` (40 ms) | shorter is a transient, not a traversal |
| a genuine changing entry | peak exceeds the level it grew from by ≥ `entryMargin` (38) | **this is what rejects a stationary offset**: an offset that was already there when the passage opened has no growth |
| amplitude | ≥ `amplitudeFloor` (0.34) of the trailing median | unchanged |
| sustained signed evidence | ≥ 3 judged samples at or above `entryMargin` in the pole's direction | three judged samples cannot be built from fewer than five raw ones |
| polarity | sign of the segment's signed sum | decision 0064, applied to the segment |
| no contradictory excursion | no opposite-sign judged sample above `exitMargin` (25) | |
| rebound guard | ≥ `guardMs` (200) since the last acceptance | **required here, not abstaining** |
| no clipping | | |

**Entry growth and peak are read from the median-of-three judgement copy**
(decision 0065), evaluated in place by `med3At()` — the recording itself is never
filtered. Gate 12 A asserts `med3At` equals `medianOfThree` elementwise on 20,000
random vectors. The in-place form exists because the segment has to be judged
while the passage is still open, before `judge_` is built, and a second
512-sample buffer would cost a kilobyte to say the same thing.

Shape is **not** applied. The verdict publishes `shapeTested: 0`, `interrupted:
1` and the outcome name — `NO_ENTRY` for a stationary offset, `TOO_WEAK`,
`TOO_SOON`, `NO_CURVE` or `WRONG_SHAPE` (contradiction) otherwise.

**Station identity, expected route polarity, expected marker, PWM and map
position appear nowhere in this function.** They select the mode, upstream.

---

## Sufficient pre-stop evidence

- `navMm` advances **exactly once**, through `Navigator::judge()`, exactly as an
  ordinary accepted magnet does — same identity check, same sequence witness,
  same telemetry, same safety state;
- the episode becomes `OCCUPIED_COUNTED`;
- the **rebound anchor is updated** at the interruption boundary
  (`acceptInterrupted()` sets `lastAcceptedCloseMs_` to the end of the changing
  evidence) and **the gain median is not touched** — a partial or stationary peak
  is not a measurement of this sensor's response to a whole magnet, and twenty of
  them would walk the amplitude floor onto the wrong number. Gate 12 I runs
  exactly that: twenty interrupted acceptances in a row leave `gain()` unmoved;
- everything further from that occupied field is suppressed through the dwell and
  the departure;
- normal traversal resumes only after the original field clears.

**Departing a counted field cannot produce a second count**, because nothing is
judged at all while the episode is `OCCUPIED_COUNTED` — the departure excursion is
recorded and discarded, and no `Passage` and no `Segment` reaches the recognizer.
It cannot produce a polarity mismatch for the same reason: `Navigator::judge()` is
never called. Gate 12 D shows it on finding 11's own record — one advance, path
`PRE_STOP/MAGNET -> OCCUPIED_COUNTED`, and nothing else for the remaining 36
seconds.

---

## Insufficient pre-stop evidence

- zero advances;
- the episode becomes `OCCUPIED_PENDING`;
- the settled level is **frozen** as the parked level (the live settle tracker
  keeps running, but it may not drift under an episode any more than
  `entryBaseline_` may drift under a passage);
- every dwell sample is excluded from the departure judgement, by construction:
  nothing is recorded until the field moves again.

When the field changes again, a **fresh** departure segment is built. It opens
when the field leaves the parked level by `entryMargin`, **sustained for
`exitHoldMs`** — one sample may not open it, because at rest the raw stream
throws single-sample outliers of 25 counts and more, and an outlier that opened a
departure would spend the episode's evidence on 8 ms of noise. It closes on a
return to the parked level, or when the sensor is clear of everything.

The same interrupted-segment requirements apply, with no Gaussian, because this
is also a physically truncated traversal.

### Two references, and a departure from the review's wording

The review said the departure segment should use the settled Hall level as its
reference. It does — **for the segment boundaries and for the entry test**, which
is what the requirement is actually protecting: proving the field changed after
the stop, and excluding every dwell sample.

**Amplitude, polarity, sustained support and contradiction are measured against
the sensor's idle reference instead**, and the harness is why:

- **polarity inverts** under a settled reference whenever the departure is the
  *decay* of a parked field. Parked on an N magnet at +200 and pending, the
  excursion measured from +200 is negative, and the segment reads S — for a
  magnet that is N. Measured from idle it reads N.
- **amplitude is inflated.** In gate 12 F7 the same segment reads ratio 1.23 from
  the settled level and 0.900 from idle. The gain median is calibrated in
  idle-relative counts; only one of those two numbers is comparable to it.
- **contradiction is real only against idle.** Returning to the idle level is not
  evidence of the opposite pole.

The reason this is safe is geometric, and it is worth stating because it is what
bounds the whole departure rule: **a departure segment can contain only one
magnet.** It ends when the sensor has been clear for 400 ms, which at departure
speeds is a few tens of millimetres; markers are 300 mm apart. So the field the
locomotive was parked in and the field it crosses on the way out are the same
magnet, with the same pole, and a same-pole parked fringe produces no
idle-relative contradiction at all. A segment that *does* show one is an
electrical artifact, and gate 12 F7 confirms it is refused and the railway stops.

Dwell samples cannot dominate the polarity either, since none of them is in the
buffer.

### At most one **acceptance**, not one attempt

The review's "at most one advance for the entire episode" is enforced on
acceptance: once a magnet is established the episode is `OCCUPIED_COUNTED` and
nothing further is judged. A departure excursion that is *refused* does not
consume the episode's only chance — a rejected artifact followed by the real
crossing is an ordinary sequence, and the ordinary path would have judged both.
The first draft capped attempts, and gate 12 caught it: a single stationary
outlier spent the whole episode's evidence.

### An episode that ends with nothing

- zero advances;
- `Ruling::Unresolved`, position withdrawn **immediately**, AUTO withdrawn, a
  specific diagnostic published, and the waveform window dumped.

Not deferred to the next polarity disagreement. Decision 0059 measured what that
costs: **six markers, about 1.8 m**, because `ROUTE_POLARITY` has same-polarity
runs of six and seven. Findings 12 and 13 are both that failure — a marker lost
at a station, six advances of confident wrong position, and the strike arriving
with the locomotive nowhere near where the dashboard said.

---

## The exactly-once invariant

Across the complete stop episode — approach, settle, dwell, departure, clearance
— the permitted number of advances **from interrupted segments** is zero or one,
never two.

```
normal closure                  -> the complete recognizer, at most one
sufficient pre-stop evidence    -> one on arrival, departure suppressed entirely
insufficient pre-stop evidence  -> zero on arrival, departure may supply one
unresolved episode              -> zero, and an immediate controlled shutdown
```

Markers crossed while the zero ramp runs its twelve seconds are **ordinary
traversals** and are not part of this count. The locomotive is still moving,
passages open and close normally, and gate 12 H shows the coast alone carrying it
across seven markers at 140 % of the measured speed — every one judged by the
complete recognizer.

---

## Station and navigation integration

- An advance during the zero ramp or the dwell **cannot** cause station
  overshoot, re-arm the station, alter the dwell or release propulsion.
  `StationMachine::tick()` exempts `Dwell` and `Depart` from the overshoot
  abandon, and the `.ino`'s section-cruise handler already requires
  `stationMachine.phase() == StPhase::Idle` before touching the throttle. Gate 12
  H asserts all four across the whole coast sweep: the approach arms exactly
  once, no `MISSED`, the dwell begins at most once, and the dwell is never
  shortened.
- A special-path acceptance updates the Navigator, the expected sequence, the
  guard anchor, telemetry and safety state **exactly as one normal accepted
  magnet would** — it is the same `Navigator::judge()` call — **except** that it
  does not enter the moving-passage gain history.
- The segment is pushed to the waveform window, oriented, with `shapeTested: 0`
  and residual 0. The wire format does not change.

---

## The harness

`tools/interrupted_replay/`. `sh tools/interrupted_replay/build_and_run.sh`.

It assembles two build trees under a temporary directory — one a verbatim copy of
`firmware/test-programs/NAVI_ONE`, one with the proposed change overlaid — runs
**the eleven existing gates against both with the same unmodified runner**, and
diffs the two outputs byte for byte.

> **The eleven are IDENTICAL between the two trees.** They include the
> 2026-08-28 survey replay — 187 real passages with their residuals — the
> 2026-08-29 lap replay, the polarity survey, and both baseline gates. The
> proposed change moves no verdict, no residual and no ruling anywhere they
> reach.

That is the equivalence proof the review asked for, and it is why the harness no
longer contains an approximate Python recognizer. The Python harness that
accompanied the first draft has been **deleted**. Gate 12 compiles the real
`HallCapture`, `MagnetRecognizer`, `Navigator`, `StationMachine` and `RouteMap`,
and `Rig` reproduces `NAVI_ONE.ino`'s `hallTask()`, `stationService()`, `loop()`
and `serviceRamp()` line for line.

**116 checks, 0 failures.** The sketch compiles for `esp32:esp32:esp32` on core
3.3.11: **+3,364 bytes of flash, +232 bytes of RAM.**

### What is modelled, and said so at the point of use

- **The ADC.** A decimated field record is expanded by median-filtering the
  stored samples and interpolating between them. The readings in between were
  never transmitted. Holding each reading instead would manufacture events the
  record itself rules out — finding 13's plateau contains one reading of −1
  against a mean of 41, and stretching it by 128 puts 128 ms inside the exit
  margin, which would have *closed* a passage the field says stayed open for
  36.7 s.
- **The field in sections E–H**, from a Gaussian model of a 30 mm magnet
  (15 mm sigma, 210 counts) and Toby's measured speed fit `3.990 × (PWM − 25.1)`.
  That fit alone reproduces the 1–3 marker coast the field records show, with no
  inertia term.
- Every real-record case is run twice, clean and with ±6 counts of bounded noise
  — wider than the ±4.2 sd measured at rest.

---

## Evidence

### The four field records, through the real stack

```
F09A  latched DC offset, stationary throughout ........................ 0   as required
      no controlled stop selected the alternate rule at all; the record
      replays as PASSAGE/WRONG_SHAPE twice, which is what 0.9 does today
F09B  the same offset, held 9m15s across two declarations ............. 0   as required
F11   Grillers CCW, parked ON MM59 .................................... 1   as required
      PRE_STOP  pol N  from 35  peak 199  growth 164  ratio 0.975
                support 260  shape 0  guard 1  gap 1049  gain 204->204  MAGNET
      path: PRE_STOP/MAGNET -> OCCUPIED_COUNTED.  Departure suppressed.
      final: AUTO RUNNING, nav DECLARED, no strike
F13   Arches CCW, parked in MM106's fringe ............................ 1   as required
      PRE_STOP  no changing evidence -> NO_CURVE -> OCCUPIED_PENDING
      DEPARTURE pol S  from 38  peak 157  growth 119  ratio 0.737
                support 182  shape 0  guard 1  gap 37039  gain 213->213  MAGNET
      final: AUTO RUNNING, nav DECLARED, no strike
```

Both station incidents now count their marker and keep running. Both latches
still stop nothing they should not.

### Stops around the arc — peak 210, sigma 108 ms, 8 s dwell

```
                                              path                        adv
rising 15% (32 counts, under the entry margin)  PASSAGE/MAGNET               1
rising 25% (53 counts)                          PRE_STOP/NO_CURVE ->
                                                DEPARTURE/MAGNET             1
rising 40% (84 counts)                          PRE_STOP/NO_CURVE ->
                                                DEPARTURE/MAGNET             1
the PEAK                                        PRE_STOP/MAGNET              1
falling 40%                                     PRE_STOP/MAGNET              1
falling 15%                                     PRE_STOP/MAGNET              1
```

Six for six, no strikes, AUTO running throughout. Two results worth noting:

- **Stopping at 15 % of peak never opens a passage at all** — the field does not
  reach the entry margin — so the whole crossing is judged afterwards by the
  *unchanged* recognizer, residual 0.0556.
- **The count moves between arrival and departure** as the stopping point crosses
  the point where 400 ms of changing evidence exists before the settle. It is
  exactly one in every case. *When* `navMm` updates changes; the marker sequence
  does not.

### End to end, over modelled track

The real `StationMachine` runs a whole Arches CCW approach, zero ramp, 30 s dwell
and departure. Where the locomotive comes to rest is not something the firmware
chooses — the ramp starts at a marker and the coast that follows is traction,
load and grade — so the sweep is over the coast, 60 % to 140 % of the measured
speed, which walks the landing 390 mm, more than the 300 mm between markers.

```
speed   coast    at rest  path through the stop                  adv  AUTO
 60%     994mm      127   PRE_STOP/MAGNET -> OCCUPIED_COUNTED      1  RUNNING
 70%    1160mm        0   -- no interruption --                    0  RUNNING
 80%    1326mm        0   -- no interruption --                    0  RUNNING
 90%    1491mm        0   -- no interruption --                    0  RUNNING
100%    1657mm        0   -- no interruption --                    0  RUNNING
110%    1823mm       -1   -- no interruption --                    0  RUNNING
120%    1989mm     -209   PRE_STOP/MAGNET -> OCCUPIED_COUNTED      1  RUNNING
130%    2154mm       -1   PRE_STOP/NO_CURVE -> DEPARTURE/MAGNET    1  RUNNING
140%    2320mm        0   PRE_STOP/NO_CURVE -> DEPARTURE/MAGNET    1  RUNNING
```

Both real geometries appear on their own: parked in a 127-count fringe, and
parked dead on a magnet at −209. The 130 % and 140 % rows are a third case the
field has not yet shown us — the apex of a *creeping* crossing during the zero
ramp looks settled, the interruption fires while the locomotive is still moving,
and the crossing completes as the departure. Exactly one, and the marker is kept.

### Uninterrupted slow crossings — the Gaussian must survive

```
sigma  108 ms at PWM 60 (139 mm/s)  apex moves 172 counts / 400 ms   1 advance
sigma  200 ms at PWM 44 ( 75 mm/s)                       82          1 advance
sigma  400 ms at PWM 34 ( 38 mm/s)                       24          1 advance
sigma  700 ms at PWM 30 ( 21 mm/s)                        8          1 advance
sigma 1200 ms at PWM 28 ( 12 mm/s)                        2          0  (see below)
sigma 1800 ms at PWM 27 (  8 mm/s)                        1          0  (see below)
```

**In all six, the interrupted path was never selected.** That is the invariant
under test, and it holds even where the apex is flat to within one count over the
settle window, because no controlled stop was in force.

---

## What this cannot do

Losing the shape test costs something, and these are the cases where it shows.
All are **false accepts during a station stop**, all found by gate 12 F, all
listed there by name:

| case | verdict | why it cannot be separated |
|---|---|---|
| an electrical step with a slow leading edge, 95 counts | accepted | growth 39, ratio 0.343 — over both floors by a hair, and shape is the only test that could tell it from an arrival |
| a gradual DC ramp into an 80-count plateau | accepted | the ramp *is* an entry; only the amplitude floor bounds it, so a drift below 0.34 × gain ≈ 71 counts cannot be accepted |
| a shoulder, two overlapping lobes, stopped on the second | accepted | one lobe is a whole arc |
| a double-lobed non-magnet, stopped in the notch | accepted | the ordinary path refuses this on shape; stopping inside it converts a `WRONG_SHAPE` into an acceptance |

The bound on all four: they require **a fault of the right size and sign, during
a station stop, in the direction the map expects.** The wrong sign strikes on
polarity. Below 0.34 × gain nothing is accepted at all. And they are reachable
only in the ramp-and-dwell window, not on the open line.

**This is the price of the decision and it should be weighed as one.** Decision
0054's survey put shape's separation at zero overlap — real 0.0473..0.0805
against non-primary 0.1948..1.1660. Nothing here replaces it; the argument is
that in the interrupted case it was never available, and that today's alternative
is losing the marker every time.

## What this proposal still does not prove

- **The settle constants are not frozen.** The four plateaus are decimated
  records: they sample the noise distribution, not its time structure. A bench
  capture of an undecimated stationary minute would finish the job. What is
  settled is that a raw ±8 band cannot fire.
- **Finding 09 B is refused on amplitude and shape, not by the entry rule.** Its
  record is decimated 2048:1 and the transition falls inside one sample. The
  verdict is right; the path is an artefact of the reconstruction.
- **The derailment capture of finding 12 does not exist.** `WaveformWindow` is
  six deep and the MM113 rejection was nine markers before the strike that dumped
  it. The review asked for that capture to be replayed and it cannot be. This is
  the **fourth** time the window depth has cost a diagnosis; the proposal to take
  it from 6 to 10 is one line and about 4.3 KB, and the compile above shows
  268,452 bytes free.
- **A magnet entered slowly from a pre-existing offset**, where the growth from
  the offset to the peak is under 38 counts, is in none of our captures.
- **Section H's track is modelled.** The magnet shape, the field amplitude and
  the absence of grade are assumptions; only the speed fit and the marker
  spacings are measured.

## A separate finding, out of scope here

Gate 12 G's two slowest crossings are refused by the **ordinary** recognizer, in
both trees identically. A passage open longer than `openMigrateMs` (2,000 ms) has
the live baseline walk under it by design — that is 0.9's latch-clearing rule
doing its job — and on a genuinely slow crossing it deforms the record. Above
about 2 s of open passage, neither path handles the crossing. It is pre-existing,
it is nothing to do with this decision, and it deserves its own finding.

---

## Scope

Not changed: normal thresholds, baseline behaviour, station offsets, route
mapping, rebound duration, Gaussian recognition, the waveform wire format, gain
calibration, and the manual, e-stop, low-voltage and position-invalidation
semantics.

Changed: the recognition requirement, for one physically identified event, as set
out above; plus the additive `stopIntent` flag, the additive `Outcome::NoEntry`
and `Ruling::Unresolved`, three new published fields, and a version bump to
NAVI_ONE 1.0.

## Ratification

Nothing here is authoritative. `git status --short firmware/test-programs/NAVI_ONE`
is empty and the harness prints it before it runs anything. The exact diff is
`tools/interrupted_replay/0070.diff` — 687 lines added, 16 removed, across six
files plus two new test files.

**No firmware will be modified until the operator ratifies this design.**
