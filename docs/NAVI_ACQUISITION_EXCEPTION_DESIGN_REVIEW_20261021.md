# NAVI — acquisition exception design review

**Date:** 2026-10-21
**Status:** Design examination only. Not a decision record. No firmware,
research code, or specification has been modified to produce this
document.
**Assignment:** David's pasted review of the ACQUISITION EXCEPTION branch
proposed in
[`NAVI_TARGET_ACQUISITION_EVALUATION_20261021.md`](NAVI_TARGET_ACQUISITION_EVALUATION_20261021.md).
**Authorization:** Design examination only (`AGENTS.md` §2).
**Superseded, §7–§8 and the addendum (2026-10-21):** David has gone
further than withdrawing IR and the ±15% tolerance — he has directed that
ordinary ACQUIRE carry **no** automatic distance- or time-based transition
to LOST at all. The map is a representation of physical reality, not a
probability distribution: when NAVI closes MM045, its target is MM046,
not a hypothesis to be abandoned because a distance estimate is
imperfect or a curve is slow to appear. §7's argument that "some bound is
indispensable" and §8's bound, along with the addendum that tried to
derive its value, are superseded in full by
[`NAVI_NON_IR_ACQUISITION_PWM90_20261021.md`](NAVI_NON_IR_ACQUISITION_PWM90_20261021.md),
which removes the automatic bound entirely rather than re-deriving it.
0098's recovery machinery is not deleted — it remains available for
genuine recovery circumstances (startup, operator redeclaration, a
failure pattern actually demonstrated in recordings) — but nothing in
ordinary ACQUIRE triggers it automatically. §1–§6, §9's disposal
reasoning for non-matching candidates, and §10's structural conclusion
(no separate branch; ordinary ACQUIRE already handles every case) remain
correct and are unaffected — they did not depend on the bound.

## Verdict, stated first, and corrected from the prior document

**Mostly A, with one indispensable, already-present boundary condition —
not a separate branch.** On examination, the "ACQUISITION EXCEPTION"
proposed previously does not describe behavior distinct from ordinary
ACQUIRE. Every case this review poses (insufficient evidence, distance
overrun, an ambiguous disturbance, an imperfect curve, even a single
contradicting-polarity curve) is correctly handled by ACQUIRE simply
continuing to hold the same target and keep observing — no new state, no
different evidence rule, nothing to name. The one thing that cannot be
handled by "continue forever" is a genuine physical impossibility of ever
acquiring the target as specified (§7 gives a documented real case) — and
the correct response to *that* is not a new intermediate state either; it
is the pre-existing, already-necessary LOST/recovery transition (0098),
reached directly from ACQUIRE's own bound. There is no third state. The
prior document's "ACQUISITION EXCEPTION" box should be deleted; its one
piece of necessary content (a bound, past which ACQUIRE gives up on this
specific target) is folded into ACQUIRE's own definition, and what it
gives up *to* is the LOST state that already existed in both prior
documents for genuine recovery.

---

## 1. Defining the exception precisely

Of the five conditions listed, only the last — "physical evidence
genuinely contradicts the expected target" — is a candidate for needing
any state change at all, and even it, examined closely (§4, §7), turns out
not to require a *branch*, only a *bound*.

- **Target not yet recognized:** not a failure. Ordinary ACQUIRE.
- **Expected distance exceeded:** not by itself a failure (§2). Ordinary
  ACQUIRE, because the mapped distance and the movement estimate are both
  approximate.
- **Ambiguous disturbance:** not a failure; it simply never clears the
  existence floor as the target's shape (§5). Ordinary ACQUIRE; nothing
  to reject because nothing was ever a candidate for the target.
- **Genuine curve, imperfect match:** not a failure (§3). Ordinary
  ACQUIRE, because imperfect evidence is still evidence, and the
  recognizer's causal, cumulative evidence never finalizes on a prefix.
- **Physical evidence genuinely contradicting the target** (resolved
  polarity disagrees on a fully-formed curve): this is real negative
  evidence about *that specific candidate*, but — examined in §4 — its
  correct disposal is still ordinary ACQUIRE continuing to watch for the
  same, unchanged target. What it is *not* sufficient to do by itself is
  prove the target itself is wrong or physically absent. Only repeated or
  prolonged failure to ever acquire the target as specified crosses into
  needing the pre-existing LOST transition (§7–§10).

Uncertainty, imperfect measurement, and delayed acquisition are not
treated as failures anywhere above, per the instruction not to.

## 2. The distance expectation (MM045 → MM046, ≈310 mm)

