# Otto X19 — three magnets counted while standing still at Arches

**2026-09-15, 18:55–18:59.** Otto 9950011, `NAVI_ONE_1_0X19_NO_CLOSURE_FIELDTEST`.
Declared 040-041 CW. Grillers clean. Struck seven seconds after leaving Arches.

---

## 1. It is not the Hall sensor, and it is not the supply

I had suggested checking the Hall wiring after two full-scale readings. That was
wrong and the operator was right to challenge it. Both rails are at **18:53**,
before the run, with

```
V = 0.74 V     A = -0.0 A
```

— the car on USB with the battery not yet on. Through the run itself the supply
is **16.55–16.68 V across 105 samples**, and through the Arches dwell
specifically **16.66 V at 0.10–0.13 A**, flat. Nothing else moved. The sensor
is fine.

## 2. What actually happened

Otto stood at Arches for thirty seconds at PWM 0, throttle 0, and **X19 counted
three magnets**:

```
18:58:06  DWELL_BEGIN Arches, mm 109, pwm 0
18:58:21.648  raw 2074  rest 1978  depart  96  peak  96  exc_n   2  w_cal 143  -> AGREE mm 110
18:58:22.580  raw 2083  rest 1978  depart  84  peak 112  exc_n  15  w_cal  84  -> AGREE mm 111
18:58:23.278  raw 2084  rest 1978  depart 106  peak 101  exc_n  46  w_cal 254  -> AGREE mm 112
18:58:36  DEPART Arches, pwm 90
18:58:43  DISAGREE mm 113 tgt 114, obs N expected S, POLARITY_MISMATCH -> strike
```

Position advanced three markers while the locomotive did not move. When it did
move and crossed the real marker, the pole contradicted the map and the
navigator struck, correctly, on a count that had been wrong since the dwell.

**Grillers, same firmware and the same thirty-second dwell, produced ZERO
candidates.** The difference is not the code. It is where the sensor came to
rest: at Arches it sat **96–106 counts off its own measured resting level**, so
Otto was parked in a magnet's fringe field.

This is field-test target 4 — "a stop positioned so the sensor rests in a fringe
field" — which both replay reports said no dataset contained and which had to be
run deliberately. It got run by accident, on the first outing.

## 3. The three events are steps, not arcs

`exc_n = 2` against `w_caliper = 143 ms`: the signal sat more than 25 counts
from its rest for 143 ms, but only **two samples** were within 34% of the peak.
That is a level change, not a magnet crossing. Once a displaced level has held
for 800 ms the detector measures it as rest, and every transition of 70 counts
or more into and out of it is an event — the behaviour gate 1 records and gate
12 bounds.

**Gate 12's bound was right; its premise was not.** It says nothing at or below
51 counts fires at any rate, 51 being the largest resting-level excursion ever
recorded here. These were 96–106. Parking in a field is not a resting-level
excursion, it is a real field, and nothing in the corpus had one.

## 4. The discriminator is in the Hall data, with no PWM in it

How much of its own width an excursion actually occupies, over this run:

```
exc_n / w_caliper_ms
  MOVING, 70 accepted magnets  min 0.71   p10 0.84   median 0.87   max 0.93
  STATIONARY, the 3 false ones                                     max 0.18
```

A factor of four, with nothing between 0.18 and 0.71. A magnet's excursion
fills most of its own width; a step fills almost none of it.

Amplitude separates too, but narrowly: real peaks 122–, false 96–112; real
ratios 0.67–, false 0.53–0.56.

## 5. What this does not settle

Whichever way it is fixed, **it is morphology** — decision 0080 says morphology
is diagnostic-only and may not silently acquire navigation authority — or it is
PWM, which X19 was explicitly built without. Both are operator rulings, and
neither is taken here. No firmware change has been made.

Also unexplained, and worth a look before the next run: why the sensor rests in
a field at Arches at all, and whether the Arches stop offset can be moved
instead. The 2026-09-02 ruling was "fix the recognizer, not the geography", so
that is a question, not a proposal.

## 6. Provenance

`~/NGR/telemetry/runs/9950011_20260915_185522.log` on 192.168.68.142, plus the
18:51–19:04 boots either side. 93 candidates, 93 accepted, 73 advances, 21
candidates at PWM 0 across the session.
