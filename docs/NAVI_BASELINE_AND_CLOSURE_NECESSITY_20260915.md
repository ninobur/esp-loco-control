# NAVI_ONE — are a baseline and a closure test necessary at all?

**Date:** 2026-09-15
**Build analysed:** `NAVI_ONE_1_0X18_LAP_BASELINE_FIELDTEST` (working tree), Otto
9950011, entry 70 / exit 25 / floor 82, `fixedAfterPrime = true`.
**Corpus:** all Otto telemetry of 2026-09-15 as one chronological experiment —
15 logger segments, 8 boots, 83,915 messages, 10:54 to **13:50:10**.
**Status:** Investigation only. No firmware change, no X19, no decision record
proposed. Nothing here is ratified.

> **Data note.** `OTTO_X18_ALL_20260915.tar.gz` was cut at 13:48:34 while the
> session was still live. The Pi holds 495 further lines to the clean shutdown
> at 13:50:10. Those lines are analysed here and they change a published
> conclusion — see §7. Work from the Pi copy, not the archive.

---

## Answer in three sentences

**A reference is unavoidable.** Detection, amplitude and polarity are all
*contrasts*, and a contrast needs something to contrast against.

**A closure test is not.** Peak and polarity are reproduced exactly by a fixed
measurement window with no closure test of any kind — demonstrated below on
every clean passage in the corpus, including at low speed.

**But closure is not the defect either.** The defect is that the reference is
**old**. A reference one second old is wrong by ≤6 counts at the 99th
percentile; the run-long reference X17 and X18 freeze at boot was wrong by up to
54. The exit margin is 25. Nothing about the *test* failed on September 15 —
the number it tested against had aged out.

---

## 1. What navigation actually reads

Traced through the source. `HallCapture::close()` is the sole producer of a
`Passage`; a `Passage` is the sole input to recognition and navigation. Of its
fifteen fields, the number that reach a decision is **three**:

```
MagnetRecognizer::examine()  reads  p.openedAtMs, p.peakCounts, p.polarity
Navigator::judge()           reads  p.polarity
```

`closedAtMs` is carried but is not consumed for *this* passage's decision — it
becomes the **next** passage's guard anchor, and feeds a display-only speed
estimate (`NAVI_ONE.ino:1278-1281`). Everything else — `oriented`, `judged`,
`sampleCount`, `preSamples`, `truncated`, `clipped`, `decimation`,
`entryBaseline`, `signedSum` — is carried for telemetry and thresholded on by
nothing. The header says so explicitly for `signedSum` (decision 0064).

**There is no sorting, orientation or morphology stage between acquisition and
navigation.** `TwoSided.h` (510 lines) is **not included by the sketch**
(`NAVI_ONE.ino:56-66`) and references a `Passage::stitchAt` field that no longer
exists. It is dead code. Morphology is out of the production path entirely
(decisions 0080/0082), so it cannot silently acquire navigation authority — it
has no path to acquire it through.

So the acquisition layer's entire job is to deliver, once per physical magnet:
**a timestamp, a peak, and a sign.**

## 2. Category-by-category: what CLOSE does, and what needs a global zero

| function of `close()` | site | needs return to a global baseline? |
|---|---|---|
| waveform framing | `:295-309` | **no** — a fixed window frames it (§4) |
| transient rejection | — | **not done at close.** Amplitude does it, in the recognizer |
| polarity | `:282` | **no** — sign of `sum_`, settled at 27 ms (§3) |
| orientation/sorting | `:283-284` | **no** — a global sign flip, follows polarity |
| morphology | — | not compiled in |
| 82 ms floor | `:251-275` | **yes, by construction.** Duration is `close − open`. This is the one function genuinely bound to the present mechanism |
| duplicate prevention | `MagnetRecognizer.h:224-236` | **no** — the 500 ms guard does this, not closure |
| detector re-arm | `:250` | **no** — a refractory or an entry-threshold re-arm does this |
| navigation advance | `NAVI_ONE.ino:1271` | **no** — downstream of the verdict |
| baseline adaptation | `:346-374`, `.ino:1283` | **inverted** — closure *bounds* contamination; see §6 |

Only the duration floor inherently requires the present definition. Everything
else is "the arc is over", and "the arc is over" does not have to be spelled
"the signal came back to a number we remember from boot".

