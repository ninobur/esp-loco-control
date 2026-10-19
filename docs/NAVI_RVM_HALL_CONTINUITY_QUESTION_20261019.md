# NAVI_RVM: can the Hall tracing alone tell a stopped ensemble from a developing one?

Analysis by Claude (Opus 5.5), 2026-10-19, in answer to David's question
"Hall-Based Recognition of Waveform Continuity". **This is evidence and
assessment only. Nothing has been implemented, no firmware changed, and no rule
is proposed here for adoption.**

## The question (David)

PWM is not a reliable indicator of movement: at MM109 Otto stood still for about
60 s at PWM 32–35. Can NAVI, from the Hall tracing alone (no PWM, IR or speed,
and no independent amplitude or duration gates):

1. recognize that an unfinished ensemble has stopped developing;
2. preserve the preceding baseline and the incomplete structure without the
   stationary plateau;
3. recognize when the waveform resumes and continue the same ensemble?

On a baseline, an unchanged tracing simply stays baseline; this is harmless.

## Evidence

**Script:**
`analysis/navi_rvm_shape_20261018/field_20261019/stationarity_evidence.py`;
outputs in `field_20261019/stationarity/`.

**Measure:** the change a tracing shows over a span τ, defined as the range
(max − min) of the 20 ms block means inside the span.

**Sources:**

| Source | What it is |
|---|---|
| Stationary inside a magnet | Run 8 stall plateaus, every τ-span. MM109: 59 s at about 2,027, PWM 32. MM023: 8 s at about 2,018, PWM 29–36. |
| Stationary on baseline | Boot 1, 58 IR-confirmed stationary spans (no odometer pulse for over 3 s), 1,759 s in total. |
| Moving inside a magnet | The flattest τ-span of each passage. Run 8: 164 recognized cores outside the stalls, PWM about 30–61. Boot 1: 1,089 annotated extents that the IR odometer shows moving throughout (above 9.7 mm/s). |

**Change over a span τ (ADC):**

| τ | Stationary in magnet, run 8 MM109 (median / p99 / max) | MM023 (max) | Stationary baseline, boot 1 (p99) | **Moving in magnet, flattest span: run 8 (min / p1)** | Moving, boot 1 (min / p1) |
|---|---|---|---|---|---|
| 0.2 s | 1.7 / 3.2 / 9.3 | 3.3 | 4.0 | **2.3 / 3.3** | 4.3 / 19.4 |
| 0.5 s | 2.3 / 4.0 / 11.9 | 3.4 | 4.7 | **11.0 / 19.7** | 28.0 / 31.8 |
| 1.0 s | 2.9 / 6.5 / 14.4 | 3.7 | 5.3 | **46.1 / 50.5** | 96.7 / 97.0 |
| 2.0 s | 3.6 / 10.5 / 20.1 | 4.2 | 5.9 | **126.4 / 126.5** | n/a (passages too short) |
| 8.0 s | 6.8 / 14.6 / 26.1 | 5.0 | 8.1 | n/a | n/a |

- **Local noise scale** (the estimator the recognizer already uses,
  √π/2 · mean |Δ|):
  - 0.54 during both stalls;
  - 0.68 (median) on IR-stationary baseline;
  - 0.5–0.9 on moving baseline (run 8 ensembles).
- **Boot-1 stationary maxima** (93–113 ADC) come from a few spans the IR mask
  calls stationary that contain Hall activity. These were not investigated, so
  the p99 is quoted.
- **The approach into the MM109 stall** (0.5 s means):
  - 1893 → 2017 in 2 s (moving);
  - then 2017 → 2030 over 2 s;
  - then about 1 ADC/s to 2034 at 612 s;
  - then wandering back to about 2,022 over the next 50 s.

  **There is no abrupt stop in the Hall.** The development rate decays
  continuously into the stall, and the stationary level then drifts by up to
  14 ADC over a minute.

## What the existing model can and cannot distinguish

**1. The statistical shape model itself cannot.**
- Each window is linearly resampled to 32 points, its principal excursion is
  normalized to +1, and it is scored by 12 DCT coefficients plus log(A/noise).
- By construction it does not know the rate at which a shape develops. A window
  that is half plateau is simply a distorted shape, which the model rejects
  (MM109 as run: never recognized).
- It scores complete windows; it has no notion of "still developing" or
  "stopped".
- This is intended: duration and speed invariance is why it works from PWM 30
  to cruise.

**2. The tracing contains the information, but only over a time span.**
- **At 1 s the recorded classes separate.** Stationary inside a magnet changes
  by at most 14.4 (p99 6.5); the flattest moving 1 s inside a magnet changes by
  at least 46 (run 8, PWM 30).
