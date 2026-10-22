# NAVI — twenty questions about the decision machinery

**Date:** 2026-10-21
**Status:** Architectural review and proposal only. Not a decision record.
No firmware, research code, or specification has been modified to produce
this document.
**Assignment:** David's pasted follow-up to
[`NAVI_INTEGRATED_RECOGNITION_ARCHITECTURE_PROPOSAL_20261021.md`](NAVI_INTEGRATED_RECOGNITION_ARCHITECTURE_PROPOSAL_20261021.md),
asking for the actual decision machinery behind ACCEPT / HOLD / NO
ENCOUNTER.
**Authorization:** Research and design only. This document does not
authorize implementation (`AGENTS.md` §2).
**Superseded in part (2026-10-21):** David has withdrawn IR evidence from
the current development stage entirely, and declared the ±15% distance
tolerance vestigial for this model. Every answer below that relies on IR
distance or the ±15% tolerance (notably §9, §13, §14, §17's IR case, and
the reachability-window arithmetic in the worked examples) is superseded
for the current stage by
[`NAVI_NON_IR_ACQUISITION_PWM90_20261021.md`](NAVI_NON_IR_ACQUISITION_PWM90_20261021.md).
Separately, this document's own candidate-set framing was itself later
narrowed by
[`NAVI_TARGET_ACQUISITION_EVALUATION_20261021.md`](NAVI_TARGET_ACQUISITION_EVALUATION_20261021.md)
to apply only to lost-position recovery, not ordinary tracking. This
document's text is left as written, as the durable record of what was
proposed and why.

No new architectural framework is introduced. Every mechanism below is
either the prior proposal's own four parts, or named, already-accepted NAVI
doctrine (0098, 0104, 0105, 0106, 0107, 0108), applied concretely. Where a
number, threshold or boundary is new rather than reused from that doctrine,
it is marked **[new, unauthorized]** and listed again in the conclusion.

## Map values used below

Per the instruction not to invent polarity or spacing, all worked numbers
come from `firmware/test-programs/NAVI_RVM_SHAPE_MANUAL/RouteMap.h`
(commit `3a8953c`), which carries QUORUM's surveyed table corrected by
David's 2026-10-02 remeasurement of intervals 011–012, 012–013 and 013–014
only — MM44–48 are untouched by that correction and match
`firmware/programs/QUORUM/QUORUM.ino`'s `NGR_DNA1`/`spacingMm` exactly.

| mm | polarity | spacing to next (CW) |
|---:|:---:|---:|
| 44 | N (1) | 300 mm |
| 45 | S (0) | 300 mm |
| 46 | S (0) | 300 mm |
| 47 | S (0) | 300 mm |
| 48 | N (1) | — |

MM45, MM46 and MM47 share the same polarity. This is not a contrived
example — it is the actual local map, and it means polarity alone cannot
distinguish among these three magnets; only movement distance can. That
fact drives several answers below.

---

## A. Recognizing an encounter

### 1. What initiates consideration of a magnetic encounter?

**Established, extended:** the signal-evidence stream initiates it. A
candidate exists only because the Hall trace departed from its fitted
baseline beyond the baseline's own noise — the recognizer's existing
deviation test. The map and movement evidence cannot manufacture a
candidate from quiet track; that would be exactly the failure principle 3
forbids (substituting a proxy — here, expectation — for a measurement that
is available and shows nothing).

