# NAVI_ONE 0.3 field finding 02 — a shape rejection near MM110 lagged one
marker, then stopped Toby on a delayed polarity mismatch

**Date:** 2026-08-30
**Found by:** the operator (first live field run of 0.3, on Toby), reconstructed
from the retained MQTT log by Codex, cross-checked against
`firmware/test-programs/NAVI_ONE/` source by Claude
**Severity:** stops the locomotive; recovery is declare + auto + GO, as designed.
**Fixed:** not applicable — no defect confirmed in the strike/latch behaviour.
One measurement (the shape residual spike) is unexplained and left open.

## What the operator saw

Toby stopped in AUTO, CCW, mid-session. The console reported a strike:
`nav_state STRUCK`, `state/auto 0`, `state/nav_ready 0`, and the retained
warning:

> WRONG MAGNET at MM108: expected N at MM107, read S. Position is not known.
> Declare it.

The operator inspected the physical route: magnets at MM105–MM117 all in
place, all north, track and mounting nominal. The locomotive coasted to a
stop physically on top of MM105.

## Reconstruction (from the retained log, `mm/marker` and `alert`)

The stop condition reported (`WRONG_MAGNET` at what the firmware called
"MM108/MM107") was correctly identified as the final event, but the true
cause sits three markers earlier and is a different rule entirely.

```
22:40:48.952  AGREE        mm:111 tgt:110  peak:216 ratio:1.038 resid:0.0745  notmag:5
22:40:50.238  NOT_A_MAGNET mm:111 tgt:110  peak:179 ratio:0.856 resid:0.1422  notmag:6  <- WRONG_SHAPE
22:40:51.398  AGREE        mm:110 tgt:109  peak:221 ratio:1.057 resid:0.0484  notmag:6
   ...        AGREE        mm:109 tgt:108  (accepted)
   ...        AGREE        mm:108 tgt:107  (accepted)
22:4x:xx.xxx  DISAGREE     mm:108 tgt:107  obs:S expected:N  -> WRONG_MAGNET, strike, withdraw
```

At 22:40:50.238, expecting the real MM110 (a north magnet), the passage was
rejected by the Gaussian shape test: residual 0.1422 against the 0.13 ceiling.
Amplitude (ratio 0.856, floor 0.34) and polarity (N, matching expected N) were
both fine — only the shape test failed. `Ruling::WrongMagnet`'s sibling,
`Outcome::WrongShape`, sets `isMagnet=false` in `MagnetRecognizer.h`, which
routes to `Ruling::NotAMagnet` in `Navigator.h:195`. That ruling does not
advance the navigator's position, does not warn, and does not stop anything —
it only increments `notmag` (5→6 here). `navMm` stayed at 111.

The locomotive's physical position kept advancing while the navigator's count
did not: the real MM110, MM109 and MM108 were then each accepted one marker
behind — logged as 110, 109, 108 in turn while physically at 109, 108, 107.
The lag surfaced only when a real polarity difference exposed it: the
navigator, still one marker behind, expected north (MM107 in its own count)
where the physical magnet was MM106 — south, per `RouteMap.h`. That is a
genuine `WrongMagnet` disagreement, and it latched correctly per the 0.3
strike design (decision 0058). Deceleration from that point is consistent
with the observed stop over physical MM105.

**No magnet was missing, mis-polarized, or out of position.** The operator's
physical inspection and the map (`RouteMap.h`) agree: MM105–117 are exactly
as surveyed. The one-marker lag is sufficient on its own to produce the
observed stop, with no track defect required.

## Two claims from an earlier pass at this reconstruction, corrected

An initial read (mine) proposed a weak-signal polarity flip at MM107/108 as
the cause, on the basis that 182 counts looked like the weakest peak of the
session and that ratios had run above 1.0 up to that point. Both premises
were wrong once the fuller log was checked:

- Peaks of 155, 171 and 175 counts appear earlier in the session; 182 (and the
  179 at the actual rejection) were not unusually low.
- Accepted ratios of 0.871 and 0.833 appear immediately before the mismatch.
  Below-median amplitude is routine on this route, not a warning sign.

The baseline through this whole window sat at 1824–1826 — flat, no drift —
so nothing in the retained log supports a baseline-driven polarity sign
reversal. That explanation is withdrawn. The lag-then-polarity-stop
reconstruction above is the one the evidence supports.

## What is NOT explained

