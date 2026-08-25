# Magnet Admission Doctrine for TEMPLATES

## Status: mandatory design constraint

- **Must be read and explicitly considered before coding `TEMPLATES`, the successor to `QUORUM`.**
- This doctrine records an operator decision about error handling and the boundary between Hall acquisition, map validation, and recovery.
- It authorizes the architectural priority stated here: **precision over recall at the navigation-admission boundary**.
- It does not approve a numerical threshold, filter, waveform template, timing constant, firmware implementation, flash, or field operation. Those require replay evidence and separate review.
- It is a companion to `TARGET_ACQUISITION_DECISION_LOGIC.md`. Neither document replaces the other.

## The governing asymmetry

TEMPLATES must prefer omission of a questionable response over inclusion of a false response.

> **False negatives are recoverable. False positives corrupt the coordinate system. Admission must therefore favor precision over recall.**

> **Upstream classification bias must align with downstream recovery capability. Because navigation can recover from omitted valid markers, signal processing must favor false negatives over false positives. A doubtful response should be discarded, producing a modeled omission, rather than admitted, producing an unmodeled position insertion.**
>
> **Rejecting spikes turns uncertainty into the recoverable error. Accepting them turns uncertainty into coordinate-system corruption.**

In this system, a “missing magnet” need not mean that the Hall sensor physically failed to detect a magnet. The operator's field conclusion is that the Hall sensors detect the track magnets. A missing-magnet condition may instead be created deliberately when the admission layer rejects a response that was actually valid but insufficiently trustworthy.

That is the safer error:

- **Omit a valid magnet:** the primary position falls behind by one marker. The downstream recovery system receives the bounded omission error it was designed to compensate for.
- **Include a phantom:** the primary position advances for a marker that does not exist. Later genuine magnets are compared with the wrong map positions; correct broad curves begin reporting as disagreements; contaminated evidence enters hypothesis scoring; and recovery may adopt a false offset or exhaust its candidate window.

TEMPLATES must not balance those failures as if they had equal cost. When evidence is doubtful, omission is the intended bias.

## Field evidence that made the rule necessary

The 2026-08-24 trace session recorded Toby and Otto with the same QUORUM detector thresholds and full Hall/navigation tracing.

| Capture | Opened excursions | Under-40-ms rejects | Timing-gate phantom rejects | Disagreements | `NO_QUORUM` |
|---|---:|---:|---:|---:|---:|
| Toby CCW | 1,202 | 1 | 1 | 1 | 0 |
| Otto CCW | 1,169 | 626 | 47 | 15 | 1 |
| Otto CW, restarted boot session | 1,637 | 639 | 35 | 18 | 1 |

Otto's rejected excursions were overwhelmingly 20–22 ms pulses and all were shorter than 40 ms. Toby produced essentially none. Otto nevertheless produced legitimate agreeing magnet curves with nearly the same median shape as Toby:

| Capture | Median agreeing peak | Median agreeing duration |
|---|---:|---:|
| Toby CCW | 198 counts | 163 ms |
| Otto CCW | 180 counts | 168 ms |
| Otto CW | 187 counts | 163 ms |

The evidence therefore does not describe a Hall channel that simply cannot see magnets. It describes normal magnet curves mixed with a very large added pulse population.

Some pulses passed QUORUM's current 40 ms floor. One post-reboot CW disagreement at map position 061 was only 43 ms wide with a 39-count peak, barely over the entry and duration thresholds. It advanced navigation before map polarity was considered. In the final CW failure, short admitted responses and broader responses accumulated around map positions 114–135; QUORUM opened, remained tied through its bounded evaluation, and reached `NO_QUORUM`. The operator independently observed an approximately 19-marker difference between physical and believed position near the end.

This is the demonstrated failure chain:

```text
added spikes or fragmented responses
        -> marginal artifact passes simple threshold/floor
        -> artifact receives navigation standing and advances position
        -> genuine later curve is compared with the wrong map marker
        -> disagreements and contaminated hypothesis evidence accumulate
        -> wrong adoption, unresolved tie, or NO_QUORUM
```

QUORUM's timing-conservation gate prevented many artifacts from advancing, but it did not categorically reject every physically impossible early response. Events separated by only a small fraction of an approximately 1,200 ms expected interval still received navigation standing in the recorded failures. TEMPLATES must make physical admissibility a precondition, not a later attempt at repair.

## Required division of responsibility

### 1. Passage acquisition: did a distinct magnet-shaped event occur?

The acquisition layer receives the rich Hall waveform, not merely a threshold-crossing notification. It evaluates evidence such as:

- entry, rise, crest, decline, and settled exit;
- event width in its speed context;
- peak excursion from an evidence-qualified baseline;
- signed and absolute integrated flux;
- completeness, gaps, saturation, and baseline quality;
- actual and commanded PWM, direction, and elapsed time;
- whether an apparent companion is part of the same passage.

An isolated narrow pulse is not a magnet. A pulse does not become a magnet merely by crossing an amplitude threshold or surviving a single duration floor.

A response that fails passage acquisition is **discarded for navigation**:

- no position advance;
- no evidence-ring insertion;
- no hypothesis score;
- no later resurrection as a possible marker.

It may still be counted, traced, and published diagnostically. Awareness does not grant candidacy.

### 2. Physical arrival gate: could the next distinct magnet have arrived yet?

