# 0092 - NAVI / IR decision model: physically possible positions first, pattern matching second

**Point 2 (the first-10-magnet IR startup calibration) superseded by 0094**
(2026-09-22): the ten-magnet IR trust-earning period is removed; see 0094.
Point 15 (the ~10-MM / ~3,000-mm position-correction history) is unaffected
and explicitly preserved by 0094. Nothing else here changes.
Status: Proposed (2026-09-22). These are the operator's decisions, reached in a
thorough review with Sam. They are not authoritative until the operator
ratifies this record.
Supersedes in part: 0090 (its sequence-authority *mechanism*: whole-route
search, 10/10 + ≥2 disagreements, window reset on reversal and multi-step
advance) and 0091 (IR trust resumption and the "IR stopped, Hall progressing"
response). The rulings of both remain part of the history; this record changes
what they mean going forward.
Decided by: David, with Sam. The text below is Sam's consolidation, which
the operator adopted verbatim: "This is what I decided. Please conserve in the
REPO."
Recorded by: Claude. Claude's analysis follows separately and is **not** part
of the decision.

## The decision (Sam's consolidation, verbatim)

> This is my summary of the decisions and principles you established during
> the questioning—not new proposals.
>
> **NAVI / IR decision model — September 22**
>
> 1. NAVI is the decision authority. Sensors provide evidence. NAVI applies
> explicit, inspectable rules. Judgment is primarily determining which
> established rule applies, not an opaque weighting or voting process.
>
> 2. IR trust is earned at session startup. Keep the existing first-10-magnet
> startup calibration. This may be conservative, but the startup interval is
> low risk and does not need another safeguard.
>
> 3. Trusted IR remains trusted through normal stops. Deceleration, zero
> movement, dwell, and restart do not inherently make IR less reliable. IR
> reporting decreasing movement as PWM decreases, followed by zero movement,
> is valuable physical evidence. Do not blind NAVI to it with a stale timer.
>
> 4. IR health and IR movement are different things. A health-monitoring
> function should recognize known IR failure signatures: inadequate contrast,
> sampling/electrical failure, implausible optical behavior, mechanical
> sensor/disk problems, etc. If those checks remain satisfactory, IR is
> considered valid.
>
> 5. IR health failure does not stop AUTO. If IR becomes demonstrably
> unhealthy, mark IR unavailable and notify the operator. NAVI continues AUTO
> using Hall/map/navigation. Once IR health is restored, IR becomes available
> again; a new 10-magnet calibration is not automatically required.
>
> 6. Zero movement is a measurement. Healthy IR reporting zero movement during
> an intended stop is normal. Healthy IR reporting no movement when movement
> is commanded, combined with no Hall progression, indicates that the
> locomotive is not progressing—possible derailment, slipping/stall,
> disconnected power, etc. That calls for shutdown and notification.
>
> 7. Healthy IR zero while successive Hall landmarks advance is presently
> inexplicable. Do not invent a diagnosis or elaborate automatic response for
> an unobserved/inexplicable condition. Record/notify it and learn from field
> evidence.
>
> 8. The current IR distance rule remains ±15%. This is provisional and
> subject to field experience. Repeated discrepancies will expose patterns
> where the model and physical railway are incompatible.
>
> 9. IR distance is measured from the last accepted MM anchor. The comparison
> is between accumulated IR travel from that MM and the mapped cumulative
> distance from that MM to the expected MM.
>
> 10. With valid IR, normal MM acceptance is simple. A Hall event must pass
> the two-sample ≥70-count detector, have expected polarity, and have IR/map
> distance within ±15%. Old timing should not veto stronger direct IR movement
> evidence. Timing remains useful evidence/telemetry and is particularly
> important when IR is unavailable.
>
> 11. An IR-outside-±15% Hall event is invalid for the expected MM. Treat the
> expected MM as missed. It missed its opportunity. NAVI advances its
> expectation to the following MM rather than waiting indefinitely for the
> missed one.
>
> If that following MM is correct, accept it normally. This is the expected
> recovery from a missed landmark.
>
> 12. One inaccurate observation does not create ambiguity. This is
> decisional inertia. NAVI remains where NAVI has good reason to believe it is
> unless a preponderance of evidence supports another explanation.
>
> A missed magnet is inconclusive history. A detected wrong-polarity magnet is
> evidence and its polarity is retained.
>
> 13. Repeated contradictions cause investigation, not panic. One
> wrong-polarity magnet does not overturn NAVI's world view. If subsequent
> observations also disagree, NAVI begins looking for alternative explanations
> using route history, IR progression, PWM and the actual MM map.
>
> 14. Position alternatives must be physically possible. The current
> whole-route search across 171 possible polarity patterns is conceptually
> wrong. A mathematical pattern match at MM140 is irrelevant if Toby's known
> history places him around MM40.
>
> The search is centered on NAVI's current believed position and bounded to
> ±10 MM: 20 alternative positions. Even within that range, IR progression
> should eliminate candidates requiring travel that did not occur. PWM/motion
> history can further constrain possibilities, especially when IR is
> unavailable.
>
> Possible positions first; pattern matching second.
>
> 15. Position correction uses approximately 10 MM / 3,000 mm of travel
> history. After session startup, autonomous repositioning cannot occur before
> this history exists. Thereafter it is a rolling physical history, not
> necessarily ten successful Hall observations.
>
> Missed magnets are UNKNOWN; they contribute neither polarity agreement nor
> disagreement. Actual observed polarities remain evidence even when they
> disagree with expectation.
>
> 16. Compare only physically viable candidates. For the incumbent and
> remaining proximal alternatives, compare the observed polarity history
> against the MM map. If the incumbent remains best, keep it.
>
> If one physically possible alternative is the unique best match and scores
> better than the incumbent, reposition to it. Any strictly better unique
> match is sufficient; no arbitrary additional scoring margin is required.
>
> If two alternatives tie for best, the evidence is non-decisional. Maintain
> the incumbent position.
>
> 17. Corrections are themselves provisional in the useful sense. If NAVI
> makes the wrong reassignment, the rolling process continues and later
> evidence can correct it again. There is no reason to build excessive
> protection around hypothetical incorrect corrections.
>
> 18. Do not erase the evidence after a correction. The rolling history is
> retained. It physically grounds the locomotive to a region of the Lowline
> and reduces future possibilities. A correction changes NAVI's interpretation
> of the observations; it does not make those observations disappear.
>
> 19. Reversal should not unnecessarily erase history. Useful position
> evidence should survive reversal if the implementation can handle the
> directional transformation cleanly. Do not discard everything merely because
> direction changed. The exact implementation remains to be examined.
>
> **Governing idea**
>
> The model we ended up describing is:
>
> NAVI has a world view supported by its travel history. Individual imperfect
> observations normally modify the evidence, not the world view. When
> discrepancies accumulate, NAVI considers only physically possible nearby
> explanations. IR and motion history constrain where the locomotive could
> be; the MM polarity map helps determine which of those possible locations
> best explains what was observed. NAVI changes its position only when a
> unique alternative has a preponderance of evidence.
>
> That is substantially richer—and more physically grounded—than the current
> "search all 171 ten-polarity patterns for a match" mechanism.