At 310 mm, 320 mm, or 350 mm with no qualifying curve yet: **ACQUIRE does
nothing different.** The mapped 310 mm is itself an approximate survey
figure already carrying tolerance (the existing ±15% practice), and IR
distance can be invalid while PWM/time is only a coarse proxy (0108) — so
"estimated travel has passed the mapped figure" is not itself a physical
fact strong enough to abandon the only target that forward motion makes
physically reachable next. **NAVI should never abandon MM046 solely
because an estimated-distance threshold was crossed.** When the curve
does begin developing at 350 mm with the expected (positive) sign, that
is simply the target being acquired a little later than the nominal
figure suggested — not a contradiction of anything.

Mapped spacing's correct role is to **inform how much benefit of the
doubt a weak or ambiguous signal deserves**, not to gate rejection on its
own: a weak departure that appears near the expected distance should be
read more generously than one appearing wildly early (which is more
likely a disturbance that does not deserve the target's benefit of the
doubt at all, §5) or implausibly late (which is where §7's bound, not a
distance-overrun rule, eventually matters). Spacing is context for
weighing *other* evidence; turning it into a standalone cutoff is exactly
the "arbitrary rejection gate" the question warns against, and nothing in
this design does that.

## 3. Imperfect Hall evidence

A weak initial Gaussian fit at MM046's expected location: **ACQUIRE
continues accumulating evidence.** This is not a special case; it is the
default behavior of a candidate that has not yet matured. Nothing
justifies rejecting it on an early, partial reading, because the
recognizer's ΔBIC/duration evidence is explicitly causal and cumulative —
it only grows or is reevaluated with more samples; it never issues a
verdict from a prefix. **An early rejection absolutely could cause NAVI to
disregard the remainder of a genuine encounter** — this is precisely how
RVM and NAVI_ONE's standalone shape/morphology vetoes and one-strike
polarity rules produced false stops and missed recognitions historically.
The only thing that can legitimately end acquisition of MM046 from Hall
evidence alone is the full candidate completing (returning to baseline)
*without* ever having cleared the existence floor at all — i.e., it was
genuine noise, not a magnet, discovered only once the whole episode has
run its course, never declared early.

## 4. Polarity disagreement

MM046 expects positive polarity. A convincing, fully-formed negative curve
appears at approximately the expected location.

**This does not, by itself, require declaring MM046 contradicted, and it
does not advance position.** The candidate simply fails to match the
target's specification — it is disposed of as "not MM046," logged for
diagnosis, and **MM046 remains the target, unchanged**, because disposing
of one non-matching candidate says nothing about whether the real MM046 is
still physically ahead (it almost certainly still is; nothing about
forward motion through a deterministic map changed). This is the direct
answer to "should NAVI preserve MM046 as its target while examining
whether the polarity measurement is reliable" — yes, and "examining" here
means nothing more elaborate than continuing to watch: if the negative
reading was a measurement artifact (cross-talk, a transient electrical
glitch, a misread sign), the real, correctly-signed MM046 curve should
still appear shortly after, which is itself the confirming evidence,
produced for free by continuing ACQUIRE rather than by any dedicated
diagnostic routine.

