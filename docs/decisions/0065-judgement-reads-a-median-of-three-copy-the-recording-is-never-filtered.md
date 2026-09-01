# 0065 — Judgement reads a median-of-three copy; the recording is never filtered

Status: **Accepted, and it stays**  (2026-09-01)

Operator's ruling, 2026-09-01, after the second field trial: *"0065 stays. If the
problem returns, it will be examined. If other problems occur, it will be
reviewed as will all other edits."*

It remains in service on that footing — conditionally, and subject to review
alongside every other edit if anything else goes wrong. It is not closed.

**History of this record, in order.** Implemented and flashed on 2026-08-31.
Three stops followed and the operator directed a rollback — "Roll it back to 04.
This dog don't hunt." The firmware returned to 0.4 and this record was withdrawn.

The cause of those stops was then identified and is **not** this change. A
sustained DC offset of 26 counts or more latches a passage permanently open:
`updateBaseline()` will not sample while a passage is open, so the offset that
holds it open is the one thing that cannot be measured away, and every magnet of
the same polarity as the offset is then invisible. Finding 08 holds the evidence,
and gate 8 reproduces it on **0.4's own code**, with results identical line for
line to 0.5's — the boundary at −24 clean, −26 latched in both. Acquisition does
not differ between the builds.

What produced the offset that evening is still unknown. The locomotive had stood
for two hours; a later five-minute stand on 0.4 produced no offset at all.

**Second field trial, 2026-08-31 19:03, passed.** 193 advances, no strike; the
only two refusals were `TOO_SOON` rebounds at peaks 41 and 29. Accepted peaks
152–293, median 206.

Decisively, **MM169 — the passage this decision exists for — crossed as an
ordinary advance**: `ADVANCED`, obs N, peak **189**, ratio 0.867, residual
**0.0787**. On 0.4 at 15:16:43 the same marker was refused `WRONG_SHAPE` at peak
313, ratio 1.6828, residual 0.1307. The tail artifact no longer sets the peak or
breaks the fit, and the recording still carries the spike.

Still Proposed: a passed trial is not the operator's ratification.

Reinstated at the operator's direction for a second trial: "If that was a unique
set of circumstances, it should run." Approval to trial is not ratification.

**This build identifies itself.** The first trial reported `NAVI_ONE_0_4` because
the sketch was compiled from an editor buffer predating the `SKETCH_NAME` bump,
and had to be identified after the fact by fingerprinting peak derivation against
captured waveforms. `SKETCH_NAME` is `NAVI_ONE_0_5` here and the boot line must
read `[BOOT] NAVI_ONE_0_5 — 9950012`. If it does not, the trial is not testing
this build.

---

Original record, as written before the first field trial:

Status: Proposed  (2026-08-31)

## Decision

At `HallCapture::close()`, after the pole is settled by decision 0064 and the
buffer oriented, **one** median-of-three copy of the passage is built. From that
copy come:

- `peakCounts`, and therefore `amplitudeRatio`;
- the Gaussian shape fit — its peak, its 20%-of-peak arc, its centre, its width
  and its residual.

`Passage` carries both: `oriented` is **the recording**, and `judged` is the
copy. The recording is never filtered, smoothed, or altered. It is what
`WaveformWindow` retains and what `diag/waveform` publishes, artifacts included.

No established threshold moves: `entryMargin` 38, `exitMargin` 25, `floorMs` 40,
amplitude floor 0.34, residual ceiling 0.13, rebound guard 200 ms — all
unchanged. Decision 0064's whole-passage polarity decision is unchanged, and
still reads the raw signed samples.

Ships as NAVI_ONE 0.5.

## Context

At 15:16:43 on 2026-08-31, running 0.4, Toby refused physical MM169 as
`WRONG_SHAPE` and struck one passage later at MM170. Finding 07 holds the
evidence.

The passage body is an ordinary bell peaking at 185. At sample 165 of 175, in
the decaying tail, between a +17 and a +21, one sample reads **+313** — 294
counts off the mean of its neighbours. Two consequences, both because a single
reading was allowed to decide:

- `peakCounts` was a running maximum, so it became 313 rather than 185, and the
  ratio was published as 1.6828 against a neighbourhood of 0.99–1.28. That false
  peak also enters `gains_[]`, the median every later passage is measured
  against.