Why the MM110 passage's Gaussian fit spiked to 0.1422 that one time. Its four
preceding passes at the same physical marker, across the session, measured
0.0800, 0.0825, 0.0788 and 0.0868. Peak (179) and ratio (0.856) on the failed
pass were both unremarkable for that marker. The retained log carries no raw
waveform, so the shape distortion itself is unresolved. Not fixed, not
explained — a single data point.

Correction: the passage immediately after the rejection, logged as MM110 with
resid 0.0484, is the lagged navigator's label for that passage — reconstructed
above as physical MM109, not MM110. It is not evidence that MM110's own shape
measurement returned to normal on a later pass; no later pass at physical
MM110 was captured in this log.

## What this is not

- Not a repeat of the earlier "stopped on top of a magnet" bench condition —
  this was a live run, in motion, at PWM 90, disagree 0 up to the event.
- Not evidence for a threshold change. One shape-residual outlier against six
  clean passes at the same marker does not justify moving the 0.13 ceiling,
  and doing so on this evidence would be exactly the kind of tuning-on-one-
  event decision 0062 already declined for the amplitude ceiling.
- Not a fault in the strike/latch design. The eventual `WrongMagnet` stop and
  the withdrawal of enrolment both worked exactly as decision 0058 specifies.

## Worth naming, separately from this event

`Ruling::NotAMagnet` (which covers both an honest non-magnet and a rejected
real magnet under `WRONG_SHAPE`) is not silent at the wire level — the event
is published on `mm/marker` (`"event":"NOT_A_MAGNET"`, `"why":"WRONG_SHAPE"`)
and `notmag` increments in the status line every second afterward. The gap is
operational, not a missing publish: nothing stops, nothing warns
prominently, and the console gives the operator no cue that position has
become unreliable until a later polarity check happens to expose it — which,
on a route with runs of same-polarity markers, could be delayed well past one
marker (the six-marker bound already measured in decision 0059 for the
sequence witness).

This raises two separate questions, which should not be collapsed into one:

1. **Why did this one passage fail shape?** Unknown without its waveform. Not
   answerable from the retained log, and not recoverable after the fact: the
   oriented samples live only in a single in-RAM ring buffer
   (`HallCapture.h:180`, `buf_[RING]`) that the very next passage overwrites,
   and nothing on this build persists raw samples anywhere (no flash log, no
   serial dump, no MQTT publish of the buffer). A future instrument — publish
   or log `buf_` specifically when `Outcome::WrongShape` fires — would give
   the next occurrence a waveform to diagnose. Noted as a possible instrument
   only; not proposed for this build without a decision to add it.
2. **What should AUTO do when a substantial passage — amplitude and polarity
   both fine — fails only the shape test?** This is a policy decision, not a
   diagnostic one, and needs the operator's ruling before anything changes.

## Update: the mechanism recurred, on a different marker

Later the same night, the same mechanism reproduced at physical MM146/147 —
see `docs/NAVI_ONE_0_3_FIELD_FINDING_03_SECOND_SHAPE_REJECTION_MM146.md`. Not
the same marker recurring, but the same failure class: a substantial, normal
passage rejected on shape alone, residual 0.1811 that time (higher than this
event's 0.1422), amplitude and polarity both unremarkable, same silent-lag,
same delayed strike. Two occurrences on two markers in one session changes
"one occurrence does not establish a pattern" (below, as originally written)
into a pattern worth instrumenting. That instrument — a six-passage waveform
window, published only when AUTO is withdrawn for a navigation reason — is
built and bench-verified per decision 0063, not yet field-validated.

## Recommendation

Declare position at MM105 (confirmed in place), `cmd/auto 1`, GO. Run again to
see whether the shape rejection repeats at MM110 or elsewhere before treating
it as a pattern. (Superseded by the update above: it did recur, elsewhere.)

On question 2 above, the operator's stated inclination is: a `WRONG_SHAPE`
outcome on an otherwise-substantial passage (amplitude and polarity both
passing) should cause **zero advance and a controlled stop**, not a looser
residual ceiling, not treating the passage as an inferred magnet, and not a
position correction. An expected weak rebound (which already passes shape
today) is a separate case and should not be affected. This is proposed, not
yet decided — no code or threshold change has been made, and none should be
made without a decision record and the operator's explicit approval.

## References

- `firmware/test-programs/NAVI_ONE/Navigator.h` — `judge()`, `Ruling::NotAMagnet`
- `firmware/test-programs/NAVI_ONE/MagnetRecognizer.h` — `Outcome::WrongShape`
- `firmware/test-programs/NAVI_ONE/RouteMap.h` — polarity map, MM105–117
- decisions 0058, 0059, 0062
- retained MQTT log, session of 2026-08-30, reconstructed by Codex
