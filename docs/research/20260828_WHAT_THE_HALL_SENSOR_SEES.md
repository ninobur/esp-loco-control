# What the Hall sensor sees on the Lowline

**2026-08-28. Toby (9950012).** ~11,000 marker crossings and 2,092 full-rate
waveform captures across twelve laps, a full-circuit environment survey, and two
manual calibration runs at PWM 90 with a consist attached. Air 99 °F.

This is a record of the railway's physical behaviour as the sensor measures it.
It is independent of any firmware, and it should outlive every sketch we have
written. Evidence in `field-records/logs/20260828_survey/`.

---

## 1. Between magnets, the sensor sees nothing

Three measurements, each stronger than the last.

**Stationary, motor off, 90 s.** Zero excursions reaching 15 counts — about
three times the sensor's own noise. Zero floor rejections. Baseline 1828, raw
wandering 1826–1827.

**Stationary, motor turning at PWM 90, 100 s.** Zero again. The H-bridge,
commutation and the ESP's own switching put nothing measurable into the Hall
sensor. This was the run most likely to produce electrical noise.

**A full circuit, 171/171 markers, 240 s.** 345 excursions: 192 magnets and 153
others — and **every one of the 153 belonged to a magnet.** The most isolated
disturbance on the entire loop sat 0.02 s from an accepted magnet. Gaps between
consecutive excursions of any kind ran a median of 1.05 s and a maximum of 2.63 s,
which is simply marker spacing at cruise.

No rail joint. No fixing. No point. No fastener. No motor transient.

**The control that makes those silences mean something:** a magnet passed by hand
registered every time, in three parts — approaching fringe, body, departing
fringe. The capture path was demonstrably awake.

### The shape of an interval

```
|<-- 167 ms magnet -->|<- 22 ->|<- ~40 ->|<---- ~1000 ms of nothing ---->|
       field above         gap    rebound         absolutely silent
       the threshold               tail
```

**A magnet occupies about 14% of the interval. The other 86% is empty.**

---

## 2. What a magnet looks like

### It is a Gaussian

Five candidate functions fitted to the median of 1,746 real passages, normalised
to unit peak:

| model | residual |
|---|---|
| **Gaussian** `exp(-(t-µ)²/2σ²)` | **0.0153** |
| Lorentzian | 0.0507 |
| half sine | 0.0768 |
| point dipole `(2z²-x²)/(x²+z²)^{5/2}` | 0.2684 |

The residual of 0.0153 equals the amount a magnet differs **from itself** pass to
pass. The Gaussian reproduces the curve as accurately as measuring the same
magnet twice does; there is nothing left to explain.

**A passage is therefore four numbers:**

```
sign   N or S
A      peak amplitude, counts        (~190 typical)
t₀     centre                         leans late by 1.1% of pulse width
σ      width ≈ 0.29 × duration above 20% of peak   (~46 ms at cruise)
```

Two details worth keeping. The centre sits at **0.5107 CW and 0.5119 CCW** —
both leaning late by the same amount, so it is a reading lag, not geometry. A
sensor sitting ahead of or behind the reference point would reverse sign with
direction. It does not.

And the **point-dipole model fits seventeen times worse** than a Gaussian. The
magnets do not behave as point sources at this distance; the field is broadened
by their physical extent.

### Its arc carries only the sensor's own noise

```
jitter of the magnet arc     4.88 counts (median), 12.04 at the 99.9th
jitter of the quiet baseline 4.13 counts
correlation with arc height  -0.03
```

**Independent of amplitude.** A 190-count magnet and a 60-count magnet carry the
same absolute jitter, because a magnetic field is smooth in space and contributes
no roughness of its own. That makes roughness a fixed ceiling rather than a ratio
— a jagged spike train reads 52 counts, a noisy plateau 64, four to five times
over.

---

## 3. The only thing that looks like a magnet is a magnet

Every non-magnet excursion on the railway is a magnet's **departing rebound** —
the dipole reversing as the sensor clears it.

```
arrives    14–64 ms after the event closes
polarity   OPPOSITE to its magnet, 149 of 150
amplitude  16–34 counts sub-threshold; 38–52 when it clears the 38-count entry
```

Nothing arrives on approach. Anything appearing "before" a magnet is the previous
magnet's rebound, and the pole inversion proves the parentage.

**The rebound is ~10% of its magnet's peak**, and that ratio is the same for
every magnet — 0.09 for the eight that occasionally cross the threshold, 0.10 for
everyone else. What differs is the starting size. A typical 186-count magnet
throws a 19-count rebound, half the threshold. The strongest throw 23–29, close
enough that ordinary variation occasionally pushes one over.

