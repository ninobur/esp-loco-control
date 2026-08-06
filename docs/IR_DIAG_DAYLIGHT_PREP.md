# IR_DIAG readied for the finescale-wheel daylight run (+ IR_TEST P1 fix)

**Date:** 2026-08-06
**Files:** `firmware/test-programs/IR_DIAG/IR_DIAG.ino` (changes 2–6),
`firmware/test-programs/IR_TEST/IR_TEST.ino` (change 1)
**Status:** built, replayed, **not flashed** — Sam reviews first.

| commit | change |
|---|---|
| `dcf3936` | 1 [P1] IR_TEST: no NVS write of an unverified blind envelope |
| `d3b6324` | 2 [P2] IR_DIAG: interval anchor invalidated on contrast collapse |
| `47ddc6e` | 3 symmetric detection thresholds, matching production |
| `652c47a` | 4 rising margin + per-phase rm/fm breakdown |
| `081bca1` | 5 `SPOKES_PER_WHEEL` 2 → 7 |
| `1e4f9bb` | 6 diagnostic LATCH lines; fixed timeout kept |

## Change 1 — validity test used

NVS is written at the silence only when **either** the transition is a
verified `STOPPED` (witness-equipped build), **or** both:
the state *before* the silence was `VALID` (edges flowed through a full
median window until the stop), **and** the trace is flat at timeout
(`lastLocalRange < ACTIVITY_MIN_RANGE`). A stopped wheel presents a
constant; a blind-while-moving sensor presents a varying trace — the same
signature `assessQuality()` keys on, used here as the motion-witness-lite.
A silence out of `REACQUIRING`, or with an active trace, saves nothing and
logs `[ENV] not saved: silence out of <state> with local_range=<n> —
unverified`. `saveEnvelope()`'s span floor still applies after the gate.

## Change 2

`prevRiseMs` is cleared in the contrast-collapse branch, so the first
post-blind pulse carries `int=0` (no valid prior) instead of an interval
spanning the outage dressed as a measurement. Counted once per episode
(guard `inPulse || prevRiseMs != 0` goes false after the clear) in
`contrastLosses`, published as `closs=` in STATS — distinct from `latch=`.

## Change 3 — with the commit-message qualification

`thrHigh = runMin + 2·span/3`, `thrLow = runMin + span/3` — IR_TEST's
exact expressions. IDLE's `thr=` offsets become `+span/6 / −span/6`.
The qualification is in the commit and repeated here: `seen=` extrema are
the strongest spoke and deepest trough over 2 s, not a guarantee for every
spoke; lowering `thrLow` helps a weak rise and makes a shallow trough
harder to catch. **This is the better next configuration, not a settled
improvement — Change 4's data decides, and the revert criterion is
written down: if rm medians sit far *above* fm medians in daylight,
revert this.**

## Change 4

`rm = rawAtRise − thrHigh`, captured at the crossing, on every PULSE line;
STATS carries `rm med/p10` beside `fm` (signed percentile helper — not
`medInt()`, which takes `abs()`). Phase index 0..`SPOKES_PER_WHEEL−1`
advances per consumed pulse, resets on **any** discard, and the reset also
drops that window's per-phase data with a printed
`PHASE  note: index restarted after a discard this window` — alignment
across a gap is unknowable and misaligned samples would smear a
phase-locked dip flat. The discard watcher runs *before* the event drain
so a just-ended blind episode resets phase before the first new pulse is
indexed. Absolute alignment is arbitrary (no index mark); the relative
profile is the finding.

## Changes 5, 6

`SPOKES_PER_WHEEL = 7` with a confirm-by-hand-turn comment;
`WHEEL_CIRCUMFERENCE_MM` stays placeholder with a the-109-mm-was-the-LGB-
wheel comment. `LATCH_TIMEOUT_MS` stays 2500 (the 20×median proposal and
its withdrawal are recorded in the source comment); `latchTimeouts` is
documented uninterpretable as a fault count for this run; the LATCH line
carries `w`, `fm`, `rm`, `base`, `span` at the abort — a genuine stop
shows raw parked mid-band under a healthy span, a real latch shows the
band itself wrong.

## Verification

**1. Builds** (`esp32:esp32:esp32`, `--warnings all`) — final state:
IR_DIAG WiFi 908,588 B (69%) / 52,768 RAM (16%); USB 282,904 B (21%);
IR_TEST WiFi 912,368 B (69%) / 51,464 RAM (15%). Braces 56/56,
preprocessor 8/8 (IR_DIAG). Warnings: exactly the three pre-existing
`-Wvolatile` (`satSamples++`, `pulseCount++`, `eventDrops++`) in IR_DIAG
and the three pre-existing in IR_TEST (`pulseCount++`, `eventDrops++`,
USB-only `lastRssiPublish`). One *new* warning appeared mid-pass
(`seenContrastLosses` unused) and was fixed before commit. None ship.

