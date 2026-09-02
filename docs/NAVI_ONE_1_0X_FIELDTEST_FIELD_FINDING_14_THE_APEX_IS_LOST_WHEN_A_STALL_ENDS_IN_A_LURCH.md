# Field finding 14 — the apex is lost when a stall ends in a lurch

**Build:** `NAVI_ONE_1_0X_FIELDTEST` (`field_accepted: 0`), booted 10:44:49.
**Date:** 2026-09-02, Arches CW departure, 10:52:50.
**Reported by:** the operator — *"Stopped normally at Arches. When leaving noted
to be spinning then caught, cars moved. then stopped. Now says position not
declared. Dashboard does not show disagreement."*
**Evidence:** `~/ngr-telemetry/pi/NGR/telemetry/all_20260902.log` lines
95484–95500. The waveform below is the one the firmware published.

---

## The short version

**The build did exactly what it was told, and it caught a real thing.** It
refused a waveform that was genuinely not a complete rise-and-fall, published
the whole waveform, advanced nothing, and stopped. No marker was miscounted, no
polarity was contradicted, and the dashboard is right to show no disagreement —
the ruling is `UNRESOLVED_INTERRUPTION`, not `WrongMagnet`.

**But the refusal has a cause worth fixing.** The stitched waveform is missing
its apex. Not attenuated — *missing*, with a 56-count step where the two halves
were joined.

## The session

| | |
|---|---|
| AGREE | 99 |
| STITCHED_REFUSED | 1 |
| NOT_A_MAGNET | 1 (the same passage, published first) |
| strikes / disagreements | **0** |
| stitched waveforms accepted | 0 — no other pause occurred all session |

99 markers counted cleanly, then one stop. Note that no station dwell ever
paused a measurement, which is what the CW landing prediction expected: at the
measured coast every CW station rests 96–136 mm clear of a marker. The only
pause all session came from the **departure stall**, not from a dwell.

## What was published

```
STITCHED WAVEFORM REFUSED at MM109: ... (WRONG_SHAPE, resid 0.1967,
ratio 0.633, paused 1600ms)

{"event":"STITCHED_REFUSED","mm":109,"tgt":110,"dir":"CW","nav_state":"STRUCK",
 "peak":138,"ratio":0.633,"resid":0.1967,"shape":1,"gap_ms":46906,"gain":218,
 "stitched":1,"paused_ms":1600,"trust":"PROVEN","adv":99,"ref":1,"notmag":1}
```

Both dumps fired as designed: the **immediate single-slot refusal dump**
(`slotTotal 1`, line 95484 — new in this build) and the six-slot trailing window
from `withdraw()` (lines 95495+).

## The waveform, decoded

179 samples, decimation 2, so each sample is 2 ms. Wall clock 2,297 ms, of
which 1,600 ms paused — 697 ms of motion, but only 358 ms of it recorded.

```
  ...
   90.. 99    75   75   76   78   77   77   80   78   79   78
  100..109    82   82   82   82  138  139  136  138  137  136
                            ^^^^^^^^^  a +56 step in one 2 ms sample
  110..119   134  133  131  133  133  132  129  126  125  124
  ...
  170..178    34   30   26   27   24   22   18   18   18
```

The median adjacent step across the whole arc is **2 counts**. The step at
sample 103→104 is **+56**.

Read it plainly: the arc rises normally to 82, goes dead flat at 82 for four
samples — the plateau that fires the pause — and then **resumes at 138 already
descending**, decaying smoothly to 18.

**Compare the same magnet crossed cleanly 47 seconds earlier** (window slot 1):

| | this crossing | the healthy one |
|---|---|---|
| peak | **138** | **227** |
| ratio | 0.633 | 1.041 |
| residual | 0.1967 | 0.0600 |
| apex position | 59 % through | 53 % through |
| outcome | WRONG_SHAPE | MAGNET |

Peak field does not depend on speed. 138 against 227 on the same magnet means
**the top 145 counts of the arc were never recorded.** He crossed the apex
during the pause, and the stitch rejoined on the far side.

## Why — leading hypothesis, from the code

Field evidence first: the step, the missing apex, and the 339 ms of moving time
absent from the samples (697 ms moved, 358 ms recorded) are facts. The mechanism
below is read from the source and is **not yet demonstrated**:

`lastFlatMs_` — the anchor the stitch reaches back to
(`HallCapture.h:538`, `back = nowMs - lastFlatMs_`) — is set by the progression
detector, which tests a **16-deep ring at 25 ms, i.e. a 400 ms window**
(`HallCapture.h:404`). When the locomotive catches and lurches, that window
still contains mostly-flat samples, so **`lastFlatMs_` keeps advancing for up to
~400 ms into the movement.** The stitch therefore reaches back from an anchor
that is already several hundred milliseconds *into* the lurch — and the apex,
crossed during exactly that interval, is not restored.

This is the same 400 ms plateau lag that was already dealt with on the *pause*
side, where the rewind to `progressFromN_` excises the flat window. **The resume
side has the same lag and no corresponding correction.** The two ends are not
symmetric after all.

A secondary bound: the full-rate ring is `kResume = 512` entries — 512 ms — and
`stitchBackMaxMs` is also 512. A pause of 1,600 ms means anything older than
512 ms is gone regardless. The ring cannot be the whole story here (a lurch
through the apex is far shorter than 512 ms) but it caps any fix.

**This capture cannot settle it.** What was published is the *recorded* waveform,
after the stitch. The raw stream during the pause was never transmitted, so this
cannot be replayed. What would settle it is a synthetic gate case: pause on the
rising flank, then lurch through the apex, and check whether the apex survives.

## Secondary observation — a one-sample dropout

```
   40.. 49    47   51   54   54   54   55   54   56   53   -5
   50.. 59    58   57   59   56   58   58   59   59   59   59
```

Sample 49 reads **−5** between 53 and 58: a single 2 ms excursion of −58 then
+63, in an otherwise smooth rise where the median step is 2. That is an
electrical or ADC artifact, not field. It did not cause this refusal — it sits
low on the rising flank — but it is in the judged samples and it does enter the
residual. Worth watching for; a second instance near a decision boundary would
matter.

## What this says about the design

- **The safety property held completely.** Partial evidence was not accepted.
  A magnetic level was not mistaken for a magnet event. 99 advances, zero false
  accepts, zero strikes.
- **The availability property did not.** A marker that was physically crossed
  went uncounted, and the session ended.
- The refusal is **correct given what was recorded**. The waveform genuinely is
  not a coherent rise-and-fall. The fault is upstream, in what was recorded.

## Recommended, not done

1. Make the resume side symmetric with the pause side: anchor the stitch to the
   **start** of the plateau window rather than to `lastFlatMs_` as it stands,
   so the ~400 ms detection lag is excised at both ends.
2. Add a gate 12 case: pause on the rising flank, resume with a lurch through
   the apex. It should either recover the apex or refuse with a reason that
   *says* the apex was not observed, rather than a generic `WRONG_SHAPE`.
3. Consider reporting this case distinctly — paused on a rise, resumed on a
   fall, with the resumed level above the paused level, means the peak was
   crossed unobserved. That is a diagnosis, not a shape complaint.

**No firmware has been changed. These need the operator's ruling.**
