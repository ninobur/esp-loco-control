# NAVI Power of Normal — Governing Design Specification

**Status:** recovered implementation specification. Requirements marked **E** are established; **O** are genuinely open and must not be invented in firmware. Sources are identifiers in the Original Decision Record (ODR).

## 1. Scope and authority

| ID | Requirement | Status | Source |
|---|---|---|---|
| GOV-01 | NAVI’s task is target acquisition at the one expected mapped magnet, not general waveform classification. | E | ODR D-02, D-06 |
| GOV-02 | The normal architecture is `Expect → Observe → Qualify → Confirm/Advance`; protection is `late boundary → stop-and-hold`. | E | ODR D-03, D-06 |
| GOV-03 | Do not add recovery, alternative identity search, morphology gates, or failure-specific machinery without field evidence and explicit agreement. | E | ODR D-01, D-02, D-04 |

## 2. Geographic state and progression

| ID | Requirement | Status | Source |
|---|---|---|---|
| GEO-01 | Maintain one last confirmed marker, direction, and exactly one next expected marker from the map. | E | ODR D-02 |
| GEO-02 | Map adjacency, polarity, spacing, and movement history describe the expected target. | E | ODR D-02, D-07 |
| GEO-03 | Only a completed, qualified encounter matching the expected target confirms it and advances exactly one marker. | E | ODR D-02, D-06 |
| GEO-04 | A nonqualifying encounter leaves confirmed position and expected target unchanged; continue observing. | E | ODR D-02 |
| GEO-05 | Do not skip, search alternatives, reacquire geography, infer passage from timing, or recover from a surprise observation. | E | ODR D-02, D-05 |

## 3. Hall observation and candidate evidence

| ID | Requirement | Status | Source |
|---|---|---|---|
| HALL-01 | Record Hall observations continuously and unconditionally, independently of navigation judgment and motor authority. | E | ODR D-06 |
| HALL-02 | A candidate opens at a 70-count excursion on two consecutive samples (70×2); capture its opening polarity and preserve pre-trigger context. | E for sentinel/polarity; O for sampling representation | ODR D-06; R1 §1 |
| HALL-03 | Do not decide a candidate from an unfinished rolling window. Collect until the Hall signal establishes return-to-baseline completion. | E | ODR D-06 |
| HALL-04 | After completion, qualify using opening polarity, minimum physically plausible width, observed completion, and expected geographic consistency. | E | ODR D-06 |
| HALL-05 | General waveform shape—Gaussian fit, bell/smoothness score, lobe count, residual, template—is diagnostic only and cannot veto. | E | ODR D-01 |
| HALL-06 | A wrong polarity is simply nonqualifying. It is not a contradiction stop and does not cause identity search. | E | ODR D-02 |

## 4. Baseline and stationary behavior

| ID | Requirement | Status | Source |
|---|---|---|---|
| BASE-01 | Treat the baseline as spatial, ordinary-track/inter-magnet evidence, not an arbitrary quiet-time sample. | E | ODR D-08 |
| BASE-02 | Do not substitute a fixed 50 ms (or other elapsed time) rule for a 50 mm spatial notion. | E | ODR D-08 |
| BASE-03 | Avoid incorporating magnetic tail into a replacement baseline. | E | ODR D-08 |
| BASE-04 | Define exact median method, travel coordinate, eligibility, promotion, return band, persistence, and zero-PWM dwell behavior from evidence before implementation. | O | ODR D-08 |
| BASE-05 | Hall observation continues while stopped/held; a stop must not silently erase evidence or create a new location. | E | ODR D-03, D-06 |

## 5. Movement prediction and protective hold

| ID | Requirement | Status | Source |
|---|---|---|---|
| TIME-01 | Use map/movement history to predict the expected target’s neighborhood and normal arrival, never as confirmation by itself. | E | ODR D-07 |
| TIME-02 | Preserve timing provenance: Otto calibration event times are RTB anchored; Toby automatic event times are onset anchored. Do not mix them as equivalent. | E | ODR D-07 |
| TIME-03 | Each mapped interval requires an empirically validated, pace-normalized latest permissible arrival. | E | ODR D-03 |
| TIME-04 | Before that boundary, absence or nonqualification means continue observing for the same target. | E | ODR D-03 |
| TIME-05 | Boundary expiry is the sole NAVI condition that requests protective stop-and-hold; retain the last confirmed position and keep recording. | E | ODR D-03 |
| TIME-06 | Three-sigma is not an authorized boundary method. Select boundary statistics, pace adjustment, cap, and sample sufficiency only from validation. | O | ODR D-03 |

## 6. Operational integration and ownership

| ID | Requirement | Status | Source |
|---|---|---|---|
| OPS-01 | NAVI stop-and-hold is a motor request; it is not a Hall-observation shutdown or geographic reset. | E | ODR D-03 |
| OPS-02 | Station, throttle, bubble/dispatcher, manual commands, battery, and E-stop remain distinct operational authorities. They must not create hidden geographic writes. | E | ODR D-05; R1 §1 |
| OPS-03 | Define, test, and explicitly approve arbitration and release semantics among NAVI hold and the other motor authorities. | O | ODR unresolved #4 |
| OPS-04 | Audit the actual r5.1 host and used dependencies before grafting NAVI; identify reachable legacy morphology, recovery, baseline, and stop paths. | E | ODR D-05 |

## 7. Validation gates

| ID | Requirement | Status | Source |
|---|---|---|---|
| VAL-01 | First perform a read-only replay of the full agreed decision cycle against Otto and Toby evidence, including documented morphology false negatives. | E | ODR D-05 |
| VAL-02 | Prove native sample preservation of 70×2, completed-event behavior, no shape veto, one-marker-only advance, nonqualifying wait, and boundary-only NAVI hold. | E | ODR D-01–D-06 |
| VAL-03 | Broaden coverage across locomotive, direction, speed, and consist before freezing numerical thresholds. | E | ODR unresolved #5 |
| VAL-04 | Record unknown/missing evidence as an evidence gap; do not turn it into a guessed rule. | E | ODR source limit; D-04 |

## Implementation prohibition list

Until a later explicit decision: no recovery engine, hypothesis list, alternate-MM matcher, morphology veto, timing-only advancement, fixed quiet-time baseline surrogate, or three-sigma deadline. No firmware change is authorized by this recovered specification itself.
