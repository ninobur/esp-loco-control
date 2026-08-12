# Phantom Hall events — competing hypotheses

**Status: CONFIRMED AND CLOSED, 2026-08-12.** The cause was **stacked double
magnets** installed as a remedy for weak markers. Replacing them with single
disks eliminated the phenomenon: weak events (peak < 80) went from 12 in 235
markers to **0 in 214**, minimum peak 39 → 113, `PHANTOM_REJECTED` 4 → 0, on the
same locomotive and the same unmodified firmware image.

- Evidence: `field-records/20260812_MAGNET_REPLACEMENT_PHANTOM_VERDICT.md`
- Ruling: `docs/decisions/0025-the-phantom-was-a-maintenance-artefact-not-a-firmware-defect.md`

**No firmware change was made and none is required.** H2's mechanism is correct
with the doubling as its amplifier; H1 (EMI) is unnecessary. This document is
retained for the reasoning, not as an open question.

---

## H3 — The phantom sites are DOUBLED MAGNETS (operator find, 2026-08-12)

**Physical evidence, not inference.** The operator, intending to replace the
magnets at mm 99–102 with disk magnets glued to the tops of the crossties,
extracted mm 102 from the ballast and found it was a **double bar magnet** — one
of the remedies applied several days earlier when that marker was identified as
weak.

**Geometry of the double, as found (operator, 2026-08-12).** The two bars were
**stacked one on top of the other, like poles aligned, south face upward**,
magnetically clamped together — not side by side. They looked single until dug
out. The operator's observation: *the field spread out.*

That geometry rules out "two sources at 52 mm spacing", which was the first
reading of this find and was wrong. The correct mechanism is the doubling acting
as an **amplifier for H2's return flux**:

- a stacked pair is stronger **and spatially broader** than a single bar;
- its return flux beyond the magnet edge is correspondingly stronger and reaches
  further out;
- the sole objection to H2 was that ~52 mm is too far for return flux off a
  single bar to still clear ±38 — for a double-height stack with a spread field
  it is not;
- south face up, so the reversed lobe reads **north** — the opposite polarity
  seen in 52 of 56 events;
- `EVENT_EXIT_HOLD_MS` 20 has already closed the first event, so the lobe arrives
  as a separate marker rather than merging.

H2 and H3 are therefore **not competing**. The doubling is why return flux clears
the threshold at all; return flux is why the second event is opposite polarity;
~52 mm is where the enlarged field's reversed lobe peaks. Single bars elsewhere
have identical geometry and never phantom because their return flux never reaches
the threshold. That is also why no detector defect is needed to explain the
polarity, and why H1 (EMI) remains unnecessary.

It accounts for every observation, including the ones H1 and H2 each failed
alone:

- **The fixed ~52 mm** (54 mm and 52 mm at speeds differing by 64%, see Q1 below)
  is a property of the enlarged field's geometry, not of time — which is why it
  is speed-independent.
- **Only the "strongest" markers.** Doubling raises the peak. mm 150 (371),
  149 (334), 103 (262), 102 (260) against a route median ~145 — these are the
  doubled ones, and they read strong *because* they are doubled.
- **Cross-locomotive reproduction.** The cause is on the track.
- **The 149/150 natural experiment.** Added pieces removed → peaks 2.35×/2.61×
  → 0.84×/0.92×, weak events 18 → 0. Direct confirmation on two markers.
- **The ballast correlation the operator identified.** Buried magnet → greater
  standoff → reads weak → doubled as the remedy → phantom. Ballast caused the
  *diagnosis* that caused the defect, not the defect.

**The weak-magnet remedy created the phantom.** That is the finding.

### Site status at the time of the confirming run

| markers | state | role in the test |
|---|---|---|
| 149, 150 | added pieces removed (2026-08-11) | already-clean control; peaks 2.35×/2.61× → 0.84×/0.92×, weak events 18 → 0 |
| 99–102 | replaced with disk magnets glued to crosstie tops | treated — 102 confirmed double on extraction |
| 61–63 | replaced with disks | treated — 63 is the Grillers site Toby reproduced |
| 80, ~84 | **untouched** | **untreated control** — both produced weak events |

The untreated pair is what makes this a comparison rather than a hope. Phantoms
vanishing at the treated sites while 80 and ~84 still produce them is a stronger
result than everything coming back clean, because it excludes any change common
to the whole run.

### The phantom is now a diagnostic