What map and movement evidence *can* do, per principle 2 (do not withhold
information from the decision-maker), is change how NAVI treats a candidate
that the recognizer's own internal logic would otherwise discard as
sub-threshold. The recognizer should report every candidate it opens, with
its continuous ΔBIC/duration trace, rather than silently dropping ones
below its own declaration gate (the current research code's `bic_gate =
10.0`, `evidence_gate_s = .11`). NAVI, holding geographic context the
recognizer does not have, is the right place to decide whether a
sub-gate candidate is still worth holding open (§Q3), not the recognizer.

### 2. What constitutes sufficient evidence that a magnetic curve exists?

**New, but directly derived from established principle, not invented
freely:** yes — there is a floor geography cannot override: an observed
departure from the fitted baseline that is not attributable to baseline
noise, sustained over more than a handful of samples, with `ΔBIC > 0`
(the line-plus-Gaussian model already fits the observed samples better
than the line alone, regardless of the recognizer's own stricter
declaration gate of `ΔBIC ≥ 10`).

Why this floor cannot be overridden by geography: `ΔBIC > 0` on an actual
baseline-relative departure is a direct physical measurement of the Hall
sensor (principle 3). A map expectation is not a measurement of the sensor
at all (principle 10, "distinguish measurement from meaning") — it cannot
substitute for one. Without this floor, NAVI would be advancing position
from an expectation with no corresponding physical evidence, which 0107
and the DNA retrospective both treat as the exact failure mode to avoid
("a correct complete DNA map" could still not yield recognition without
real sensor agreement).

This floor is below the recognizer's own current declaration threshold and
is therefore a **new** distinction this document introduces: "evidence
that *something* departed from baseline" (the floor) versus "evidence
sufficient to *declare* a curve on signal grounds alone" (the existing
`bic_gate`). This split needs operator review (see Conclusion §6).

### 3. Marginal Gaussian evidence, everything else agrees

Scenario: near MM46, polarity observed South (agrees — though note it
would equally agree with MM47; see §Q7), movement distance plausible for
MM46, weak-but-real curve (clears the §Q2 floor, below the recognizer's own
declaration gate).

**ACCEPT.** This is the assignment's own central claim and the reason the
integrated architecture exists at all: geographic and movement evidence
establish that something real existed (clears §Q2's floor) and narrows
*which* magnet it is to exactly one physically reachable, polarity-
compatible candidate (§Q9, §Q15). The decisive evidence is the
reachability narrowing, not curve strength — curve strength only needed to
clear the existence floor, which it does.

### 4. Excellent Gaussian evidence, geography disagrees

Scenario: unmistakable curve (clears the floor and the recognizer's own
declaration gate easily), but observed polarity contradicts the expected
next magnet.

**Existence of a magnet:** concluded with near-certainty. Signal evidence
this strong is a direct physical measurement (principle 3/10) and is not
weakened by a disagreeing map expectation — the map cannot overrule a
direct sensor measurement about what the sensor itself observed.

**Geographic identity:** not concluded as the originally expected magnet.
NAVI searches the set of mapped magnets physically reachable from the
last trusted position (§Q9's reachability window) for any whose expected
polarity matches what was *actually* observed. Using the real map: if NAVI
held MM45 and expected MM46 (South) but observed North, the reachable set
at ordinary forward-running distances is {MM46(S), MM47(S)} — neither is
North. MM48 is North but is not reachable at this distance. Result:
**HOLD**, not an automatic stop. This is precisely where the prior
`RvmNavigator.h` (`research/navi-rvm-shape-manual`, commit `3a8953c`)
differs — its `onEnsemble()` treats exactly this case as `Contradiction`
with no recovery ("There is no recovery, no skipping, no alternative
interpretation," by its own header comment). That one-strike rule is the
documented inherent limit the retrospective attributes to RVM core and to
NAVI_ONE's 115 false stops; this architecture's HOLD is the deliberate
departure from it, not a quiet reinterpretation of existing doctrine.

### 5. Recognizing a magnet before the complete curve has passed

**Yes, in part; established partial precedent extended.** The recognizer
already works causally — its `declared_s` fires once sustained duration
clears `evidence_gate_s` (110 ms) *inside* an open candidate, before
return-to-baseline is observed. NAVI's judgment step can act on that
evidence, and on the floor-clearing evidence of §Q2, before RTB — nothing
in 0107 requires waiting for RTB to *start* interpreting; 0107 requires
recognizing RTB from the phenomenon (not a timer) specifically to know
*when the baseline reference is valid again*, which is a separate question
from when geographic identity may be asserted.

What should **not** happen before RTB: using the candidate's final total
duration or shape as a discriminator between competing candidates (e.g.,
if two reachable candidates were distinguishable only by total curve
width) — that information does not exist yet. Operational consequence of
waiting for RTB specifically: slightly higher latency before NAVI treats
the baseline as trustworthy again for the *next* candidate, not before it
commits geographic identity for the current one if other evidence already
resolves it. This split (identity vs. baseline-trust) is new and should be
confirmed, not assumed (Conclusion §6).

## B. Geographic identification

### 6. Why the next mapped magnet is preferred

**Established, not new.** In automatic operation the locomotive moves
forward through the mapped sequence and does not navigate in reverse
(stated directly in the 2026-10-21 retrospective and consistent with
direction being first-class state, carry-forward ledger §1). Given a
trusted position and direction, the next mapped magnet is the default,
strongest hypothesis — not one equally-weighted guess among many — because
reaching any other mapped magnet first would require the locomotive to
have passed through it, which is a physical impossibility in forward
running. This expectation guides *interpretation* (narrows which
candidates are worth testing first); it does not, by itself, authorize an
advance without clearing §Q2's evidence floor.

### 7. Identifying a magnet other than the expected next one

**Do not assume missed magnets are normal; this must be rare and
evidenced, following 0098's own stance.** Advancing by two or more
positions requires all of:

- measured movement evidence (IR distance when healthy; §Q9) consistent
  with having covered the mapped spacing of the intervening magnet(s) —
  e.g., to justify MM47 over MM46 from MM45, accumulated distance must be
  near 600 mm (300 + 300), not near 300 mm;
- observed polarity matching the further candidate specifically — though
  in the MM45–47 neighborhood this cannot discriminate at all, since all
  three share polarity South; here distance is the *only* discriminating
  evidence, which is exactly why it must be required, not merely
  preferred;
- no partial signal evidence (even sub-declaration-gate, per §Q1) of an
  encounter at the intervening magnet's expected position. Any such
  partial evidence weighs against skipping and toward HOLD instead.

Where the evidence is ambiguous (plausible distance, but not clearly ruling
out the nearer magnet), the default is **HOLD**, not a confident skip —
consistent with 0098's refusal to let "8/10 agreement" relocate NAVI
without a uniquely better candidate.

### 8. Polarity's role

**Established in substance, made concrete here.** Polarity agreement is
supporting evidence; it is not sufficient alone to identify a magnet,
because a single polarity bit is geographically ambiguous (the DNA
retrospective's own finding, concretely demonstrated by MM45/46/47 sharing
one bit). Polarity disagreement is a hard filter against accepting the
*specific* candidate it contradicts — it is treated similarly to the §Q2
existence floor, because the observed field orientation is itself a direct
measurement, not an inference (principle 3/10). Strong Hall and movement
evidence cannot make NAVI accept a candidate as the originally expected
magnet when the observed polarity directly contradicts that magnet's
mapped polarity. What strong Hall and movement evidence *can* do is
identify that some *other* reachable, polarity-compatible magnet is the
right interpretation (§Q4, §Q7) — polarity disagreement overrides identity
as originally expected; it does not make the encounter stop existing, and
it does not force an irreversible stop (the departure from
`RvmNavigator.h` noted in §Q4).

### 9. Spacing's role

**Established tolerance reused, not reinvented.** Observed travel distance
(IR, when valid) is compared against the mapped spacing between the last
trusted position and each candidate, using the existing nominal ±15%
tolerance window from prior NAVI/QUORUM practice (0098's own record notes
this figure is "not certified accuracy" — reused here as the best existing
number, not re-derived). When IR distance is unavailable or unreliable:
this is *missing* evidence, not *negative* evidence (0108, extended in
§Q17). NAVI falls back to the degraded-mode proxy already sanctioned by
principle 3 — elapsed time at the known/measured PWM speed — with a wider
tolerance band, because the proxy is weaker. **[new, unauthorized]**: the
specific widened-tolerance multiplier for the degraded case is not
established anywhere and needs its own number.

### 10. Preventing premature geographic commitment

**Established, directly reused from 0098.** When two interpretations are
each plausible, NAVI does not resolve by any residual score or margin; it
holds (0098: "no extra score margin... only a unique strictly better
candidate corrects position"). The trigger for selecting one is a specific
evidential event, not a comparison of two uncertain scores against each
other: either the (reachable ∩ polarity-compatible) candidate set
narrows to exactly one member, or the physically plausible window
expires per §Q14's outer bound, producing LOST rather than a guess.

## C. The meaning of HOLD

### 11. What HOLD preserves

All of the following, not a collapsed single value:

- the incumbent trusted geographic position, **unchanged** (Evidence
  Accumulation Before Revision, 0104: "retain the incumbent view while it
  remains best supported");
- the candidate's raw observation — immutable (carry-forward rule 2:
  "a sensor event... [is] different" from an interpretation; nothing
  about it is rewritten later, only annotated);
- the movement evidence accumulated since the last trusted position (IR
  distance or its degraded-mode substitute), so it is not lost or re-zeroed
  by holding;
- the current (possibly empty, singleton, or multi-member) set of
  reachable, polarity-filtered candidate interpretations for this specific
  encounter, each tagged with what would resolve it.

What it explicitly does **not** produce is a synthesized "probably MM46"
working position exposed to station or peer consumers as if it were
trusted — carry-forward rule 3/5 is explicit that position uncertainty
must travel with any report and that downstream consumers cannot validate
a navigation model (or be fed by one) on a partial score.

### 12. What happens while holding

Yes to both. The recognizer keeps running (a currently open candidate may
still be accumulating ΔBIC, or a new candidate may open while an earlier
one's identity is unresolved — these are different held items, not one).
Movement evidence keeps accumulating against the unchanged last-trusted
reference. Subsequent observations can resolve a HOLD without any reset of
trusted position, because nothing authoritative was ever committed during
the hold — this is 0104's RESOLVE step, not a new state.

### 13. Converting HOLD to ACCEPT

New evidence that resolves ambiguity:

- accumulated distance (IR, once healthy, or continued elapsed-time
  estimate) narrowing into only one candidate's tolerance window;
- a *subsequent* encounter whose polarity is consistent with only one of
  the previously-held candidate chains — because the DNA map's "every
  window of length 10 is unique" property (`RouteMap.h`'s own stated
  survey property) means two different candidate identities necessarily
  diverge in their own next neighbor, so resolving the next encounter
  resolves the one before it;
- confirmed continued quiet track through a position where a competing
  candidate's *own* next-expected magnet should have appeared by the
  accumulated distance, functioning as bounded negative evidence against
  that competing candidate (§Q17 distinguishes this from "missing"
  evidence: this is Hall data that is present and shows nothing, which is
  itself a valid result per principle 16/0108).

**NAVI does reconsider earlier observations.** The raw record is immutable
and still available, so a later resolution is applied as a new annotation
to the earlier held observation, not a rewrite of it — carry-forward rule
2 ("attach hypotheses rather than rewriting evidence") and 0098's own
"accumulated contradictions" language both presuppose this. The trusted
position itself still only updates at the moment of resolution, and the
update is logged with the resolving evidence that justified it (principle
15's RESOLUTION EVIDENCE).

### 14. HOLD persisting across several magnets

**Yes, bounded — reusing, not inventing, 0098's existing recovery
machinery.** Ambiguity can span more than one observed candidate before
resolving (0098's rolling ~10-position history already anticipates this
for full recovery). The sequence/map relationship is kept by tying each
held candidate to its own immutable event record and its own position in
the accumulated-movement sequence, rather than collapsing the chain to a
single scalar offset guess — consistent with the DNA lesson that an
*ordered sequence*, not a single marker, is what disambiguates.

What prevents indefinite accumulation: the same absolute outer boundary
0098 already established for recovery (±10 MM). Past that boundary without
resolution, NAVI does not keep holding — it declares LOST and requests
operator input, per the same precedent. **Open question (Conclusion §6):**
whether that same ±10-MM bound is the right one for *ordinary forward-
running* ambiguity (this document's scenarios) as opposed to the
full-recovery case 0098 was written for, where the locomotive's position
was already in question before the ambiguity began.

## D. Evidence integration

### 15. How the different kinds of evidence are actually combined

Not weighted scores, not likelihoods summed, not independent votes —
principle 9/0105 rule these out explicitly. The actual calculation is a
sequence of **set-narrowing constraints**, applied to the set of mapped
magnets, plus one independent gate on raw signal evidence:

1. **Existence gate** (§Q2): does the Hall evidence for this window clear
   the floor at all? If not, there is no candidate to narrow — see §Q18.
2. **Reachability constraint**: given the last trusted position, direction,
   and accumulated movement evidence (± the applicable tolerance, §Q9),
   compute the set of mapped magnets physically reachable right now. This
   is arithmetic on measured distance against `ROUTE_SPACING_MM`, not a
   score — principle 9's own stated example ("a measured distance... does
   not add points to a candidate landmark; it determines which landmarks
   were physically reachable at all").
3. **Polarity filter**: remove from that set any magnet whose
   `ROUTE_POLARITY` entry contradicts the observed polarity, once
   resolvable (§Q17 — do not apply this filter while polarity is still
   unresolved).
4. **Decision**: exactly one member remains after (2)+(3), and (1) was
   cleared → ACCEPT. Zero or more than one member remains → HOLD. (1) was
   never cleared → NO ENCOUNTER, regardless of (2)/(3).

This is deliberately not symmetric — existence (signal) and identity
(map/polarity/movement) are different questions answered by different
evidence, per §Q4, and are not merged into one number.

### 16. Avoiding double-counting correlated measurements

Group inputs by the **physical quantity** they actually measure, and use
one representative per quantity, preferring the direct one (principle 3):

- Hall curve duration and the Gaussian's fitted width (`sigma`) are two
  descriptions of the *same* fitted curve — one piece of evidence (signal
  existence/strength, step 1 above), not two independent confirmations.
- PWM and IR-measured distance both describe the *same* underlying
  quantity, distance traveled. IR is used when healthy; PWM/elapsed-time
  is its substitute when IR is unavailable (§Q9) — never both added
  together as if they were two independent confirmations of movement, which
  would double-count one physical fact as if it were corroborating
  evidence from two sources.
- Only genuinely independent physical quantities — curvature/existence
  (from the Hall waveform) versus location (from movement + map) versus
  field orientation (from polarity) — are allowed to jointly constrain the
  decision, because those really are separate facts about the world,
  each with its own failure mode.

### 17. Negative evidence versus missing evidence

Decision 0108 already establishes this distinction for IR specifically;
this is its direct extension to polarity and Hall, not a new principle:

- **IR:** unavailable/invalid is *missing* evidence — it does not shrink
  or exclude from the reachable-candidate set, and it is not read as "no
  movement occurred." It triggers the degraded-mode distance proxy (§Q9),
  not an assumption of zero travel. Contrast: IR *healthy*, reporting zero
  pulses, is a genuine measurement result (0108) and *is* evidence (of no
  movement), applicable to the reachability constraint.
- **Polarity:** a candidate whose curve has not yet matured enough to read
  a clean sign is *unresolved*, not disagreeing — it must not be filtered
  out by step 3 above while unresolved. Only an actually-read, disagreeing
  polarity is negative evidence.
- **Hall:** a sensor read failure (no sample, an electrical fault) is
  *missing* evidence and must not be read as "no magnet here." Hall
  samples that are present and show no departure from baseline are a
  genuine measurement result (principle 16/0108's own wording, "no
  significant movement measured" is a valid result, extended here to "no
  significant field departure measured") and function as weak negative
  evidence against an encounter having already occurred at that point —
  useful for §Q13's resolution-by-non-occurrence, but not by itself proof
  that a mapped magnet is missing or broken.

### 18. What NO ENCOUNTER actually means

It is a conclusion about the **signal alone**: for the observation window
in question, raw Hall evidence never cleared the §Q2 existence floor —
quiet track is a genuine, sufficient measurement result (principle 16),
independent of what the map expected. It is **not** "a candidate was
rejected on geographic grounds" (a candidate that clears the floor but
matches no reachable/polarity-compatible magnet is HOLD, §Q4/§Q15 step 4,
not NO ENCOUNTER), and it is not merely "no geographic advance was
justified" (HOLD also produces no advance, for a different reason). The
test that separates them: did raw Hall evidence ever cross the existence
floor? No → NO ENCOUNTER. Yes, but identity unresolved → HOLD. Collapsing
this distinction would let a geographic non-match erase the fact that
something real happened on the sensor, which carry-forward rule 1
explicitly forbids ("a sensor event... and a movement command are
different things").

## E. Operational behavior

### 19. Stopping mid-curve

**Established, applied concretely.** Decision 0106 (motive PWM = 0 is a
non-navigation interval for sensor changes) governs this directly, and
0107 (RTB is recognized from the phenomenon, not a timer) constrains how.
Concretely:

- If PWM goes to 0 while a candidate is open, the candidate's already-
  accumulated evidence (ΔBIC trace, duration-so-far, pre-candidate
  baseline fit) is **preserved**, neither discarded nor advanced.
- Samples gathered *while stationary* must not be allowed to accumulate
  toward either (a) more curve evidence (a parked locomotive cannot
  travel distance it did not travel, so duration-based gates must not
  keep ticking against wall-clock time while PWM = 0), or (b) a valid
  return-to-baseline (quiet-while-parked-next-to-a-magnet is a different
  physical claim than quiet-while-moving-past-one; 0106 already states
  PWM-0 sensor changes carry no navigation authority in either direction).
- Geographic evidence (last trusted position) is held unchanged across the
  stop (0104).

This is precisely what the retrospective's RVM AUTO MM064 incident
("Toby stopped over a magnet... contradiction") got wrong, and precisely
why this document treats it as a named operational case rather than an
emergent accident. **Contradiction flagged (Conclusion §7):** the current
research recognizer (`research/navi_wolf_pack/normal_curve_recognizer.py`,
commit `98db802`) consumes no PWM input at all, and its own candidate-
censoring timeout (2 seconds of wall-clock time without declaration) would
discard a genuinely open candidate during any stop longer than two
seconds. That is a direct conflict with the rule above and with 0106, not
a hypothetical one — it exists in the frozen research code today.

### 20. Worked example

**Setup:** trusted position MM45, direction CW, PWM 90, approaching MM46.
Direction CW is assumed because the map in §"Map values used above" is
indexed ascending CW and the question stipulates forward approach to the
next higher marker; this is a stipulation of the scenario, not an invented
map fact.

**Baseline run.** While accumulated IR distance since MM45 is below the
MM46 reachability window (roughly 255–345 mm, the existing ±15% tolerance
applied to 300 mm), quiet Hall samples produce repeated NO ENCOUNTER
(§Q18) — correct, because this is genuine quiet track, not evidence of
anything missing. A Hall departure opens a candidate once IR distance
enters that window; ΔBIC grows; once it clears the §Q2 floor and, once
resolvable, observed polarity reads South — the reachability constraint
(§Q15 step 2) already narrows the candidate set to {MM46} alone (MM47
would need ~600 mm, outside the window), so polarity confirms rather than
discriminates. **ACCEPT MM46.** Decisive evidence: the reachability
constraint, not polarity (ambiguous here) and not curve strength alone.

**(a) Weak but plausible Gaussian curve.** Same as above but the curve
only barely clears the §Q2 floor, well below the recognizer's own
declaration gate. **ACCEPT MM46** anyway — this is §Q3's case exactly,
because the reachability constraint already narrows to a single candidate
independent of curve strength; curve strength was only needed to establish
that *something* real occurred.

**(b) Polarity disagreement.** A curve clears the floor easily at the
MM46 distance window, but observed polarity reads North. The reachability
constraint again gives {MM46(S), MM47(S)} — the polarity filter removes
both, leaving the empty set. **HOLD.** NAVI retains MM45 as trusted,
records the encounter immutably as unresolved, and continues: this is
§Q4's worked case with the real map substituted in, and it differs from
`RvmNavigator.h`'s unconditional `Contradiction` precisely by not stopping
here.

**(c) Invalid IR distance.** IR is unavailable approaching MM46. NAVI uses
the degraded elapsed-time-at-PWM-90 proxy (§Q9) with a wider tolerance.
If a curve appears near the proxy's estimate with agreeing polarity and
the widened window still excludes MM47 (900 ms of known PWM-90 travel does
not plausibly cover 600 mm), **ACCEPT MM46** — the proxy is coarser but
still sufficient here, because the reachability gap between MM46 and MM47
(300 mm) is large relative to the proxy's expected error at this distance.
If the widened window could not exclude MM47, the correct outcome is
**HOLD**, pending IR's return or a disambiguating subsequent encounter
(§Q13) — not a forced guess.

**(d) Temporary HOLD followed by additional evidence.** Continuing from
(b): MM45 remains trusted. The locomotive continues forward. Quiet Hall
samples are observed through the distance window where MM47 would be
expected (~600 mm) — a genuine measurement result (§Q17), weakening MM47
as the held candidate's identity by non-occurrence. A subsequent curve then
appears with South polarity at a distance consistent with MM46 (~300 mm
from MM45, i.e., appearing on schedule once the locomotive is understood
to still be between MM45 and MM46). This resolves the earlier anomalous
North reading as not corresponding to any reachable mapped position at its
claimed location — it is annotated, not deleted, as an unresolved
artifact (carry-forward rule 2) — and the new South/MM46-distance
candidate is **ACCEPT**ed as MM46. Decisive evidence: the appearance, on
schedule, of a correctly-spaced, polarity-correct candidate that the
original anomalous reading does not match at all.

---

## Required conclusion

### 1. Compact decision algorithm

For each candidate that opens (clears the existence gate, §Q2/§Q15 step 1):

1. Compute the reachable-magnet set from the last trusted position,
   direction, and accumulated movement evidence (IR, or the degraded PWM
   proxy if IR is unavailable), within the applicable tolerance.
2. Filter out any member whose mapped polarity contradicts an *already-
   resolved* observed polarity (do not filter on unresolved polarity).
3. If exactly one member remains: **ACCEPT** — advance trusted position,
   reset the movement accumulator, log the resolving evidence.
4. If zero or more than one member remains: **HOLD** — preserve the raw
   observation, the current candidate set, and accumulated movement
   evidence; re-run steps 1–2 against all still-open holds whenever new
   evidence (further movement, a subsequent candidate's polarity,
   confirmed non-occurrence at a competing candidate's position) arrives.
5. If HOLD persists past the existing ±10-MM outer boundary (0098) without
   resolving to a single member: declare LOST, request operator
   declaration — do not keep holding indefinitely.
6. If the existence gate was never cleared for an observation window:
   **NO ENCOUNTER** — no candidate, no further steps.

### 2. Minimum state NAVI must retain

- Last trusted mapped position, direction, and the reference point
  (time/odometer) it was established at.
- Accumulated movement evidence since that reference, tagged as
  direct (IR) or degraded (PWM/time), never summed across both for the
  same interval.
- For each open/held candidate: its immutable raw observation (start time,
  ΔBIC summary, resolved polarity or "unresolved"), and its current
  reachable∩polarity-filtered candidate set.
- Current IR health state, kept separate from the distance value itself.
- How long the current hold has been open, against the outer boundary.

### 3. Evidence required for geographic ACCEPT

Existence-gate evidence (§Q2) clears, **and** exactly one mapped magnet is
both reachable (within the movement-evidence tolerance) and polarity-
compatible (matches if resolved; not excluded if unresolved).

### 4. Conditions that produce HOLD

Existence-gate evidence clears, but the reachable∩polarity-filtered
candidate set has zero or more than one member — including the case where
polarity directly contradicts the only otherwise-reachable candidate
(§Q4/§Q8), and the case where two or more reachable magnets remain
undistinguished pending more evidence (§Q10).

### 5. Conditions that produce NO ENCOUNTER

Existence-gate evidence never clears for the observation window: the Hall
signal is genuinely, sufficiently explained by continued baseline
behavior, independent of what the map expected there.

### 6. Decisions still requiring operator input or additional research

- Whether the §Q2 existence floor (a new distinction from the recognizer's
  own declaration gate) is the right split, and where exactly to set it.
- The degraded-mode tolerance widening factor for IR-unavailable spacing
  comparisons (§Q9) — no such number currently exists.
- Whether 0098's ±10-MM recovery boundary is the correct outer bound for
  *ordinary forward-running* HOLD (this document's scenarios), as opposed
  to the full lost-position recovery case it was written for (§Q14).
- Whether a held, unresolved candidate interpretation may ever be surfaced
  to station/peer consumers in any form, or must be strictly withheld
  until ACCEPT (§Q11) — carry-forward rule 3/5 argues for strict
  withholding, but this needs an explicit ruling for this specific design.
- The identity/baseline-trust split proposed in §Q5 (committing geographic
  identity before RTB, while still requiring RTB to re-trust the
  baseline) — plausible but new, and not yet reviewed.
- Everything beyond PWM 90 / Otto / CW-majority / single-locomotive
  operation (Stages 2–5 of the original assignment) — entirely
  unaddressed here, as in the prior proposal.

### 7. Contradictions with existing principles or requirements

- **A real, present contradiction, not hypothetical:** the frozen research
  recognizer (`98db802`) has no PWM input and a wall-clock 2-second
  candidate-censoring timeout. Any stop longer than two seconds mid-
  candidate would cause the *research code as currently written* to censor
  a genuinely open encounter, directly conflicting with decision 0106's
  requirement that PWM-0 intervals carry no navigation authority in either
  direction (§Q19). This must be resolved — the censoring logic needs a
  PWM-aware pause, not removal, since the two-second bound still serves a
  purpose while moving — before any Stage-1 implementation proceeds.
- **No other contradiction identified.** The remaining machinery above
  (existence gate, reachability narrowing, polarity filter, HOLD/RESOLVE,
  the ±10-MM outer bound) is composed from 0098, 0104, 0105, 0106, 0107
  and 0108 as written, not a departure from them. Where this document adds
  something new (§6 above), it is flagged as new rather than presented as
  already-settled doctrine.

## References

- `docs/NAVI_INTEGRATED_RECOGNITION_ARCHITECTURE_PROPOSAL_20261021.md`
  (the proposal this document examines).
- `firmware/test-programs/NAVI_RVM_SHAPE_MANUAL/RouteMap.h`,
  `RvmNavigator.h` (commit `3a8953c`); `firmware/programs/QUORUM/QUORUM.ino`
  (`NGR_DNA1`, `spacingMm`) — the real map values and the one-strike
  precedent this design departs from.
- `docs/decisions/0098-proximal-physical-recovery-before-polarity-scoring.md`,
  `0104` (Evidence Accumulation Before Revision), `0105` (relational
  evidence), `0106` (PWM-0 non-navigation interval), `0107` (Hall baseline
  is spatial; RTB from the phenomenon), `0108` (measurement applicability).
- `docs/reviews/navigation-retrospective-20261021/` (RVM AUTO r1–r5.1
  failure classification; MM064 stopped-over-magnet incident).
- `research/navi_wolf_pack/normal_curve_recognizer.py` (commit `98db802`).
