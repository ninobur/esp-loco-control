# NAVI — can the next magnet be found without ever deciding the last one ended?

**Date:** 2026-09-15 · **Otto 9950011** · X18 (`NAVI_ONE_1_0X18_LAP_BASELINE_FIELDTEST`)
**Status:** replay evidence only. No firmware written, no X18 change, no X19, no
decision record proposed. Test-only Python; nothing in this document is policy.

---

## 0. The proposition under test

> We do not need to determine when the previous magnet ended. We need to identify
> when the next magnet begins.

Two different questions were put side by side:

* **A — closure.** Has Hall returned sufficiently toward ordinary track?
* **B — next event.** Has Hall begun doing something that identifies another
  physical magnet?

The previous replay showed that making the *level* detector eligible again at
+500 ms fails: 9 of 52 recorded waveforms are still above the 70-count entry
margin at that instant and re-trigger immediately. This document tests whether
that failure is a property of **closure being necessary** or a property of
**the level test being the wrong question**.

Constraints honoured throughout: no ±25 closure, no requirement to fall below
ENTRY, no local-baseline closure, no waveform-return closure, **no PWM anywhere
in the detector**, no new baseline predictor. PWM appears only in §8, after the
Hall-only result, as diagnostic labelling.

**Answer: 1 — NO CLOSURE REQUIRED**, on every case the September 15 corpus
represents, with three specific gaps named in §10 that the corpus cannot close.

---

## 1. What the waveforms actually look like

The brief asked that the data be examined before any algorithm was chosen. It
was, and the shapes settle the matter before any threshold is picked.