- `fitResidual` derives its own peak the same way, took the arc threshold as 20%
  of 313, and then fitted a bell carrying a 313-count spike ten samples from the
  end where the fitted Gaussian is nearly zero. Residual **0.1307** against a
  0.13 ceiling. It failed by 0.0007.

This is the same artifact class as findings 05 and 06, which decision 0064
addressed at the entry crossing. The landing site decides the symptom: at entry
it flips the pole; in the body or tail it inflates the amplitude and corrupts
the shape. 0064 does not and was never going to address the second.

The operator's ruling: *one reading should not determine the shape* — the same
principle as 0064, applied to a second field.

## Alternatives considered

- **Correct the peak only.** Tested first, and it **does not work**: MM169's
  residual stays at 0.1307. The arc-finder extends through every positive sample
  regardless of the threshold, so the fit window is unchanged and the spike's own
  squared error still dominates the RMS. Rejected on evidence.
- **Trim the single largest squared error from the residual.** Rescues MM169
  (0.1307 → 0.0674) and disturbs nothing else. Rejected because it fixes only
  the shape: `peakCounts` would remain 313, the amplitude would still be
  misreported as 1.68, and the false value would still poison the gain median.
  Two patches where one rule suffices — and "drop the worst sample" is also what
  one would do to hide a genuine defect.
- **Filter the input before the threshold.** Rejected, as in 0064, and now for a
  second reason: it would alter the recording, and the recording is the only
  reason any of this was diagnosable.
- **Move the residual ceiling.** Never on the table. MM169 was a real magnet
  misjudged, not a ceiling that is too tight.

## Consequences

- One more `RING`-sized buffer: **+1,024 bytes** of globals, 58,164 → 59,188
  (17% → 18%). Flash +96 bytes. One O(n) pass at close, n ≤ 512, once per passage.
- **The artifact remains fully visible.** Gate 6 part D asserts this directly:
  the same fit against the recording still returns 0.1307 and still fails. The
  spike is not hidden, only outvoted. Nothing here explains where it comes from.
- Peaks on ordinary passages shift by **0 to 7 counts** — the median trims the
  very tip of a bell. Across 28 captured and 187 survey passages no verdict
  changes but MM169's.
- `peakCounts` is now derived from the stored buffer rather than from every
  sample seen. For a decimated passage — one long enough to halve the buffer,
  which in practice means a station dwell — the peak is read from the decimated
  copy. No captured or surveyed passage is affected.
- Findings 02 and 03 (MM110 residual 0.1422, MM146 0.1811) are **candidates**
  for this mechanism, not confirmed by it. At MM110 the peak (179) and ratio
  (0.856) were both normal, so any artifact there did not become the peak, and
  this change might not have rescued it. Those remain open.
- The physical source of the artifacts remains unknown. Unratified until the
  operator reviews it; approval to implement is not approval to flash.

## Verification

All seven host gates pass. Compiles clean against Toby's core
(esp32:esp32:esp32 3.3.11) with no warnings from NAVI_ONE sources — 966,747
bytes flash (73%), 59,188 bytes globals (18%).

- **gate 6 part D**, new: MM169 replayed through the real `HallCapture` and the
  real `fitResidual`. Against the recording, residual **0.1307 — still fails**.
  Against the judgement copy, **0.0744 — passes**. Peak **184**, not 313.
- **gate 6 part A**: 28 captured passages. 25 nominal keep their pole exactly and
  their peak within 8 counts; the three known artifact cases — MM70, MM119,
  MM169 — are corrected.
- **gate 1**: the 2026-08-28 survey through the real recognizer, now building the
  judgement copy as the firmware does. Every primary accepted, no non-primary
  counted.
- **gate 7**: 187 of 187 untruncated survey passages keep their pole.
- Gates 2, 3, 4, 5 unchanged and passing, including the 172-advance lap replay.

## References

- `docs/NAVI_ONE_0_3_FIELD_FINDING_07_ARTIFACT_IN_THE_TAIL_WRONG_SHAPE_MM169.md`
- `docs/decisions/0064-polarity-is-the-sign-of-the-summed-passage.md`
- `firmware/test-programs/NAVI_ONE/MagnetRecognizer.h` — `medianOfThree`, `fitResidual`
- `firmware/test-programs/NAVI_ONE/HallCapture.h` — `close()`
- `firmware/test-programs/NAVI_ONE/tests/gate_polarity.cpp` — part D
- `field-records/logs/20260831_navi_one_waveforms/captured_passages.tsv`