- **At 0.5 s they overlap** (11.9 against 11.0). **At 0.2 s they do not
  separate at all** (9.3 against 2.3).
- **The physical reason:** at a magnet's extremum the field gradient is zero,
  so a moving sensor shows almost no change there. Only curvature, which takes
  time, reveals motion.
- So any Hall-only test must look over a span, and **the span is set by the
  slowest development that must still count as moving.** That is a speed limit
  expressed in time. **Hall alone cannot remove that time constant.**
- Its value can come from evidence (the slowest recorded running) rather than
  from a free choice. Whether that is acceptable or counts as a "duration gate"
  is David's decision.

**3. The boundary is not sharp.** The MM109 approach shows a continuum: 10, 6, 2
and 1 ADC/s, then drift.
- A development slower than the stationary drift (up to about 0.5 ADC/s
  sustained, 14 ADC over 60 s here) **cannot be distinguished from
  standstill**, by any method that uses only the Hall.
- Those creeping blocks would be classed as unchanged and withheld.
- In the counterfactual replay, withholding 606–665 s, which included the creep,
  restored MM109–MM113. **MM109's LLR was then only 18.9** (typical 42): the
  remaining shape is a slow approach followed by a fast exit, a non-uniform
  speed inside one window, which the model tolerates only partly.
- Run 3's stops over magnets gave similar margins (17.5–37.5).

**4. Inside a magnet versus on baseline** needs no extra subsystem. The local
baseline line and its noise already define whether the tracing is off-baseline:
- the MM109 plateau sat 134 ADC above the line, about 250 noise units;
- a stall on a magnet's outer flank, a few ADC off the line, is
  indistinguishable from baseline. By David's reasoning, treating it as baseline
  is harmless, although it shifts the local line slightly.

## Within the existing architecture

| David's point | Assessment |
|---|---|
| **2. Preserve baseline and incomplete structure** | **Already exists.** `WaveformPause` withholds blocks while keeping the waveform, local baseline and held candidate exactly. It was proven on the track with three stops over magnets (run 3) and in replay. What changes is only *which signal* says "unchanged": today applied PWM < 15, in the question the tracing itself. |
| **Detection lag** | A Hall test needs a span of τ before it can say "unchanged", by which time τ of plateau has been seen. Those blocks must not already have been appended. The existing hold-back ring plus the newest-first arrival trim (judged against the stationary range) is that mechanism. It would hold the most recent τ continuously instead of only after a commanded stop. **Cost: recognition sees the tracing about τ later** (about 1 s, against the existing half-window acceptance delay of 0.1–4.4 s). |
| **3. Resumption** | **Already Hall-only.** The tracing leaves the stationary range [min, max] observed during the pause and stays out for two blocks on the same side. It worked at three stops over magnets on run 3. At MM109 the exit (2,069 within 1 s) is unambiguous. |
| **4. No PWM, IR or speed** | The decision can be made from the Hall alone. **One time constant remains (τ)**, and the stationary band must cover drift (up to about 14 ADC/min here, not proportional to noise). Both come from Otto's recordings, not from the shape model. |
| **One mechanism for baseline and magnet** | Consistent with the evidence: stationary and moving baselines look alike (noise 0.54–0.68), and withholding or keeping them is harmless either way. |

## What is not known

- **No ground-truth recording of the slowest moving case.** No IR-truth
  recording exists of Otto *crawling* through a magnet slower than PWM 30
  (decelerating into a stall).
  - The 46 ADC/s figure is the slowest *recognized* running, not the slowest
    possible.
  - If Otto can creep through a magnet at a few ADC/s, the classes merge and τ
    would have to grow.
  - In line with policy §8, the way to settle this is a **bounded track test**
    (slow crawls and stalls through magnets with the IR odometer recorded), not
    a synthetic scenario.
- **MM023** is not recovered even with its plateau withheld. The cause is not
  established; its approach overshoots (about 2,139) before settling at about
  2,018.
- Toby has not been measured.

## Answer in brief

- **The shape model itself cannot make the distinction.** It is
  duration-invariant by design.
- **The Hall tracing can, in the recorded evidence, over a span of about 1 s.**
  The existing architecture already has the preserve, trim and resume machinery,
  which is Hall-based except for its entry trigger.
- **What cannot be avoided is a time span** (Hall carries no velocity
  information at a magnet's extremum) **and a stationary band wide enough for
  drift.** Below a development rate of about 0.5 ADC/s, standstill and creep are
  indistinguishable by any Hall-only method.
