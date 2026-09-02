# Finding 14, at the root — and a fix that was written, measured and withdrawn

**Date:** 2026-09-02
**Branch:** `agent/toby-1-13-flash`
**Locomotive:** Toby (9950012)
**Builds involved:** `NAVI_ONE_1_0X_FIELDTEST` (the two refusals), `1_0X2`, `1_0X3`, `1_0X4`
**Status:** ANALYSIS. No firmware change to the recording path was kept. The
operator's ruling is required on what comes next.

---

## Why this exists

The operator's instruction, after the third Arches incident:

> Agreed. Shut down when dwell on magnet is a weed that will grow again and
> again if not eradicated at the root.

This is the search for that root. It found one — and then found that the
obvious fix for it costs more than the fault does.

---

## 1. What actually happened at Arches, from the telemetry

Three clockwise Arches departures produced stitched waveforms on 2026-09-02.
All three are the same event and all three are at MM110, the first marker past
the platform:

| time | mm | paused | peak | gain | residual | verdict |
|---|---|---|---|---|---|---|
| 10:52:50 | 109→110 | 1,600 ms | 138 | 218 | **0.1967** | REFUSED, session stopped |
| 11:03:54 | 110→111 | 4,126 ms | 143 | 217 | 0.1223 | accepted |
| 11:11:04 | 109→110 | 4,075 ms | 140 | 218 | **0.1818** | REFUSED, session stopped |

`gap_ms` on the refused pair is 46,906 and 67,251 — far longer than a dwell.
**Toby was not parked on the magnet.** He rested clear of MM110, departed, entered
the field, lost traction *inside the arc*, and the wheels spun for 1.6 s and
4.1 s before they caught. `paused_ms` is the slip, not the dwell.

This corrects the shorthand the incident has been carried under. The condition
is **a departure that slips and catches while crossing a marker**, and it has
nothing to do with dwelling on one.

### The splice is visible in the record

The refused waveform of 10:52:50, samples 96–115 (decimation 2, so 2 ms apart):

```
80 78 79 78 82 82 82 82 | 138 139 136 138 137 136 134 133 131 133 133 132
                        ^ +56 counts in one 2 ms sample
```

The median adjacent step in that record is **2 counts**. A 56-count step at
7.6 counts/mm is 7.4 mm of track in 2 ms — **3.7 m/s**. Toby does not do 3.7 m/s.
The step is a splice artifact, not a measurement.

### How much arc is missing

Fitting a Gaussian independently to each side of the splice:

| side | apex implied | speed | end of the side |
|---|---|---|---|
| rising (pre-slip) | 156 counts | 49 mm/s | x = −16.9 mm |
| falling (post-catch) | 140 counts | 164 mm/s | x = +2.5 mm |

**19.4 mm of track is missing, straddling the apex** — the stitch resumes
2.5 mm *past* the top. And the two sides disagree on both apex and speed: the
locomotive left the magnet **3.3× faster than it entered it**.

---

## 2. The defect in the code

`HallCapture.h` excises the stationary interval at both ends. The two ends do
not use the same reference point.

**Pause side** (`pauseMeasurement`): cuts at `progressFromN_`, which is `n_` at
the **oldest entry** of the 16-sample plateau window — the *start* of the window.

**Resume side** (`resumeMeasurement`): reaches back `nowMs - lastFlatMs_`, and
`lastFlatMs_` is the timestamp of the **last flat window** — its *end*.

The comment claims these are "the same test, and the same threshold". They are
the same test read off opposite ends of the same 400 ms window. The gap is real
arc.

It matters most exactly when it should matter least. A trailing window notices a
*fast* departure later, in millimetres, than a slow one. Measured in the rig,
crawl 49 mm/s into a 1,600 ms stall, then a catch:

| catch speed | arc never stitched back |
|---|---|
| 60 mm/s | 2.9 mm |
| 100 mm/s | 2.3 mm |
| 140 mm/s | 3.2 mm |
| 164 mm/s | 3.8 mm |
| 200 mm/s | 4.6 mm |
| 260 mm/s | 6.0 mm |

**Worse the harder the catch.** That is backwards, and it is a genuine defect.

The 512 ms `stitchBackMaxMs` ceiling was never reached in any of these — the
reach-back ran 100–250 ms. The earlier suggestion that the cap was the mechanism
is not supported.

---

## 3. The fix that was written

Both excisions were changed to be **read off the record at the moment resumption
is confirmed**, rather than taken from a detector that necessarily lags:

- walk the full-rate ring backwards from confirmation; every sample outside a
  ±`settleSpan/2` band around the plateau level is arc; a quiet stretch longer
  than 100 ms is the locomotive standing still, and the walk stops there;
- the same band test trims the pause side forward from the window rewind.

The 100 ms hold is not arbitrary — it has to outlast a band crossing (13 ms at
260 mm/s, 55 ms at 60 mm/s) and fall short of the stationary tail left in the
512-sample ring (190 ms and up).

### It did what it was designed to do

Over 32 slip-and-catch departures (stop points −24 mm to +14 mm, catches 60 to
260 mm/s):

| | 1.0X3 | with the fix |
|---|---|---|
| splice step | 25–102 counts | **11–27 counts** |
| lost the apex | 2 of 32 | **0 of 32** |
| arc never stitched back | 2.3–6.0 mm, growing with catch speed | **~1.0 mm, flat** |
| **refused** | **20 of 32** | **18 of 32** |

The record became markedly more faithful. **The verdicts barely moved**, and in
individual cells they moved both ways.

