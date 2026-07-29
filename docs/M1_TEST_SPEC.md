# M1 test specification

**What M1 claims:** position cannot be confidently wrong.

**Why it is the gate:** CTO2's collisions were correct clearance arithmetic
applied to a leader position that was wrong. If M1 cannot be crossed, CTO is
not viable on this sensor, and that is worth knowing before M2–M7 are built on
top of it.

Three tests. A can be run today with existing tools. B and C need the firmware
change.

---

## Test A — odometer error distribution

**Question:** when the odometer is wrong, which way is it wrong, and by how
much? This sizes the reacquisition window and decides whether it should be
symmetric.

The two error types push in opposite directions:

| aligner result | cause | effect on odometer | true position |
|---|---|---|---|
| **deletion** | magnet not detected | does not advance | `navMm + 1` |
| **insertion** | spurious event, no magnet | advances anyway | `navMm − 1` |

### Method

Existing tools, no firmware change.

1. Flash `SENSORTEST`. Acclimatise outdoors, then power-cycle so calibration
   happens at track temperature.
2. `mosquitto_sub -t 'ngr/marker/+/event' -v > run.log` — verify the window
   goes silent, which is how you know the redirect took.
3. Run continuously, one direction, normal speed. **Do not stop and restart**;
   a continuous run is what the aligner can score.
4. `python3 align_markers.py run.log`

### Reading it

```
wrong polarity     N     substitutions — recoverable, no odometer effect
missed markers     N     deletions     — odometer runs BEHIND
spurious events    N     insertions    — odometer runs AHEAD
```

### Expectation, and the honest limit

At the measured 0.5% error rate, five laps (855 markers) yields perhaps four
events. Enough to see whether the distribution is one-sided; **not enough to
size a window precisely.** Twelve laps would be, and nobody is going to run
twelve laps to tune a constant.

So Test A is not a gate. Run it, note the shape, and **start with a symmetric
±5 window** — which is generous, since expected drift is 0.5 markers after a
hundred lost markers. The `off` telemetry from Test B is what will actually
size it, from operational data over weeks rather than from one afternoon.

If insertions turn out to be far rarer than deletions — plausible, since a
phantom must clear both the 35-count peak gate and the 40 ms floor while a
missed marker only needs a weak read — the window can become asymmetric, say
−2/+5. Do that on evidence, not on the reasoning above.

---

## Test B — constrained reacquisition

**Question:** does the constrained search reacquire correctly, and does it
refuse to reacquire wrongly?

**Prerequisite:** reacquisition essentially never fires at 0.5% error. It must
be induced, or the test cannot be run. Add a debug command:

```
ngr/loco/<id>/cmd/force_lost   →   navEnterLost("TEST")
```

That makes the test repeatable and takes one line. It is a test fixture, not a
feature — but a fixture that cannot be triggered on demand produces a test
nobody runs.

### Method

Twenty induced LOST events across a normal AUTO run, at varied points:

- on straight track at cruise
- during a station approach
- in the brick section MM020–056
- immediately after a declaration
- immediately after a previous reacquisition

For each, record from the `REACQUIRED` telemetry:

- `off` — offset of the accepted match from the odometer
- markers elapsed between LOST and REACQUIRED
- whether the accepted position agreed with the true position from offline alignment

### Pass criteria

| | threshold |
|---|---|
| Reacquisitions landing on the wrong position | **zero out of twenty** |
| Reacquisitions succeeding within 20 markers | ≥ 18 of 20 |
| `off` values | all within ±3; if any reach ±5 the window is too tight for comfort |
| False accepts under deliberate corruption | zero — see below |

### The negative test

A test that only shows reacquisition succeeding proves half of what matters.
Also demonstrate it **refusing**:

Force LOST, then deliberately corrupt the reading stream — cover the sensor for
several markers, or run a pass through the hot brick section from cold. The
locomotive must either reacquire correctly or stay lost. It must never announce
a position that offline alignment contradicts.

**Staying lost is a pass.** Announcing the wrong position is the failure this
whole milestone exists to prevent.

---

## Test C — M1 crossing test

The gate. Everything above feeds it.

### Method

Five laps, both directions, AUTO, with `SENSORTEST`-equivalent marker logging
running alongside navigation so the run can be aligned offline.

### Pass criteria

1. **Zero position assertions inconsistent with the odometer by more than 2
   markers.** This is the CTO2 failure, and it must not occur once.
2. Every phantom and missed marker found by the aligner is also flagged by the
   firmware at the time — this validates 1c timing detection.
3. Reacquisition, where it occurs, lands within 2 markers of dead reckoning.
4. The published position agrees with offline alignment on every marker of all
   five laps.

### If it fails

Do not proceed to M2–M7. The failure mode tells you where to work:

| failure | means |
|---|---|
| wrong reacquisitions | search or confirmation is still too permissive — tighten 1a/1b |
| position drifts without a reacquisition | phantom/missed markers uncaught — 1c is not working |
| LOST fires frequently | sensor reliability, not navigation logic — back to M2 |

---

## Instrumentation these tests require

None of it is a feature; all of it is measurement.

- `cmd/force_lost` — repeatable induced LOST
- `off` published on every `REACQUIRED` — the number that sizes the window
- marker-level logging alongside navigation, so runs can be aligned offline
- `align_markers.py` handling multi-lap runs — it does

Total: two small firmware additions and an existing script.

---

## What these tests deliberately do not measure

Anything about behaviour *while* lost. Per the reliability principle, a lost
locomotive stops and hands to the operator; there is no degraded mode to
validate. The tests measure how rarely it gets lost and how reliably it
recovers, which is the only thing that determines viability.
