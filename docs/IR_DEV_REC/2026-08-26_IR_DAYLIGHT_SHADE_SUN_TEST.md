# IR daylight shade/sun comparison — natural light does not reproduce the artificial-light false-motion failure

**Date:** 2026-08-26
**Firmware:** `IR_SCOPE_ESPNOW_TX_1_0` / `IR_SCOPE_ESPNOW_RX_1_0` (not yet in this repo;
built at `/Users/davidbrown/NGR/NGR-Files/IR_SCOPE_ESPNOW_{TX,RX}/`) — untethered
1 kHz raw-waveform transmitter on GPIO34, ESP-NOW channel 11 broadcast to a USB
receiver on the Pi. Computes no speed on-device; reports raw samples, per-sample
rise/fall/inPulse/contrast flags, and cumulative pulse/latch/contrast-loss counters.
**Data:** `field-records/logs/20260826_ir_daylight_0{1..5}_*.log` (Pi-timestamped raw
serial capture via `ir_scope_serial_record.py`).
**Analysis tool:** `tools/ir_scope_espnow_analyze.py` (packet parser + per-phase
summary; run with no args to reproduce this report's numbers, or pass explicit
log paths).

## Question

Following the 2026-08-25 stationary truth test, which found that a **severe
close-range artificial work light** produced a false `speed_valid:1` reading on
`IR_SPEED_LOCAL` while the wheel was physically stationary
([IR_STATIONARY_TRUTH_TEST_2026-08-25.md]): does ordinary daylight sun reproduce
that failure, or does it only reduce/invalidate contrast?

## Method

Five phases, each a separate recording (no in-band markers — `IR_SCOPE_ESPNOW`
has none; file boundaries and operator narration served as markers instead),
same test car and wheel as the prior full-circuit test:

1. Stationary, shade — ~90s
2. Stationary, direct sun — ~95s
3. Rolling, sustained shade — ~109s
4. Rolling, sustained sun — ~100s
5. Rolling, shade→sun→shade boundary crossing — ~136s, first crossing at
   operator-noted t≈25s

Weather/sun conditions (angle, cloud cover, intensity) were not instrumented.

## Results

| Phase | Packets | Loss | Pulses Δ | Rise/Fall flags | Latch/Contrast-loss Δ | Span median/max |
|---|---:|---:|---:|---|---|---:|
| 1 stationary shade | 892 | 5.1% | **0** | 0 / 0 | 0 / 0 | 31 / 255 |
| 2 stationary sun | 821 | 17.3% | **0** | 0 / 0 | 0 / 0 | 31 / 31 |
| 3 rolling shade | 961 | 15.1% | 2827 | 2491 / 2486 | 1 / 1 | 447 / 1407 |
| 4 rolling sun | 762 | 26.9% | 2571 | 1852 / 1849 | 0 / 0 | 1855 / 3423 |
| 5 rolling boundary | 1253 | 11.3% | 1439 | 1338 / 1332 | 1 / 2 | 2079 / 3247 |

(Loss is ESP-NOW broadcast air loss, computed from batch-sequence gaps —
consistent with the 20–30% loss range seen in the 2026-08-26 full-circuit test.
`badlen=0`, `qdrop=0` throughout; no malformed or dropped-at-receiver packets.
Cumulative counters are exact regardless of RF loss since they're read from
whichever packets survive; only the per-sample flag *totals* undercount.)

### Phase 1/2 — stationary

Zero pulses, zero rise/fall flags, in both shade and direct sun, over a
combined ~185s. Span stayed low and flat in both (median 31 in each; shade
had one brief excursion to 255, sun stayed pinned at 31). **No false motion
in either condition.**

This is the headline result: the 2026-08-25 false-valid reproduction (six
completed pulses, `speed_mmps:37.27`) needed a **high-intensity portable work
light aimed directly along the track at close range**, with the surrounding
environment dark — a severe, artificial, close-range exposure. Ordinary
outdoor direct sunlight did not come close to reproducing it in this test.

### Phase 3/4 — rolling