---

## Claude's analysis (not part of the decision)

Evidence: the three 2026-09-22 field logs in
`field-records/logs/20260922_navi_coherence_*` (967 accepted advances), plus
simulations against `ROUTE_POLARITY`. The scripts are reproducible from
`RouteMap.h`.

### A. Where NAVI_COHERENCE 0.5 differs from this model

| Point | 0.5 today |
|---|---|
| 2 "existing" 10-magnet IR calibration | **Not implemented.** 0.5 trusts IR from the first anchor. The calibration was proposed in 0091 but never built |
| 3 IR trusted through stops | Any non-TRACKING sample voids the interval, so every stop voids IR |
| 4–5 IR health vs movement | No health function. Optical state is the only test |
| 6 No IR + no Hall while driving → shutdown | No such path |
| 10 Two-sample **≥70-count** detector | 0.5 opens at **38** counts (two samples): `HALL_DEADBAND_COUNTS 25 + HALL_ENTRY_MARGIN_COUNTS 13`. 70 is decision 0088's NAVI_SIMPLIFIED convention |
| 11 Outside window → expected MM missed | Refused, then accepted two-step at the next magnet: the same outcome by a different route |
| 14 ±10 MM search | Whole route (171) |
| 15 Missed = UNKNOWN; rolling history | The window restarts after any multi-step advance |
| 16 Strictly better unique; ties hold | Unique 10/10, and incumbent must disagree on ≥2 |
| 18 History retained after correction | Retained (shifted) ✓ |
| 19 Reversal keeps history | The window is cleared on reversal |

### B. Point 10's 70-count threshold would have prevented today's only false advance

