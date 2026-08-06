# Road to CTO — milestone plan

**Goal:** two locomotives running the Lowline simultaneously, coordinating
their own spacing, without collisions or operator intervention.

**Premise:** CTO2 was abandoned after repeated rear-end collisions. Those were
not coordination failures. The follower computed clearance correctly from a
leader position that was wrong — asserted at certainty 1.000 after a handful of
bad reads. **CTO cannot be safer than the position reports it consumes.**
Every milestone below exists to make those reports trustworthy before anything
depends on them.

Each milestone has a **crossing test**: a measurable result, not a judgement.
Nothing downstream begins until the test passes.

---

## Governing principle — measured velocity is the control goal

Not PWM. Every speed in the system is presently a throttle number, and a
throttle number is a request, not a result. What it produces depends on grade,
load, battery state, railhead condition and motor temperature. This is why
eight hand-tuned constants exist, why adding two coaches required re-tuning,
and why Grillers needs its own numbers.

**i. Speed limits are prototypical.** A railway has speed restrictions
expressed in speed — through a station, over a grade, approaching a stop. "PWM
72" is not a restriction, it is a guess at one. Stating limits in mm/s makes
them mean the same thing on every gradient and behind any consist, and makes
the railway behave like a railway rather than like a toy with a dial.

**ii. Holding a speed prevents stalls — and prevents a navigation failure
mode.** This is the part that is easy to miss. On 2026-07-27 the locomotive was
given too little throttle for the Grillers climb with three coaches. It
crawled. Magnet events stretched from 176 ms to 3,814 ms, occupancy of the
median window reached 90%, the baseline was pulled into the magnets, and
navigation was lost — 55% polarity errors in that stretch against 0% elsewhere
in the same run.

A throttle number cannot notice it is failing. A speed controller adds power
when the train slows and the failure never begins. **Speed control is not only
about realism and stalls; it is a navigation safety mechanism**, because the
sensor has a hard minimum speed below which the baseline collapses.

**iii. Speed and navigation must derive from separate systems.** Position and
speed currently both come from the Hall sensor, so a bad magnet read corrupts
both simultaneously and nothing can notice the contradiction. Independent
sources let each check the other: *"DNA says I reversed direction; the wheel
says I have been rolling forward throughout."* That cross-validation is
unavailable today at any price, and it is the single strongest guarantee
available against the failure that ended CTO2.

This principle sets the shape of M2 and M3, and is the reason they are not
optional refinements.

---

## M0 — Established (done)

Recorded so later work is not rediscovery.

| | evidence |
|---|---|
| Sensor error rate 0.5–2.3% at normal speed | 195 markers offline-aligned, 2026-07-27 |
| Median baseline replaces gated IIR | 27% → 0.5%; no freeze in a full session |
| Speed floor exists | >50% median occupancy → runaway errors; ~2.5 s/marker |
| Thermal transit corrupts baseline | brick section MM020–056, 117 °F vs 78 °F indoors |
| Dead reckoning survives bad reads | reacquisitions at MM056 and MM087 both agreed with the odometer |
| Stations work | six clean stops across two runs |

---

## M1 — Position that cannot be confidently wrong

**The one that killed CTO2.** Until this is crossed, nothing else matters.

**1a. Search from certainty, not from scratch.**
Reacquisition currently considers all 171 positions equally. It should not.
The locomotive knows where it was last *confirmed* and how many markers it has
counted since; the answer lies within a small window of that. Constrain the
search to positions reachable from the last confirmed fix given the odometer
count, plus a tolerance for accumulated miscounts.

This changes the failure probability by an order of magnitude. Unconstrained,
roughly one in eight garbage windows is a valid codeword somewhere on the
layout. Constrained to ±5 markers, almost none of them land inside the window.

*Build on certainty, not on doubt.*

**1b. Two confirmations, not one.**
A candidate presently proves itself by predicting a single binary reading, so
a garbage match passes half the time. Require two consecutive correct
predictions (25%) or three (12%).

**1c. Timing validation.**
`dt` is published on every marker and `spacingMm[]` covers all 171, and the
navigator uses neither. Arrival time is the largest piece of unused evidence
in the system:

- an interval far shorter than physically possible → **phantom**, do not advance the odometer
- an interval near an exact multiple of the expected → **missed marker**, advance by that multiple

This converts the two silent permanent errors into detected events at the
moment they occur, rather than as five accumulated disagreements later.

### Crossing test
Five laps, both directions, offline-aligned against the map.

- **Zero** position assertions inconsistent with the odometer by more than 2 markers
- Every phantom and missed marker found by the aligner also flagged by the firmware at the time
- Reacquisition, when it occurs, lands within 2 markers of dead reckoning

---

## M2 — Independent speed (IR)

Position and speed presently come from the same sensor, so a bad magnet read
corrupts both at once and nothing in the system can notice. An independent
speed source is the only witness that can catch the magnets lying.

Wheel sensor on the power car, same ESP32, no link required.

### Crossing test
- Agrees with marker-derived speed within 10% on clean segments
- Remains sane through a deliberate marker glitch
- Zero dropouts across a full lap in full sun, after the differential-sampling fix
- Survives 30 minutes without baseline drift affecting it

---

## M3 — Closed-loop stops

