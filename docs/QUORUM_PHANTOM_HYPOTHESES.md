# Phantom Hall events — competing hypotheses

**Status:** open. Two live hypotheses, one decisive test not yet run.
**Do not close this on telemetry alone.** Both are consistent with parts of the
evidence, and the instrument that separates them is an oscilloscope, not a log.

---

## H1 — EMI / PWM noise coupling (operator brief, 2026-08-12)

Submitted for the record, in full substance:

PWM edges produce fast dv/dt, and the motor produces di/dt current spikes. These
couple into the Hall wiring through parasitic capacitance, inductive coupling in
shared or parallel cable runs, or radiated EMI. At start-up and acceleration the
motor draws higher current, the duty cycle is changing, and the shared battery
rail can sag. High-impedance signal lines are especially vulnerable, and a noise
spike easily looks like a valid magnet edge to a polling routine. This is a
widely reported failure mode with Hall and encoder signals near PWM drives, it
typically appears only when the motor is driven, and it usually disappears with
filtering or a different driver.

Contributing mechanisms named: shared power/ground and poor decoupling; wiring
layout, unshielded or un-twisted runs beside motor leads; edge detection without
minimum-pulse-width filtering; the motor's own field during high-current
acceleration; a marginal Hall device under supply noise.

Mitigations proposed, in order of ease: RC low-pass at the ESP input (≈1 kΩ with
a few nF to tens of nF) and/or a stronger external pull-up (1–5 kΩ); 0.1 µF
across the motor terminals; separate and twist the Hall run away from motor
power, shield grounded at the controller end only; local decoupling and a clean
low-impedance ground; minimum stable pulse width, or blanking during known
PWM ramps; a quieter driver, a different PWM frequency, or motor-side EMI
suppression. And: **scope the Hall signal during acceleration.**

## H2 — The detector splits one magnet's bipolar field into two events

`QUORUM.ino` LAYER 2. Thresholds sit at baseline ±38 to open and ±25 to close;
`EVENT_EXIT_HOLD_MS` is 20 ms; polarity is taken from the **opening** pole.

A disk magnet passed edge-on presents a field that goes one way, through zero,
then the other. So: the strong lobe opens an event; the field crosses the ±25
deadband and holds there for 20 ms — about **3.6 mm of travel** at 180 mm/s —
which closes it; the opposite lobe then crosses ±38 and **opens a second event**,
which survives `EVENT_FLOOR_MS` 40 because it lasts 40–150 ms.

The detector's own header anticipates the merged pair and chooses opening-pole
so it "cannot be arbitrated into the wrong answer". What it does not anticipate
is the 20 ms exit hold splitting that pair into two events before the merge rule
can apply.

---

## Evidence

### Favours H2

1. **Polarity correlation: 52 of 56.** The weak event is the *opposite pole* of
   the strong read immediately preceding it, across four captures and both
   locomotives. Noise crossing a threshold has no reason to track the polarity
   of the previous magnet; the second lobe of a bipolar field does so by
   construction. This is the single strongest discriminator.
2. **Only the strongest magnets.** The phantom sites are the four strongest
   markers on the route — mm 150 (peak 371), 149 (334), 103 (262), 102 (260) —
   against a route median of ~145. Only a strong magnet's second lobe still
   clears ±38. EMI has no reason to prefer strong magnets.
3. **Cross-locomotive at the same place.** Toby, different motor, driver, wiring
   and Hall installation, reproduced the event at Grillers. EMI is
   installation-specific; a magnet's field geometry is not.
4. **Quiet baseline.** `|raw − baseline|` has a median of **2 counts** against a
   ±38 trigger threshold — the line is quiet between magnets.
5. **Floor rejects scale with speed.** `floor_rejects` runs 5.8% of markers on
   the fast constant-91 leg against 1.05% on the ramped session — consistent
   with a second lobe that shortens below the 40 ms floor as speed rises, which
   is also why the phantom is ramp-associated without being PWM-associated.

### Favours H1, or is not explained by H2

1. **Ramp association is real.** mm 101 fired on 9 of 9 ramped CCW crossings and
   0 of 1 at constant 91 pwm. H2 explains this by transit time; H1 explains it
   by drive state. Both fit.
2. **A weak event occurred at mm 82/86 on legs with no station armed**, so the
   phenomenon is not exclusively at ramps under either hypothesis.

### What the telemetry CANNOT settle

The `delta` in `loopstat` is a 1 Hz published snapshot, **not a noise
measurement**. A quiet median says nothing about microsecond spikes between
samples. `detectorSample()` polls at `HALL_TASK_TICK_MS` 1 ms over an averaged
ADC read, so a PWM-synchronous spike could be aliased or averaged away and still
have crossed a threshold on the sample that mattered. **H1 cannot be excluded
from logs.**

---

## The decisive test

Both hypotheses recommend the same instrument, which is a good sign:

**Scope the Hall signal (and ideally the supply rail) through a pass over
mm 150 or mm 102, accelerating.** Trigger on the event. Then:

- a clean bipolar excursion — strong lobe, zero crossing, opposite lobe, all at
  magnet timescales — confirms **H2**, and the fix is in the detector;
- PWM-synchronous hash riding on the signal, or a supply dip coincident with the
  extra trigger, confirms **H1**, and the fix is hardware.

A second, zero-cost check: **push the locomotive by hand over mm 150** with the
motor unpowered and the sketch running. H2 predicts the second event still
appears; H1 predicts it does not. That single pass discriminates without an
oscilloscope.

---

## Position

H2 currently has more explanatory power — particularly the 52/56 polarity
correlation and the strong-magnet selectivity, neither of which H1 accounts for.
But H1 is a well-established failure mode, is not excludable from telemetry, and
its mitigations are cheap and independently worthwhile regardless of which
hypothesis wins.

**No firmware change is proposed on either hypothesis until the hand-push test
or the scope trace has been run.** Both would otherwise risk papering over a
hardware fault, or adding hardware filtering for a firmware defect.
