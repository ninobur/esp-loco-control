# NAVI — evaluating the operator's target-acquisition approach

**Date:** 2026-10-21
**Status:** Architectural review and evaluation only. Not a decision
record. No firmware, research code, or specification has been modified to
produce this document.
**Assignment:** David's pasted proposal, "NAVI — Can We Use the Operator's
Target-Acquisition Approach?", asking for an honest evaluation of
CLOSE → BASELINE → TARGET → ACQUIRE → CLOSE against the candidate-selection
machinery in the two prior documents.
**Authorization:** Research and design only (`AGENTS.md` §2).
**Superseded in part (2026-10-21):** David has withdrawn IR evidence from
the current development stage entirely, and declared the ±15% distance
tolerance vestigial for this model. The IR-dependent portions below
(§2's "movement information," §9's minimum-state IR health item, and the
±15%-tolerance assumptions throughout) are superseded for the current
stage by
[`NAVI_NON_IR_ACQUISITION_PWM90_20261021.md`](NAVI_NON_IR_ACQUISITION_PWM90_20261021.md).
The target-acquisition sequence itself (CLOSE → BASELINE → TARGET →
ACQUIRE → CLOSE), the verdict that it is a sound foundation, and the
correction it made to the two prior documents' candidate-search scope are
unaffected and remain current.

## Verdict, stated first

**Yes. Target acquisition is a sound foundation for NAVI, and it corrects a
real over-generalization in my own prior proposal.** My candidate-set
machinery was built to solve position *recovery* (what 0098 already governs
for a genuinely uncertain position) and I applied it unconditionally to
ordinary forward tracking, where the railway's determinism means there is
almost never more than one candidate to search among. That was solving a
harder problem than the one in front of NAVI most of the time. Your
sequence states the common case correctly and simply: NAVI knows its
target before it looks for it.

It is not, however, a *complete* replacement for everything candidate
selection does — one specific circumstance still needs something like it,
and the proposed sequence as stated has one real gap (an undefined response
when ACQUIRE's evidence actively contradicts the known target, not merely
falls short of it). Both are addressed concretely below, and both are
narrow — neither reopens the general multi-candidate search for ordinary
running.

---

## 1. Can target acquisition replace candidate-selection machinery?

**Mostly yes.** In ordinary forward operation, the next mapped magnet *is*
the target — not one hypothesis scored against others. Searching a
candidate set for the common case was unnecessary machinery; your sequence
removes it correctly.