Station speeds and stop offsets are hand-tuned per consist. Adding two coaches
required re-tuning and caused a stall. With wheel speed, a stop becomes a
**distance** rather than a ramp duration, and grade and load stop mattering.

Depends on M2.

### Crossing test
- All four stations stop within ±150 mm of target
- Same numbers work with 0, 1 and 3 coaches — **no per-consist tuning**
- No stall on the Grillers grade under any load tested

---

## M4 — Correct behaviour when lost

Present behaviour — slow to 60 and keep running — is right for a solo
locomotive and dangerous with a follower.

**Under CTO, LOST ends AUTO.** The locomotive stops and hands control back to
the operator. It does not creep, does not attempt stations, and does not
resume on its own. A train that does not know where it is may be run manually
or not at all.

Also removes the M0-era compromise where stations act on unverified position.

### Crossing test
- LOST in AUTO → controlled stop, AUTO dropped, warning published, no autonomous restart
- Position can be declared from any point on the layout and is then trusted
- No station action ever occurs while position is unconfirmed

---

## M5 — Trustworthy self-report

The locomotive must state not just where it is but how sure it is, in a form
another train can act on. The payload already exists from the 2_5 work:
`rear_bound_mm`, `front_bound_mm`, `envelope_mm`, `est_mm_s`, `halted`,
age of last confirmed fix.

What has never been tested is whether the bound is *correct*.

### Train extent

A train is not a point. The Hall sensor sits in the power car, and the consist
extends behind it — so **occupied track runs from Hall +2 markers to Hall −4
markers**, roughly 0.6 m ahead and 1.2 m behind at 305 mm spacing.

This must be folded into the published bounds, not left for a consumer to add:

```
front_bound = position + 2  + forward uncertainty
rear_bound  = position - 4  - accumulated dead-reckoning uncertainty
```

A follower holding clear of a reported *position* would be holding clear of the
power car and driving into the last coach. The extent is a fixed property of
the consist, so it belongs in configuration alongside the station speeds, and
it changes when cars are added or removed.

### Crossing test
- Across five laps, the true position (from offline alignment) lies inside the
  published bound **100% of the time**
- Bound width stays under 3 markers in normal running
- Broadcast continues uninterrupted through a WiFi dropout and recovers

---

## M6 — Peer awareness

### Harvest CTO2 before writing anything

CTO2 was abandoned for a navigation failure, **not a coordination failure**.
The peer registry, the ESP-NOW messaging, the traffic-control logic, the
pairing rules and the bubble/express state machines were the product of a long
development effort and much of that thinking was sound. It failed because it
was fed positions that were confidently wrong.

Discarding it and starting over would repeat months of work and lose hard-won
detail — the Circuit Express algorithm alone ran from v1.0 to v8.1 before
reaching working status, and the resolution ("restrict CE pairing to
CE_EXPRESS only; let geometry create the pairing sequence after CE") is the
kind of conclusion that is expensive to rediscover.

**First task of M6 is an audit, not a port.** Read
`NGR_LL_DNA_CTO2_r12_CONTINUITY_FIRST.ino` and the CTO2-era logs and produce a
written inventory:

| category | disposition |
|---|---|
| ESP-NOW transport and peer registry | port largely as-is |
| pairing and role-assignment logic | port after review — the reasoning is sound |
| traffic control / block occupancy | review: assumed reliable positions |
| anything consuming position without a bound | **rewrite** — this is what failed |
| speed coordination from Hall timing | **discard** — superseded by IR |

Only then port. Onto SOLONAV, not the reverse.

Design decisions already settled, in `CTO3_DESIGN_NOTES.md`:
- a silent peer becomes an **unresolved obstruction**, not empty track
- separation measured from the follower's own front bound to the leader's rear bound
- `LOST_MIN_GAP_MM` established by measurement, not assumption

### Crossing test
- Two locomotives exchange bounds for 30 minutes with zero gaps beyond one interval
- Each correctly identifies the other's position within its published bound
- A powered-down peer is still treated as occupying track

---

## M7 — Two-train operation

Depends on all of the above.

### Crossing test
- Ten laps, two trains, **zero collisions**
- Zero operator interventions
- Every hold decision traceable in the logs to a published bound
- One deliberate induced failure (peer powered off mid-lap) handled without contact

---

## Sequence and dependencies

```
M1 ──► M4 ──► M5 ──► M6 ──► M7
 │              ▲      ▲
 └─► M2 ──► M3 ─┘      │
        └──────────────┘
```

M1 first and alone. M2 can proceed in parallel once M1 is underway, since the
IR work is hardware and largely independent. M3 needs M2. M5 needs M1 and M2.
Nothing touches M6 until M5 has passed.

**The CTO2 audit is the exception** and can begin at any time. It is reading
and writing, not building, and it costs nothing to have the inventory ready
before M6 arrives.

## Rules for this plan

1. **A milestone is crossed by a measurement, not by an opinion.** If a test
   cannot be run, the milestone is not crossed.
2. **No work begins on a milestone whose dependency is unproven.** CTO2 failed
   because coordination was built on unverified navigation.
3. **One change at a time between field runs.** Two changes in one run means a
   failure is unattributable.
4. **Guards are added in response to logs, not to imagination.** Every wedge in
   this project so far has been a guard, not a magnet.