## 3. When is the evidence actually sufficient?

79 `diag/waveform` messages decode to 55 passages, 52 complete. For each, the
earliest point at which each input becomes final:

```
clean single passages (accepted MAGNET, 85-260 ms), n = 21

  amplitude test decidable      2 -  12 ms   (median   8)
  polarity confident           23 -  33 ms   (median  27)
  running peak reaches final   13 -  88 ms   (median  58)
  ------------------------------------------------------
  all three satisfied          13 -  88 ms   (median  58)
  CURRENT CLOSE                87 - 222 ms   (median 122)
```

*Amplitude* is decided almost at open, because the entry margin (70) already
exceeds the amplitude floor (0.34 × gain ≈ 60). A passage that opens has
essentially already passed the amplitude test.

*Polarity confidence* is measured non-circularly, as decision 0064 frames it:
the earliest point at which the running signed sum reaches 20× the largest
single sample seen so far. The artifacts that decision exists to refuse (+41,
−43) sat against final sums of −12,691 and +19,742; on this corpus the final
ratio is 60–144× and the 20× threshold is crossed at **23–33 ms**, which is
9–15% of the passage.

**The decision is complete at roughly 45% of the current passage duration.**

One honest qualification: *"the running peak reached its final value"* is an
oracle. Online you do not know the peak is final until the signal falls away.
Declaring it requires either observing the decline (the operator's N-ms-below-
peak rule, `docs/NAVI_PEAK_CLOSE_REPLAY_20260913.md`) or simply waiting a fixed
time — which is §4.

## 4. Closure is not necessary: the fixed-window result

Measure peak and polarity in a **fixed window from detection**, with no closure
test of any kind:

```
window    clean singles (n=21):  peak differs   polarity differs
 200 ms                                 0               0
 300 ms                                 0               0
 400 ms                                 0               0
```

**Exact reproduction.** The tail contributes nothing to either quantity, which
is what §3 predicts: the peak is past by 88 ms and the sum's sign is settled by
33 ms.

It holds where it matters:

```
                     full peak   window(300ms) peak
 SINGLE+tail 1348        220           220      tail discarded, unchanged
 SINGLE+tail 1294        227           227      tail discarded, unchanged
 MERGE 1443              222           159      measures the FIRST magnet, not a blend
 MERGE 1852              205           204      measures the FIRST magnet
 low-speed pwm42 431     134           134      unchanged at low speed
 low-speed pwm42 672     137      128 (137@400) unchanged at 400 ms
```

The merges are the point. Today the 1443 ms merge reports a single peak of 222
— the *second* magnet's — and a polarity computed over both. A 300 ms window
reports 159, the first magnet's, uncontaminated. **The window does not merely
tolerate the merge; it prevents it.**

One failure: the 3652 ms merge opens on a low shelf, so a 300 ms window
measures peak 76 (ratio 0.43) rather than the magnet at 1,008 ms. That is a
weak accept. It is not a regression — the present system accepted that passage
too — but it is not fixed either.

So: **no, a closure test is not necessary.** Peak, polarity and timestamp are
all obtainable without one.

## 5. …but closure is not what broke. The reference's AGE is.

This is the measurement that reframes the question. Using only **clean** shadow
samples (§6), how far does the resting level move over a lag *L*?

```
   lag        n     p50    p90    p99    max      (absolute counts)
    1 s    1123       0      2      6     20
    2 s    1183       0      2      9     22
    5 s    1150       1      4     13     37
   30 s    1037       2     12     22     35
   60 s     984       3     13     22     35
  120 s     938       5     13     27     40
```

**A reference one second old is wrong by ≤6 counts at p99. The exit margin is
25.** A reference two seconds old is wrong by ≤9. A reference that is *never*
refreshed — X17 and X18's `fixedAfterPrime` — accumulates the whole excursion:
clean offsets on the day ran **−53 to +54**, a 107-count span.

The closure test was never mis-specified. 25 counts is a correct noise margin
against a reference of the age the test assumes. X17 and X18 changed the age of
the reference from ~1 second to ~40 minutes and left the margin alone.