**Distinguishing an incorrect polarity interpretation from a genuinely
unexpected magnetic event:** a single disposed, contradicting candidate is
not sufficient evidence of either — it is simply evidence that *that*
candidate was not MM046. What would make the distinction matter is
repetition or persistence (§7's bound): one clean negative reading,
disposed and forgotten while ACQUIRE keeps watching, is not something that
needs resolving at all unless it keeps happening or MM046 never
materializes correctly within the bound.

## 5. Invented magnets from a non-developing disturbance

A brief departure between MM045 and MM046 that does not develop into the
expected curve: **ordinary ACQUIRE disposes of it automatically**, because
it never clears the existence floor as a sustained, target-shaped
encounter (§1's third case). No exception is needed because nothing ever
became a candidate for the target in the first place — a return to
baseline without ever having built qualifying evidence is simply more
quiet track, the same outcome as if the disturbance had never occurred.
What prevents it from advancing position is exactly the same thing that
prevents any non-matching evidence from advancing position: CLOSE AND
ADVANCE only occurs when the target's full specification (shape past the
floor, correct sign, consistent location, genuine RTB) is actually met.

## 6. Stops and restarts

Unchanged from the prior documents and directly governed by 0106/0107:
while PWM = 0, the currently accumulating candidate's evidence is
preserved, not discarded or advanced; duration and distance clocks do not
accrue stationary wall-clock time or stationary ADC wander as either more
curve evidence or movement; return-to-baseline is not concluded from
stationary samples. No timeout fires from elapsed *stopped* time alone —
whatever bound exists (§7–§10) is itself a bound on accumulated *movement*
evidence, which does not accrue while stopped, so a long stop simply
pauses everything and resumes exactly where it left off. This remains the
direct fix for the documented RVM/Toby MM064 "stopped over a magnet"
incident, and requires the same PWM-aware fix to the frozen research
recognizer's 2-second wall-clock candidate-censoring timeout already
flagged in the prior document.

## 7. An actual contradiction — the central question

**The strongest realistic example is documented, but from a different
architecture and era, and should be labeled accordingly.** The QUORUM
MM008 migrated/stacked-magnet episode (15–16 August field record, cited in
the 2026-10-21 retrospective: "the MM008 migrated/stacked magnet episode
showed that quarantining an implausible event can reveal both the missing
and duplicated location") is the closest available real precedent for a
case where continued acquisition of a target is **genuinely untenable —
not because of any flaw in Hall measurement, polarity reading, or
movement estimate, but because the physical premise itself was briefly
false.** A magnet had been physically moved or duplicated, so the map's
statement "the next magnet is here, with this polarity" was, for that
stretch of track, not true. In that circumstance, no amount of patient,
generous, bias-free continued observation would ever produce the expected
curve at the expected place, because there was nothing there to produce
it — and ordinary ACQUIRE, which is built entirely around "a real target
is physically present and will eventually be observed," has no mechanism
that can ever terminate on its own in that situation.

**This is why ordinary ACQUIRE cannot handle the situation by itself, and
why some bound is logically indispensable, not merely a convenience**:
without one, a genuine physical map/hardware anomaly produces silent,
permanent non-advancement, invisible to the operator, rather than a
reported, recoverable state. This is a different failure shape from §4's
single contradicting candidate — it requires *persistence* of the
contradiction or *exceeding* a generous travel bound, not a single
observation, to become real evidence that the premise itself, not the
measurement, is what's wrong.

No documented case exists yet of this occurring under a target-acquisition
architecture specifically (it hasn't been fielded); MM008 is offered as
the closest real analog for *why the physical possibility exists*, not as
proof this exact design has been tested against it.

## 8. Minimum behavior, specified without inventing a third state

Because §7 establishes that *some* bound is indispensable, here is its
complete content — and it is deliberately as small as the question asks
for:

- **Entry condition:** accumulated *distance* evidence since the last
  confirmed magnet exceeds a bound derived from the map, as specified
  precisely in the addendum below, without a completed, correctly-
  specified acquisition of the current target. (The addendum below
  revises this away from the two-part criterion given in the first
  draft of this section — see its correction there.)
- **Trusted geographic information preserved:** the last confirmed
  magnet and its reference point, exactly as under ordinary ACQUIRE —
  nothing new to preserve, because nothing changed.
- **Observations that continue:** none, specifically. This condition is
  the termination of ACQUIRE for this target, not a parallel observing
  mode with its own rules.
- **Does the target remain MM046:** no — past this bound, MM046 is no
  longer assumed to be the correct next target without evidence; this is
  precisely the LOST transition.
- **What permits return to ordinary ACQUIRE:** the pre-existing, already-
  bounded recovery search (0098: "only a unique strictly better candidate
  corrects position") reestablishing a trusted last-confirmed magnet, from
  which a fresh target is computed and ordinary ACQUIRE resumes.
- **Does any geographic position change during the bound itself:** no.
  Position only changes once recovery resolves it — never merely because
  the bound was reached.

This is the entirety of what the prior document's "ACQUISITION EXCEPTION"
was describing. It is not a new state with its own evidence-handling
logic; it is ACQUIRE's own loop-termination condition, falling through
directly to the LOST/recovery state that 0098 already defines and that
both prior documents already reserved for genuine recovery.

## 9. Testing against the historical failure mechanisms

- **Invented magnets:** cannot occur, because CLOSE AND ADVANCE requires
  the target's full specification to be met (shape past the existence
  floor, correct sign, plausible location, genuine RTB) — a disturbance
  that does not meet it is disposed of, never advanced on, regardless of
  how it arose (§5). Nothing in §8's bound creates a new way to advance
  position either; reaching the bound produces LOST, not a fabricated
  position.
- **Rejected magnets:** cannot occur from ordinary variability, because
  weak fits, delayed acquisition, distance overrun, and even a single
  contradicting candidate are all disposed of as "not yet" or "not this
  one," not as a verdict against the target itself (§3, §4). Rejection in
  the sense of giving up on MM046 only happens at the explicit, generous
  bound in §8 — and even then the response is a reported LOST state
  inviting recovery, not a silent wrong conclusion or an irreversible
  stop.

## 10. Is the exception necessary at all?

**As a separate branch: no — this review's own examination of §1–§6
shows ordinary ACQUIRE, retaining the same target and continuing to
observe, handles every case except one.** The one exception is not a
behavioral branch; it is the boundary condition established in §7–§8: a
bound on accumulated movement evidence (or repeated contradicting
candidates) without successful acquisition, past which ACQUIRE stops and
falls through to the already-existing LOST/recovery state. That bound is
indispensable — not because every case needs it, but because without any
bound at all, a genuine physical anomaly (§7's MM008 analog) produces
permanent, silent non-advancement with no way out. It is not preserved
merely because the architecture previously included a named branch for
it; the named branch is being removed here specifically because, on
examination, it added a state without adding behavior.

---

## Addendum — specifying the bound precisely (David's follow-up)

David asked three direct questions about the one surviving piece of
machinery: what exactly is bounded, what determines the bound's value,
and what happens when it is reached.

### 1. What exactly is bounded

**Distance — not elapsed moving time, and not a separate count of
observations, as its own bound dimension.** The bound is evaluated against
accumulated *distance* since the last confirmed magnet: the direct
measurement (IR) when healthy, or the degraded PWM/time-based proxy when
it is not (0108, principle 3). Elapsed moving time is not a second,
independent bound — it is already *the same thing* as the degraded-mode
proxy used when IR is unavailable, so counting it separately would
double-count one physical quantity as if it were two confirming
measurements, exactly the error the twenty-questions document's §16 warns
against.

This corrects the two-part entry condition given in §8 above. On
reflection, the second part of that condition — a count of disposed
contradicting candidates, triggering independently of distance — is not
actually grounded in any demonstrated field case. §4 already establishes
that a single contradicting candidate is ordinary and disposed of without
consequence; nothing in the evidence reviewed shows that a *second* or
*third* one, considered purely as a count and not as distance, carries any
different physical meaning. Keeping it as a second bound dimension would
be adding machinery "just in case," which is exactly what this whole
review has been removing. It is dropped as a decision trigger. A running
count of disposed candidates is still worth retaining as **diagnostic
telemetry** (principle 15 — transparent judgment) for whoever reviews a
LOST event after the fact, but it does not itself decide anything.

### 2. What determines the bound

**Derived from the map's own surveyed spacing and the tolerance already
in use elsewhere in NAVI, not an arbitrary safety margin, and not one
constant for the whole railway.** The physically motivated question is:
at what accumulated distance does "the target is just late" stop being a
defensible reading of the evidence, because the *next* mapped magnet
beyond the target has itself become a competing, equally physical
explanation?

Concretely, using the real local map (`RouteMap.h`): MM45→46 is 300 mm;
MM46→47 is a further 300 mm. Applying the existing ±15% tolerance
(reused, not invented — the same figure already used for ordinary
reachability) to each leg independently:

- MM46's own plausible window, from MM45: roughly 255–345 mm.
- MM47's own plausible window, from MM45: roughly 510–690 mm (300 mm +
  300 mm, each ±15%).

Between 345 mm and 510 mm in this specific interval there is a genuine
gap — neither window covers it. Accumulated distance in that gap means
the target is simply overdue, with no competing explanation yet; ordinary
ACQUIRE should keep waiting, exactly as §2 above already concludes. **The
bound belongs at the far edge of that gap — the point where MM47's own
tolerance window begins (≈510 mm here) — because beyond it, a correctly-
signed curve could plausibly be either a very late MM46 or an on-time
MM47, and distance alone can no longer defend the single-target
assumption ACQUIRE depends on.** This is not a round number chosen for
safety; it is read directly off the surveyed spacing table and the
tolerance convention already in force, and it will differ somewhat from
one magnet to the next because the real spacing table is not uniform
(280–360 mm across the route) — a single network-wide constant would
itself be the kind of ungrounded number this review has been removing.

This bound is deliberately a different number, and a different purpose,
from 0098's existing ±10-MM figure. 0098's bound sizes the *search width*
of recovery once LOST has already been declared; this bound decides *when
to declare LOST* in the first place. Conflating the two — which the first
draft of §8 above came close to doing by analogy — would be a mistake;
they are evaluated at different moments and derived from different
physical facts (the local two-leg spacing gap here; the outer plausible
range of a lost position there).

### 3. What happens at the bound

**Everything accumulated is preserved and handed forward; nothing is
discarded.** This follows directly from carry-forward rule 2 (immutable
observations; attach hypotheses, never rewrite) and from 0098's own
practice ("keep observations after correction"):

- The last confirmed magnet and its reference point remain exactly as
  they were — they are not invalidated by reaching the bound, and they
  are precisely what 0098's subsequent recovery search needs as its own
  anchor.
- Every candidate observed and disposed of during the overrun (including
  any contradicting ones from §4) is preserved with its own evidence —
  resolved polarity, approximate location, ΔBIC summary — and handed to
  recovery as evidence, not discarded as noise. A disposed candidate's
  polarity and approximate distance are exactly the kind of datum 0098's
  recovery search already weighs (did something of the right polarity
  for some *other* nearby mapped position appear during this window?).
  Recovery therefore starts with full context, not a blank slate.
- The accumulated distance evidence itself (and whether it was direct or
  degraded-proxy, 0108) is preserved and carried into recovery rather
  than reset to zero.

Reaching the bound changes only one thing: ACQUIRE stops treating MM046
as the sole physically available explanation and hands the question to
0098's existing recovery search, which starts from the preserved last-
confirmed position using the preserved observations — not from nothing.

### Correction this addendum makes to the document above

§8's "Entry condition" bullet, and the corresponding row in the
conclusion table below, are superseded by this addendum: the bound is a
single, map-derived distance threshold evaluated per interval from the
real spacing table and the existing ±15% tolerance, not a generic
"generous" figure with a separate contradiction-count trigger.

## Required conclusion

**Recommendation A, with the clarification that follows.** No separate
exception branch is required. Ordinary ACQUIRE retains the target and
continues observing through insufficient evidence, distance overrun,
ambiguous disturbances, imperfect fits, and even a single contradicting
candidate (§1–§6). The one thing ACQUIRE cannot do is run with no
terminating condition at all (§7), so ACQUIRE's own definition includes an
explicit, generous bound past which it gives up on the current target and
transitions directly to the pre-existing LOST/recovery state (0098) —
not a new intermediate state, not a richer behavior, and not a general
candidate search invoked for anything short of genuinely lost position.

This corrects the prior document's "ACQUISITION EXCEPTION" box: delete it
as a named state. Its one piece of real content (the bound) was already
necessary and was already described there; it simply was not, and does
not need to be, a distinct branch with distinct behavior. The smallest
coherent machine is now:

```
CLOSE -> BASELINE -> TARGET -> ACQUIRE (continues through all ordinary
         uncertainty; disposes non-matching candidates without advancing
         or committing) -> CLOSE AND ADVANCE (on a genuine, fully
         specified match) -> next TARGET
                           -> (bound exceeded, §8) -> LOST -> 0098 recovery
                              -> re-established last-confirmed magnet
                              -> TARGET -> ACQUIRE (resumed)
```

### What is established evidence vs. proposed here

| Status | Item |
|---|---|
| Established | 0098's recovery search and its own bound/discipline; 0104/0106/0107/0108 as cited; the MM008 field record and its documented cause (a physically moved/duplicated magnet, not a measurement defect). |
| Corrected from the prior document | The "ACQUISITION EXCEPTION" branch is removed as an unneeded named state; its content folds into ACQUIRE's own bound plus the already-existing LOST transition. |
| Proposed here, resolved by the addendum | The bound is a single distance threshold, derived per-interval from `ROUTE_SPACING_MM` and the existing ±15% tolerance (the point where the next mapped magnet's own window begins), not a network-wide constant and not a separate contradiction-count trigger. |
| Not established | Any reliability number; whether this exact collapse (no separate branch) holds once Toby, other speeds, acceleration, or stop/restart interactions are field-tested — entirely outside this review's scope, as in the prior documents. |

## References

- `docs/NAVI_TARGET_ACQUISITION_EVALUATION_20261021.md` (the document this
  one corrects in part).
- `docs/decisions/0098-proximal-physical-recovery-before-polarity-scoring.md`,
  `0104`, `0106`, `0107`, `0108`.
- `docs/reviews/navigation-retrospective-20261021/NGR_navigation_four_questions_2026-10-21.md`,
  `NGR_navigation_carry_forward_ledger_2026-10-21.md` — the MM008
  migrated/stacked-magnet episode.
- `research/navi_wolf_pack/normal_curve_recognizer.py` (commit `98db802`)
  — the 2-second wall-clock censoring timeout that still needs a
  PWM-aware fix regardless of this document's conclusion (§6).