**2. Identifier / line-reference check** — Codex's references were
accurate: IR_TEST timeout path 1246–1255 (prompt said 1245–1255), IR_DIAG
collapse branch 334–337 exact, IR_TEST thresholds at 787–788 (prompt said
~786). Created names: `contrastLosses` (`closs=`), `riseMargin` (`rm=`),
`phaseIdx`/`phaseRm`/`phaseFm`/`phaseN`. No prompt identifier differed
from source.

**3. Line formats after this pass:**
```
PULSE #%5lu  int=%5lums  w=%4lums  peak=%+5d  raw=%4d rm=%+5d fm=%+5d base=%4d span=%4d  <OK|MARGINAL|UNAVAIL>[  *SATURATED*]
IDLE          raw=%4d  env=%4d/%4d  seen=%4d/%4d  base=%4d span=%4d  thr=%+5d/%+5d  pulses=%lu[   <-- NO USABLE CONTRAST]
STATS %2lus  n=%lu rate=%.1f/s | int med=%lu min=%lu max=%lu jit=%lu | w med=%lu | peak med=%d | rm med=%+d p10=%+d | fm med=%+d p10=%+d | sat=%lu miss=%lu latch=%lu closs=%lu drops=%lu | ~%.0fmm/s ~%.1fpkph
PHASE %2d: rm %+4d fm %+4d  n=%lu                     (per phase, after STATS)
PHASE  note: index restarted after a discard this window — per-phase data dropped
LATCH #%3lu  w=%lums fm=%+5d rm=%+5d base=%4d span=%4d — pulse DISCARDED, no event
```
Parser deltas vs yesterday: PULSE inserts `rm=` before `fm=`; STATS
inserts `rm med/p10` before `fm` and `closs=` after `latch=`; IDLE `thr=`
second value now negative; `PHASE` and the note are new line types; LATCH
reformatted entirely (old: `inPulse stuck <n>ms > <n>ms`).

**4. `prevRiseMs` cannot survive a discard** — cleared in both: the latch
branch (was already) and the contrast-collapse branch (Change 2). Those
are the only discard paths; the only other write is the normal
end-of-pulse assignment.

**5. No pulse from a latch** — unchanged from yesterday's pass: the latch
branch emits nothing, and the artificial fall finds `inPulse` false.

**6. Phase resets on every discard** — both counters feed one watcher
(`lt + cl` vs `seenAny`; both monotonic, so the sum is change-complete);
the watcher runs before the drain; reset clears the index *and* the
window's per-phase accumulation and sets the printed-note flag.

**7. Replay** — finescale segment (1170 pulses), symmetric thresholds,
rolling pulse-level percentile envelope:

|  | median | p10 |
|---|---|---|
| fm (lower-bound est.) | **+232** | −14 |
| rm (peak-clearance est.) | **+425** | +29 |

Per phase over the longest contiguous run (301 pulses, n=43/phase):

```
PHASE 0: rm +467 fm +239    PHASE 4: rm +486 fm +190
PHASE 1: rm +345 fm +250    PHASE 5: rm +403 fm +281
PHASE 2: rm +403 fm +250    PHASE 6: rm +500 fm +295
PHASE 3: rm +490 fm +256
```

Readings, stated with their limits:
- **The profile is flat.** No phase-locked dip in darkness — consistent
  with the uniform-interval finding that motivated committing to this
  wheel. Phase 4's fm (+190) is the low pole but within noise of the rest.
