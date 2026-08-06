# IR_DIAG — percentile envelope, falling-edge margin, latch timeout

> **SUPERSEDED, 2026-08-06.** The thresholds documented below are the old
> asymmetric pair (`baseline + span/3` / `baseline + span/6`) — replaced by
> the symmetric production expressions — and every line format shown here is
> stale: PULSE gained `rm/rh/fh`, STATS switched its decision numbers to
> headrooms and gained `closs=`, PHASE lines exist, and LATCH was
> reformatted. **Do not check a parser against this document.** Current
> formats: `IR_DIAG_DAYLIGHT_PREP.md` (including its CODEX addendum).
> Retained as the record of the envelope replacement and its replay.

**Date:** 2026-08-05
**Sketch:** `firmware/test-programs/IR_DIAG/IR_DIAG.ino`, `SKETCH_NAME` still `IR_DIAG_1_0`
**Commit:** `8c78297` (single commit, three changes, per task)
**Evidence:** finescale-steel-wheel run in
*"Original data, purple, then repainted purple, the finescale steel wheel..txt"*
**Status:** built, replayed offline, **not flashed** — Sam reviews first.

## Evidence verification

The file holds **13 boot segments** (purple, repainted, finescale). The
evidence segment is the last: **1170 unique pulses, base median 1827, span
median 3528** — exactly as the task stated. Measured from it under the
committed logic (`thrLow = base + span/6`):

| population | raw at fall (med / p10) | fallMargin (med / p10) |
|---|---|---|
| normal (w ≤ 500 ms), n=1165 | 2203 / 1926 | **+164 / +21** |
| latched (w > 500 ms), n=5 | 2432 / 2082 | +29 / +6 |

Task said +180/+24 on a 1058/112 split — same picture; the difference is
latch classification (I used width > 500 ms; the 4073 and 2621 ms widths the
task cites are both in my latched set). The structural finding is confirmed
in full: the envelope was not drifting (base/span near-identical in both
populations), `runMin` was anchored ~2100 counts below the real signal, and
the falling edge was marginal on every pulse.

## Change 1 — envelope: **option 1, percentile tracking** (as preferred)

`runMin`/`runMax` are now the **5th/95th percentiles of a rolling
2048-sample (~2 s) window**, recomputed every 250 ms from a 256-bucket
histogram (sample in / oldest out is O(1); the percentile scan is O(256);
bucket resolution 16 counts, noise-level against margins in the hundreds).