Operator policy, 2026-08-12: **if another phantom appears, dig.** Under H3 a
phantom is a reliable pointer to a doubled or over-strong magnet, so the extra
event has more value as a signal than cost as a defect.

This is an additional and independent argument against firmware containment:
implementing the decision 0024 timing rule would suppress the signal that
identifies which magnet to excavate.

### Consequence for firmware

Under the governing rule — no compensation without demonstrated need — **the
timing-expectation change of decision 0024 must not be implemented on the
strength of this phantom.** It would be firmware compensation for a
maintenance-induced physical fault. Correct the track, re-measure, and only then
ask whether any containment gap remains.

Two things do survive independently and are not affected by this find:

1. The **identity** error (Toby's Event B: wrong-polarity member of a close pair
   retained) is a real containment weakness. It is now explained — two real
   magnets, and the detector kept the first — but the ring is still poisoned by
   it, and decision 0024 records that the timing rule provably cannot fix it.
2. The **peak-magnitude-per-polarity detector** sketched in Q2 below would still
   resolve identity. It is now less urgent, not less correct.

---

**Below: H1 and H2 as originally argued. Retained deliberately.**

H2's mechanism survives; H2's *sufficiency* did not. It was argued from telemetry
alone and could not say why only four markers on the route produced the effect —
the answer was a maintenance action on those four, visible only by digging. An
earlier guess that mm 102/103 were doubled was dropped on report rather than on
measurement, and the analysis then worked to explain the ~52 mm without it.

The general lesson for this document: the telemetry contained the *signature* of
the fault but never its *cause*, and no amount of further analysis of the same
logs would have produced it.

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

---

## Open questions carried forward (operator, 2026-08-12)

Two operator questions that neither hypothesis currently answers. Both were
raised after H2 had been written up, and both weaken it.

### Q1 — The separation is a fixed DISTANCE, ~52 mm, not a fixed time

H2 predicted the second event should be speed-suppressed: faster transit, shorter
second lobe, killed by `EVENT_FLOOR_MS` 40. The operator objected that the
phantom occurs at close to cruising speed. Converting the two measured cases
using the real `spacingMm[]` (~300 mm, **not** the 200 mm assumed in an earlier
verbal estimate, which gave a wrong ~35 mm):

| case | spacing | prev dt | speed | weak event at | distance |
|---|---|---|---|---|---|
| mm 102→101, ramped | 300 mm | 1862 ms | 161 mm/s | +335 ms | **54 mm** |
| mm 80, constant 91 pwm | 300 mm | 1136 ms | 264 mm/s | +195 ms | **52 mm** |

The speeds differ by 64%; the distances agree within 5%. A fixed distance is the
signature of **geometry**, which argues against H1 (EMI has no characteristic
length). But ~52 mm is far beyond the range at which a bar magnet's return flux
would still clear the ±38 enter threshold, so it argues against **H2 as written**
as well.

What has a characteristic length of ~52 mm? Unanswered. Candidates worth
measuring before theorising: sensor-to-magnet standoff and any second ferrous or
magnetic element on the locomotive at a fixed offset from the Hall sensor. Note
the effect reproduces across two locomotives, which constrains any
locomotive-side geometry to something common to both.

**This supersedes the speed-suppression argument** used in decision 0024
("floor rejects scale with speed") as support for H2. That correlation may hold
for other reasons but no longer follows from the mechanism.

### Q2 — Why does the detector not examine the whole excursion?

Because LAYER 2 is a threshold-and-hysteresis edge detector, not a waveform
analyser. Polarity is latched on the opening sample
(`evOpenPole = (raw >= northEnter) ? 1 : 0`); the event closes after the field
holds inside the deadband for `EVENT_EXIT_HOLD_MS` 20; the machine is then idle
with no memory that an excursion occurred. It never sees a wave — only a
sequence of crossings.

The detector header anticipates a merged bipolar pair and selects opening-pole so
it "cannot be arbitrated into the wrong answer" — but that reasoning holds only
while the pair remains inside a single event. The 20 ms exit hold splits it
before the merge rule can apply.

A **peak-magnitude-per-polarity detector** — buffer the excursion, group by a
window, emit one event per magnet carrying the dominant lobe's polarity — is the
only proposal on the table that addresses the SOURCE rather than containment, and
per decision 0024 the only one that could resolve an **identity** error, which
the timing rule provably cannot. Costs: a buffer, a grouping constant, and
emission latency while waiting to see whether a second lobe arrives.

Not proposed, not designed, not approved. Recorded here so it is not rediscovered.
