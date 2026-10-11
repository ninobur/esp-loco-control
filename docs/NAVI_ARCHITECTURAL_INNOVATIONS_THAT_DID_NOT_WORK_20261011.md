# NGR NAVI — Architectural innovations that did not work

**Recorded:** 2026-10-11. **Provenance:** David supplied a prior ChatGPT retrospective in the Train Control conversation and explicitly requested that it be preserved in the repository.

**Status:** Historical engineering inventory, **not a governing decision**, firmware specification, or authorization to change code. The entries preserve the source retrospective's distinctions between **field-demonstrated failures**, **analysis/review-discovered defects**, **rejected designs**, and **questioned or untested proposals**. Its historical "dispositions" describe what occurred **at the time**; subsequent operator rulings and dated decision records govern new work. In particular, the retrospective's description of decision 0116 must not erase David's later October 11 consideration of restoring a 650-ms fallback in a different evidence structure. Do not mechanically promote any old numerical threshold, baseline alternative, or recovery rule into NAVI_EWO_0_4.

The recurring pattern is that an architectural mechanism introduced to solve a real problem depended on an assumption the physical railroad could not reliably satisfy.

## 1. The 650-ms Hall-only fallback
- **Objective:** Continue navigation when IR distance was unavailable.
- **Mechanism:** After a fixed interval, permit Hall confirmation without coherent IR distance.
- **Observed failure:** September 30: one sustained field generated false MM013, MM012, and MM011 confirmations.
- **Mistake:** Elapsed time was treated as proof of a new physical magnet.
- **Historical disposition:** Withdrawn for EWO by 0116. **Later October 11 discussion revisited degraded fallback**; the observed failure and its mechanism remain binding evidence regardless of the latest proposed rule.

## 2. IR health used to reject coherent distance
- **Objective:** Exclude defective optical measurements.
- **Mechanism:** Diagnostic flags, including inadequate contrast and counter changes, could invalidate IR distance.
- **Observed failure:** Normal stationary diagnostics triggered unavailable IR and enabled the faulty Hall fallback.
- **Mistake:** Conflating optical status with the integrity of cumulative completed-pulse measurements.
- **Disposition:** Preserve/report diagnostics; they do not automatically destroy a coherent cumulative measurement.

## 3. Fixed-time protection against repeated Hall events
- **Objective:** Reject repeated encounters with one magnet.
- **Mechanism:** Refractory/elapsed-time guard.
- **Failure:** A stationary or slowly moving locomotive can remain in a field longer than the guard; the September 30 repeated confirmations demonstrate the danger.
- **Disposition:** Time alone is insufficient evidence of physical separation. Physical separation, not mere timeout, must govern duplicate protection when that measurement is available.

## 4. Assuming full mapped interval distance after declaration
- **Objective:** Confirm the first magnet within ±15% of the nominal interval.
- **Mechanism:** Treat declaration as though it located the train at the preceding marker.
- **Observed failure:** October 2: the first 24 consecutive targets were recorded as missed despite Hall support.
- **Mistake:** Interval identity does not identify precise within-interval coordinate.
- **Disposition:** First appropriate Hall target becomes the physical landmark without a declaration-to-marker distance window (0118). A subsequent test reported 324 Hall-supported confirmations and zero misses.

## 5. Five-position IR-based startup Hall reference
- **Objective:** Prevent stationary samples from dominating the startup reference.
- **Mechanism:** Sample five distinct IR-observed physical positions, then form the Hall reference.
- **Problem:** Depends on IR movement and on those positions actually representing ordinary track.
- **Disposition:** Replaced by first eligible native Hall sample at nonzero PWM (0115). **Design rejection; not proof of repeated field failure.**

## 6. Fixed-distance Hall baseline replacement
- **Objective:** Refresh Hall reference after a verified magnet.
- **Mechanism:** 0–100 mm clearance; 100–200 mm reference collection.
- **Concern:** These distances are assumed clearance regions rather than an independently observed return to ordinary-track Hall behavior; some references were anomalous, but later operation was stable.
- **Disposition:** Questioned, **not conclusively disproved**. Direct Hall recognition of ordinary track remained a proposal, not a demonstrated replacement.

## 7. Suppressing Hall target evaluation during reference collection
- **Objective:** Avoid contaminating the reference with magnet observations.
- **Mechanism:** Redirect Hall samples exclusively to spatial-reference collection.
- **Observed failure:** At MM096, undercounting IR made an actual magnet approximately 300 mm away appear to lie in the 100–200 mm reference-collection region.
- **Mistake:** One imperfect sensor was allowed to determine whether another sensor's observations could have navigation meaning.
- **Disposition:** Later simplification rejected the suppression.

