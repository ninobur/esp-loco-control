# NAVI Original Decision Record — Power of Normal recovery

**Purpose:** evidence archive, not a new design. It preserves the Oct. 21–24 agreements accessible in this Work session.

## Source key and retrieval limit

| Key | Source | Authority/status |
|---|---|---|
| C | Supplied ChatGPT conversation extract, messages 610–623, 24 Oct. 2026 | Primary; directly available |
| B | Supplied conversation breadcrumb, “Baseline Revision Analysis,” message 31 | Primary fragment |
| R1 | Conference record, `9a6bbb7b` | Durable repository record; reconciliation target |
| R2 | Clean-sheet proposal, `04ce4b6` | Proposal, not authority |
| R3 | Integrated proposal, `340b999` | Later proposal; baseline comparison only |

The supplied extract begins after the original “Ten Commandments of NAVI” discussion. Its text was not retrievable. No quotation or decision from that missing segment is invented. The requested independent review at `05f9682` was unavailable in the checkout and named remote branch; this is an evidence gap.

## Chronological agreements

### D-01 — Shape is diagnostic, never an admission veto (Proposition 7)

**Original agreement:** “NAVI uses the 70 × 2 opening, observed excursion completion, minimum physically plausible width, and geographic consistency to confirm the expected magnet. Shape matching is not an admission requirement.”

The evidence was 1,523/1,528 genuine accepted events passing a 0.13 Gaussian ceiling; five real events would still be rejected. In the PWM-90 rebound survey, Gaussian fit passed 122/149 rebounds and smoothness 149/149. Historical MM110, MM146, MM169, and MM113 false negatives were principally morphology rejections. Gaussian/bell metrics remain retrospective diagnostic/calibration evidence only. **Final: established.** [C:604–609; R1 Prop 7]

### D-02 — Exactly one expected target; no identity search (Proposition 8)

**Original wording:** “There is only one legitimate next magnet. NAVI either confirms it or continues looking for it.” The target is set by last confirmed position, direction, and map. A wrong-polarity, too-short, or otherwise nonqualifying encounter changes neither confirmed position nor target. It is not a reason to consider another MM. **Final: established.** [C:612–614; R1 Prop 8]

### D-03 — Late arrival is the sole NAVI stop trigger

“Keep waiting, not stop. Stop only happens if the boundary is reached with no confirmed magnet at all.” The per-interval, empirically validated, pace-normalized latest permissible arrival commands stop-and-hold; position is retained and Hall observation continues. NAVI diagnoses no cause while stopping. **Final: behavior established; boundary method and pace cap open.** [C:612–615; R1 §1 and Prop 6B]

### D-04 — Normal operation defines the model (Proposition 9)

No failure-specific navigation mechanism is introduced unless field evidence shows the normal cycle plus stop-and-hold is inadequate. This discipline demoted Gaussian shape and rejected three-sigma rather than preserving either as theoretical protection. **Final: established architectural constraint.** [C:615–616]

### D-05 — Demonstrate simplicity by host audit and replay (Proposition 10)

Recognition machinery was considered sufficient; geographic recovery is not to be designed. Required proof work is a read-only historical replay and a read-only audit for reachable vestigial recovery/morphology logic in the actual host: `NAVI_RVM_AUTO_STATIONS_BUBBLE` r5.1, `5b7a518b`, including used shared headers. No cleanup was authorized. **Final: established verification requirement.** [C:617–623]

### D-06 — Continuous observation and completed evidence

The agreed cycle is **Expect → Observe → Qualify → Confirm and Advance**. Hall observation is continuous and unconditional. A 70×2 opening is the candidate sentinel. The signal itself must establish completion by return to baseline before retrospective qualification. Correct opening polarity, minimum physically plausible width, observed completion, and expected geography are required; shape is not. **Final: established except return-band and width details.** [C:611, 613, 615; R1 §1]

### D-07 — Movement predicts; Hall confirms

Movement history predicts the expected neighborhood and arrival, but cannot confirm passage. Otto calibration timing is RTB anchored; Toby automatic timing is onset anchored, so the two historical conventions must not be conflated. **Final: established; exact prediction method remains open.** [R1 Props 3–4]

### D-08 — Spatial, inter-magnet baseline; no timer substitution

David rejected silently substituting “50 ms or another time interval for 50 mm”: “Let’s just not use it.” The available primary fragment says: “Use Magnet Close to start the Baseline measurement and Magnet open 70ADCx2 to stop,” with a concern not to capture a magnet tail; PWM=0 was identified as a condition that prevents that concern. **Final: the spatial/inter-magnet principle is established.** The exact median/promotion/return algorithm is not recoverable from this fragment alone. [B]

## Rejected or non-authoritative alternatives

| Alternative | Disposition |
|---|---|
| Gaussian residual, bell, smoothness, lobe count as admission gate | Rejected |
| Search/reacquisition/recovery among alternative MMs | Rejected |
| Wrong polarity as immediate abnormal stop | Rejected |
| Three-sigma late-arrival rule | Rejected; replacement open |
| Fixed time window substituted for spatial baseline distance | Rejected |
| Firmware cleanup during audit | Unauthorized |

## Genuine unresolved questions

1. Native sampling representation; exact baseline/return-to-baseline and median promotion behavior, including zero-PWM dwell.
2. Polarity measurement convention; width convention and minimum value.
3. Interval-boundary method, data sufficiency, pace adjustment/cap.
4. Protective-hold release and motor/station/manual/E-stop arbitration.
5. Wider locomotive/speed/direction coverage and missing raw MM110/MM146 data.
