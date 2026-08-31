# NAVI_ONE 0.3 field finding 03 — a second shape rejection, at a different
marker, confirms the MM110 mechanism as a pattern rather than a one-off

**Date:** 2026-08-30/31 (same session as field finding 02, later)
**Found by:** the operator; reconstructed from the Pi's `ngr-runlog` retained
log (`all_20260830.log`) by Claude
**Severity:** stops the locomotive; recovery is declare + auto + GO, as designed.
**Fixed:** not applicable. Same disposition as finding 02: no defect in the
strike/latch design. Prompted a new instrument — see decision 0063.

## What the operator saw

Toby stopped in AUTO again, CCW, later in the same session as field finding
02. Retained warning:

> WRONG MAGNET at MM145: expected N at MM144, read S. Position is not known.
> Declare it.

## Reconstruction (from `ngr-runlog`'s `all_20260830.log`, confirmed already
recording — see "Recording status" below)

```
23:09:46.391  AGREE        mm:147 tgt:146  peak:200 ratio:0.990 resid:0.0578  notmag:4
23:09:47.517  NOT_A_MAGNET mm:147 tgt:146  peak:191 ratio:0.946 resid:0.1811  notmag:5  <- WRONG_SHAPE
23:09:48.637  AGREE        mm:146 tgt:145  peak:195 ratio:0.965 resid:0.0548  gap_ms:2109  notmag:5
   ...        AGREE        mm:145 tgt:144  (accepted)
23:09:50.926  DISAGREE     mm:145 tgt:144  obs:S expected:N  peak:192 ratio:0.955 resid:0.0574  ->
              WRONG_MAGNET, strike, withdraw
```

Exactly the same mechanism as MM110 (field finding 02): a real, well-shaped,
normal-amplitude passage was rejected on the Gaussian shape test alone
(residual 0.1811, against a ceiling of 0.13 and three immediately preceding
clean passes at the same physical marker measuring 0.0578, 0.0613, 0.0583).
`navMm` stayed at 147; the real MM146 and MM145 were each accepted one marker
behind. The lag surfaced when the real next magnet — MM143, per
`RouteMap.h` index 143 = 0 (South) — was checked against the lagged
expectation of "MM144, should be North" (index 144 = 1). `RouteMap.h` also
confirms MM144–147 are four consecutive Norths, which is exactly why three
lagged-but-polarity-matching passages were accepted before the mismatch
surfaced.

**Peak, ratio, and gap were unremarkable on the rejected passage — only the
residual spiked, nearly double the highest clean value at that marker.** This
is the same signature as MM110, on a different marker, in the same session,
at the same PWM and direction. Two data points on two different physical
locations rules out "something specific to MM110" and argues for either a
recurring but rare capture artifact, or a shape-test sensitivity to some
condition not yet identified (see "not explained," below).

## Recording status

At the time this was investigated, it was confirmed that `ngr-runlog.py`
(systemd service `ngr-runlog` on the Pi, `192.168.68.142`) was already active
and had been recording continuously — subscribed to `ngr/#` (all
locomotives), timestamped at receipt, written to a rolling
`all_YYYYMMDD.log` system-of-record plus per-run segmented files. No gap: this
event's full `mm/marker` trail, including the `WRONG_SHAPE` rejection, was
available directly from that log. Nothing needed to be stood up.

## What is NOT explained

Same as MM110: why this one passage's Gaussian fit measured 0.1811 against a
recent clean history of 0.0578–0.0613 at the same marker, with peak (191) and
ratio (0.946) both unremarkable. No waveform was retained for this event
either — `HallCapture`'s passage buffer had already been overwritten well
before this was investigated. The leading hypothesis discussed with the
operator is a transient — electrical or otherwise — landing mid-passage,
which would distort exactly the shape fit while leaving peak, amplitude
ratio, duration, and gain untouched, matching both events. That remains a
hypothesis, not a finding: nothing published from either event distinguishes
a transient from a genuine brief speed variation or sensor-side noise.

## What this is not

- Not evidence for a threshold change. Two shape-residual outliers against a
  long run of clean passes at two different markers still does not justify
  moving the 0.13 ceiling — if anything, two data points at two locations
  argues against a marker-specific or track-specific cause, which a threshold
  change would not address anyway.
- Not a fault in the strike/latch design, which again worked exactly as
  decision 0058 specifies.
- Not yet grounds to conclude the mechanism will recur at a predictable rate
  or location. Two occurrences in one session is a pattern worth
  instrumenting, not yet a characterized failure rate.

## What followed

The operator ruled on three open design questions raised after this second
occurrence (window depth, trigger, fidelity) and authorized building a
six-passage waveform window that publishes on any navigation-caused AUTO
shutdown. Built, bench-tested (host gate 5, 16 checks; full Arduino compile
against the exact esp32 core Toby runs, clean, no warnings), not yet flashed
or field-validated. See decision 0063 for the full design and its
consequences.

## Recommendation

No code or threshold change from this finding beyond the waveform-window
instrument already ruled on. If the mechanism recurs a third time with the
instrument in place (once flashed and field-validated, on the operator's
separate say-so), the actual waveform would be available to distinguish
between a transient, a speed effect, and a sensor artifact.

## References

- `firmware/test-programs/NAVI_ONE/Navigator.h` — `judge()`, `Ruling::NotAMagnet`
- `firmware/test-programs/NAVI_ONE/MagnetRecognizer.h` — `Outcome::WrongShape`
- `firmware/test-programs/NAVI_ONE/RouteMap.h` — polarity map, MM143–147
- `docs/NAVI_ONE_0_3_FIELD_FINDING_02_SHAPE_REJECTION_LAG_STOP.md` — the first occurrence, MM110
- decisions 0058, 0059, 0062, 0063
- `/home/david/NGR/telemetry/all_20260830.log` on the Pi (`ngr-runlog`), and
  `/home/david/NGR/telemetry/ngr_runlog.py`
