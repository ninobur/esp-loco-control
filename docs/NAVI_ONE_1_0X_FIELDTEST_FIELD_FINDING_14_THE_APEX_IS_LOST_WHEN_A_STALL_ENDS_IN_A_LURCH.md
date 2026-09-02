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

**CORRECTION, 2026-09-02, after the second event below.** The first version of
this document compared the refused peak of 138 against 227 — the passage held in
window slot 1, forty-seven seconds earlier. **That is a different magnet, and a
strong one.** MM110's own amplitude, from every clean CW crossing in the two
days before this build was flashed, is:

| | peak | ratio | residual |
|---|---|---|---|
| 2026-08-31, 20 crossings | 168–185 | 0.82–0.87 | 0.069–0.094 |
| 2026-09-01, 10 crossings | 168–184 | 0.83–0.88 | 0.054–0.097 |

**MM110 normally reads about 175, not 227.** The apex loss is therefore about
**35 counts, not 145** — roughly 20 % of the peak, not 64 %. The mistake was
mine and it overstated the magnitude by four times.

What survives the correction, and is the point: at 138 the recorded peak is
**21 % below what this magnet gives every other lap**, the residual is
**0.1967 against a normal 0.085**, and there is a +56 single-sample step where
the halves were joined. The apex was crossed unobserved.

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

## The second event — the same marker, one lap later, the other verdict

At **11:03:54**, one lap on, Toby departed Arches CW again. The operator: *"Some
wheel slippage on restart but progressed without incident this time."* The
telemetry says it was not incident-free — it was the same event with the
opposite verdict:

```
{"event":"AGREE","mm":110,"why":"MAGNET","peak":143,"ratio":0.659,
 "resid":0.1223,"stitched":1,"paused_ms":4126,"gain":217,"adv":171}
```

| | paused | peak | ratio | residual | verdict |
|---|---|---|---|---|---|
| MM110 normally | — | ~175 | ~0.84 | ~0.085 | MAGNET |
| 10:52:50 | 1,600 ms | 138 | 0.633 | **0.1967** | **WRONG_SHAPE** |
| 11:03:54 | 4,126 ms | 143 | 0.659 | **0.1223** | **MAGNET** |

**Both lost the apex.** 138 and 143 are the same waveform class — a fifth below
this magnet's normal amplitude, with residuals two-and-a-half and one-and-a-half
times normal. The second one did not resume cleanly; **it passed by 0.0077 of
residual.** The ceiling is 0.13.

**This is finding 13's margin, realised.** The same event at the same marker
landed on opposite sides of the shape ceiling on consecutive laps. It is no
longer a hypothetical about a reconstructed capture at Arches CCW — it happened,
twice, in CW, an hour after the build was flashed.

Note also what the pause durations say: **the longer pause produced the better
result** (4,126 ms accepted, 1,600 ms refused). Whatever discriminates these two
is not the length of the stop.

### Two things this exposes

1. **There is no test that the apex was observed.** A waveform can satisfy
   amplitude, whole-wave polarity and the Gaussian residual while missing its
   peak, because the fit is global and tolerant of a stitched top. By the
   governing rule — *only a completed, coherent rise-and-fall waveform
   establishes a moving magnet* — an unobserved apex is not a completed
   waveform. The accepted case reached the right answer on evidence the rule
   does not actually endorse.
2. **The accepted case was not published.** The build dumps on refusal only, so
   the deformed-but-accepted waveform — the more dangerous of the two — exists
   only as six summary numbers. Its samples are gone.

## What this says about the design

- **The safety property held completely.** Partial evidence was not accepted.
  A magnetic level was not mistaken for a magnet event. 99 advances, zero false
  accepts, zero strikes.
- **The availability property did not.** A marker that was physically crossed
  went uncounted, and the session ended.
- The refusal is **correct given what was recorded**. The waveform genuinely is
  not a coherent rise-and-fall. The fault is upstream, in what was recorded.

## Recommended, not done

1. **Stop reaching backwards for the samples.** Anchor the stitch to the
   *start* of the plateau window rather than to `lastFlatMs_` as it stands, so
   the ~400 ms detection lag is excised at both ends. CODEX's formulation is
   better still — **retain provisional post-plateau samples until resumption is
   either confirmed or rejected**, instead of reaching back into a ring for
   them. Right recommendation; see the note on its reasoning below.
2. **Dump every stitched waveform, pass or fail.** Today only refusals are
   published, so the deformed-but-accepted case is invisible — six summary
   numbers and no samples. That is the case most worth seeing, and the one that
   advances a marker.
3. **Test that the apex was observed.** Paused on a rise, resumed on a fall,
   with the resumed level above the paused level, means the peak was crossed
   unwitnessed. Report it as that rather than as a generic `WRONG_SHAPE`, and
   rule on whether such a waveform may advance anything at all. Today one did.
4. **Add the gate 12 case:** pause on the rising flank, resume with a lurch
   through the apex. Nothing in the existing gate covers it — section D stops
   and stays stopped, section F stalls but does not lurch through a peak.

**No firmware has been changed. These need the operator's ruling.**

## On CODEX's reading

CODEX is right that a stitched waveform passed on the later departure, right
that wheel rotation does not prove track movement, right that the pause detector
correctly witnessed loss of translation, and right in its recommendation.

Two corrections. **It did not "resume sufficiently cleanly."** Peak 143 against
this magnet's normal 175, residual 0.1223 against a normal 0.085: it is as
deformed as the one that was refused, and it passed on 0.0077 of margin. Reading
it as a clean resumption makes the system look like it works when the catch is
quick; what the numbers show is a coin toss against the ceiling.

**And the 512 ms rewind history is probably not the binding constraint.** In the
refused case only ~150 ms of samples follow the stitch — nowhere near the 512 ms
cap, which therefore never bound. The longer pause of the two is also the one
that passed. The anchor (`lastFlatMs_`, lagged by the 400 ms plateau window)
fits the evidence better than the cap does. Neither is proven: as noted above,
the raw stream during a pause is never transmitted, so only a synthetic gate
case can settle it.