**This is why no baseline *predictor* helps, and the data says so directly.**
The p99 at 30 s and at 300 s are the same (22 and 21). The level does not trend;
it wanders and reverses. Knowing where it was tells you almost nothing about
where it is — beyond a few seconds, extra history adds no information. X18's lap
controller requested corrections of `-29 -21 ... 12 13 19` and **saturated
against its ±2 cap on 13 of 23 laps (57%)**. The estimator was right; the
authority, and the premise that the target is predictable, were wrong.

## 6. Shadow: classified before use

Shadow is a 41-sample rolling median and is not automatically ground truth.
Every alert sample classified by what the median could contain at that instant:

```
  recent_passage   4358   42.1%   magnet samples may lie inside the 1.025 s window
  stale_held       3603   34.8%   pwm <= 24: mayAdapt false, median frozen
  clean            1696   16.4%   moving, no passage open or recently closed
  open_frozen       625    6.0%   passage open < 2 s: median held
  contaminated       64    0.6%   passage open > openMigrateMs: magnet entering the median
```

Only the 1,696 clean samples are used as evidence anywhere in this report. For
contrast, the extremes come from the excluded classes: the most negative
non-clean reading is **−134** (11:32:05, contaminated, 13,126 ms hand-pushed
passage) and the most positive is **+97** (11:29:42, contaminated). Neither is a
resting level; both are a magnet being read as one.

That 0.6% is the `openMigrateMs` expiry already reported as finding (a) in
`field-records/20260915_OTTO_HALL_ZERO_SHIFT.md` §4 — and it is the reason the
reference was frozen in the first place. **The dilemma is real but false:** an
adapting reference is corrupted by a parked magnet; a frozen one ages out. The
resolution is neither, it is *gating* — take median samples only while moving
and while no passage is open, without the 2-second expiry that currently lets
the magnet back in. Under that gate 93.4% of samples remain available, so a
41-sample median fills in ~1.9 s instead of ~1.0 s. Still 15× faster than the
drift it must track.

## 7. A correction to the 2026-09-15 field record

`field-records/20260915_OTTO_HALL_ZERO_SHIFT.md` §1 offers an independent check:

> The firmware believes MM031 (North); the frozen shadow reads 51 counts
> negative, which is a South magnet under the sensor.

**That inference does not hold.** The complete log (only on the Pi) shows the
−51 was established at **13:20:56**, and held while the locomotive was *moving
at PWM 91 across MM027 → MM031*:

```
13:20:55  base=1964  shadow=1962  delta=  -2   pwm=91  moving=1  mm=26
13:20:56  base=1964  shadow=1914  delta= -50   pwm=91  moving=1  mm=27   <- 48 counts, one sample
13:20:57  base=1964  shadow=1913  delta= -51   pwm=91  moving=1  mm=28
...
13:21:06  base=1964  shadow=1913  delta= -51   pwm= 0  moving=0  mm=31   <- freezes here
```

A value present four markers earlier, at full speed, cannot be a magnet under
the sensor at MM031 — and a *moving* median cannot read a magnet anyway, which
is the design rationale quoted in `HallCapture.h:312-317`. This is the zero
stepping, exactly the bistable behaviour the field record itself documents in
its §2. The mechanism section is right; this particular check reads a zero shift
as a magnet. The −51 then sat frozen and unchanging for 95 s of `pwm=0` to the
shutdown — a textbook `stale_held` sample.

## 8. Survivorship: independent evidence about what was missed

Accepted passages cannot prove detection was complete. Independent check:
**a missed magnet doubles the open-to-open interval.** Marker spacing is
280–355 mm (median 300, `ROUTE_SPACING_MM`, 171 markers, 52.15 m circuit), a
±13% spread, so a 2× interval is separable. Normalising each interval by the
surveyed span actually traversed and comparing to the local median of the
surrounding eleven:

```
  boot 6:  649 advances   intervals > 1.6x local median: 2
  boot 7: 3436 advances   intervals > 1.6x local median: 3
  both  :                 intervals < 0.6x local median: 0
```

Of those five, three are the acceleration from a standing declaration
(12:09:51, 12:09:53, 11:51:53 — PWM still ramping). The remaining two are
**12:06:21 (the 1443 ms merge) and 13:20:55 (the 1294 ms passage beside the
76 ms floor rejection)** — the two events that produced the two strikes.

