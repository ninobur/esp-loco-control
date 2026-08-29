# 0054 — NAVI_CL2 recognizes a magnet by four conjunctive characteristics

**Date:** 2026-08-29
**Status:** Accepted (operator, reviewed item by item)
**Refines:** 0053 — same principle, the contract as settled in full
**Rests on:** docs/research/20260828_WHAT_THE_HALL_SENSOR_SEES.md (b45d0eb),
0048 (per-direction tables), 0049 (shape does not identify), 0050 (the map
identifies), 0052 (taxonomy of false signals)

## Decision

The sketch is **NAVI_CL2**. Its navigator has one task: recognize each genuine
magnet passage exactly once. Because the magnets are in known order, every
confirmed magnet is the next marker. Like a rosary — skipping beads is not
permitted.

### 1. Starting knowledge (core principle)

Position must be declared before anything advances. Declaration is the
dashboard enrollment: FORWARD, orientation CW/CCW, and the start interval
(e.g. MM120-121 — locomotives do not start on a magnet). navDir remains
derived at applyDirection(), the single assignment point.

### 2. One target

Exactly one target per event: `nextMm(navMm, navDir)`. Never a list, never a
candidate. True missed magnets are extremely rare; the engineering problem is
admission — reject the magnet-caused spurious fluxes, admit the magnets, and
the map does the identifying.

### 3. Recognition — four characteristics, all required

A true magnet has all four. Conjunctive; every test has evidence on every
event, so the vote is always 4-of-4.

1. **Smooth distribution** — one clean passage:
   jitter below a fixed ABSOLUTE ceiling (the field contributes no roughness;
   real arcs 4.9 counts median, 12 at p99.9; spike trains 52–64), and a
   Gaussian fit residual within the pass-to-pass repeatability band (0.0153).
2. **Adequate amplitude** — RELATIVE, never a fixed count: peak against the
   running median of recent accepted peaks. Real magnets 1st pct 0.63, false
   events top out at 0.26, Otto's weak magnets 1.01 — one rule, two
   locomotives. A fixed floor of 140 would refuse MM012 thirty times a lap.
3. **Possible dt** — a physically possible distance from the last true
   magnet. The 500 ms debounce from the last ACCEPTED marker is the floor
   (every re-read arrives inside 347 ms; the shortest real interval is
   876 ms). No upper bound — slow is always possible.
4. **Expected polarity** — against the map at the one target.

30% of rebounds pass every morphology test — they ARE magnets, seen leaving.
No single test survives alone; conjunctively the measured populations do not
overlap anywhere.

Ahead of recognition, three input filters that are not identity: the 38-count
entry threshold (~150 rebounds/lap never open an event), the 500 ms debounce
(re-reads never register), and boot recalibration when the Hall is out of
limits from booting on a magnet.

### 4. The advance

Pass → `navMm = nextMm(navMm, navDir)`, exactly one. `navMm` has exactly
three writers: declaration, the reversal correction in applyDirection()
(geometry, not recovery), and this advance. Nothing else — no adoption, no
relocation, no QUORUM write.

### 5. Failure

Fail = stop. Advance zero, preserve the last confirmed `navMm`, ramp to a
controlled stop in AUTO, and wait for the operator. Manual never touches the
throttle; every refusal is recorded and published. Recovery cannot be used to
excuse uncertain admission.

On every refusal NAVI_CL2 publishes the failing waveform, which of the four
tests failed and by how much, and the proximal events — a short ring of the
most recent detections — so the cause is findable from the log alone.

### 6. Nothing else exists

No offsets, no scoring, no adoption, no quarantine committee, no phantom
arbitration, no recovery, no velocity model. Supporting evidence — polarity,
strength, duration, dt, IR movement/distance, magnet family — informs
expectation and can veto; it never authorizes. QUORUM may remain as a
diagnostic consistency check; it must not invent unseen magnets or relocate
position.

The IR sensor (Toby's car, GPIO 34) provides an independent measure of
movement, distance and speed. Movement and distance let expectations be
referenced to distance travelled rather than elapsed time, which deletes the
station-dwell special cases instead of patching them.

### Acceptance test

Starting between MM040 and MM041 travelling CW: recognize all 171 physical
magnets in order; produce exactly 171 advances of one; zero false detections;
zero duplicate counts; zero inferred missing magnets; return physically and
logically to MM040–041; stop. If NAVI_CL2 cannot do that, navigation has
failed.

## Open before the constants are fixed

- The jitter ceiling, Gaussian residual band and amplitude ratio are
  Toby-measured. Otto's captures must confirm the same separation before his
  thresholds are trusted. Constants go to the operator for review before they
  are frozen.
- The running-median amplitude reference needs a seed rule for the first
  crossings after declaration.
- Recognition tests 1 requires the detector to buffer the full waveform of
  each event, which QUORUM's production detector does not do (peak and
  duration only). The 1.13W capture path shows it is affordable.
