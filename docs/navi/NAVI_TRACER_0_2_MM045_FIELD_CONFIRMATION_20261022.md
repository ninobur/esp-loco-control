# MM045 field confirmation — Otto, 22 Oct 2026, 22:49:53 PDT

**Status: field evidence only. Not a decision record. No firmware changed.**

David ran a dedicated short test: Otto, CW, declared MM034→target35 at
22:49:52, PWM ramped to ~90 and held. Files provided: native 5 kHz ADC
(`otto_run2_adc.csv.gz`), MQTT telemetry (`9950011_20261022_225001.log` and
siblings), and a plot (`otto_run2_hall_adc.png`) showing ~10 clean, sharp,
~150–200 ADC pulses against a flat ~1955 ADC background as Otto passed
through this stretch.

## Finding: MM045 is real, strong, and NAVI_TRACER_0_1 recognized it correctly here

The telemetry log shows a perfect, unbroken sequence from MM034 through
MM056 — **22 consecutive CLOSE_AND_ADVANCE events, zero contradictions, zero
disposals, zero suspensions**, all at PWM ≈ 90–91:

```
2026-10-22T22:50:15.970  OPEN_CANDIDATE   mm:44 target:45  departure:180.14
2026-10-22T22:50:16.008  ACQUIRED         mm:44 target:45  departure:173.96
2026-10-22T22:50:16.211  CLOSE_AND_ADVANCE mm:45 target:46 departure:3.43
```

Directly in the raw native ADC (`otto_run2_adc.csv`, `received_unix_us`
1792734615503894–1792734616495303): a clean, single South-polarity dip,
background ~1955–1965, minimum **1776 ADC** (departure ≈ 180–190 ADC),
recovering fully to background within about 150 ms. This is an
unambiguous, strong, well-formed magnetic waveform — not a borderline or
noisy case.

Baseline level/slope for this candidate: 1955.87 / +4.8067 ADC/s — small,
unremarkable, nothing like the PWM-60 contamination seen in the 15:33 Run 2
session (MM044's slope there was +213.80 ADC/s; the phantom "MM045"
candidate in that run was +23.84 ADC/s).

## What this confirms, and what it doesn't

**Confirms:** the physical MM045 magnet exists, is correctly mounted, and
produces a strong, clean, easily-recognized signal. Nothing about MM045 as
a piece of track hardware is in question. The failure documented in
`docs/navi/NAVI_TRACER_0_2_STAGE_B_PHYSICAL_WAVEFORMS_20261022.md` (no real
MM045 waveform found inside the declaration window from the 15:33 PWM-60
run) is therefore not explained by a missing or weak magnet.

**Does not, by itself, explain** why that earlier run's software-declared
"MM045" fell where it did. The most consistent reading, combining this
result with the already-documented evidence: in that earlier CCW/PWM-60
run, the recognizer's confirmed position had very likely already drifted
from the real locomotive position before reaching the area of the real
MM045 magnet — plausibly a downstream consequence of the 63-second MM048
stall (and the general PWM-60 contamination pattern) earlier in that same
run — so the declaration it labeled "MM045" was made somewhere that wasn't
physically at or near the real magnet. This is consistent with, not a
contradiction of, the earlier finding; it points at *where* the error
entered (position tracking drifting silently during/after an earlier
stall), not at the magnet itself. Tracing exactly where that drift started
is not done here — out of scope for this note.

This run also did not continue tracking the single additional pulse visible
in the plot after PWM began ramping down past t≈30 s (no further
OPEN_CANDIDATE logged after MM56's close) — noted, not investigated here.