## 8. Automatic recovery after PWM-zero IR movement
- **Objective:** Reacquire position automatically after the wheel moved at PWM=0.
- **Mechanism:** Keep declared MM interval but reset fine offset and anchor to the next matching Hall onset.
- **Analysis failure:** MM001→002 is 340 mm; MM002 and MM003 are both South; MM002→003 is 330 mm. A South observation after 335 mm is ambiguous if the actual initial coordinate is unknown.
- **Mistake:** Interval identity and unsigned measured distance do not prove which matching magnet was crossed.
- **Disposition:** Rejected. Decision 0120 requires operator verification and position declaration after observed PWM-zero displacement.

## 9. Alternative-position search
- **Objective:** Recover when NAVI questions geographic identity.
- **Mechanism:** Consider candidate identities across the 171-magnet map, sometimes using a long retrospective window.
- **Observed danger:** A mathematically plausible sequence could imply a geographically impossible jump. Broad searching produced confidently wrong locations; the later 11-encounter approach was also abandoned.
- **Disposition:** Abandoned broad search. Narrow transparent hypothesis evaluation discussed earlier is a separate idea and is **not** automatically approved for 0_4.

## 10. Waveform morphology as navigation authority
- **Objective:** Identify true magnets from waveform shape.
- **Mechanism:** Opening/closure, width, polarity evolution, fit and lobes used to admit/refuse.
- **Observed failure:** Genuine magnets produced unexpected waveforms, including Toby's Arches WRONG_SHAPE refusal.
- **Disposition:** Morphology remains diagnostic, not an independent accept/reject authority (0080).

## 11. Adaptive baseline / return-to-rest schemes
- **Objective:** Adapt Hall detection to environmental variation.
- **Mechanism:** Settling, quiet/stability gates, baseline updates and return-to-baseline tests.
- **Observed failures:** A sustained magnetic field could be learned as background, or a stale baseline prevented re-arming and swallowed magnets.
- **Disposition:** Unrestricted adaptive-rest schemes abandoned. Direct recognition of the end of an encounter is related but **not the same design**.

## 12. Strike-based rejection and latched uncertainty
- **Objective:** Protect NAVI from suspect Hall events.
- **Mechanism:** Strikes, uncertainty latches and navigation withdrawal.
- **Failure:** Ordinary imperfect observations could accumulate into persistent loss of useful position, making additional evidence less helpful.
- **Disposition:** NAVI_ONE tough-love strike operation abandoned in favor of transparent judgment.

## 13. IR/Hall competing-world architectures
- **Objective:** Detect erroneous assumed position using independent measurements.
- **Mechanism:** Parallel interpretations and evidence-fusion recovery trees.
- **Failure:** Routine sensor imperfections produced unresolved geographic alternatives. In NAVI_IR 0.1, unavailable IR could be treated too much like contradictory IR.
- **Disposition:** Subsequent versions improved this distinction; the broad branching architecture was not retained.

## 14. CTO2 coordination based on confidently wrong positions
- **Objective:** Coordinate locomotives using occupancy, train length and clearance.
- **Mechanism:** Apply otherwise logical traffic rules to NAVI-reported geography.
- **Observed failure:** Rear-end collisions despite internally consistent clearance calculations based on wrong locomotive positions.
- **Mistake:** Downstream calculations cannot convert an uncertain location into physical truth.
- **Disposition:** CTO2 integrated approach abandoned; useful coordination ideas retained as prior art pending reliable navigation.

## Shared lessons

| Recurring mistake | Examples |
|---|---|
| Proxy mistaken for physical evidence | 650-ms timer; assumed reference clearance distance |
| Sensor preprocessing given excessive authority | Diagnostic IR veto; waveform-shape rejection; Hall suppression |
| Uncertainty converted to unsupported certainty | Map-wide relocation; PWM-zero automatic recovery |
| Incorrect premise propagated downstream | CTO2 position calculations; declaration treated as physical anchor |
| Defensive machinery creating its own faults | Strike latches; fallback navigation; competing-world recovery |

An individual observation establishes only what it physically measured:

- A Hall departure establishes a magnetic change, **not** a particular MM.
- An IR pulse establishes measuring-wheel rotation, **not necessarily signed locomotive route travel**.
- An operator declaration establishes geographic context, **not a precise within-interval coordinate**.
- A timer establishes elapsed time, **not movement**.

Preserve ideas that were never properly tested, but do not resurrect mechanisms that have demonstrated identifiable failure modes. The direct Hall ordinary-track-recognition and local native-IR processing proposals were not established failures.

## Source and evidentiary boundaries

This is a **faithful edited repository rendering** of the fourteen-item retrospective provided by David, not a fresh forensic audit of every historical branch. Its numerical observations and interpretations should be checked against original logs and decisions when used as a current implementation basis.

Useful repository cross-references: `docs/NAVI_MM045_NAVIGATION_INVESTIGATION_20261009.md`; `field-records/analysis/20260930_otto_ewo_ir_authority_correction.md`; `docs/NAVI_AUTO_CORRECTION_FAILURE_20260923.md`; `docs/NAVI_EWO_GOVERNING_DOCUMENTS.md`; decisions 0080, 0109–0122; `docs/NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md`.
