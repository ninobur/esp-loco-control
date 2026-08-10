# IR_SCOPE first field capture — findings (2026-08-09, night running)

**Capture:** `irscope_20260809_194417.csv` (evening tow runs, dark garden);
primary analysis session `284c89b7` (ride-through build, 99.25% coverage).
**Replay:** review-branch tool after the resync-anchor fix below; semantics
self-check 100% (1607/1607 rises, 1601/1601 falls vs the recorded detector).

## Verdict on the instrument's questions — for NIGHT conditions

1. **Seven distinguishable peaks and troughs per revolution: YES.**
   Rolling phase (~70–163 s): intervals tight at 34–80 ms (median 58),
   physical-peak structure 915 peaks vs 904 pulses, `p/rev-int` **7.00**,
   `p/7pk` 6.92, zero multi-peak pulses. (Absolute revolutions still await
   a hand-turn `rev`-marker run.)
2. **Merged pulses from troughs that fail to cross `thrLow`: NOT
   REPRODUCED at night.** Zero merged intervals in 904 pulses. Troughs are
   NARROW (~10 ms; duty ~83%) but DEEP — raw falls to ~70–200 counts
   against thrLow ≈ 1150: enormous falling margin in the dark.
3. **Would a different falling threshold change anything? Not at night.**
   1/3, 0.40, 0.50, 0.60 all yield 904–905 pulses, zero merges, zero
   shorts. The threshold question lives in DAYLIGHT, where the historic
   doubled-width telemetry was taken — and tonight quantifies exactly what
   daylight must do to cause merging: lift the ~10 ms trough floor by
   ~2,000+ counts so it misses thrLow. That is the measurement the next
   (daylight) capture must make.
4. **Contrast gate honesty: CONFIRMED.** Parked with a spoke in the beam
   (15–70 s), the percentile envelope converged to span 31, contrast went
   invalid, and the detector emitted nothing — no noise chatter, no
   phantom motion.

## Replay-tool correction found by this data (resync anchor)

The fix-7 debounce-horizon rule ("below thrLow AND > 15 ms past the last
above-thrHigh sample") deadlocks on high-duty waveforms: an 83%-duty
trough never sits 15 ms clear of the plateau, so post-gap segments never
resynchronized and their pulses were silently excluded (242 of 1611 falls
unmatched). Corrected with a two-phase anchor: until the first
below-thrLow observation (which provably idles every detector variant,
and bounds any in-gap latch-rearm), every above-thrHigh sample advances
the anchor; afterwards only upward thrHigh CROSSINGS do — resolving at
the second trough (~60 ms) while remaining exact-or-conservative on the
debounce bound. A gap arriving mid-resync now records the pending span
instead of dropping it. Field session validates 100%; all seven synthetic
scenarios unchanged.

## Transport context (same evening — see addendum 4 and commits)

Ride-through build: parked-and-rolling capture with 99.25% coverage
through recurring ~10 s RF stalls (channel 11, association never lost,
`bdrop` 0 during the monitored stall phases, sampler `miss_n`/`late_n` 0
after the core-1 move). Remaining stalls are an RF-environment question
(channel-11 interference, Saturday-evening RF), not an instrument defect.

## Next captures

1. **Daylight run** — the original problem's regime: measure trough floor
   vs `thrLow` under sun/ambient; the replay table then judges the
   falling fractions on the data that actually exhibits the fault.
2. **Hand-turn `rev`-marker run** — anchors absolute pulses/revolution
   and the one-peak-per-spoke assumption.
3. Minor tool follow-up: overlay drew episode bands inside an x-window
   that provably contains no episodes (parked stretch) — rendering
   artifact to fix; does not affect any tabulated number.