**A persistent field is flat. A magnet is an excursion.** That is the whole
finding. Three examples, drawn from the recorded `diag/waveform` dumps
(values are counts from the passage's frozen entry baseline):

```
11:29:59.317   1945 ms, the signal never returns
   t=   0  70 |  t= 512  78 |  t=1024  65 |  t=1536  78 |  t=1888  59
   flat at 74-79 for the whole event; peak-to-floor movement 14 counts

11:29:57.084   5478 ms, a real magnet arrives on top of a persistent field
   t=   0  70 | ... | t=2400  65 | t=2480  83 | t=2560 149 | t=2640 223
                          t=2720 192 | t=2800  91 | t=2880  63 | ... flat again

11:50:57.196   4996 ms, the boot whose reference primed ~112 counts wrong
   shelf pinned at 83 the entire time, with four clean spikes:
   t=1216 -> 269,  t=2496 -> 286,  t=3760 -> 269   (plus the one at t~0)
```

In `11:29:57` and `11:50:57` the signal never once comes within 25 counts of the
reference, and never falls below the 70-count entry margin. Closure is
unavailable for the whole event. The magnets are nevertheless obvious: they are
150–200 count departures from a level that had been steady for seconds.

**The two merges that cost the day's first two strikes have the same shape.**

```
11:05:20.531  1852 ms, MM150, two North magnets swallowed into one passage
   t=  84 -> 205   ...  shelf at 27-40 for 1.2 s  ...  t=1440 -> 206

12:06:24.617  1443 ms, MM013, same failure
   t=  76 -> 159   ...  shelf at 21-36 for 1.0 s  ...  t=1320 -> 223
```

The inter-magnet floor sat at **27–40 and 21–36 counts** — above the 25-count
exit margin, which is exactly why the passage never closed and two magnets became
one. But as a *departure* those two magnets are 170 and 190 counts clear of the
floor between them. They are not marginal. They are not even close to marginal.

---

## 2. The criterion, chosen after looking

The signature the data offers is **departure from the quietest thing the sensor
has seen recently** — which is a question about the future, not the past. It
never asks whether anything ended.

```
L(t)  = the sample of smallest magnitude in the trailing 300 ms
trigger when  |v(t) - L(t)| >= 70  for 2 consecutive samples
```

Three properties matter, and all three fall out of the arithmetic rather than
out of tuning:

1. **It is differential.** `v(t) - L(t) = raw(t) - raw(L)`. The operative
   reference cancels. A reference that is 45 counts wrong — or 112 — changes
   nothing about whether the rule fires.
2. **It is asymmetric in the right direction.** On a decaying tail the most
   recent samples *are* the smallest, so `L` follows the signal down and the
   departure stays near zero. On a rising edge `L` is stuck at the older, quieter
   sample and the departure grows. Decay cannot masquerade as arrival.
3. **On quiet track it is the present entry test.** With `L = 0` the rule reads
   `|v| >= 70`. **No new constant is introduced.** The 70 is the existing
   `entryMargin`; it has simply been re-anchored from a global reference to an
   observed local one.

Four other families were run against the same corpus before this one was
preferred. The comparison is in §7; the two that reach a clean result are the
two that use a *minimum*, and the three that use a *central* statistic (trailing
median, trailing mean, fixed-lag difference) cannot get below 2–3 unexplained
candidates at any operating point, because on a decaying tail a central statistic
still sits well above the present sample.

Full architecture as simulated — **no closure test appears anywhere in it**:

```
candidate  -> fixed 400 ms acquisition window, measured relative to L
           -> peak (median-of-3 judge copy), polarity, width screen
           -> 500 ms refractory from the trigger instant
           -> search for the next departure
```

---

## 3. Case 1 — the nine records a level detector re-triggers on

Every one of the nine, replayed Hall-only. "Real magnets present" is the
independent excursion census of §6, not the detector's own output.

| record | dur | Hall at +500 ms | real magnets past +500 ms | rule fires |
|---|---:|---:|---:|---|
| 11:18:10.395 | 672 | 95 | 0 | **0** ✓ |
| 11:29:24.399 | 656 | 129 | 0 | **0** ✓ |
| 11:29:24.387 | 2 775 | 86 | 0 | **0** ✓ |
| 11:29:59.317 | 1 945 | 76 | 0 | **0** ✓ |
| 11:29:57.084 | 5 478 | 75 | 1 | **1**, at +2 560 ✓ |
| 11:30:09.202 | 2 107 | 85 | 1 | **1**, at +1 176 ✓ |
| 11:50:57.196 | 4 996 | 83 | 3 | **3**, at +1 152 / +2 448 / +3 712 ✓ |
| 11:50:57.201 | 22 053 | 83 | 1 | **1**, at +19 264 ✓ |
| 11:29:43.146 | 16 004 | 101 | 0 clear | 1, at +1 792 — see below |

**Eight of nine exactly right. The ninth is the 16-second hand-push at 11:29:43**,
whose single extra candidate lands on a census excursion of 87 counts and 288 ms
— magnet-shaped, on a car being pushed by hand past the markers, so more likely a
real encounter than a false one. It disappears at a detect threshold of 80. It is
counted as unmatched throughout this document rather than argued away.

The first two rows are the instructive ones. `11:18:10` and `11:29:24.399` are
**single clean arcs** — one magnet, peak 137 and 187, still on its own downslope
at +500 ms. A level test calls that a second magnet. A departure test sees a
falling signal whose trailing minimum falls with it, and correctly reports
nothing. *The 9/52 re-trigger result was never evidence about closure; it was
evidence that a level is not an event.*

**DEMONSTRATED.**

---

## 4. Case 2 — the 3 652 ms shelf

The adversarial case, at 8 ms resolution (the dump decimates; see §9).

```
detection at t=0 on a 75-count transient, then a noisy 33-68 count floor
  t=  1024 -> 124      t=  2240 -> 128
  t=  1088 -> 204      t=  2304 -> 224     <- the two real magnets
  t=  1152 ->  97      t=  2368 -> 117
  floor resumes 31-68 for the rest of the 3.6 s, and the event never closes
```

The false initial candidate occurs 992 ms *before* the real magnet, exactly as
the brief describes. The rule re-arms at +500 ms with the signal still at 45
counts and nothing to declare, then fires at **+1 024 ms** (peak 168, width
176 ms) and again at **+2 240 ms** (peak 171, width 232 ms). The shelf between
and after them produces nothing.

The event is never declared closed. It does not need to be.

**DEMONSTRATED.**

---

## 5. Case 3 — long persistent fields, and case 5 — normal passages

**Persistent fields.** Six records hold a field for seconds without closing.
Candidates produced after the initial one, free-running:

| record | duration | extra candidates |
|---|---:|---:|
| 11:29:59.317 | 1.9 s flat at 75 | **0** |
| 11:29:24.387 | 2.8 s flat at 97 | **0** |
| 13:21:03.054 | 1.2 s shelf at 50 after one magnet | **0** |
| 11:15:04.481 | 25 s | **0** |
| 11:17:48.342 | **111 s** | **0** |
| 11:50:57.201 | 22 s at 83 | **1** — and there is exactly one real spike in it |

A signal that stays unchanged manufactures nothing, at any duration up to
111 seconds, because an unchanging signal has zero departure from its own recent
minimum by construction. **The suppression is structural, not a tuned threshold.**

**Normal passages.** 33 records of 500 ms or less, run free-running with a
quiet prefix: **exactly one candidate on 32 of 33**. The one exception
(12:49:17.921) is a 97 ms `TOO_SOON` fragment whose 400 ms window also contains a
larger neighbouring peak, so the width screen measures the wrong excursion.

Fixed-window measurement against production's own `close()` output, median-of-3
judge copy per decision 0065, `dec=1` records:

```
window 150 ms  29/33 exact peak      window 300 ms  32/33 exact
window 200 ms  31/33                 window 400 ms  32/33
window 250 ms  32/33                 window 500 ms  32/33
```

The single non-match differs by +9 counts because the fixed window outlived
production's early close and caught a higher sample. **Any window at or above
250 ms reproduces the production peak exactly.**

**DEMONSTRATED.**

---

## 6. Case 4 — merged passages, and the day's actual navigation failures

Ground truth was built independently of the detector: every local maximum in
every record longer than 600 ms that rises ≥50 counts above that record's own
20th-percentile level. Eleven of those excursions are unambiguous physical
magnets (rise ≥100 counts, width ≥100 ms) lying past the first refractory — the
population the rule must find.

**11 of 11 found. 1 unmatched candidate in the whole corpus** (the hand-push
above). Zero unmatched at a detect threshold of 80.

Two of the eleven are the merges that produced strike 1 and strike 2:

| | production | with the departure rule |
|---|---|---|
| 11:05:20 MM150 | one 1 852 ms passage, one advance, count now one short | **two candidates**, +0 (peak 204) and +1 332 (peak 175) |
| 12:06:24 MM013 | one 1 443 ms passage, one advance | **two candidates**, +0 (peak 159) and +1 260 (peak 201) |

Both lost advances are recovered. The third strike (13:21:03, a floor rejection
under a high reference rather than a merge) has no waveform of its own, so it is
addressed as SUPPORTED, not demonstrated, in §8.

**A second genuine magnet arriving while no closure criterion has been satisfied
is recognised as a second magnetic event.** **DEMONSTRATED.**

---

## 7. Sensitivity, and why this rule rather than another

All figures: 11 real next-event targets, unmatched candidates in brackets.

```
detect threshold D (trailing window 300 ms, width screen on)
   D=50  11/11 [2]     D=70  11/11 [1]     D=90  10/11 [0]
   D=60  11/11 [2]     D=80  11/11 [0]     D=100 10/11 [0]
        -> operating band D = 50..80; the knee at 90 loses one target

trailing window WL (D=70)
   100 -> 9/11   150..600 -> 11/11 [1]   1000 -> 11/11 [2]
        -> anything from 150 ms to 600 ms behaves identically; 100 ms is too
           short because L starts chasing the rising edge

persistence P            refractory            measurement window
   P=1  11/11 [3]          300 ms 11/11 [3]      150 ms 11/11 [1]
   P=2  11/11 [1]          400 ms 11/11 [1]      250 ms 11/11 [1]
   P=3  11/11 [0]          500 ms 11/11 [1]      400 ms 11/11 [1]
   P=4  10/11 [0]          1000 ms 11/11 [1]     500 ms 11/11 [1]
```

Every parameter has a plateau rather than an optimum. Nothing here is fitted.

**Rule families compared** — each swept over its own best operating point:

| family | trailing statistic | best result |
|---|---|---|
| **min-magnitude** | quietest recent sample | **11/11, 0 unmatched** |
| **rise in \|v\|** | same, magnitude-only | **11/11, 0 unmatched** |
| trailing median | central | 11/11, 2 unmatched |
| trailing mean | central | 11/11, 2 unmatched |
| fixed-lag difference | `v(t) − v(t−W)` | 11/11, 3 unmatched |

The three central statistics fail for the reason §2 gives: during a decaying
tail they sit above the present sample, so the tail keeps generating departures.
Only a minimum follows the signal down.

---

## 8. Where the reference still matters, and where it stops mattering

**Detection stops depending on the reference entirely.** Two independent
continuous observations of the quiet line:

```
diag/departure, 10 Hz, |raw - baseline| with no passage open, n=145
   median 2   p90 15   p99 24   max 60
|raw - baseline| at the instant each passage closed, n=4 351
   median 13  p90 18   p99 22   max 24        (bounded by design)
gap between consecutive passages, n=4 340
   min 28   p1 849   median 1 089    only 0.4% are shorter than 300 ms
```

So in ordinary running the trailing 300 ms window lies wholly between magnets
99.6% of the time, and `L` takes a value of roughly 0–25 counts. Against the
day's accepted-magnet peaks (n=4 229; min 82, p1 120, median 173) the departure
available to the weakest 1% of magnets is **95 counts against a threshold of 70**.

Compare the two tolerances directly:

| | budget before detection fails |
|---|---|
| level test, reference error E | `A − 70 − |E|` — today E reached **112** |
| departure test | `A − 70 − L`, and `L` is an observed sample, not an estimate |

**This is what makes boot 5 work.** At 11:50:57 the prime was ~112 counts wrong;
the level test is hopeless for the whole boot. The departure rule finds all four
magnets in that record because it never consults the reference. Labelled
**DEMONSTRATED** for detection.

The third strike (13:21:03) was a floor rejection: a genuine magnet starved to
76 ms because the reference sat 18 counts high, so the arc spent less time above
entry and less time above exit. Measured from a local `L` rather than from a high
reference, both its amplitude and its width recover. There is no waveform dump
for that passage, so this is **SUPPORTED BUT NOT PROVEN**.

**PWM, used only here and only as a label.** The detector above never reads it.
Of the 9 level re-triggers, 6 are from the hand-push (pwm < 25), 1 is moving at
pwm 42, 2 are boot 5. The departure rule resolves all nine *without* the motion
gate, and motion gating would not have resolved boot 5 at all. PWM was not needed.

---

## 9. What closure was still doing, and what now does it

| closure's job | replacement | status |
|---|---|---|
| end the measurement | the fixed 400 ms window | **DEMONSTRATED** — peak exact 32/33 |
| supply the guard anchor | the detection instant | **DEMONSTRATED** — §7 refractory sweep |
| reject fragments (82 ms floor, 0085) | excursion width inside the window | **needs re-derivation — see below** |
| decide polarity aperture | the excursion, not the window | **see below; this one is a new hazard** |
| feed `lapBaseline.advance(shadow, close)` | nothing — there is no close sample | **UNTESTED consequence** |

**The 82 ms floor cannot be carried across unchanged.** Measured conversion,
`dec=1` records:

```
width at 34% of the window peak  / production duration : median 0.83
width at a fixed 25-count caliper / production duration : median 0.93
```

So 82 ms of production duration is about 68 ms or about 76 ms of window width
depending on how the width is measured, and a screen set literally to 82 loses
three genuine magnets including the known 83 ms borderline passage. This is the
same coupling flagged in `NAVI_PEAK_CLOSE_REPLAY_20260913.md` §3. **Decision
0085 is operator-approved and this document does not propose a number for it**;
it only records that the derivation must be redone against whatever width
definition is chosen. At any screen of 75 ms or below, 32 of 33 normal passages
give exactly one candidate and the 16 ms transient in `11:05:22.540` is rejected.

**The fixed window creates a polarity hazard that closure did not have.**
Decision 0064 takes polarity from the signed sum over the whole passage. A
passage ends when the signal returns; a fixed window does not, so it accumulates
a long quiet tail, and every count of error in the reference for that tail is
multiplied by the window length. Measured, `dec=1` records:

```
reference error that flips the polarity sum
  summed over the full 400 ms window : min 6   p10 19   median 30  counts
  summed over the excursion only     : min 70  p10 88   median 104 counts
```

**Six counts.** The fixed window must not be the polarity aperture. Summing only
the samples belonging to the excursion restores a 70-count worst-case margin,
and the running sign becomes stable at a median of 24 ms and a worst case of
77 ms, well inside any window considered. This is a real requirement the replay
surfaced, not a preference. **DEMONSTRATED.**

**X18's lap controller loses its input.** `NAVI_ONE.ino:1283` feeds
`lapBaseline.advance(ns.navMm, j.shadowBaseline, j.closeBaseline)` at close. With
no close there is no such sample. `L` is a better-conditioned estimate of the
same quantity — it is by construction the quietest recent observation — but that
substitution is **UNTESTED** here and is out of scope for this question.

---

## 10. What September 15 cannot answer

Three gaps, stated plainly rather than inferred past.

**a. First detection, and low-speed first detection, are not testable.** Every
`diag/waveform` record begins at the entry crossing and carries only a 12-sample
pre-roll — which, at ordinary speed, already sits on the foot of the arc at
40–60 counts. The corpus therefore cannot supply the 300 ms of pre-magnet
history the rule wants at the *first* detection of an isolated magnet. Every
result in §3–§6 is a **next-event** result, where the history is inside the
record. The maximum-departure figures for the short records (65–92 counts) are
truncation artifacts and are **not** measurements of low-speed margin.

**b. Low-speed moving, kept separate from stopped.** Following the previous
report's correction, these populations are not combined:

```
LOW-SPEED MOVING  25 <= pwm < 60   9 waveform records, all pwm 42-49
                                   peaks 131-138, durations 343-672 ms
                                   next-event behaviour: correct, 0 extras
                                   first-detection margin: NOT MEASURABLE (see a)
STOPPED           pwm < 25        10 waveform records, all hand-push or struck
                                   no route magnet was encountered in any of them
NOTHING AT ALL    pwm 25..40       no records exist in this band, all day
```

**c. No magnet was ever recorded riding on an opposite-sign field.** The dumps
are orientation-normalised (`WaveformWindow.h:22`), and within every one of the
18 long records the shelf and the magnet carry the same sign. So the claim that
`L` subtraction *corrects* polarity under a DC offset — which the arithmetic
supports — has no observed instance behind it. **UNTESTED.**

### The experiment that would close all three

One run, one firmware change that is purely instrumentation:

1. **Extend the pre-roll from 12 samples to 512 ms**, or dump the raw 1 kHz Hall
   stream continuously for one lap. This alone converts (a) and most of (b) from
   untestable to measured, and costs nothing in navigation behaviour.
2. **A deliberate slow section at pwm 25–40**, which the railway has never run.
3. **One stop positioned so the sensor rests inside a fringe field**, then a
   normal departure — the in-field dwell that neither this corpus nor the
   previous one contains.
4. **A stop and restart between two opposite-pole markers**, to produce the
   opposite-sign shelf of (c).

---

## 11. Conclusion

**1 — NO CLOSURE REQUIRED.**

Question B can be answered without ever answering question A, on every case the
September 15 corpus represents: 11 of 11 genuine next-event magnets found, 1
unmatched candidate in 52 waveforms (and 0 at a threshold of 80), 0 spurious
magnets from persistent fields lasting up to 111 seconds, exactly one candidate
per magnet on 32 of 33 normal passages, and both of the day's merge-induced lost
advances recovered. No counterexample waveform requiring closure was found.

The mechanism is not a new test bolted on. It is the existing 70-count entry
margin measured against the quietest recent sample instead of against a global
reference — which is why it costs no new constant, and why a reference that is
112 counts wrong stops being able to blind the detector.

Two conditions attach, both demonstrated rather than speculative:

* the fixed window **must not** be the polarity aperture — a 6-count reference
  error flips the sign if it is;
* the 82 ms floor **must be re-derived** against whatever width definition
  replaces passage duration. It is operator-approved and is not proposed here.

And the architecture is not yet shown to work at first detection, at pwm 25–40,
or against an opposite-sign field, because no such data exists. §10 says exactly
what run would produce it.

---

## 12. Provenance

`~/ngr-telemetry/pi/NGR/telemetry/runs/9950011_20260915_*.log` via 192.168.68.142.
55 `diag/waveform` dumps, 52 complete after chunk reassembly; 4 367 `state/nav`
records; 10 346 `alert`; 155 `diag/departure`; 24 `diag/acquisition`; 23
`diag/baseline_lap`. Route polarity and spacing from `RouteMap.h` (171 markers,
280–355 mm, median 300).

**Decimation limits resolution.** `HallCapture.h:220` subsamples
(`if (decPhase_++ % dec_) return;`) with no averaging, halving the rate each time
the 512-sample buffer fills. The 3 652 ms shelf is at 8 ms, the 22 s record at
64 ms, the 111 s record at 256 ms. Sub-sample structure — and therefore true
transient width — cannot be recovered from a decimated record, and the 1 kHz
behaviour of a 2-consecutive-sample persistence test can only be evaluated on the
`dec=1` records.

Corrections carried forward from earlier in this investigation and not repeated
here: the pre-roll does not provide a clean local zero; station dwells on this
day did not leave the sensor in a field; the 22-second event at 11:50:57 is boot
5's bad prime, not a station dwell.