**The specific circumstance that still needs it:** position recovery —
when NAVI does not have a trusted "last confirmed magnet" to compute a
target from at all. This happens after a declared LOST state (an
acquisition exception, §9 below, that runs past its bound unresolved), an
ambiguous or stale operator declaration, or startup before the first
confirmed magnet. In that specific circumstance there is no single known
target by construction, and 0098's existing bounded candidate search
(±10 MM, rolling history, "only a unique strictly better candidate
corrects position") is exactly the right tool — reserved for that state,
not run in parallel with ordinary target acquisition on every magnet.

This is a narrower claim than my prior proposal made. I had not
distinguished "searching among candidates because position is genuinely
unknown" from "searching among candidates because I built one search
procedure for both cases." They are different procedures for different
operating states, and only the first is actually needed for the Stage-1
PWM-90 single-locomotive case this work is scoped to.

## 2. Does this better exploit the railway's determinism?

**Yes, substantially.** Before the locomotive reaches the next magnet,
NAVI already has, from the map alone: the exact expected polarity, the
exact expected distance (and therefore a specific window in which a real
encounter should appear, given movement evidence), and — from PWM and the
corrected research's own width/duration study — an approximate expected
curve width and minimum sustained duration. That is four pieces of
*prior* information, not evidence NAVI has to discover afresh at
acquisition time.

The correct use of this is to turn TARGET into a **matched search
specification** handed to ACQUIRE, rather than running a generic "is
there a curve anywhere here" classifier and separately checking it against
the map afterward (which is what my candidate-set machinery effectively
did — detect, then constrain). Searching for a known target inside a known
window, with a known expected sign, is a strictly easier and more specific
problem than open-ended shape classification, and it is also exactly what
makes a weak signal usable (§4): a departure that would be ambiguous in
isolation is immediately informative when it appears at the right distance
with the right sign.

## 3. Can the normal-curve recognizer operate inside this process?

**Yes, with a reframing of its role, not its mathematics.** Its ΔBIC
computation (`research/navi_wolf_pack/normal_curve_recognizer.py`, commit
`98db802`) does not need to change to serve target acquisition — what
changes is what it is asked: not "classify whatever departs from baseline
here" but "does the signal in the window TARGET specifies look like the
curve I was told to expect." Concretely, the recognizer's grid search over
`mu`/`sigma` can be centered and bounded by the target's expected location
and PWM-implied width rather than run generically, and its sign can be
checked against the target's known polarity as part of the same evidence,
not as a separate downstream filter. Its output is still a continuous
ΔBIC/duration trace (principle 2 — do not withhold intermediate evidence
from NAVI) — it supplies *shape confirmation* of a known target, which
polarity, duration, and movement then corroborate, exactly as your ACQUIRE
step describes. No individual one of these needs to reach 99.9% alone,
per the original assignment's own stated objective.

## 4. Handling imperfect evidence without premature rejection

**This is 0104 (Evidence Accumulation Before Revision), applied to
acquisition instead of to geographic revision, and it already answers
this.** A marginal Gaussian fit or a temporarily unresolved polarity reading
is *insufficient* evidence, not *contradicting* evidence — ACQUIRE simply
continues observing (more samples, more ΔBIC accumulation, polarity
resolving as the curve matures). Rejection is only appropriate for evidence
that actually **contradicts** the target — a clearly wrong polarity sign
once resolved, or a clear absence of any encounter well past the expected
distance window — not for evidence that is merely weak or not yet
complete. This distinction (insufficient vs. contradicting) is the one
piece of explicit machinery target acquisition still needs beyond your
five steps, and it is addressed directly in §7 and §10.

## 5. How this prevents invented magnets

Because the window, sign, and approximate shape are all specified by
TARGET *before* ACQUIRE runs, a disturbance has to agree with several
independent, pre-committed physical facts simultaneously to be mistaken
for the real magnet: it must occur inside the expected distance window
(spacing), have the expected sign (polarity), and resemble the expected
curve (shape/duration). A baseline-noise event that happens to look
curve-like but occurs far outside the expected distance window, or with
the wrong sign, fails the target specification on its face — it was never
a candidate for *this* target, so there is nothing to invent. This is a
more disciplined version of what my candidate-set machinery did by
narrowing a set *after* detection; specifying the window *before* looking
is simpler and, for ordinary running, equally effective, because
(deterministically) there is only one target to specify.

## 6. Which RVM 5.1 rules become unnecessary

Per the retrospective's own classification of RVM's inherent limits and
NAVI_ONE's development-policy failures:

- **The irreversible per-event polarity contradiction**
  (`RvmNavigator.h`'s `onEnsemble()`: a disagreeing polarity immediately
  becomes `Contradiction` with "no recovery, no skipping, no alternative
  interpretation"). Target acquisition's answer to a disagreeing sign is
  "this does not match the target I was looking for" — an acquisition
  exception (§10), not an irreversible navigation verdict.
- **NAVI_ONE's one-strike polarity stop policy** (115 wrong-magnet stops in
  71 sessions) — the same reasoning applies; a single disagreeing
  observation against a known, specific target should prompt continued or
  bounded-exception handling, not an immediate strike.
- **A standalone shape/morphology veto exercised near its own uncertain
  boundary** — the carry-forward ledger explicitly preserves "shape/
  morphology as an alarm and investigation aid... not a routine veto near
  its uncertain boundary." Once shape evidence is confirmation of an
  already-known, already-expected target rather than the primary means of
  discovering an unknown event, a marginal shape fit no longer needs to
  carry veto authority by itself (§4).
- **Candidate-fence-based recovery scoring run on every ordinary advance**
  (the generalized form of my own prior proposal, and of QUORUM's `{-1,
  0,+1,+2,+3,+4}` fence) — unnecessary for ordinary tracking once there is
  exactly one target; still needed, narrowly, for actual lost-position
  recovery (§1).

## 7. Is geographic HOLD still necessary?

**Functionally, yes — but it is not a separate "geographic candidate
ambiguity" state anymore, and that is a real simplification.** Under
target acquisition there are only two situations once a candidate departs
from baseline inside the expected window:

- **Insufficient evidence so far** — the curve hasn't matured, polarity
  hasn't resolved. This is simply ACQUIRE continuing; it needs no separate
  named state, consistent with your proposal and with §4.
- **Evidence that actively contradicts the target** — wrong polarity once
  resolved, or no encounter at all well past the expected window. This
  case cannot be silently absorbed into "keep acquiring the same target
  forever," because the physical world did not promise the target would
  always appear exactly as expected (a magnet can be weak, a map can have
  an unaudited error, a prior magnet could genuinely have been missed).
  Something has to happen here, and its content — preserve the
  observation, do not advance, do not fabricate certainty, wait for
  bounded additional evidence or declare an exception — is the same
  content HOLD always had. What target acquisition removes is the need to
  search *which other magnet this might be* while in this state for the
  ordinary case: the open question is simply "is my known target still
  valid, or has something I don't yet understand happened," not "which of
  several candidates is this."

So: HOLD's *content* survives, narrowed to a single question and
triggered far less often (only on actual contradiction, not on ordinary
marginal evidence); its *framing* as "searching among geographic
candidates" does not survive and should be dropped, per your proposal.

## 8. Stops and restarts

Directly carried over from decision 0106 (PWM = 0 is a non-navigation
interval for sensor changes) and 0107 (RTB is recognized from the
phenomenon, not a timer) — nothing new is needed:

- While PWM = 0, whatever partial evidence ACQUIRE has already gathered
  toward the current target (ΔBIC trace so far, duration so far, any
  resolved polarity) is preserved unchanged — the stop neither completes
  nor discards the in-progress acquisition.
- Samples gathered *while stationary* do not count toward duration (a
  parked locomotive accrues no new physical evidence merely because time
  passes) and do not count as a valid return-to-baseline (quiet-while-
  parked-on-a-magnet is a different physical claim from quiet-while-
  having-passed-one) — this is the exact RVM/Toby MM064 "stopped over a
  magnet" incident, and the reason it must be handled explicitly rather
  than left to a wall-clock timer.
- The current target itself (which mapped magnet, its polarity and
  distance) does not change across a stop. CLOSE AND ADVANCE only occurs
  on a genuine, moving return-to-baseline evidenced after resumption.

**One concrete implementation risk, carried over from the prior
twenty-questions document:** the frozen research recognizer
(`98db802`) has no PWM input and censors any candidate open for more than
two seconds of wall-clock time. That censoring must become PWM-aware
(paused during PWM = 0, not simply removed) before this sequence can run
through a stop without losing in-progress acquisition evidence.

## 9. Minimum state required

Your proposed list is essentially sufficient. Stated precisely:

- the last confirmed magnet, and the reference point (odometer/time) it
  was confirmed at;
- the current target: the next mapped magnet, its polarity, and its
  mapped distance from the last confirmed magnet;
- the established baseline (level, slope, noise) from the most recent
  BASELINE step;
- the current magnetic observation for the active target, if ACQUIRE has
  an open candidate (ΔBIC trace/duration so far, resolved polarity or
  "unresolved");
- movement evidence accumulated since the last confirmed magnet, tagged
  as direct (IR) or degraded (PWM/time proxy), per 0108 — never both
  summed for the same interval.

Two additions are genuinely necessary, not optional embellishment:

- **IR health state, kept separate from the distance value itself** (0108
  — an unavailable instrument is missing evidence, not zero movement).
- **How long the current target has been outstanding**, against a bound —
  needed only to decide when "still acquiring" must become an exception
  (§10) rather than wait indefinitely. This is the one piece of state your
  five steps don't explicitly name, because the five steps as written
  don't define what happens if ACQUIRE never completes.

## 10. The strongest argument against this approach, taken on its own merits

The proposed sequence, exactly as written, has no defined exit from
ACQUIRE other than success. CLOSE AND ADVANCE happens when the curve is
recognized; nothing in the five steps says what happens when the expected
curve does not arrive as expected — either because the observed polarity,
once resolved, actually contradicts the target, or because the locomotive
has evidently traveled well past the expected distance with no qualifying
encounter at all.

This is not a hypothetical gap. It is the exact shape of the two kinds of
incidents this redesign is meant to prevent: *inventing* an encounter
(if an undefined ACQUIRE were ever to accept a disturbance just because
*something* happened near the window) and *rejecting a genuine one*
(if an undefined ACQUIRE instead defaults to an immediate stop on the
first disagreement, reproducing NAVI_ONE's and RVM's one-strike failure).
A sequence that only describes the success path will, in practice, be
completed by whoever implements it — and if that completion is not
designed deliberately, it will default to one of the two historical
failure modes by accident.

The fix does not require reopening candidate search. It requires exactly
one bounded exception path: when ACQUIRE's evidence contradicts the
target (resolved polarity disagrees) or the target is overdue by more than
a generous, explicit distance/time margin past its expected window with no
qualifying encounter, NAVI stops silently continuing to acquire the same
target and instead holds — preserves what was observed, does not advance,
and either resolves via further bounded evidence (a subsequent, correctly
specified target eventually appears on schedule, §13 of the prior document)
or, past an outer bound, declares LOST and falls back to 0098's existing
recovery search (§1). That is the only place the earlier candidate-
selection machinery still belongs, and it is reached rarely, not on every
advance.

---

## Required conclusion

**This is a sound foundation for NAVI**, and it should replace the
general candidate-set machinery in
`NAVI_INTEGRATED_RECOGNITION_ARCHITECTURE_PROPOSAL_20261021.md` and
`NAVI_TWENTY_QUESTIONS_DECISION_MACHINERY_20261021.md` for ordinary
forward tracking. Those documents' reachability-then-polarity set-
narrowing should be understood as correct only for the genuinely-lost-
position recovery case (§1, §6), not as the default mechanism for every
magnet — that was this review's own over-generalization, now corrected.

### Smallest coherent implementation

```
CLOSE  -> confirmed magnet M, reference point R (odometer/time) set.
       -> current target T = (next mapped magnet, its polarity, its
          mapped distance from M).
BASELINE -> fit level/slope/noise from quiet samples since R; require a
            sufficient observed span (0107) before trusting it.
ACQUIRE  -> search the Hall trace, bounded to the distance window implied
            by T's mapped distance ± tolerance and PWM-implied timing,
            for a departure of T's expected sign. Run the Gaussian ΔBIC
            fit centered on that window. Accumulate evidence; continue
            (no state transition) while evidence is merely insufficient.
            On a genuine return-to-baseline with evidence that agrees
            with T (sign matches; curve/duration cleared the existence
            floor): -> CLOSE AND ADVANCE: M := T's magnet, R := now,
            compute the new T from the map, go to BASELINE.
            On evidence that contradicts T (resolved sign disagrees) or
            on exceeding the overdue bound with no qualifying encounter:
            -> ACQUISITION EXCEPTION: preserve the observation and
            accumulated movement; do not advance; keep evaluating against
            the *same* T for a bounded further distance in case the
            contradiction resolves (e.g., a genuinely clean, correctly-
            specified encounter still appears on schedule, implying the
            earlier reading was a non-navigation disturbance); past that
            bound, declare LOST and invoke 0098's existing bounded
            recovery search.
```

Stopping (PWM = 0) suspends BASELINE and ACQUIRE's clocks without
discarding accumulated evidence (§8); it does not appear as a separate
state above because it is a pause on whichever state is active, not a
transition.

### Offline PWM-90 evaluation

Using the map, polarity, Hall shape, and movement evidence together, as
the original assignment's Stage 1 requested:

1. Freeze an independently bounded PWM-90 Otto replay set (the same
   requirement raised in the first proposal — the existing historical
   ADC-derived annotations are explicitly disqualified as ground truth by
   the recognizer's own `REPORT.md`).
2. Replay CLOSE → BASELINE → TARGET → ACQUIRE → CLOSE unchanged against a
   held-out split, using the real map (`RouteMap.h`) for each target's
   polarity and distance.
3. Report four outcomes distinctly, not one reliability number: targets
   acquired as expected, targets that triggered an acquisition exception
   and how each resolved (same target confirmed late / LOST), and any
   window in which a disturbance was correctly ignored because it did not
   match the active target's specification (the direct test of §5).
4. Compare specifically against the standalone recognizer's own 75-
   declaration result (`98db802`): does anchoring the search to a known
   target change which of those 75 would have been accepted, held, or
   correctly ignored, versus the recognizer's unanchored declarations?
   That comparison — not a fresh percentage — is the useful result, exactly
   as the prior proposal specified for its own Stage 1.
5. Do not proceed to PWM 60, acceleration, stop/restart, or Toby, and do
   not design the LOST-recovery path's bound, until this review is done
   and authorized.

### What is established evidence vs. proposed here

| Status | Item |
|---|---|
| Established | Forward-only deterministic map traversal (retrospective, direction as first-class state); 0104/0106/0107/0108 exactly as cited; the real MM44–48 polarity/spacing values. |
| Corrected from my own prior work | Candidate-set search is for lost-position recovery (0098's actual scope), not for ordinary tracking — my two prior documents over-applied it. |
| Proposed here, not yet evidenced | The specific acquisition-exception bound (distance/time margin past the expected window); anchoring the recognizer's grid search to the target window rather than running it generically; the PWM-aware pause for the 2-second censoring timeout. |
| Not established | Any reliability number; generalization beyond Otto/PWM-90; Toby, other speeds, acceleration, stop/restart field validation. |

## References

- `docs/NAVI_INTEGRATED_RECOGNITION_ARCHITECTURE_PROPOSAL_20261021.md`,
  `docs/NAVI_TWENTY_QUESTIONS_DECISION_MACHINERY_20261021.md` (the
  documents this one revises in part).
- `docs/decisions/0098-proximal-physical-recovery-before-polarity-scoring.md`,
  `0104`, `0106`, `0107`, `0108`.
- `firmware/test-programs/NAVI_RVM_SHAPE_MANUAL/RouteMap.h`,
  `RvmNavigator.h` (commit `3a8953c`) — the map and the one-strike
  precedent both documents depart from.
- `docs/reviews/navigation-retrospective-20261021/` — RVM AUTO r1–r5.1 and
  NAVI_ONE failure classification underlying §6.
- `research/navi_wolf_pack/normal_curve_recognizer.py` (commit `98db802`).