- **Symmetric thresholds trade fall margin for rise margin**, as the
  qualification predicted: fm est. drops from ~+774 (asymmetric,
  yesterday's replay) to ~+232, rm rises correspondingly. Both healthy in
  darkness. Daylight decides whether the trade was right.
- Estimate limits: the log has no 1 kHz raw stream. fm uses `raw@fall`
  from the *old* threshold crossing as the dark-floor bound — the true
  dark dips lower, so true fm is **larger** than estimated and the −14 p10
  is an artifact of the bound, not a predicted failure. rm uses peak
  clearance, an **upper** bound on the rise-crossing margin. The on-wheel
  `rm`/`fm` from tomorrow's run are the real numbers.

## What tomorrow's run decides

1. **Which edge governs** (rm vs fm medians) → keep or revert Change 3.
2. **Whether the wheel is geometrically sound** (flat vs dipped PHASE
   profile).
3. Whether polished steel in sun behaves like painted steel in darkness —
   the surface most likely to differ, now instrumented rather than
   inferred.

---

## Addendum, 2026-08-06 — CODEX review round before flash

CODEX declined sign-off on the pass above: two central measurements could
report a reassuring result while weak spokes were being missed. All four
technical findings were accepted and fixed, one commit each:

| commit | finding |
|---|---|
| `660b879` | 1 [P1] IR_TEST: NVS now writes the **pulse-proven** envelope snapshot (`provenMin/provenMax`, taken at each completed, filter-fed pulse), never the live pair — a flat trace also describes stuck/saturated/disconnected/sun-blinded, and unconditional expansion means the live pair may already hold the extreme of whatever ended the run. The flat test decides *whether*, the proven pair decides *what*. No proven pulse this boot → no write, logged. |
| `e788377` | 2 [P1] `rm` is survivor-biased (an event exists only because raw crossed `thrHigh` — every emitted pulse has positive `rm`, and a spoke that never crosses produces nothing). Added **headrooms**: `rh` = pulse peak − `thrHigh`, `fh` = `thrLow` − preceding-gap trough, both against at-rise thresholds. STATS med/p10 and PHASE switch to `rh/fh`; `rm/fm` stay on PULSE as edge-slew numbers. The gap trough resets on contrast loss; `fh` reports 0 with no prior gap. |
| `d7089e2` | 3 [P1] The existing miss test (interval > 1.8× running median) now restarts the phase window **before** the event is accumulated — a missed spoke shifted every later phase and would have smeared the daylight defect flat. Detection and reaction share one branch. |
| `9414783` | 4 [P2] A **discard epoch** stamped into each `PulseEvent` at capture replaces the counter-sampling watcher for phase resets: the reset keys on the epoch carried *in* the event, so it is ordered with the stream by construction — queued pre-discard events finish their epoch, the first post-discard event opens the new one. The watcher now only prints the LATCH line. |

`IR_DIAG_ENVELOPE_AND_MARGIN.md` marked superseded (finding 5).

### Line formats after this round (supersedes §3 above)

```
PULSE #%5lu  int=%5lums  w=%4lums  peak=%+5d  raw=%4d rm=%+5d fm=%+5d rh=%+5d fh=%+5d base=%4d span=%4d  <OK|MARGINAL|UNAVAIL>[  *SATURATED*]
IDLE          raw=%4d  env=%4d/%4d  seen=%4d/%4d  base=%4d span=%4d  thr=%+5d/%+5d  pulses=%lu[   <-- NO USABLE CONTRAST]
STATS %2lus  n=%lu rate=%.1f/s | int med=%lu min=%lu max=%lu jit=%lu | w med=%lu | peak med=%d | rh med=%+d p10=%+d | fh med=%+d p10=%+d | sat=%lu miss=%lu latch=%lu closs=%lu drops=%lu | ~%.0fmm/s ~%.1fpkph
PHASE %2d: rh %+4d fh %+4d  n=%lu
PHASE  note: index restarted after a discard this window — per-phase data dropped
LATCH #%3lu  w=%lums fm=%+5d rm=%+5d base=%4d span=%4d — pulse DISCARDED, no event
```

Deltas vs §3: PULSE appends `rh=`/`fh=` after `fm=`; STATS med/p10 fields
are now `rh`/`fh` (labels changed from `rm`/`fm`); PHASE labels likewise.
The offline per-phase replay in §7 used peak-clearance and trough-bound
estimates that are, in retrospect, closer to `rh`/`fh` than to `rm`/`fm` —
its flat-profile conclusion carries over unchanged, but the on-wheel
numbers to compare it against are the PHASE `rh/fh` medians.

### Phase-alignment guarantees after this round

The phase window restarts on: latch discard, contrast-loss discard (both
via the event-carried epoch, race-free), and any detected missed spoke
(interval > 1.8× median, in the same branch as the detection). Each restart
drops the window's per-phase data and prints the restart note. A phase
histogram can no longer be assembled across any known discontinuity;
completely missed spokes that evade the 1.8× test (consecutive misses at
exactly 2×, at very low n) remain the residual risk, and `miss=`/`n=`
per phase make that visible.

### Final builds this round

IR_DIAG WiFi 908,728 B (69%) / 52,768 RAM (16%); USB 283,048 B (21%).
IR_TEST WiFi 912,512 B (69%) / 51,480 RAM (15%). Braces 56/56,
preprocessor 8/8. Warnings: only the six pre-existing `-Wvolatile`/unused,
none introduced.