---

## 4. Why it was withdrawn

Gate 12 failed it, twice, and both failures matter.

**1. Finding 13's clean waveform stopped being accepted: 0.1271 → 0.1507.**
That is the operator's own first acceptance condition for this build.

**2. Section E — a false accept.** *"An electrical step with a slow leading edge,
then parked"* is one of the four artifacts the first design accepted and this one
exists to reject. Under the fix it scores 0.1196 and is judged **MAGNET**. It
advances the marker count.

That is the trade laid bare: **a rare unnecessary stop, exchanged for a rare
silent false advance.** On this railway that is the wrong way round. An
unnecessary stop is visible, annoying and safe. A false advance is invisible and
corrupts position — the failure class decisions 0064 and 0065 exist to prevent.

`HallCapture.h` was reverted to the flashed baseline. The analysis stands; the
code does not.

---

## 5. What the field says, and where it contradicted the rig

459 shape-tested passages on 2026-09-02, bucketed by how far past a station stop
they were taken:

| markers past a station stop | n | median | p90 | max | over 0.13 |
|---|---|---|---|---|---|
| 0 | 11 | 0.0676 | 0.0771 | 0.0796 | 0 |
| **1** | **30** | **0.0743** | **0.1818** | **0.1967** | **4** |
| 2 | 9 | 0.0695 | 0.0805 | 0.1223 | 0 |
| 3–8 | 60 | ~0.070 | ~0.078 | 0.0862 | 0 |
| 9 or more (ordinary running) | 349 | 0.0684 | 0.0755 | 0.0830 | 0 |

Every value over 0.13 all day is one of the three stitched Arches passages.

**This refuted a conclusion I had drawn from the rig.** A rig control in which
the speed changed 4× *instantaneously with no stop in it* was refused 14 times in
28, which suggested the shape test simply cannot survive a departure. The field
says otherwise: 30 passages taken one marker past a station — every one of them
crossed under hard departure acceleration — sit at a median of 0.0743 against
0.0684 for ordinary running. **Ordinary acceleration costs nothing.** My control
was modelling an instantaneous velocity step, which is a slip-catch, not an
acceleration. The field data is right and the rig control was a strawman.

---

## 6. What is NOT established

**I cannot reproduce the field's numbers.** The best rig reconstruction of the
10:52:50 event — stop at −17 mm, 49 mm/s in, 164 mm/s out, both read off the
waveform itself — produces 0.10 on the flashed build and 0.12 with the fix. The
field produced 0.1967.

The reason is circular and unavoidable: **the speeds were fitted from the damaged
record.** The arc that would tell me the true speeds is the arc that was thrown
away. Reconstruction from a mutilated waveform cannot recover what mutilated it.

The instrument that settles this is **an undecimated dump of every stitched
waveform, accepted or refused** — recommendation 2 of finding 14, still not
implemented. The build currently dumps only refusals, so the one stitched
passage that *passed* (0.1223) was never captured, and it is the most
informative of the three.

---

## 7. Where it stands after today's track work

The operator went to Arches twice with a shovel:

> I did some track work at arches. Elevated the track so that departure is less
> uphill.

> I made additional track work at arches. Dip removed. Train no longer spinning.

The stop point moved three times to follow it: +1 → +2 → +1 → 0.

**The conditions that produce finding 14 have been removed from Arches. The
fault has not been removed from the firmware.** A departure that slips and
catches anywhere on the railway still splices a waveform, and the shape test
will still refuse some of them. Arches was where the grade made it happen; it is
not the only place it can.

---

## 8. Recommendations, for the operator's ruling

1. **Dump every stitched waveform, accepted or refused.** Cheap, no behaviour
   change, and it is the only thing that will settle section 6. Nothing further
   should be attempted on the recording path until there is one clean capture.

2. **Do not pursue the reach-back fix as written.** It is measured and it is not
   worth its cost. If it is revisited, it must be revisited together with
   whatever keeps section E's artifacts refused.

3. **The judging question, which is the operator's and not an engineer's.** A
   stitched waveform is a real magnet measured in two sittings at two speeds. The
   options are:
   - **(a) Judge the two halves separately.** Each side is recorded at a constant
     speed and is individually a clean Gaussian half. Fit each against the
     unchanged recognizer and the unchanged ceiling, and require both to agree on
     amplitude and polarity. Nothing is loosened; the test is applied twice
     instead of once to a waveform that is not one passage.
   - **(b) Refuse, but do not stop.** Advance zero, keep the declaration, raise a
     flag. This is a change to the operator's own field-test condition 4 and
     accepts position drift in exchange for a running railway.
   - **(c) Leave it.** Accept that a slip-catch over a marker costs a session,
     and treat it as a track-maintenance signal — which, at Arches today, is
     exactly how it was treated, and it worked.

   **(a) is the recommendation.** It is the only one of the three that eradicates
   rather than tolerates, it moves no threshold, and it follows directly from
   decision 0070's own principle: if a passage may not span a stop, then what
   spans one is not a passage and should not be judged as though it were.

---

## 9. Standing corrections to the record

- The condition is a **departure slip**, not a dwell on a magnet. `paused_ms` of
  1.6 s and 4.1 s is wheel-spin; `gap_ms` of 47 s and 67 s shows the dwell was
  over before the passage opened.
- The 512 ms stitch ceiling is **not** implicated. Measured reach-back was
  100–250 ms in every case.
- The rig control suggesting departure acceleration alone breaks the shape test
  is **wrong**, and the field data is what showed it.