**The eight that do:** MM004, 061, 063, 080, 081, 100, 109, 144 — six of them
among the strongest on the railway (MM100 ranks 2nd, MM063 3rd, MM064 5th).

### Rebounds pass every morphology test

Held to the magnet template, of 149 rebounds with usable curves:

| test | passes |
|---|---|
| jitter (smooth enough) | **149/149 — 100%** |
| Gaussian fit | 122/149 — 82% |
| template distance | 49/149 — 33% |
| **all three** | **45/149 — 30%** |
| **reaches magnet amplitude (≥140)** | **0/149 — 0%** |

**Thirty percent of them look like magnets.** Of course they do — they *are*
magnets, seen on the way out. Morphology asks "what shape is this?" and the
honest answer is "the shape of a magnet." What separates them is that they are
weak and too soon.

---

## 4. Magnets cannot be told apart

Four independent methods, all converging:

| method | markers separable from their neighbours |
|---|---|
| whole normalised curve, RMS | 68% / 55% |
| residual after subtracting the common arch | 53% / 55% |
| **3 principal components** (90% of all variation) | **60% / 64%** |
| 10 principal components | 59% / 65% |

When a crude description and a well-chosen one give the same answer, the
limitation is not the description.

**Individual character is real but insufficient.** Subtract the arch every magnet
shares and each has a remainder that repeats pass to pass at **r = +0.73**, smooth
rather than jittery. It is about **3% of the signal**. Genuine, and not enough:

```
a magnet vs ITSELF on another lap     0.013
a magnet vs its NEXT-DOOR neighbour   0.019
a magnet vs the railway average       0.035
```

Magnets on opposite sides of the layout differ properly. Adjacent magnets barely
differ at all — only a little more than a magnet differs from its own previous
lap.

**Why:** what shapes the pulse — sensor height, track geometry, local speed,
ballast — varies *smoothly along the track*. Neighbours share their surroundings,
so they share their character. The fingerprint fingerprints the **place**, not
the magnet. Illustrated at
`https://claude.ai/code/artifact/b24d7f37-24fd-4be7-be83-4df8f6c22243`.

### Worked example: five consecutive magnets

MM082–086, all bar magnets, seven passes each:

```
distance between DIFFERENT magnets   0.0085 – 0.0219
distance from a magnet to ITSELF     0.0132 – 0.0199
```

MM085 and MM086 differ by 0.0085 — **less than MM085 differs from itself** lap to
lap.

---

## 5. What the shape does encode: magnet type, at 99.4%

Pulse width — the fraction of the normalised excursion above half height — is
**bimodal**, two populations separated by **4.9× their own spread**:

```
narrow  0.640  (n=67)   bar, level with the sleeper
wide    0.702  (n=100)  disk, on top of the sleeper
```

Every marker's type was predicted from width alone, split at 0.671, and published
**before** the operator walked the railway. Scored against the survey:

```
166 of 167 correct — 99.4%
every stretch boundary placed correctly
MM158 — a lone bar between two disk stretches, flagged in advance as the
        least confident call — is a bar
```

**The single miss is the most useful part.** MM012 was wrong by **nine
ten-thousandths** (width 0.6719 against a 0.6710 split) and was the second most
variable marker in the classification, at 2.1× the railway median spread. It was
found sitting on concrete and off-centre. **The physical fault appeared as
variance, not as a wrong value** — and variance is measurable without knowing the
answer in advance.

**But type is laid out in eleven blocks**, so only **4%** of adjacent pairs differ.
The signal is strong and spent where it cannot help. Map at
`https://claude.ai/code/artifact/8c1db1ad-ee48-4b2c-8784-60033b204d50`.

---

## 6. Direction changes the readings, and grades are why

Two runs back to back — same locomotive, consist, speed, air. Means agree exactly
(strength CW−CCW mean +0.0). But:

```
Duration, CW minus CCW, across MM060–MM085
  +5 +24 +48 +53 +59 +51 +34 +48 +58 +47 +44 +40 +40 +42
  +66 +73 +62 +68 +48 +50 +38 +43 +38 +30 +33 +16
  net sign: +26 of 26
```

Twenty-six consecutive markers, all slower CW, by up to **73 ms — nearly 60% at
MM075**. MM027–031 runs −5 of 5; MM129–140 runs −12 of 12.