After a credible magnet passage, TEMPLATES must enforce a conservative earliest physically possible arrival for a separate next marker.

The bound must be justified by layout spacing and conservatively bounded or measured motion. It may use speed/PWM context, but it must include an absolute physical impossibility boundary where appropriate. It must not be a guessed delay chosen only to improve a dataset.

Activity inside this window is handled as one of two things:

- part of the current passage, including a defensible fringe or opposite-polarity edge lobe; or
- a pulse/artifact discarded without navigation standing.

It is not accepted as another marker merely so downstream recovery can decide later.

### 3. Map target validation: is it the physically expected next magnet?

Only a waveform-qualified, physically timely passage reaches map validation. The expected marker's polarity, route order, and direction are then tested **before** the primary position changes.

If a credible passage contradicts the expected polarity, TEMPLATES says “No Way”:

- do not advance the primary position;
- do not pretend it was the expected marker;
- record the credible contradiction and reduce confidence as approved;
- continue looking for the expected marker or enter a separately reviewed recovery/stop response.

A wrong-polarity observation is not automatically an electrical spike. Shape and physical timing decide whether it is an artifact; map polarity decides whether a credible passage is the expected target.

### 4. Recovery: compensate for credible omission, not admitted garbage

The recovery layer is deliberately downstream. It receives only credible magnet passages. Its principal recoverable error is the conservative classifier's omission of one or more valid magnets.

Recovery must not override a categorical physical impossibility, and rejected pulses must not testify for alternate hypotheses. Candidate ranges and stopping rules must be reviewed against the deliberate precision-over-recall policy, so the omission errors created by conservative admission are actually representable.

## Opposite-polarity edge responses

Past field work identified reverse-polarity activity at magnetic-field edges. This remains a required test case, but the 2026-08-24 traces show why polarity alone is not a classifier.

In Otto's restarted CW session, 205 broad responses were followed within 350 ms by an opposite-polarity threshold excursion. Of those companions, 189 were 5 ms or shorter; their median width was 1 ms and median amplitude was approximately 41 counts. Numerous same-polarity companions also occurred. Those near-threshold impulses are far more consistent with the recorded pulse population than with a second broad magnetic passage.

A smaller group of wider opposite-polarity companions remains unresolved and must be plotted and reviewed. TEMPLATES must be able to keep a passage open through defensible edge structure or merge related lobes into one passage. It must not count each polarity crossing as a separate magnet, and it must not discard a broad credible passage solely because its polarity contradicts the current map belief.

## Required decision sequence

```text
Hall samples
    |
    +-- incomplete, narrow, pulse-like, or physically impossible passage?
    |       yes -> discard and log; no navigation standing
    |
    +-- part of the current passage or its edge/fringe structure?
    |       yes -> merge/close the one passage; never count twice
    |
    +-- credible distinct broad passage?
            no  -> omit and log; recovery may later compensate
            yes
             |
             +-- too early for the next physical marker?
             |       yes -> discard; no advance and no hypothesis evidence
             |
             +-- matches the expected map target?
                     yes -> advance exactly once
                     no  -> “No Way”; do not advance; invoke only the
                            separately approved confidence/recovery policy
```

## Consequences for implementation and validation

Before TEMPLATES code is approved:

1. Replay Toby and Otto's complete traces through candidate passage and arrival gates.
2. Report false inclusions and deliberate omissions separately. Do not optimize a single blended “accuracy” score.
3. Demonstrate that every rejected pulse leaves primary position, evidence rings, and hypothesis scores unchanged.
4. Demonstrate that an omitted valid curve produces a recoverable missing-marker error.
5. Include slow station approaches, curves, stops, reversals, PWM changes, baseline shifts, incomplete data, and opposite-polarity edge structure.
6. Normalize apparent location clusters for time spent, speed, PWM, and station behavior before claiming a track-location cause.
7. Use independent operator anchors and the magnet map. Internal polarity agreement cannot validate absolute position by itself.
8. Inspect and improve Otto's physical Hall installation—especially signal routing near motor/high-current wiring, grounding, connectors, decoupling, and filtering—before treating firmware rejection as the primary cure.
9. Prefer eliminating the spike source. Admission gates are defense in depth, not permission to leave a correctable electrical defect in place.
10. Select numerical thresholds only from evidence. The doctrine approves the error bias and layer boundaries, not the numbers.

## Mandatory coding review questions

Every TEMPLATES Hall/navigation implementation review must answer:

- Can any rejected or impossible response advance position?
- Can it enter, alter, or later re-enter hypothesis evidence?
- What proves an accepted event is a distinct passage rather than a pulse or companion lobe?
- What is the earliest physically possible next-marker time, and what evidence justifies it?
- Is expected polarity checked before primary position changes?
- If the classifier omits a genuine magnet, can recovery represent and repair that error?
- Are reverse-polarity edge lobes treated as waveform structure rather than automatically as another marker?
- Are operator-grounded physical anchors kept separate from the navigator's own belief?

TEMPLATES code is not ready for approval if these questions are unanswered.

## Related required reading

- `TARGET_ACQUISITION_DECISION_LOGIC.md`
- `HALL_PROCESSING_PREBUILD_REVIEW_PLAN.md`
- `TARGET_ACQUISITION_GUIDANCE.md`
- `REVIEW_NOTES.md`
- `docs/QUORUM_NAV_AUDIT_AND_GATE_PROTOTYPE.md`