So across **4,085 advances in the two good sessions, the timing evidence finds
exactly two lost magnets, both at the known failure points, and zero false
extra events.** Detection was otherwise complete. This is the strongest
available statement that problem A — transient flux — was not an active failure
mode on September 15: with position known, the recognizer refused **4** passages
all day, against 4,229 accepted. The two sustained low-amplitude shelves (25 s
and 111 s, peaks 54 and 56) both occurred in boot 3 *after* it was already
struck.

## 9. How wrong can Fixed become before DETECTION fails?

Joining each accepted passage to a clean shadow sample within 3 s (n = 1,711)
and inverting `measured_peak = A + s·E` for pole sign `s`:

```
  inferred true amplitude A:  p0.5 123   p1 128   p5 146   p50 174   p95 225
```

Detection needs the measured peak to clear the entry margin of 70. For the pole
opposed to the offset, `measured = A − |E|`, so a magnet is lost once
`|E| > A − 70`:

```
  median magnet   A=174   ->  lost once |E| > 104
  p5              A=146   ->  lost once |E| >  76
  p1              A=128   ->  lost once |E| >  58
  weakest credible A~102  ->  lost once |E| >  32     (3 of 1711 below A=120)
```

Against: **closure fails once |E| > 25**, and observed clean |E| ran −53..+54.

So for a typical magnet the global reference has roughly **4× more headroom for
detection than for closure** (104 vs 25). For the weakest 1%, the two limits
converge (32–58 vs 25) and the headroom is small. The day's offsets defeated
closure constantly and came within a few counts of defeating detection of the
weakest magnets.

**This is the quantitative form of the working hypothesis, and it holds — but
less comfortably than "detection is fine, only closure is fragile" would
suggest.**

## 10. What this implies, without choosing for you

Two routes are supported by the same evidence. They are not alternatives so much
as different places to spend the same insight.

**Route A — keep closure, stop freezing the reference.** §5 says a reference
≤2 s old is inside the 25-count exit margin at p99. §6 says the gate needed to
keep magnets out of it is the existing motion gate plus an *unconditional*
open-gate (dropping the `openMigrateMs` expiry). This is a **smaller** change
than X18's lap controller: it deletes a mechanism rather than adding one. It
requires no change to the 82 ms floor, to polarity, to orientation, or to the
recognizer, and every existing empirical justification survives untouched.

**Route B — keep the reference global, delete closure.** §4 shows a fixed
window reproduces peak and polarity exactly, the 500 ms guard already does the
counting (§8: zero false extras in 4,085 advances), and re-arming can key on the
*entry* threshold rather than a separate exit margin — which removes the
hysteresis gap between 25 and 70 where the offset currently hides.

Route A is smaller and touches nothing empirically justified. Route B is more
robust in principle and is the only one that survives an offset beyond ~54
counts. **They are independent and could be done in either order**; doing A
first would make B testable under a healthy reference, which is the condition
under which B's remaining unknowns (§11) are actually measurable.

Neither is "another baseline controller", and the data in §5 argues specifically
against building one: beyond a few seconds, history carries no predictive value.

## 11. Demonstrated / hypothesis / untestable

**Demonstrated by this data**
- Navigation reads exactly three scalars from a passage (source-traced, §1).
- `TwoSided.h` is not compiled; there is no sorting/morphology stage (§1).
- Decision is complete at ~45% of passage duration; polarity at 27 ms (§3).
- A fixed window reproduces peak and polarity exactly, n=21, 0 differences (§4).
- The zero moves ≤6 counts/s at p99 and ≤27 counts/120 s; it does not trend (§5).
- X18's controller saturated on 57% of laps (§5).
- Shadow provenance, and that the ±97/−134 extremes are contaminated (§6).
- The field record's 13:21 "independent check" is unsound (§7).
- Exactly two magnets lost in 4,085 advances; zero false extras (§8).
- Detection headroom 104 counts (median magnet) vs closure 25 (§9).

**Hypothesis, consistent with but not proven by this data**
- That a correctly-gated local median would have held the offset inside 25
  counts all day. §5 measures the *drift rate*, which bounds the error; it does
  not replay a controller that was never running.
- That Route B's fixed window generalises below PWM 40. The window held at
  PWM 42 (n=2 passages); there is nothing slower with raw samples.
