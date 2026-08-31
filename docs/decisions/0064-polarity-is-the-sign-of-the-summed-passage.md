# 0064 — Polarity is the sign of the summed passage, not of the entry sample

Status: Accepted  (2026-08-31)

Reviewed and approved by the operator, 2026-08-31, for implementation as
described. Approval covers the design and the committed implementation; it is
not by itself authority to flash, which remains a separate go-ahead.

## Decision

`HallCapture` no longer latches a passage's pole from the single sample that
first crosses `entryMargin`. Samples are stored signed and unoriented as they
arrive. At `close()`:

1. every sample of the completed passage is summed, signed;
2. the **sign of that sum** is the pole;
3. the peak is the largest excursion **in that direction**;
4. the buffer is oriented once, and the existing amplitude, shape and rebound
   checks run against it unchanged.

The map is not consulted. `RouteMap` may never inform the pole — a mis-read
passage told what it ought to have been would stop being evidence.

Nothing is refused on ambiguity. No threshold is added, moved, or relaxed:
`entryMargin` 38, `exitMargin` 25, the amplitude floor, and the 0.13 residual
ceiling are all untouched.

## Context

On 2026-08-31 Toby stopped twice in fifteen minutes, at MM70 (10:46) and MM119
(11:01), each time surrendering position. Both markers were physically
inspected and are healthy. Both stops were caused by the instrument.

The `diag/waveform` capture of decision 0063 recorded both. In each, a single
artifact sample at the entry crossing — **+41** at MM70, **−43** at MM119, three
to five counts over `entryMargin` and opposite to the field that was arriving —
latched the pole. Every later sample was then negated on storage, so the real
bell (−183 and +223, both full amplitude for their neighbourhoods) could never
raise `peak_`, which stayed at the artifact's own value. The recognizer saw
peaks of 41 and 43, ruled `TOO_WEAK`, did not advance, and the resulting
one-marker lag was exposed two and three passages later as a polarity strike.

Artifacts of this size are ordinary on this channel: fifteen single-sample
deviations of 30 counts or more across the 3,691 samples captured. They are
harmless inside a passage. They were catastrophic only at the entry crossing,
because that was the one place a single sample decided anything.

The physical source of the artifacts is **not known** and this decision does not
depend on knowing it. Findings 05 and 06 hold the evidence.

## Alternatives considered

- **Keep the entry latch, filter the input.** A median or de-glitch filter ahead
  of the threshold would suppress the artifact, but it also alters every sample
  the shape test sees, and the residual excursions of findings 02 and 03 are
  still unexplained. Changing the signal while an unexplained signal problem is
  open was rejected.
- **Larger excursion wins (`maxPos >= maxNeg`).** Fixes both recorded cases and
  was the first proposal here. Rejected on review: it still lets one sample
  decide, if that sample is the largest. Demonstrated — a single spike twenty
  counts above the true peak defeats it, and gate 6 asserts this.
- **Median, or majority of sample signs.** Both resolve both incidents. Compared
  directly against the summed sign across three candidate windows (whole
  passage, `|v| >= 25`, `|v| >= 20%` of peak) over the 22 captured and 187
  untruncated survey passages: **all three rules, in all three windows, were
  correct on every passage.** Nothing separated them on evidence, so the
  simplest was taken. Median and majority also make the choice of window
  load-bearing — a long near-baseline tail can outvote the arc — where the sum
  does not, since near-zero samples contribute near-zero.
- **Refuse ambiguous passages.** Deferred. No captured passage is bipolar, so
  there is nothing to site a threshold from, and a badly sited one would re-
  reject exactly the passages this decision recovers.
- **Consult the map.** Rejected outright. See above.

## Consequences

- A passage's pole now costs one 64-bit add per sample and one negation pass at
  close, bounded by `RING`. Segmentation is untouched: entry and exit already
  tested absolute magnitude, so nothing about where passages begin or end moves.
- `Passage` gains `signedSum`, the value the judgement was made on. Nothing
  thresholds on it. No MQTT topic or payload format changes.
- **The artifact stays visible.** It is not filtered, smoothed, or suppressed —
  it remains in the stored waveform and in every `diag/waveform` dump. This
  decision stops one sample outvoting a hundred and fifty; it does not hide the
  sample, and it must not be read as having explained it.
- The peak can now only rise relative to 0.3, never fall, so the amplitude gate
  is marginally more permissive for any passage whose pole was previously
  mis-latched. Across 22 captured and 187 survey passages the only changes are
  the two known mis-latches. This is the thing a wider replay should watch.
- Findings 02 and 03 — the `WRONG_SHAPE` residual excursions at MM110 and MM146
  — are **not** addressed. A 46-count mid-arc artifact produced residual 0.0613;
  reaching MM110's 0.1422 needs roughly 200 counts. Different mechanism, still open.
- Ratified by the operator on 2026-08-31, per his ruling of 2026-08-30 that no
  record here has force until he reviews it personally. Flashing remains a
  separate go-ahead.

## Verification

All seven host gates pass, and the sketch compiles against Toby's exact core
(esp32:esp32:esp32 3.3.11) with no warnings from NAVI_ONE sources —
966,651 bytes flash (73%), 58,164 bytes globals (17%).

- **gate 6**, new: the 22 passages captured 2026-08-31 replayed through the real
  `HallCapture`. 20 nominal passages unchanged in both pole and peak; both
  mis-latches corrected — 41→183 pole S, 43→223 pole N. Then 168 single-sample
  opposite-polarity spikes, at every seventh position, at amplitudes to 4,000
  counts (full ADC scale): **none flips the pole**, while the same spike is
  shown to defeat the rejected extremum rule.
- **gate 7**, new: the 2026-08-28 circuit survey. **187 of 187** untruncated
  magnet passages agree with the surveying firmware's pole. Eight truncated
  records are excluded and reported, not scored: a truncated buffer can begin
  mid-arc and end on a rail, and cannot arbitrate a polarity rule.
- Gates 1–5 unchanged and passing, including the 172-advance lap replay.

## References

- `docs/NAVI_ONE_0_3_FIELD_FINDING_05_IMPULSE_FLIPPED_POLARITY_AT_MM70.md`
- `docs/NAVI_ONE_0_3_FIELD_FINDING_06_SECOND_ENTRY_IMPULSE_MM119.md`
- `firmware/test-programs/NAVI_ONE/HallCapture.h` — `tally()`, `close()`
- `firmware/test-programs/NAVI_ONE/tests/gate_polarity.cpp`
- `firmware/test-programs/NAVI_ONE/tests/replay_polarity_survey.cpp`
- `field-records/logs/20260831_navi_one_waveforms/captured_passages.tsv`
- decision 0063 — the waveform window that made this diagnosable
