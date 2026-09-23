# 0094 — IR validity is continuous instrument health, not an earned ten-magnet trust period

**Date:** 2026-09-22
**Status:** **Accepted, 2026-09-22**
**Decided by:** David, reached in discussion with Codex (Sam).
**Supersedes in part:** 0092, point 2 (the first-10-magnet IR startup
calibration/trust-earning requirement). Nothing else in 0092 changes: the
~10-MM / ~3,000-mm rolling position-correction history (0092 point 15) is
explicitly a different mechanism and is untouched by this record.
**Recorded by:** Claude, from the operator's own pasted transcript of the
decision conversation with Codex, at the operator's request, after Claude
flagged the apparent conflict with 0092 while reviewing
`docs/NAVI_IR_HEALTH_AND_MM_REFERENCE_20260922.md`.

## Decision

There is no ten-magnet (or any) IR trust-earning probation. IR is an
instrument, not a navigation agent that must prove itself. Its measurements
are used from session startup onward, continuously, whenever its own health
checks show the measurement system is functioning. IR becomes UNAVAILABLE
when a recognized measurement-system failure occurs (no signal, inadequate
contrast, sampling/electrical failure, implausible pulse behaviour) and
becomes AVAILABLE again as soon as health checks pass — no recalibration
ceremony, no stop/restart requirement, no repeated ten-magnet wait.

The ~10-MM / ~3,000-mm rolling history that NAVI uses for *position
correction* (0092 point 15) is a completely different mechanism and is not
affected. The two had become conceptually entangled; they are now kept
separate.

## Context: the decision trail

This was reconsidered, not decided casually — the record below is the
operator's own account, pasted directly into this conversation, of two
passes at the same question.

**Pass 1 (earlier): the question was raised and the ten-magnet calibration
was kept.** The operator had asked Codex: *"During the initial 10-magnet IR
calibration, is IR evidence unavailable to NAVI — or merely not yet trusted
enough to rule by itself?"* His answer at the time: *"Keep Session startup
like it is. Probably is overly cautious. It looks like IR is doing a good
job. Session Startup is low risk."* That settlement is what 0092 point 2
recorded. Codex, revisiting it, called reopening it at that point
unnecessary: *"I should have relied on the decision trail instead of asking
you again."*

**Pass 2 (today, after further field data): the operator reopened it
himself, deliberately, and reversed his own earlier caution.** Verbatim:

> Lets talk for a minute. Failure mode analysis. IR is not functioning. no
> signal. Not enough contrast. What then. NAVI uses own resources. NAVI's
> wheels move but he doesn't. That is useful data. But really, startup is
> operator observed. I tend to watch it to make sure the loco actually
> moves. Really though, the safeguards that ensure that IR data are
> meaningful should work anytime. Startup is not that special. I think that
> my trepidation about introducing a new measure is showing as unnecessary
> caution.

Codex's response, endorsed and built on further:

> The IR system already contains the information needed to determine
> whether its measurements are meaningful continuously. If those tests are
> good enough after magnet 10, they should generally be good enough after
> magnet 1 — or magnet 500... The only special thing about startup is that
> you are there, you declared the starting location, and you normally
> observe that the locomotive actually starts moving. The IR hardware
> doesn't suddenly become more trustworthy after ten magnets... I would now
> simplify the concept considerably: IR does not "earn trust" by surviving
> ten magnets. IR is valid whenever its continuous health checks say the
> measurement system is functioning.

The operator's own closing formulation of *why*, which is the actual
governing principle of this record:

> Really, the earn trust concept implies lack of trust. It is a measurement.
> It is always on. If it malfunctions, NAVI needs to recognize that and
> compensate. Otherwise, it is always used.

Today's field evidence mattered (IR correctly distinguishing genuine
movement from false Hall events; the one dramatic failure — the test car
physically left behind at Patio — being a rigging problem that disappears
once IR is mounted on the locomotive itself, not a measurement-quality
problem the ten-magnet period would have caught anyway). But the operator
and Codex went back afterward and located something underneath the field
evidence: pass 1 and pass 2 used different mental models of what IR *is*,
and that, not the field data alone, is what actually moved the decision.
The exchange below — supplied by the operator afterward, at Codex's
suggestion, specifically so this record would carry the reasoning and not
just the outcome — is that discovery.

### The mental model that was steering the architecture

The aside happened while discussing the existing detector state
`SIGNAL_STALE`, verbatim:

> **David:** "Why do you need STALE or NO RECENT MOVEMENT at all. Why not
> just '0' or STILL?"
>
> **Codex:** "I don't think we do need it. The detector was designed around
> pulse acquisition, so after 2.5 seconds without a pulse it created a
> special state. But from NAVI's perspective, that is unnecessarily
> categorical. If the IR system is healthy and the pulse counter has not
> advanced: movement = 0. That's the measurement. NAVI already knows
> elapsed time, PWM, station state, Hall progression, etc. It can decide
> what zero movement means in context."
>
> **David:** "I was thinking of other things that could cause data to stop,
> but they don't mean that the sensor is not working."
>
> **Codex:** "Exactly. No new pulse is not the same thing as no new IR
> data. Several perfectly normal conditions can produce no pulse-count
> increase: Toby is physically stopped, Toby is moving extremely slowly,
> Toby has traveled less than one pulse distance, or the wheel is
> positioned between optical transitions. Meanwhile the IR unit can still
> be transmitting fresh packets with good contrast, uninterrupted sampling,
> and an unchanged pulse counter. That is a healthy instrument reporting no
> measured incremental movement, not stale data."
>
> **David:** "My cyclometer works on a similar principle, but with a magnet
> and a sensor. When I stop the bike, the signal has ceased, it says 0. If
> the magnet falls off, it also says 0. But that is better than having the
> cyclometer say STALE when I stop."
>
> **Codex:** "Yes. The cyclometer analogy exposes the right abstraction.
> The IR system should behave like an odometer/speed sensor, not like a
> communications system that expects periodic movement events. If fresh IR
> packets continue arriving and the pulse count does not change: IR is
> working, movement = 0. Whether that zero means intentionally stopped,
> stalled, wheel not turning, or the optical disk/magnet equivalent fell
> off is not for the IR sensor to decide — NAVI has Hall, PWM, operating
> state, and history to interpret it."
>
> **David:** "Have you noticed how our mental models pull us off track. I
> think that this STALE concept is a holdover from IR as agent mentality."
>
> **Codex:** "Yes. I think you've identified exactly what happened.
> `SIGNAL_STALE` makes sense if we unconsciously treat IR as an agent that
> is supposed to keep reporting 'I'm moving.' When it stops producing
> pulses, the agent appears to have stopped talking. But IR isn't an
> agent. It's an instrument. Its pulse counter is a measurement. The
> cyclometer analogy fixes the mental model: the IR unit keeps reporting.
> The number simply isn't changing. That is entirely valid data."

Codex generalized the pattern across the project, not just IR:

> This same conceptual shift has appeared several times in this project:
> Hall detector reports a magnetic observation; it doesn't decide which MM
> it is. IR reports wheel movement; it doesn't decide whether the
> locomotive is moving correctly. The baseline system supplies the
> reference against which Hall is measured; it doesn't decide whether
> something is a landmark. NAVI possesses the world model and applies the
> rules.

**The architectural consequence** is what actually generalizes past IR: an
instrument's job is (1) determine whether its own measuring apparatus is
functioning, and (2) report its measurement, including zero. Interpreting
that measurement in context — locomotive state, other sensors, route
history, the map — belongs to NAVI alone, never to the instrument. Section
2's "IR is not an agent" and Section 6's "IR odometry vs. MM reference"
distinction, both already written into
`docs/NAVI_IR_HEALTH_AND_MM_REFERENCE_20260922.md` before this decision
record existed, are downstream of this same correction, not independent
choices — which is also why that document and 0094 agree without either
having been written to match the other.

In Codex's own closing words on why this belongs in this record rather than
staying a chat aside: *"0094 wasn't an arbitrary reversal of the earlier
'earn trust' decision; we discovered that the earlier rule arose from an
inappropriate conceptual model of what IR is."*

## Failure modes, restated cleanly (Codex's formulation, operator-endorsed)

1. **No signal / inadequate contrast / sample-stream failure** — the
   instrument cannot measure reliably. NAVI marks IR unavailable, notifies,
   and navigates on its other resources (Hall/map/PWM history).
2. **IR functioning, reports wheel movement** — useful physical evidence
   that the wheels are turning. Does not by itself prove the locomotive is
   progressing (wheel slip). Hall/map progression is the independent check.
3. **IR functioning, reports zero movement** — also useful data. Expected
   and coherent during a commanded stop. During commanded movement, if Hall
   also fails to progress, that is a real physical problem — already
   decided (0092 point 6) to mean shutdown with notification.

## Implications, including the ones already flagged

- **The doc this record ratifies**
  (`docs/NAVI_IR_HEALTH_AND_MM_REFERENCE_20260922.md`) and its paired,
  still-uncommitted firmware package
  (`firmware/NAVI_COHERENCE/NAVI COHERENCE IR HEALTH/`) were written
  *before* this record existed, on the strength of the chat conversation
  above. This record is what makes them official — they were correct in
  substance but previously unattributed and apparently in conflict with
  0092 on paper. That tension is resolved by this record, not by editing
  0092's text (0092's history stands; this record changes what point 2
  means going forward, exactly as 0092 itself did to 0090/0091).
- **Startup is not IR-special anymore, but it is still operator-special.**
  The operator's own point: he watches startup to confirm the locomotive
  physically moves. That is operator practice, not an IR gate, and this
  record does not remove it.
- **The two "tens" are now formally separated.** IR health has no magnet
  count attached to it at all. Position-correction's rolling ~10-MM /
  ~3,000-mm history (0092 point 15) is unchanged and unrelated.
- **This raises IR's influence from the first Hall-confirmed magnet of a
  session**, not the eleventh. If IR's health checks are ever found to pass
  while its distance reports are subtly wrong in a way they wouldn't have
  been caught by ten magnets of nothing happening anyway (the old
  calibration validated *against surveyed spans*, which needed real
  travel), that gap is now uncovered from magnet 1 rather than magnet 11.
  Nothing in today's field evidence shows this happening; it is the
  residual risk the removed safeguard used to sit in front of.
- **No firmware has been changed by this record.** The paired health
  package remains uncommitted pending the operator's separate decision on
  committing/flashing it.

## References

- `docs/NAVI_IR_HEALTH_AND_MM_REFERENCE_20260922.md`
- `firmware/NAVI_COHERENCE/NAVI COHERENCE IR HEALTH/README_IR_HEALTH_CHANGE.txt`,
  `IrHealth.h`, `MovementEvidence.h` (uncommitted as of this record)
- `docs/decisions/0092-navi-ir-decision-model-physically-possible-positions-first.md`
  (point 2, superseded in part; point 15, explicitly preserved)
- `docs/decisions/0091-ir-trust-is-earned-kept-through-stops-and-revoked-by-introspection.md`
  (the original ten-magnet-trust framing this record retires)