Both phases show healthy, matched rise/fall counts (shade 2491/2486, sun
1852/1849 — under 1% imbalance, consistent with clean pulse formation) and
near-zero latch/contrast-loss discards. The notable difference is **span**:
rolling in sun ran ~4x higher (median 1855 vs 447) than rolling in shade,
with zero contrast-loss events despite the larger swing. Sun did not reduce
contrast here — if anything it increased the dynamic range between spoke and
gap, likely from stronger specular/diffuse reflection off the wheel. This
runs opposite to the "sun reduces contrast" half of the original question,
at least for this wheel/lighting combination.

### Phase 5 — boundary crossing

Span shows a clear, fast transition centered around **elapsed t≈18–22s**
(300 → 2000–3300, i.e. entering direct sun), a few seconds ahead of the
operator's t≈25 mark — expected slop between the physical crossing and
noting it. Through that transition:

- pulses advanced steadily (5637 → 5820 across t=21–32s) with no freeze,
  no burst
- zero latch/contrast-loss events fired during the transition window

The three latch/contrast-loss discards in this phase all occurred later,
clustered at t=56.1s, 119.4s, and 121.0s — each preceded by a sharp span
drop (e.g. 335→239 right before the t=119.4s latch), consistent with the
car slowing/stopping rather than the shade/sun boundary itself. **Crossing
the boundary while rolling did not destabilize detection.**

## Verdict

Under natural ambient daylight (not the severe artificial work-light
exposure of 2026-08-25):

1. Stationary direct sun does not produce false motion — 0 pulses, 0 edges,
   flat span, over 95s.
2. Rolling in sun does not reduce contrast — span was substantially *higher*
   than rolling in shade, with no discard penalty.
3. Crossing a shade/sun boundary while rolling causes a fast (~4s) envelope
   adaptation but no spurious pulses or discards during the transition.

The false-motion failure mode identified on 2026-08-25 remains real and
must still gate IR out of any control/speed-fusion role per that report and
[decision 0021][0021]. This session's evidence says ordinary daylight is not
the trigger for it — the trigger requires something closer to that severe
artificial-light exposure. That distinction narrows, but does not close,
the outstanding daylight-susceptibility question.

## What not to conclude

- Do not conclude sunlight is categorically safe for IR admission — this is
  one session, one wheel, unknown sun angle/intensity/cloud cover, and one
  set of exposure durations (~90–95s stationary). A lower sun angle,
  different reflective surface, or longer exposure could behave differently.
- Do not conclude the 2026-08-25 finding is superseded — it stands as a
  reproduced failure under its own conditions; this test simply shows a
  materially milder, more realistic exposure did not reproduce it.
- Do not treat the span/contrast numbers here as calibration inputs for
  `IR_SPEED_LOCAL` thresholds — this firmware computes no speed and applies
  no threshold; it is a waveform-capture instrument only, same posture as
  `IR_SCOPE`.
- Do not read the per-phase packet loss (11–27%) as a data-quality problem
  for this analysis — the zero-pulse finding in phases 1/2 is robust to loss
  (absence held across hundreds of surviving samples per phase); it would
  matter for anything computing absolute pulse *rate* from these logs.

## Known issue, unrelated to this test

`IR_SCOPE_ESPNOW_RX`'s startup banner reports `mac=00:00:00:00:00:00`.
Reception is unaffected (this session: `badlen=0`, `qdrop=0` throughout),
but this must be corrected before any future move to unicast + ACK/retry,
per the standing pre-capture punch list.

## Next

- Record sun angle/cloud cover/time-of-day alongside future sessions.
- Longer stationary-sun exposure (the artificial-light failure took ~71s to
  manifest from phase start; 95s here is adequate but not generous margin).
- Low sun-angle (glancing light) condition, not yet tested.
- The pre-capture reliability punch list (dedup/ACK-retry, sequence
  accounting, in-band markers, MAC banner fix) remains open for the next
  full-circuit raw capture.

[IR_STATIONARY_TRUTH_TEST_2026-08-25.md]: /Users/davidbrown/NGR/NGR-Files/IR_STATIONARY_TRUTH_TEST_2026-08-25.md
[0021]: ../decisions/0021-ir-speed-is-local-and-telemetry-is-summary.md
