# 0087 — Hall navigation is the opening sign, guarded 645 ms from detection

**PROPOSED, 2026-09-16. Not authoritative until the operator reviews and
approves it.** Implemented in the X21 build so it can be field-tested; the
record exists to say what was done and what it costs, not to license anything.

## Context

X19 removed closure from Hall event framing. X20 added the station dwell. In
both, a candidate was *detected* by a >=70-count departure held for two
consecutive 1 kHz samples, and then *identified* 400 ms later by the window
that followed: argmax over the whole window, an excursion taken at 34% of that
peak, a signed sum over the excursion, and the sign of that sum published as
the pole.

MM136, 2026-09-15 20:19 (`field-records/20260915_OTTO_X20_MM136_POLARITY_
INVERSION.md`). A genuine South magnet opened at -74 counts, reached -106, and
was followed by a +117 shelf that held to the end of the window. The shelf won
the argmax 297 ms after the magnet. The record contradicted itself in one line
— `depart -74 -> polarity N` — the map expected South at MM137, and Otto
struck and stopped between stations.

263 of that run's 266 records took their pole from the magnet that declared
them. Three took it from something 163–240 ms later; two of those three
disagreed with their own opening sign. The distribution is bimodal with
nothing between 20 and 160 ms.

Separately, the guard being replaced was 500 ms measured from `EVENT_CLOSED`,
a definition X19 deleted along with closure.

## Decision

1. **Polarity is the sign of the opening departure, fixed at the detection
   sample.** Detection and polarity determination are one act. No later peak,
   opposite lobe, excursion integral, width, rise/fall or morphology may
   revise it.

2. **The navigation event is complete at that sample.** It is queued to the
   navigator there, not 400 ms later.

3. **The 400 ms window, the window-wide peak, the excursion, the signed sum,
   both widths, and the recognizer's amplitude screen keep running and keep
   publishing, and none of them has authority over navigation.** They cannot
   delay, reverse, reject or alter a decision already taken.

4. **The guard is 645 ms from the detection sample.** 145 ms (median detection
   -> `EVENT_CLOSED` on Otto's QUORUM corpus, PWM 90 exactly, n=1117, commit
   `c4dd775`, `docs/DETECTION_TO_CLOSURE_PWM90.md`) + the original 500 ms. It
   is armed at detection and does not observe closure. `EVENT_CLOSED` is not
   reintroduced as a prerequisite merely because the old number was measured
   from it.

5. **No new navigation event at ramped PWM 0**, by any route — widened from
   X20's `stationHolding && actualPwm == 0`.

## Implications, including the ones we did not want

**This decision removes a screen and does not replace it.** The recognizer's
amplitude test reads the window-wide peak, which is exactly the authority
being withdrawn, so it can no longer refuse anything. A weak artifact that
survives >=70 counts for two consecutive samples *and* happens to match the
expected next polarity will now **advance the map**. Under X19/X20 it might
have been refused as `TOO_WEAK` with no position consequence. The detector's
two-sample persistence and the 645 ms guard are the only screens left. This is
the largest cost of the decision and it is accepted on purpose: the point of
the build is to measure how far the simplest mechanism actually gets.

**A single-conversion transient now sets a pole.** Bench 2026-09-03 measured
the single-conversion population at 1.1 in 1000 held still, so two in a row is
1.2 in a million and the persistence test is the protection. But if one ever
does pass, X21 publishes its sign where X20 would have overruled it from the
window. The corpus replay shows the mechanism once (a decimated record whose
zero-order hold stretches one -71 sample to 4 ms) and cannot say which answer
was right.

**645 ms is a time proxy for spatial separation and is imperfect at low
speed.** A single magnet re-read is refused because 645 ms has not elapsed,
not because the locomotive has demonstrably left it. Nothing here solves that,
and nothing was added to paper over it.

**Every advance now lands 400 ms earlier**, so a station's ZERO_RAMP triggers
400 ms sooner and the locomotive comes to rest correspondingly short of where
X20 stopped it — roughly 60 mm at 150 mm/s approach speed. Station offsets
were calibrated against the delayed trigger and will need re-checking in the
field.

**Widening `stopped` to PWM 0 puts REST / IN_OLD_FIELD on every stop**,
including MANUAL and post-strike stops, not only station dwells. That covers
the post-strike condition observed at MM136 (four candidates while parked,
`suppressed` 25 -> 558). The `IN_OLD_FIELD` branch has still never executed on
a locomotive.

## What was deliberately not done

No second timing threshold, no 50 ms rule, no morphology, no width or
rise/fall requirement, no speed-dependent Hall threshold, no PWM-derived
distance estimate, no new baseline algorithm, no IR, and no speculative
protection against failure modes the evidence does not demonstrate.
