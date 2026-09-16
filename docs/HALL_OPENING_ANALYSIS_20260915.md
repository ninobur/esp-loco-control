# What the Hall signal already knows at the moment it opens — all of 2026-09-15

**Analysis only. No firmware was changed and no threshold is proposed.**

Two questions, kept separate: (A) is the sign at opening already reliable about
polarity, and (B) how long after opening must we watch before we know the
opening was a real magnet.

---

## 0. The corpus, and what is actually usable

31 locomotive runs today across three builds. Source:
`~/NGR/telemetry/runs/9950011_20260915_*.log` on 192.168.68.142.

| | X17/X18 | X19 | X20 |
| --- | --- | --- | --- |
| per-candidate records (`diag/excursion`) | none — topic did not exist | 111 | 458 |
| marker events | 4351 | 111 | 458 |
| **complete waveform records** | **58** (37 undecimated) | **28** | **68** |

**Compatibility.** X19 and X20 are the same format and the same detector, so
they pool without qualification. X18 is a 40-byte version-less record: it
decimates, has no pre-roll, and its "open" is an entry crossing of the *global*
reference rather than a departure from a local minimum. Its samples are
oriented to the finally-assigned pole, which is what makes it usable for
question A. **X18 numbers are reported alongside, never pooled.**

A decoding trap worth recording: X19's header says a decoder can branch on the
first byte because X18 records "begin with slotIndex, which is small". slotIndex
can be **2**, which is exactly X19's version number. Branch on the run's build,
not on the byte.

---

## A. Does polarity ever change after a magnet signal opens?

### A.1 The direct count

| | denominator | opening sign wrong about the pole |
| --- | --- | --- |
| X19+X20, navigator ruled ADVANCED (map-confirmed) | 338 | **0** |
| X18, undecimated waveforms, opening vs assigned pole | 37 | **0** |
| **total** | **375** | **0** |

One decimated X18 record disagrees, at 2–256 ms per sample, where the stored
first sample is not the opening sample. It is reported and not counted.

### A.2 Every exception, individually

Five events today were `WRONG_MAGNET` with a recorded opening sign.

| build | time | tgt | map | opening sign | assigned | reading |
| --- | --- | --- | --- | --- | --- | --- |
| X19 | 18:54:44 | 41 | S | **N** (+338) | N | open agrees with assigned, both differ from map |
| X19 | 18:58:43 | 114 | S | **N** (+76) | N | " |
| X20 | 20:08:09 | 41 | S | **N** (+70) | N | " |
| X20 | 20:09:26 | 45 | S | **N** (+103) | N | " |
| X20 | 20:21:33 | 137 | S | **S** (−74) | **N** | **open agrees with the MAP; the assignment is wrong** |

In the first four the Hall was internally consistent — opening sign and
assigned pole agreed with each other — and a neighbouring marker carries the
opening pole in every case (`tgt+1` at 18:54 and 20:08; `tgt−1` at 18:58 and
20:09). Those are **counting** failures against a map pointer that had drifted,
not polarity failures. 18:58:43 is the known one: it followed the three false
Arches dwell counts, so the pointer was three markers wrong.

MM136 is the only event of the opposite kind today, and there the opening sign
was **right**.

### A.3 Does a genuine opposite excursion ever develop?

All 96 complete X19/X20 waveform records, re-signed by the opening departure.
Opposite-side maximum as a fraction of the opening-side maximum, over the
measurement window:

```
  >1.00  (opposite wins)      2
  0.75–1.00                   2
  0.50–0.75                   1
  0.25–0.50                   9
  0–0.25                     82
```

The two where the opposite side wins:

| build | det (ms) | opening side | opposite side | when | shape | assigned | opening |
| --- | --- | --- | --- | --- | --- | --- | --- |
| X20 | 2371149 | 115 | 906 | +186 ms | **shelf** | N | S |
| X20 | 689984 (**MM136**) | 108 | 117 | +297 ms | **shelf** | N | S |

Both are shelves — still at ≥50% of their own maximum in the last 50 ms of the
window, i.e. the signal went there and stayed. **Zero records in the entire
corpus contain a genuine opposite-polarity excursion after opening**: an arc
that rises on the far side of rest and returns. Not one.

### A.4 The same thing measured on all 569 candidates without waveforms

`exc_first − pre` is how long after the opening sample the excursion the pole
was taken from begins.

```
  0–20 ms   512      101–160 ms   13
  21–50       7      >160 ms      31
  51–100      6
```

Of the 57 beyond 20 ms, **49 are in two clusters — 18:59 and 20:40–20:50 — with
peaks to 1141 and departures to ±1050.** Otto's measured physical ceiling is
peak 390 with raw confined to 1777–2112, so those are handling, not running.
On the railway the pole comes from the feature that opened the event.

