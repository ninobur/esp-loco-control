# 0061 — Presence and thresholds are declared per locomotive, and the build enforces it

**Status:** Accepted
**Date:** 2026-08-30

## Context

Two things in NAVI_ONE 0.1 were inferred that should have been declared.

**IR presence was probed.** At boot the sketch read GPIO 34 for four seconds
and called the sensor fitted if the peak-to-peak span exceeded 60 counts. The
reasoning is backwards. A *floating* pin swings freely and reads as present —
which is the case the probe was written to catch. A *fitted* sensor sitting
still over uniform ballast has almost no variance and reads as absent,
permanently, for the whole session. The probe also sampled pin 34 at 1 kHz for
four seconds while the Hall baseline primed for two, so the session's baseline
was taken under exactly the ADC crosstalk the same file documents as harmful.

**Measured thresholds were global.** The recognizer's constants — 200 ms guard,
0.34 amplitude floor, 0.13 residual ceiling, 190-count bootstrap gain — sat in
the sketch. Every one was measured at the midpoint of a gap in Toby's
2026-08-28 survey. Otto enters at 70 counts against Toby's 38: a materially
different sensor environment, in which none of those numbers has ever been
measured. Nothing marked them Toby's, and nothing stopped an Otto build from
inheriting them in silence.

## Decision

**The operator's principle applies to instruments as much as to position: the
declaration is truth.**

1. `IR_FITTED` is declared in the locomotive's profile. Declared absent, pin 34
   is never read — not even to probe it, which is also what keeps the floating
   input off the Hall line. Declared fitted, it is sampled at 100 Hz from boot,
   observing only. The boot probe survives as a **report**: it prints what the
   pin looked like and, when the span is small, says so — *"quiet — stationary,
   or check the wiring"* — and acts on nothing.
2. The recognizer block lives in the profile and is stamped
   `NAVI_RECOGNIZER_MEASURED_ON <loco id>`. `LocoConfig.h` compares it against
   `LOCO_ID` and **refuses to build** on a mismatch, or when the block is
   missing at all:

   > This profile has no measured NAVI_ONE recognizer block. Run the survey on
   > THIS locomotive and record the values in its profile; do not copy
   > another's.

   Verified 2026-08-30 by selecting Otto: the build stops on that line.
3. The selector file was rewritten. It had re-grown both traps its own comments
   record — the header named the wrong locomotive, and one locomotive's include
   appeared twice — and its boot-verify instruction still named a QUORUM banner
   and the other locomotive's ID, so following the file's own procedure would
   have failed every correct NAVI_ONE build. One line per locomotive; every
   line points at a file that exists; the verify text names this sketch.

## Consequence

Fitting IR to a locomotive is now an edit to that locomotive's profile, and a
commit. That is the intent: an instrument's presence is a fact about the
locomotive, and the person who fitted it is the one who knows.

Running NAVI_ONE on Otto is now blocked at compile time until his survey is
done. That is also the intent.

## References

- `firmware/test-programs/NAVI_ONE/LocoConfig.h`, `LL_LocoConfig_9950012.h`
- `docs/reviews/NAVI_ONE_0_1_REVIEW_20260829.md` — Findings 8, 9
- decision 0056 (the declaration is truth), 0060