Why over option 2 (fast asymmetric decay): robust **by construction**, not
by tuned rate. One outlier moves a percentile of 2048 samples by zero
buckets, and every sample ages out in ≤ ~2 s — the requirement ("a single
outlier must not hold the envelope wrong for more than a second or two") is
met by the data structure, with nothing to tune wrong.

Consequences worth knowing:
- **Stationary wheel → NO USABLE CONTRAST.** A flat trace converges the
  percentiles, span drops under `MIN_USABLE_SPAN`, and the IDLE line says
  so. For a diagnostic that is the truth (no contrast *is* present); the
  window refills within 2 s of motion. This differs deliberately from
  IR_TEST's envelope-hold — IR_TEST must ride through stations; IR_DIAG
  must report what the sensor sees right now.
- Edge emission additionally requires `ENV_PRIME_N` (512) samples in the
  window, replacing the old first-sample prime.
- `DECAY_INTERVAL_MS` and `DECAY_STEP` are deleted.
- Threshold **fractions unchanged** (`base + span/3`, `base + span/6` —
  i.e. 0.83/0.67 of range, asymmetric vs the survey sketch's 2/3 / 1/3).
  Per the task, the defect was the stale `runMin`, not the fractions;
  whether the asymmetry is deliberate is still worth confirming, but with
  an honest span the margins below show it is no longer starving anything.

## Change 2 — margin telemetry

- **PULSE:** `fm=%+5d` — `thrLow − rawAtFall`, positive = healthy (the fall
  went that far below threshold).
- **IDLE:** `env=<runMin>/<runMax>` **and** `seen=<true window min/max>` —
  a stale bound is a visible disagreement, not arithmetic three hours later.
- **STATS:** `fm med=… p10=…` over the 64-pulse window, using a new
  **signed** percentile helper (`pctIntSigned`) — the existing `medInt()`
  takes `abs()`, which would hide exactly the negative margin that matters.

## Change 3 — latch timeout

`LATCH_TIMEOUT_MS = 2500`. Derivation: every genuine width in the log is
≤ ~210 ms, so 2500 is an order of magnitude above any real pulse — while
even the stale 2-flag constants at a 20 mm/s creep bound give ~2.9 s for a
*half revolution*, and no optical feature spans half the wheel — and it
catches both observed latches (2621, 4073 ms). On expiry, in the sampler:
`inPulse` cleared, **pulse discarded** (no event), `prevRiseMs` zeroed so no
interval can span the latch, `latch_timeouts` incremented. loop() prints
`LATCH #n inPulse stuck …ms` and STATS carries `latch=`. If the signal is
still above threshold the machine re-arms and re-latches — honest, each
increment is another 2.5 s of confirmed blindness. A latch consumes a pulse
number without an event: a seq gap with `latch=` advancing is a latch, not
a loss.

## Verification

1. **Balance/compile:** braces 49/49, preprocessor 8/8. WiFi build
   907,900 B (69%) / 50,664 RAM (15%); USB build 282,252 B (21%). Three
   `-Wvolatile` warnings (`satSamples++`, `pulseCount++`, `eventDrops++`) —
   all present at HEAD before this change, none introduced.
2. **Identifiers differing from the prompt:** `rawAtFall` → the struct
   field is `rawAtEdge` (it is captured at the fall); prompt's `base` is
   the line label — the variable is `baseline`; `latch_timeouts` created as
   `latchTimeouts` (STATS label `latch=`). Everything else matched exactly,
   including the quoted threshold formulas.
3. **Line formats after the change:**
   ```
   PULSE #%5lu  int=%5lums  w=%4lums  peak=%+5d  raw=%4d fm=%+5d base=%4d span=%4d  <OK|MARGINAL|UNAVAIL>[  *SATURATED*]
   IDLE          raw=%4d  env=%4d/%4d  seen=%4d/%4d  base=%4d span=%4d  thr=%+5d/%+5d  pulses=%lu[   <-- NO USABLE CONTRAST]
   STATS %2lus  n=%lu rate=%.1f/s | int med=%lu min=%lu max=%lu jit=%lu | w med=%lu | peak med=%d | fm med=%+d p10=%+d | sat=%lu miss=%lu latch=%lu drops=%lu | ~%.0fmm/s ~%.1fpkph
   LATCH #%3lu  inPulse stuck %lums > %lums — pulse DISCARDED, no event
   ```
   Parser deltas: PULSE gains `fm=` between `raw=` and `base=`; IDLE gains
   `env=` and `seen=` and its `thr=` fields widen to `%+5d`; STATS gains
   `fm med/p10` and `latch=`; `LATCH` is a new line type. Max line lengths
   ~110/135/185/70 — all inside the 224-byte payload.
4. **Replay:** on the 1170-pulse evidence segment, rolling percentile
   bounds give **fallMargin ≈ +774 median, +603 p10**, against +164/+21
   committed. Estimate from pulse-level data (the log has no 1 kHz raw
   stream): p05 was estimated from the raw-at-fall population, which is an
   *upper bound* on the true dark floor — the real margin would be at least
   this. The task's "roughly 800" prediction holds.
5. **No pulse from a latch:** the latch branch emits nothing; the eventual
   artificial fall finds `inPulse` false and emits nothing; `prevRiseMs`
   is zeroed so the next real pulse cannot compute an interval spanning
   the latch. No emit site is reachable from the latch path.

## Untouched, deliberately

`SPOKES_PER_WHEEL` (2) and `WHEEL_CIRCUMFERENCE_MM` (115.0) — both wrong
for the 7-spoke finescale wheel; separate change per task. STATS mm/s and
pkph are wrong until then; `fm`, `int`, `w`, and `latch` are all
calibration-independent and correct now.