### A.5 Answer to A

**375/375. The sign at opening has never been wrong about the polarity of the
magnet that opened the event, in any usable record from today.** Every
polarity failure today was produced *after* opening, by the 400 ms peak search
selecting a later feature — and every such later feature was a shelf, never a
genuine opposite excursion.

---

## B. How many samples after opening are needed to know it is a magnet?

### B.1 Populations

A magnet passage requires motion, so traction decides this, not the navigator's
ruling. (The three Arches dwell events were *ruled* `ADVANCED`; they are false.)

- **GENUINE — 42.** Navigator ruled `ADVANCED` **and** PWM > 0.
- **FALSE — 35.** PWM 0, peak ≤ 390. By context: **3 station dwell (Arches)**,
  7 parked after a strike, 12 stationary before a run, 13 bench/handling.
- **EXCLUDED — 15.** Post-strike moving, `WRONG_MAGNET`, peak > 390.

Caveat, stated plainly: PWM 0 means no traction, but PWM > 0 does not guarantee
motion. And the genuine set is the 15-second waveform sample, 42 records out of
4567 accepted passages today — a sample, not a census.

### B.2 What does not separate, at any T

- **Amplitude at opening.** Genuine 70–88; false 70–338. No information at all.
- **Amplitude at any later T.** Genuine max 273, false max 382 — overlapping at
  every T from 0 to 200 ms.
- **Growth (max-so-far minus opening).** Never separates, at any T out to the
  end of the window. One setup transient at 18:54:44 opens at +338, spikes to
  +382 within 10 ms and collapses through zero to −93 by +20 ms; a running
  maximum never forgets it.

### B.3 What does separate

Where the signal **is** at time T relative to where it opened — not where it
has been. Median of the three samples at T, minus the median of the first
three:

```
  T ms     genuine floor    worst false      separation
     5           −5              +30         overlap
    10           +3              +18         overlap
    16           +8              +19         overlap
    18          +12              +12         overlap
    20          +13               +3         +10
    22          +15              +15         overlap
    24          +15              +25         overlap
    26          +16               +8          +8
    30          +23              +21          +2
    32          +24              +18          +6
    34          +25               +1         +24
    36          +24               +5         +19
    38          +22               +1         +21
    40          +21               +1         +20
    44          +15               +1         +14
    46           +9               +9         overlap
    48           +6              +18         overlap
```

**The separation is a window, not a threshold that improves with time.** Below
34 ms it flickers in and out — separated at 20, 26, 28, 30, 32 and overlapping
at 18, 22, 24. That is noise, not a gap, and **no claim of separation before
34 ms survives its own neighbours**. Above 44 ms it closes again, because a
genuine arc has passed its apex and is descending while a shelf is still there.

### B.4 The earliest defensible point, and what determines it

**≈34 ms after opening.** Gap 24 counts, and it does not hang on single
records — dropping the closest record on each side leaves +26.

The two worst-case events:

- **Genuine, +25:** X20 20:21:30.415, MM133, PWM 90, opened −75, peak 105. One
  of the low-amplitude wide S magnets on the 130s stretch — the same family the
  MM136 failure came from. Trace from opening, every 10 ms:
  `75 93 102 104 97 75 51 34 2 −15 −29 −33 −41`.
- **False, +1:** X20 20:49:32.586, on the bench being handled, opened −110.

**Against the false events that actually matter for navigation the margin is
far wider.** At 34 ms the three Arches dwell magnets read −26, −46 and −30, and
the seven parked-after-strike ones −10 to −103, against a genuine floor of +25
— a separation of 50 to 128 counts. The 24-count figure is set entirely by
bench handling, which is not a condition Otto navigates in.

### B.5 Independent corroboration from X18

23 accepted undecimated X18 passages, same statistic, same time base:

```
  T ms      X18 min      X19/X20 min
    10          −11              +3
    20          +24             +13
    30          +37             +23
    34          +38             +25
```

A different detector, a different reference, a different framing rule, and the
same shape of answer: nothing usable at 10 ms, unambiguous by 30–34 ms.

### B.6 Answer to B

**At the opening sample itself, nothing distinguishes a magnet from a level
change.** The information is not present, at any amplitude. It arrives as the
signal continues to rise, it is not reliable before **≈34 ms**, and it starts
to disappear again after ≈44 ms as genuine arcs descend.

---

## What this does not establish

- 3 of the 35 false events are an actual station dwell. The navigation-relevant
  false population is three records.
- The genuine set is a 15-second sample, 42 of 4567.
- The statistic in B.3 is descriptive. It is **not** proposed as a test, a
  threshold, or a firmware change, and no such change has been made.
- Question B was answered on X19/X20 framing. X18 corroborates the timing only.