At held PWM, duration is magnet length over speed, and speed depends on gradient.
A stretch climbed one way is descended the other. `durationMs90[]` was never a
property of the magnet.

**Cost of ignoring it:** carrying one direction's table into the other refuses
**52 of 170** markers.

**Measured speeds at PWM 90**, from crossings against surveyed spacings:

```
slowest section  218 mm/s
median           267 mm/s
fastest (MM069)  319 mm/s     280 mm takes 876 ms
```

---

## 7. Detection margins, as measured

```
real magnets    peak min 101 (MM012), 1st pct 130, median 186–190
                duration min 131 ms, median 158–167 ms
rebounds        peak max 52, duration max 103 ms
noise floor     4.1 counts RMS; entry threshold 38 counts
```

The strongest rebound and the weakest real magnet are **52 against 101** — a
factor of two, not the clean daylight a single lap suggests. **1.68% of cruise
crossings read under 140 counts**, concentrated at MM012 (median 127), MM141
(139), MM152 (144), MM162 (150).

A fixed floor cannot serve two locomotives: Otto's genuine magnets read ~50
counts where Toby's read ~190. Measured **relative** to the running median of
recent accepted peaks, the populations separate — false events top out at 0.26,
real magnets sit at a 1st percentile of 0.63, and Otto's weak ones sit at 1.01.

---

## 8. Physical faults are visible in the data before they are visible by eye

**MM128 — found by the numbers, three days into failing.**

```
26 Aug   reads S, 14 of 17, median peak 186   correct
27 Aug   reads S, 15 of 23, peak 173
28 Aug   reads N, 6 of 6,   peak 108          58% of neighbours
```

Recovered upside down, tilted, and lying between sleepers two sleepers from its
assigned position. Neighbours MM127 (187) and MM129 (181) unchanged at the same
temperature, which excluded a thermal cause.

**The displacement was measurable from timing alone.** At held PWM 90:

```
             before repair   after
MM127→MM128     1251 ms      1055 ms
MM128→MM129      836 ms      1108 ms
pair total      2087 ms      2163 ms   — conserved, because the neighbours had not moved
```

A long leg then a short leg, the sum fixed. Against the mapped 590 mm that is a
**66 mm shift** — about 33 mm per sleeper, matching the operator's "two sleepers"
observed by eye.

**This is worth building on: polarity checking cannot detect displacement.** A
magnet knocked 66 mm along the track but still the right way up passes every
polarity test, every lap, silently. MM128 was only caught because it *also*
flipped. The signature is consecutive legs deviating in opposite directions while
their sum stays true to the map — and it distinguishes a moved marker from a slow
locomotive without any speed model.

**MM152 — cleared, no excavation.** Suspected stacked double magnet on a recorded
123 → 85 drop. Across four sessions, peak as a percentage of each session's own
gain: 75.4%, 81.6%, 75.8%, 78.7%. **Stable, not falling.** The 123 was a stale
table value; the magnet was always ~78%. Not displaced (leg test ~15 mm,
inconsistent in sign) and not deeper (duration normal — a deeper magnet reads
*longer*, not merely weaker).

"Weak" and "failing" are different findings, and what separates them is a time
series, not a single reading.

---

## 9. Method notes, each of which cost something today

**In-sample validation of a per-marker template is not evidence.** A shape band
read 0.0% false refusals in-sample and **7.6% under leave-one-out** — thirteen
stops a lap. Three curves were fitting themselves.

**A negative result needs a positive control.** Two silent survey runs meant
nothing until a hand-passed magnet proved the capture path was alive.

**Check provenance separately from consistency.** A stretch with a magnet skipped
is internally consistent — steps of one, polarity matching — so it survives the
ten-magnet proof while sampling the wrong marker's field. Contamination showed as
a *contiguous block* (MM128–135) moving together; isolated magnets do not conspire.

**An instrument that saturates must say so.** Fourteen of the first 204 captures
returned flat tops with no indication. It took decoding them to notice. Every
record now carries a clip count.

**Test the excuse, don't lean on it.** "The templates are just noisy" was the
obvious defence of the shape result, so the survey was doubled — 3 passes per
marker to 6, leave-one-out from n=44 to n=1016. The numbers did not move at all.

**Sampling rate is a trap you only see once.** A June 2026 survey logged at ~40 ms
and caught a magnet in three points. At that rate "shape does not discriminate"
would have been concluded from data that never held any shape. These captures run
~1 kHz — 160 samples for a 129 ms passage.
