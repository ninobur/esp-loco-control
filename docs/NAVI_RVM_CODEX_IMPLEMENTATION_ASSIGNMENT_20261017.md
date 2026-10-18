# NAVI_RVM — Codex Implementation Assignment

**Canonical assignment copied from [GitHub Issue #10](https://github.com/ninobur/esp-loco-control/issues/10) on 2026-10-17.**

## Assignment: build a flashable NAVI_RVM sketch for Otto

**Implementation owner: Codex. Independent code reviewer: Claude.** This issue is the fixed handoff; do not treat the exploratory kernel as validated. Work on a dedicated branch, do not alter operational firmware, flash hardware, or change live services. Deliver code and results **pushed to GitHub** so Claude can inspect them remotely.

### Goal
Implement the actual persistent, continuous Hall-landscape recognizer, with the map as a fixed linear puzzle board and one expected next MM. The operator wants a **compilable field-test candidate**, not another research prospectus or a mandatory exhaustive simulation. Keep existing motor control, independent E-STOP, operator declaration, STOP/GO memory, and tile/station routines. Preserve manual-control behavior. No mid-run reversal.

### Required source reading
- `docs/NAVI_Rear_View_Window_Architecture.md` and `docs/NAVI_Rear_View_Window_Mathematical_Specification_M1.md` on `docs/navi-rear-view-window-architecture-20261017`.
- Claude M0/M1 independent reviews on `claude/navi-rvm-m0-review-20261017` are **advisory**.
- Existing Otto hardware/firmware, map, station-control integration, Hall/IR data and original recordings. Verify actual source locations; do not invent data or ground truth.
- Exploratory kernel `research/navi_rvm_kernel_prototype/` on `research/navi-rvm-kernel-prototype-20261017` (commit `131a82c1`) is **non-governing** and known defective.

### Governing mechanism
- Use **continuous waveform context** and a fixed fuzzy landscape, not a Hall-event counter with additional accept/reject gates. Each piece `MM(j-1) → BL → MM(j)` overlaps the next, but the shared MM contributes only **one** new quality assessment.
- Operator-declared location/direction immediately initializes the entire internal state consistently, including search bounds and phase. One expected next geographic location; no 171-position re-localization search.
- Recognition of the developing **whole piece**, including appearance of the expected new MM in context, produces one idempotent tile token. A weak/early leading lobe or flat trailing lobe is interpreted as part of an ensemble; do not equate first ADC departure with geographic commitment.
- Normal progression is expected. An atypical MM can count as progression but **must not become the new alignment anchor**. The next piece compares with the unchanged map and previously coherent context. Five successive **new-magnet** incoherent pieces is a provisional LOSS OF PATTERN criterion; do not double count the overlapping MM.
- PWM is movement context, not proof of displacement. IR supports and cannot veto coherent Hall/map progression; IR interval quality is determined from IR's **own integrity** and resets for the next interval (no inherited guilt). A genuine commanded stall → existing E-STOP; sustained loss of geographic coherence → controlled STOP. Avoid unnecessary stops: uninterrupted correct operation is the primary goal.
- Ordinary PWM-zero STOP/GO retains position, developing ensemble, and reference. No geographic advancement/rebaseline from PWM-zero sensor activity. Do not implement speculative coasting handling or model succession.
- Use **one fixed reference** from available recordings. No automatic learning, rollback UI, new sensors, or model-management subsystem in this first candidate.
- Keep existing station landmarks and stop routines initially; report any necessary compatibility change. Independent later per-station/direction adjustment remains possible.

### Critical corrections from Codex's review of the prototype
1. Constructor seeded the frontier at the declared MM but left its search center at zero; starting from MM other than zero could get stuck. **Test geographically separated start declarations.**
2. Pruning removed pre-landmark alternatives, making the commitment margin **infinite**. **Never interpret computational pruning as positive magnetic evidence.** Confidence must arise from actual observed waveform.
3. A corrupted 10-sample interval produced premature advancement and shifted alignment; a final total of four advancements concealed the failure. **Validate every individual commitment and the persistent map-backed context, not just final counts.**
The old kernel's `mm_peak` crossing and four-sample confirmation are not approved recognition criteria. Reuse its continuous alignment primitive only if its conceptual and technical deficiencies are corrected.

### Engineering latitude and boundaries
Codex may choose appropriate bounded mathematical methods and optimize for the ESP32, but **must not silently replace continuous pattern recognition with a cascade of per-lobe acceptance thresholds, morphology gates or geographic recovery logic**. If a true conflict prevents implementation, document the conflict and implement the closest honest testable candidate; do not claim fidelity when absent.

### Minimum verification
- Compile against the **actual Otto ESP32 target**; report toolchain, memory and timing feasibility.
- Verify nonzero starting tile, normal pattern progression, split/multilobe immunity, one anomalous MM followed by a normal one without reanchor, ordinary STOP/GO, motor/manual integration and independent E-STOP.
- Use focused recorded-data checks where feasible, not an exhaustive replay prerequisite. A synthetic smoke test alone is not evidence of successful navigation.
- Log one row per decision: expected/committed MM, source serial/time range, commit time, match quality, applied PWM, IR integrity, current trusted context, mismatch streak and reason. Never use future data for a past live decision.
- Identify known limitations candidly. Do not fabricate successful field results.

### Required GitHub deliverables for Claude
Push a dedicated **development branch** to `ninobur/esp-loco-control`. Include:
1. Complete compilable `.ino` sketch and necessary headers/reference data.
2. `docs/NAVI_RVM_IMPLEMENTATION_RECONCILIATION.md`: each governing requirement, exact code location, implemented/deferred/contradicted, with reasons.
3. `docs/NAVI_RVM_BUILD_AND_TEST_REPORT.md`: exact compile commands/results, ESP32 memory, focused checks, failures, known limitations and source provenance.
4. `docs/NAVI_RVM_FIELD_TEST_PROCEDURE.md`: safe initial single-locomotive test, expected telemetry and rollback procedure.
5. Any small reproducible test scripts and **textual result files** needed for review, committed (not only local paths). Large raw recordings can be linked by existing repository provenance rather than duplicated.

**At completion:** report branch name, full commit SHA, exact sketch path and clickable GitHub links to the three documents and test evidence. Confirm all commits are **pushed** and remotely visible. Claude must be able to evaluate source and results from the repo alone, without accessing Codex's local worktree.

**Stop before flashing Otto or modifying operational services.**

---

**Review handoff:** Push all implementation and test evidence to a dedicated GitHub branch. Claude must be able to review everything through GitHub, without access to a local worktree.
