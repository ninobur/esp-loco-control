# 0076 — Navigation recovers from one wrong observation; the recognizer is allowed to be fallible

**Date:** 2026-09-04
**Status:** PROPOSED. Not authoritative until the operator reviews and approves it.
**Follows:** 0054 (one target, four conjunctive tests, fail = stop), 0059 (the
ten-magnet witness cannot disagree: it fired zero times in 171 induced
errors), 0035 (QUORUM's quarantine and shadow hypothesis), 0074 (shape is
diagnostic), 0075 (the discard fix).
**Evidence:** `docs/NAVI_ONE_AMPLITUDE_FLOOR_SWEEP_20260904.md`; the operator's
conversation with CODEX of 2026-09-04; five days of `mm/marker` history
(2026-08-30 to 09-03, 15,215 accepted passages, two false advances observed).
**Builds:** none. This record sets direction; it is not a build. The next
flight is X13 (0074 + 0075), and this record is not to be built until X13 has
run and the archive has been counted.

---

## The decision proposed, in the operator's words

> Shutting down is not a sustainable strategy for operations.

> The NAV should not entertain doubtful observation. A magnet is identified
> as a magnet. If it is a non-magnet and incorrectly promoted as a magnet, it
> will not match the pattern. Instead of shutting down, which is a good
> development strategy, the algorithm can use pattern matching, like I
> imagine QUORUM to work, to determine the actual position.

Stated as a rule:

1. The recognizer answers one question, "is this a magnet", with physical
   tests: duration, spacing, amplitude, expected polarity. It is allowed to
   be wrong occasionally. It does not police waveforms and it does not keep
   doubtful passages in limbo.
2. Navigation advances on every accepted magnet, as it does today.
3. Navigation keeps the sequence of polarities it has actually observed,
   independently of where it believes it is, including the reading that
   disagrees.
4. When the observed sequence stops matching the route at the believed
   position, navigation does not shut down. It tries three explanations:
   the position is right, one observation was extra (a false magnet was
   counted), or one observation was missing (a magnet was missed). It
   continues reading magnets until exactly one explanation survives, then
   corrects its position and resumes.
5. The budget is **one unresolved error at a time, of either type**. If a
   second inconsistency arrives before the first is placed, navigation
   declares position unknown, withholds station automation, and asks for a
   declaration. Every such event is recorded.
6. Shutdown is a development tool that preserves the scene. It is not the
   operating policy.

## What this is, and what it is not

It is QUORUM's good idea, the candidate window that lets later observations
decide which position was right, pointed at the error that actually occurs.
QUORUM's window was `{-1, 0, +1, +2, +3, +4}`: room for several missed
magnets and one false advance. Five days of evidence show the opposite
population. No magnet has been missed since the survey; two false advances
have been observed (finding 09 A, 2026-09-01; the Arches CW approach of
2026-09-02 11:26:12), both stop-adjacent acquisition faults since fixed or
bounded, both arriving alone. The window is made symmetric and small:
`{-1, 0, +1}`.

It is not a return to quarantine (0035). Nothing is held back at admission.
A magnet is a magnet when accepted; the correction comes afterwards, from
the route, when and only when the pattern disagrees.

It is not the ten-magnet witness (0059). That witness stored a polarity only
after it had matched the expected marker, so it could never disagree with
the position that produced it. The matcher proposed here keeps the observed
stream first and compares it to the route second.

## Why one error, and not more

The route's polarity sequence is a string of two symbols. A single inserted
or deleted observation is usually exposed within a few magnets and always
within ten, because every ten-magnet window on the route is unique (0054's
evidence). Two overlapping errors multiply the surviving explanations and
can stay ambiguous much longer, and while the matcher is unsure the train has
no trustworthy position for a station stop. A wide budget does not make the
railway safer; it makes "position unknown" harder to declare honestly.

"One at a time" means one unresolved, not one per lap. The archive will
count how often a second error arrives inside the first's recovery. If that
count stays at zero, the budget was right. If it does not, the budget is
widened by a measured number, not a guess.

## Why the recognizer may be fallible

The amplitude sweep of 2026-09-04 showed that the amplitude ratio cannot be
tightened into a guarantee: artifacts and magnets are separated cleanly in
counts (86 against 120) but the ratio divides both by a gain that has ranged
169 to 278 in the field, and across that range the populations cross. The
residual, the only test that ever refused those artifacts, has refused real
magnets on every day it has run and stopped the train four times in two days
(0074). There is no perfect recognizer available. Tolerance has to live in
navigation.

## The unintended consequences to name now

- **A false advance is no longer a stop; it is a few magnets of wrong
  position.** During recovery the locomotive is somewhere within one marker
  of where it believes, and a station stop taken in that interval could land
  a marker off. Rule 5 withholds station automation while an inconsistency is
  unresolved. That is the cost of not stopping, and it must be visible on the
  dashboard, not silent.
- **A recovery mechanism can mask a recurring acquisition fault.** If false
  advances become frequent, the matcher will keep correcting them and the
  railway will look healthy. The archive (0072) records every passage and
  every recovery; the count of recoveries is the health measure, and it must
  be reported, not just stored.
- **This is new navigation code on a project that has been burned by
  layers.** It is one mechanism replacing several (the stop-episode rule, the
  archaeology's rescue role, the shape veto, the shutdown-on-first-mismatch).
  It gets one build of its own, with its own acceptance test, after X13 has
  flown. It is not to be bundled with anything.
- **Direction is assumed known.** The matcher searches positions, not
  directions. A wrong direction declaration is not within its budget and
  still ends in position unknown.
- **IR is not part of this.** A stationary check from the IR car would be
  prevention for the F09A class and would have caught both observed false
  advances. It is a separate sensor with its own proving history, and it
  neither gates this record nor is gated by it.

## Order of work

1. Rule on and fly X13 (0074 + 0075). Count false advances and
   `shape_refuse` events in the archive.
2. Design the matcher from that count: the observed-sequence store, the
   three-explanation comparison, the withholding of station automation,
   the reporting. One behavioural change, one build, one acceptance test.
3. Acceptance is operational: the locomotive continues through an induced
   or observed single error with position corrected within ten magnets, no
   shutdown, station stops withheld and then resumed, the event recorded.
4. Widen the budget only from a measured count of second errors.

## Attribution

The rule, the rejection of doubtful observations, the QUORUM template
pointed forward, and the budget of one error at a time are the operator's,
from the conversation of 2026-09-04. The symmetric window, the reasons for
one error, the withholding of station automation during recovery, and the
order of work are an agent's engineering proposals; none is his ruling until
he says so.