- Across 967 accepted advances in three runs, exactly one had a peak below 70:
  serial 245 at Arches (peak 38). That was the false one.
- The weak refused events were also sub-70: peaks 38, 40, 45 and 63.
- Caveat: the weakest real magnets peaked at **70, 71 and 72**, one in each
  run. A 70-count two-sample opening would sit right at their edge. Real
  magnets near that edge could go undetected. That would be a missed magnet,
  which the model already handles (point 11), not a false one.

### C. Point 11 needs one distinction: too early is not missed

- The event can fall **short** of the window (the magnet not yet reached) or
  **long** (the magnet passed).
- Today 11 of the 13 refusals were short: magnets read twice or weak
  openings, 10–50 mm after a genuine magnet.
- Read literally, point 11 would treat each of those as "expected MM missed"
  and advance the expectation, putting NAVI one ahead each time. The next real
  magnet would then look short against the new expectation.
- Suggested reading for Sam:
  - **Long** outside the window: the expected MM was missed; advance the
    expectation (MM122 today, 0.837, was a near miss of this kind).
  - **Short** outside the window: not a landmark; the expectation stands.

### D. Point 16 ("any strictly better unique match") with the ±10 bound

- Some windows within ±10 MM differ from the true window by **one** polarity:
  the minimum Hamming distance within ±10 is 1. Within ±3 it is 2.
- **Single-misread simulation.** Rolling 10-magnet history, the rule tested at
  every magnet, one misread at each possible position, every start, both
  directions:
  - 7% of cases (240/3,420) make a false correction.
  - Every false correction is a jump of **6–10 MM** (1.8–3 m), because nearer
    windows always differ on ≥2.
  - All of them **self-heal within 2–9 magnets** (median 2), exactly as
    point 17 expects.
- **Field rate of misreads:** 0 polarity misreads in 967 accepted advances
  today.
- **Point 14's IR elimination is what would stop these jumps.** A 6–10 MM
  jump means 6–10 magnets were missed or invented. An IR-validated history
  rules that out (each span was measured). Point 17 makes the jump harmless in
  open running. Near a station, a transient 6–10 MM jump could arm or abandon
  an approach for the two or so magnets before it heals.
- **What IR can and cannot eliminate.** It rules out candidates that need
  missed or extra magnets. It cannot tell apart two candidates that differ
  only by a constant offset over the same history, because the route's spans
  are nearly uniform (280–355 mm, mostly 300). There, polarity decides.

### E. Point 7 needs a statement of what IR does meanwhile

- If IR stays "healthy" but reports zero while Hall advances, and IR still
  gates acceptance (point 10), then **every Hall event fails the window**
  (0 mm) and NAVI's position freezes while Toby moves.
- Patio today was exactly this case: a car left standing, its link healthy.
  0.5 survived only because a standing car's optical state self-invalidated.
- Point 4 lists inadequate contrast as a failure signature, but a *correctly*
  stopped car also shows low contrast. The health function will need to tell
  "stopped" apart from "failed" (point 3). One available tell is Hall
  progression against IR zero, which is point 7's condition itself.
- **Suggestion:** under point 7, record and notify (as decided), and suspend
  IR's gating role until the stop and restart. That is not a diagnosis, only
  preventing a frozen position.

### F. Point 2 and point 5 interact

- Point 5 restores IR without recalibration once health returns. Point 2's
  calibration is then a once-per-session event.
- At Patio today the car was recoupled mid-session. Its count resumed from
  its own running total, so a restored IR measures from whatever anchor is
  current, and that is consistent with point 9.

## Implications and unintended-consequence risks

- **The whole-route search goes.** A true offset larger than ±10 MM (a wrong
  declaration by 11+, or handling that moves the loco further) can no longer
  be corrected automatically. It will show as repeated discrepancies with no
  viable candidate. Under point 1 that is UNKNOWN, and the operator
  re-declares.
- **"Strictly better, no margin" trades false jumps for speed** (analysis D).
  It is acceptable because corrections self-heal (point 17). Its safety rests
  on point 14's IR elimination, and while IR is unavailable only PWM/motion
  history constrains the candidates.
- **A 70-count detector** removes the sub-70 spurious openings seen today but
  sits at the edge of the weakest real magnets (70–72).
- **Keeping history through reversal and missed magnets** gives more evidence
  per decision. It also means one early misread can influence decisions for
  longer. Point 17 again carries that.
