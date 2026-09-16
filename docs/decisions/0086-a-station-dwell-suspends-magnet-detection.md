# 0086 — A station dwell suspends magnet detection, and departure from it is not a new magnet

**Date:** 2026-09-15
**Status:** **PROPOSED — not authoritative.** No record in this directory has
force until the operator has personally reviewed and approved it.
**Proposed by:** Claude, from the operator's design principles of 2026-09-15
**Builds on:** X19's excursion framing. Amends nothing.

## What the operator ruled

These are his words' substance, stated as policy. The technique below is mine
and is not his ruling.

1. A magnet is encountered when X19's ordinary entry criterion is satisfied:
   signed Hall departure of at least 70 counts for two consecutive samples.
   That event is not to be confused with the later completion of the 400 ms
   measurement window.
2. **Station deceleration remains normal Hall operation.** Detection is not
   suppressed because a stop has been commanded. The locomotive can still be
   moving through the station magnet during the ramp.
3. For this purpose, **DWELL begins only when the ramped/actual motor PWM
   reaches exactly 0.**
4. Once it does, **no new Hall magnet event may be generated during that
   dwell.** Sampling and telemetry continue; Hall observations during a dwell
   must not advance navigation.
5. **Do not create another stitching problem.** If a magnet was encountered
   while moving and PWM reaches 0 before the 400 ms window has elapsed,
   stationary samples must not be appended to the moving ones. The locomotive
   has stopped traversing the field; waiting another 200 ms does not create
   another 200 ms of spatial magnet information.
6. Polarity must therefore come from the legitimate moving portion.
7. The station design intends the locomotive to stop **between** magnets.
   Stopping in a field is an exceptional condition navigation must survive,
   not the normal mechanism.
8. At PWM 0, preserve enough Hall state to know whether it stopped at/near the
   established resting field, or still displaced inside the field of a magnet
   already encountered while moving. **Do not infer a new magnet from the
   static displaced value.**
9. **The ordinary 500 ms refractory is not carried across a dwell.** Magnets
   are roughly a metre apart and the next real field can be met within 500 ms
   of departure. If it stopped at rest, arm immediately. If it stopped inside
   the known field, keep treating the changing values as that same field until
   the sensor is back in resting territory, then arm immediately. There is no
   arbitrary post-dwell blind time.
10. The ordinary moving refractory is untouched for ordinary moving passages.

## The technique chosen to enforce it — mine, not his

- One boolean, `stopped = stationHolding && actualPwm == 0`, is passed into
  `ExcursionDetector::sample()`. It is not a speed, a threshold or a motion
  model, and MANUAL never raises it.
- It freezes `restRef_` (principle 4/8), refuses declaration (4), truncates an
  in-flight window at the stop sample (5), and classifies REST vs
  IN_OLD_FIELD by `|raw - frozen rest| >= departCounts` (8).
- The old-field hold releases when the signal is back within `departCounts` of
  the frozen rest for `persistSamples` (9B). No constant is added.

A different technique could satisfy the same ten principles. If this one is
wrong, the principles are not thereby wrong.

## Implications and unintended-consequence risk — stated on first record

- **A truncated measurement is a smaller measurement.** Peak, polarity and both
  widths now come from the moving portion. Amplitude is judged relatively
  against a trailing median of accepted peaks, so a magnet truncated very
  early reports a lower peak and a lower ratio. It cannot fall below the
  70-count entry criterion, which puts the floor case near ratio 0.37 against
  a 0.34 floor — thin. The visible failure would be `TOO_WEAK` on a real
  station magnet, and the marker going uncounted.
- **The old-field hold is unbounded, on purpose.** Principle 9 forbids an
  arbitrary post-dwell blind time, and a departure ramp can legitimately spend
  many seconds leaving a field. The cost is that a badly wrong frozen rest
  would hold detection off indefinitely. This is made loud rather than
  bounded: `state/loopstat` carries `old_field_hold_ms` at 1 Hz and
  `diag/dwell` carries the suppressed count. **Silent blindness is the thing
  this project refuses, so if that number is ever seen climbing through a
  departure, the unboundedness is what to revisit first.**
- **One propulsion fact now exists in the event path** where X19 had none. It
  is narrow and it is a station fact rather than a speed, but the claim "no
  PWM anywhere in the accept path" is no longer literally true of this build
  and should not be repeated as if it were.
- **This does not make stopping in a field safe, only survivable.** Principle 7
  says the intended fix for Arches may still be geographic. Nothing here
  decides that, and the 2026-09-02 "fix the recognizer, not the geography"
  ruling is not being invoked as licence either way — it is a question for the
  operator, not a precedent to act on.

## Evidence

`field-records/20260915_OTTO_X19_ARCHES_DWELL.md` — three magnets counted while
Otto stood still at Arches, 96–106 counts inside a fringe field; Grillers, same
firmware and dwell, clean. Gates 13 and 18 of `NAVI_ONE_X20/tests` reproduce
both the false count and the inverted departure pole under X19 and show them
absent under X20.
