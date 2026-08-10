# IR_SCOPE first light — field findings, 2026-08-09 (night)

**Instrument:** IR_SCOPE transport-fix build (`agent/ir-scope-review`,
f62467d) on the data car; towed by Otto; plotter logging on the capture
laptop. All data after dark. Captures: `irscope_20260809_*.csv`,
principally sessions `284c89b7` (metal wheel) and `d80c8382`
(black plastic wheel).

## 1. The founding hypothesis is REFUTED for the metal wheel at night

The instrument was built to test: *the inter-spoke trough stays above
`thrLow`, so the detector never rearms.* The waveform says otherwise:

- Every trough of the 7-spoke finescale metal wheel plunges to ~70–100
  counts — true dark, 1,000+ counts BELOW `thrLow` (~1,150). The falling
  threshold is nowhere near binding.
- Replay across candidates 1/3 → 0.60 changes almost nothing
  (1,369 → 1,385 pulses; merged 24 → 25 of ~1,350). **Changing `thrLow`
  does not recover the missing spokes.** Decision 0010's question is
  answered: the threshold is exonerated; do not touch production values.
- The real mechanism: the metal wheel runs at **~83% duty** — the optical
  trough is only ~10 ms wide at moderate speed and shrinks further with
  speed, converging on the 1 kHz sampling + debounce resolution. Merging
  is a **target-geometry / time-resolution** problem, not a threshold
  problem. Structural check agrees: p/7pk 5.94 vs interval 6.56 (~15% of
  optical peaks unresolved as pulses at speed).
- Per-spoke portraits (eye diagram, 906 pulses): razor 3 ms rises,
  distinct per-spoke reflectivity bands (~2930/3060/3270/3340 counts),
  uniform deep troughs. The wheel is optically healthy; its geometry is
  merely fast-narrow.

## 2. Black plastic 10-spoke wheel — the counter-experiment

Same sensor, same night, wheel swapped by the operator:

- Smooth near-sinusoidal waveform, **span ~500 counts** (~25× less
  contrast than metal) but **~60% duty with wide symmetric troughs**.
- 9,696 pulses; **zero multi-peak pulses; 0.2% merged; pulses ≈ physical
  peaks 1.00** — every optical feature became exactly one pulse, at every
  speed tried. (The report's p/rev columns assume 7 spokes; for this
  10-spoke wheel the valid statement is the 1:1 pulse-to-peak ratio.)
- **Replay semantics check passed on field data**: 9,706/9,706 rises,
  9,695/9,695 falls matched — the full chain (firmware detector →
  transport → CSV → replay) validated end-to-end in the field.

## 3. Operator hypothesis confirmed

*"Metal spokes may be best only for this algorithm"* — the data agrees
and sharpens it: the metal wheel is ideal for a threshold detector **in
darkness** (deep troughs = huge margins) but its 83% duty makes detection
speed-fragile. The plastic wheel's geometry is what a generally robust
target looks like — but its 500-count span is only ~1.7× MARGINAL_SPAN,
and daylight ambient will compress it. Both wheels' daylight behaviour is
the open axis; neither is decided.

## 4. Instrument status after first light

- Transport: rides ~10 s RF stalls losslessly (bdrop 0 through stall
  phases; delayed-not-lost delivery), recovers from every interruption
  without reboot, sampler untouched by radio thrash (miss_n/late_n = 0
  on the final build, core-1 sampler). Remaining gaps occur when the
  link passes nothing longer than the 40 s buffer — RF-environment
  stalls also seen by Otto (QUORUM 1.11 signature), an infrastructure
  question (channel-11 occupancy), not firmware.
- Semantics-check failures on the metal-wheel sessions trace to marginal
  edge timing at 1 ms resolution plus gap density — flagged honestly by
  the tool; the plastic session shows the chain exact when edges are not
  marginal.

## 5. Open items

1. Daylight captures of both wheels (the aperture effect now works FOR
   the metal wheel at night; daylight reverses the sign).
2. Rev-marker hand-turn run on any candidate wheel (absolute
   pulses/revolution; per-spoke identity anchoring).
3. If the 10-spoke plastic wheel advances: SPOKES_PER_WHEEL and
   circumference constants re-derived (decision 0008 revisit — operator
   + CODEX).
4. Channel-11 occupancy survey (WiFi analyzer during an evening stall).

No production change is proposed from this data alone.

---

## Addendum, same night — operator reframe: the processing was the defect

The operator rejected the wheel-selection framing: *great waveforms
producing unreliable data was the Hall story (27% "missed" with clean
magnets), and the cause there was processing.* Tested immediately against
tonight's metal-wheel captures by replaying with production IR_TEST's
2.5 ms debounce in place of IR_DIAG's 15 ms (which IR_SCOPE inherited):

| metal-wheel data | 15 ms guard | 2.5 ms guard |
|---|---|---|
| cruise session pulses | 1,369 | **1,606** (+17%) |
| cruise pulses per 7 peaks | 5.94 | **6.97** |
| full night, all metal sessions | 7,869 pulses / 14,278 peaks (p/7pk 3.86) | **15,822 pulses / 15,906 peaks (p/7pk 6.96)** |

**With production's debounce constant, the metal wheel detects
essentially 1:1 pulse-per-peak across the entire night, fast runs
included.** The 15 ms guard — sized in the two-tape-flag era, already
identified and fixed in IR_TEST's own source comments, never reconciled
into IR_DIAG — was silently deleting up to half the spokes at speed. The
founding "merged/doubled pulses" telemetry that motivated this instrument
was IR_DIAG data: the mystery is, in substantial part, the diagnostic's
own guard. Same failure class as the Hall history: excellent optics,
stale processing constant.

Consequences (for CODEX ruling, not unilateral change):
1. Propose `DEBOUNCE_US` 15000 → 2500 in IR_DIAG and IR_SCOPE, citing
   IR_TEST's own derivation — this is *compliance* with decision 0009
   (diagnostic matches production), not tuning: production is already at
   2500 and was right.
2. The wheel verdict REOPENS: with correct processing both wheels detect
   cleanly at every speed driven tonight. Wheel choice returns to being
   a genuine trade (25× contrast margin vs 4× trough-duration margin,
   daylight behaviour of each) instead of a forced retreat from a
   detector defect.
3. Residual at 2.5 ms: ~1.5% merged + 32 multi-peak pulses concentrated
   at the fastest stretches — the true geometry/resolution tail, now
   small enough to measure honestly.