- That the 3652 ms merge's weak-shelf opening (§4) is rare.

**The telemetry cannot answer**
- *Opening.* There is no continuous raw stream. The 52 recovered waveforms all
  begin at the existing detector's open, and the 155 departure records are 10 Hz
  — too coarse for a 150 ms passage — covering 5 episodes in one boot. **No
  replay in this report tests acquisition coverage.**
- *The floor rejections.* A rejection is deliberately not a `Passage`
  (`HallCapture.h:73-81`) so no waveform is published. All 24 are known only by
  their scalars; the 13:20:55 starvation cannot be replayed.
- *Sub-event polarity.* Stored samples are already oriented by the parent
  passage's polarity, so split-event poles are inherited, not independently
  measured.
- *Slow running and station dwell.* 4,204 of 4,229 accepted passages (99.4%)
  were at PWM ≥ 80; 21 at 60–79, 4 at 40–59, none below. Every low-speed
  passage with raw samples comes from boot 3's struck, badly-offset period — the
  failure regime itself. **The low-speed sample is drawn entirely from the
  population any fix is meant to repair, so it cannot serve as a control.** This
  is the largest gap in the corpus.
- *Whether the 82 ms floor survives a change of duration semantics.* Decision
  0085 was derived against `close − open`. Any mechanism that redefines duration
  requires the operator to re-derive it; it cannot be carried across.

## 12. The smallest experiment that would falsify this

**Not a policy change. A measurement aperture** — which, per the standing note
of 2026-09-02, is not a behavioural change and does not owe one-change-per-build.

A build identical to X18 in every acquisition, recognition and navigation
decision, which additionally publishes, per passage:

1. the **ungated local median** (the reference Route A would use), alongside the
   fixed baseline and the present shadow, with its sample provenance;
2. where the **fixed window** would have put peak and polarity, and whether that
   differs from what `close()` produced;
3. the **duration distribution** under the window, which is what decision 0085
   must be re-derived against.

One session of the size already achieved (3,436 advances) then answers, over
thousands of passages rather than 21:

- Does a correctly-gated local reference ever leave the 25-count margin? *If it
  does, Route A is falsified and §5's drift bound was not the whole story.*
- Does the window ever disagree with `close()` on a passage where the reference
  was healthy? *If it does, Route B is falsified.*
- What fraction of genuine magnets would fall under 82 ms?

**It must be run with station stops and at least one deliberately slow
section**, because that is the population the September 15 corpus does not
contain, and it is where both routes are least constrained by evidence.

## 13. Provenance and corrections

Source: `david@192.168.68.142:~/NGR/telemetry/runs/9950011_20260915_*.log`,
fetched 2026-09-15 15:18, read-only. The serial port was not touched. Boots
segmented by `alert.uptime_ms` regression. Waveforms decoded from
`diag/waveform` base64 per `WaveformDump.h` (40-byte header,
`<BBBBBBBBHHHHHHffIII`), reassembled by `(openedAtMs, closedAtMs)`.

Corrections made in the course of this work, recorded so they are not repeated:

- *"The pre-roll gives a local, uncontaminated zero."* **False.** At ~300 mm/s
  the 12 ms pre-roll sits on the foot of the arc — clean passages enter it at
  40–60 counts and close at 14–25. It measures the magnet, not the track. A
  local reference needs a window of hundreds of milliseconds, taken before the
  arc begins, not the existing pre-roll.
- *"A fraction-of-peak close is the simple answer."* It is not: the offset (to
  68 counts) and a weak magnet's body (peaks 98–112) are the same order of
  magnitude, so no scalar fraction separates them.
- *"Quiescence — close when the signal stops changing — is baseline-free and
  therefore right."* It discriminates well in principle (magnet apex varies ≥52
  counts per 80 ms; inter-hump plateau 8–16) but **shatters every low-speed
  passage**, 2–7 events each. Falsified.
- *"Markers are roughly 1 m apart."* They are 280–355 mm, median 300. The margin
  between footprint and spacing is ~10:1, not ~30:1 — which matters to any
  distance- or refractory-based scheme.
- *"The archive is the complete day."* It was cut at 13:48:34, 95 s before
  shutdown. §7 depends on the missing tail.
