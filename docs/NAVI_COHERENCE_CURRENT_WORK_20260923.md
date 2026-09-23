# NAVI_COHERENCE: current work and catch-up

Date: 2026-09-23. Author: Codex.
Scope: read-only firmware review, baseline verification, and documentation
housekeeping. No navigation implementation, firmware flash or hardware control.

**Later update, 2026-09-23:** David requested the first implementation step.
The independent IR classes and executable tests now exist in
`firmware/reference/NAVI_COHERENCE/IR_ARCHITECTURE_0_4/`. Host tests and an ESP32 compile
fixture pass; NAVI_COHERENCE 0.5 and the shared detector are unchanged. See
[the step-one report](NAVI_IR_HEALTH_STEP1_20260923.md). The original catch-up
findings below remain the record of the pre-implementation inspection.

## Lineage and present priority

**Subsequent integration, 2026-09-23:** David requested sketch health indicators
before field laps. NAVI_COHERENCE 0.6 IR_HEALTH now adds observation-only
instrument/readiness/epoch/MM-reference telemetry alongside unchanged 0.5
navigation. Host tests and the full ESP32 build pass. Not flashed or field
accepted. TX stationary contrast remains unresolved. See the
[0.6 implementation and field handoff](NAVI_COHERENCE_0_6_IR_HEALTH_20260923.md).

David clarified: "Actually, your NAVI_IR work was seminal. We just carried it
forward." NAVI_COHERENCE continues that work; it does not discard its provenance.
Sam led the subsequent design iterations, David supplied decisions and field
observations, and Claude supplied repo reviews, integration work and run records.
The practical implementation base is now the field-exercised COHERENCE 0.5,
not a restart from NAVI_IR 0.2 or a new navigation rewrite.

David's current priority is implementing the outcomes of the twenty-question
design discussion. His supplied handoff is preserved verbatim in
[NAVI_COHERENCE_TWENTY_QUESTIONS_HANDOFF_20260923.txt](NAVI_COHERENCE_TWENTY_QUESTIONS_HANDOFF_20260923.txt).
It is a supplied consolidation, not a claim that the complete discussion was
available here. It also resolves the early-versus-missed distinction in 0092.

The engineering structure is detection, then physical constraints, then NAVI
judgment. Sensors report measurements; NAVI owns interpretation. The twenty
questions are not twenty new exception rules.

## The cyclometer test for every build

David reaffirmed this as central to the architecture on 2026-09-23, recalling
Sam's formulation: is this something a cyclometer would know, or a rider would
know? Use it as a code-placement and review test, not merely an analogy in prose.

- **Cyclometer / IR:** measured wheel counts, elapsed measurement time, distance,
  derived speed, measurement health/readiness, and continuity of its own data.
  Fresh valid unchanged counts report zero measured movement. No fresh or usable
  data reports unavailable measurement, not zero. Neither report diagnoses why
  the locomotive is or is not progressing.
- **Rider / NAVI:** intended movement, PWM and operating state, route location,
  MM identity, consistency with Hall/map/history, response to discrepancies,
  fallback navigation, and whether AUTO should continue or stop.
- **Boundary:** IR measures travel without knowing a single MM. NAVI establishes
  an MM reference using the corresponding measured count and timestamp. The
  reference belongs to navigation, even when packaged alongside odometry classes.
- **Review question:** if an IR method needs the route map, expected magnet,
  commanded propulsion, station state, or a theory of locomotive behavior to
  reach its conclusion, that interpretation belongs in NAVI. Instrument fault
  detection remains in the measurement layer; operational response does not.
- **Failure and recovery:** an Epoch break states that measured continuity was
  lost. It neither relocates the locomotive nor commands a stop. Recovery gives
  fresh odometry immediately; NAVI decides when independent landmark evidence
  establishes a new MM reference. IR does not earn navigational trust.

This is David's current instruction, consistent with the reasoning preserved in
[0094](decisions/0094-ir-validity-is-continuous-health-not-earned-trust.md).
No firmware behavior was changed by recording it here.

## Where the artifacts are

| Artifact | Role and status |
|---|---|
| `firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_5_AUTO_ENABLED/` | Tracked, complete build tree; field-exercised Toby baseline; AUTO enabled |
| `firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_4/` | Tracked predecessor with two Manual run reports |
| `firmware/reference/NAVI_COHERENCE/` | David's incoming Sam packages and extracted patch copies; untracked at inspection; not the complete 0.5 build tree |
| `firmware/reference/NAVI_COHERENCE/NAVI_IR_ARCHITECTURE_0_3.zip` | Latest supplied IR architecture proposal; five files, no executable test harness; not integrated |
| `firmware/reference/NAVI_COHERENCE/NAVI COHERENCE IR HEALTH/` | Earlier intermediate health patch; still checks `unreliableSamples`, so not the final Epoch design |
| `docs/decisions/0092-...`, `0093-...`, `0094-...` | Decision trail for proximal correction, provisional 650 ms fallback, and continuous instrument health |
| `docs/NAVI_COHERENCE_0_5_FIRST_AUTO_RUN_20260922.md` | Field findings, including remaining failures and limits |
| `field-records/logs/20260922_navi_coherence_*` | Preserved field evidence |

No paths were moved or renamed. Incoming packages and field evidence remain
intact. Existing dirty server changes were left untouched. No blanket git add
or commit was made. Decision 0090 already belongs to the later sequence/AUTO
record; it must not be overwritten with the earlier unfinished 0.2 work.

## Agreed direction versus running implementation

| Design in David's current handoff | 0.5 implementation / next work |
|---|---|
| Incumbent position has inertia; a single odd observation need not relocate NAVI | Basic continuity already present; preserve it |
| IR health, readiness, odometry Epoch, and MM reference are separate | New package only; not wired into 0.5 |
| Fresh valid unchanged count means measured zero; no IR trust probation | 0.5 still requires TRACKING at both endpoints and unchanged `unreliableSamples` |
| Ended Epoch never reopens; new READY starts fresh odometry; old MM reference cannot revive | 0.3 package fixes ordinary outage recovery, but needs compiled tests and transport integration |
| Hall opening: two consecutive 1 kHz samples at absolute departure >=70 | Baseline profile currently sums 25+13=38; change only the intended new COHERENCE build |
| X22 locked per-interval baseline stays | Already inherited; do not replace baseline architecture |
| Hall-only detection-to-detection fallback 650 ms | `MIN_MARKER_MS` is still 500 |
| Cumulative mapped +/-15% windows; early event rejected without advancing expectation; traversed unobserved window means expected MM missed | +/-15% exists, but missed progression currently occurs during later Hall judgment/multi-step acceptance, not as a complete independent window-crossing model |
| Physical possibility first, +/-10 MM outer correction bound, rolling ~3 m history, unique strictly better alternative, ties hold | Whole-route 171-position search, 10/10 match plus >=2 disagreements remain |
| Actual polarity retained, missed landmarks UNKNOWN, history survives correction and useful reversal | Correction shifts the observed labels; multi-step and reversal clear observed correction history; a separate mapped ring fills missed entries from the map |
| IR unavailable: notify and use permitted non-IR navigation; recover odometry immediately but resynchronize MM independently | Hall-only fallback exists; explicit availability/epoch/reference transitions and notifications are not integrated |

The two uses of ten must remain separate: no ten-magnet IR probation; approximately
ten-MM/3,000-mm physical history before autonomous position reassignment remains.
Existing sequence tests verify the old mechanism, not the newly approved policy.

## Field evidence worth preserving

Claude's 0.5 report records 710 advances from 723 Hall events, 15 station stop
sequences in both directions, and an ending MM41->42 matching David's physical
041-042 report. It also records one false advance corrected by sequence, one
real Hall observation rejected by the +/-15% window then recovered two-step,
and Hall-only operation when the test car uncoupled. Physical platform stopping
positions were not independently checked. This is useful field evidence, not
blanket acceptance of every mechanism or the new Epoch architecture.

## Architecture 0.3 review

1. **Claude's ordinary-outage finding is repaired in the source logic.**
   `!haveSnapshot_ || !epochActive_` forces a fresh epoch after an unhealthy or
   not-ready interval, even with unchanged counters. `validFor()` checks epoch
   identity, so the old reference fails without explicit external invalidation.
   This is source tracing, not a claim the supplied review checklist executed.
2. **The package does not compile unchanged against the repo wire format.**
   In `IrInstrument.h:9`, `s.detectorReason=w.opticalReason` assigns a `uint8_t`
   to `ir_movement::Reason`. Native syntax compilation fails with an incompatible
   type error. Correct the boundary conversion with an explicit validity check;
   do not weaken compiler checking. No fix was applied in this catch-up.
3. **Claude's two smaller findings remain:** ordinary recovery starts an epoch
   with reason NONE; MM and Hall-time fields have no public accessors. Carry
   those into the first bounded implementation rather than losing them again.
4. **Reset lifetime must be tested.** `IrOdometryEpoch::reset()` resets its ID
   to zero. If an existing reference survives that reset, the same boot,
   calibration and pitch with a new Epoch 1 and a nondecreasing count can match
   a former Epoch 1 reference again. This is a source-visible API risk, not a
   reported live failure. Reset/re-pair ownership must prevent identifier reuse
   while references survive; do not rely only on the ordinary outage tests.
5. **MM synchronization needs temporal alignment.** `synchronize(mm,hallMs,o)`
   stores `o.pulses()` without proving that those pulses correspond to `hallMs`.
   0.5 captures the opening but judges after a 400 ms window. Integration must
   supply the aligned snapshot and its epoch, not the current/latest count.
6. **No packets means no calls to ingest.** The transport must explicitly end
   continuity on freshness expiry, queue loss and relevant source changes.
   Merely classifying received packets cannot handle a silent link. Existing
   transport rejects same-boot calibration/pitch changes as Old: simply attaching
   the new class downstream would hide those transitions rather than recover.
7. **Invalid distance must remain distinguishable from measured zero.**
   `distanceFromMm()` returns 0.0 when invalid. Callers must use validity as part
   of the result/contract, never publish or interpret that fallback as zero travel.
8. **Receiver semantics alone do not prove dwell continuity.** The current
   shared detector's finite contrast window can report INADEQUATE_CONTRAST at a
   stationary wheel; the field report observed non-TRACKING around dwells.
   0.3 correctly distinguishes SIGNAL_STALE from packet loss, but still ends an
   epoch on inadequate contrast. Real stop/dwell/restart traces must establish
   what the current TX actually reports. Do not silently ignore true measurement
   loss to claim continuous odometry, or claim a receiver-only fix solves it.

## Verification actually performed

- Read the handoff, latest decision records, IR health design, COHERENCE reviews,
  development history, AUTO report, current 0.5 source, transport, shared wire
  and detector, intermediate health patch, and all five 0.3 archive members.
- Re-ran the 0.5 navigation host suite with `-Wall -Wextra -Werror` and
  AddressSanitizer/UndefinedBehaviorSanitizer: PASS (2,052 offset corrections,
  6,840 single-misread holds plus its focused assertions).
- Re-ran station-correction tests with the same settings: PASS (32 approaches).
- Extracted 0.3 into a temporary tree with real relative common headers and ran
  C++17 syntax compilation: FAIL at the enum conversion above. Therefore no
  claim is made that the nine architecture cases passed as executable tests.
- No new ESP32 build, field-log reanalysis, Pi access, motor command or flash.

## Bounded implementation order

1. Make the 0.3 health/readiness/Epoch/MM-reference package compile, restore its
   diagnostic accessors/reasons, and turn the nine review cases into runnable
   tests. Add reset/source identity, rejected packet, hidden fault, valid zero,
   freshness, and delayed-Hall alignment cases. Keep host tests under `tests/`.
2. Connect validated transport and read-only telemetry alongside 0.5 without
   changing NAVI decisions. Observe health, readiness, epoch and MM-reference
   state separately, then verify stationary/dwell/outage/recovery behavior.
3. Apply the requested 70-count detector and 650 ms Hall-only fallback in
   bounded changes, preserving X22 baseline, station/AUTO integration and manual
   safety controls. Stronger valid IR distance must not be vetoed by the fallback.
4. Implement and test exact early/in-window/missed expected-landmark behavior.
5. Replace global sequence correction with physically viable local correction
   and rolling physical history only after those inputs are dependable.

Before implementing policy not specified by the supplied answers, surface the
choice. Examples needing precise operational definitions later: how to enforce
the approximate history threshold, what establishes physical impossibility
without IR (PWM alone is not travel), and the time/distance bounds for commanded
nonprogress or missing-Hall AUTO shutdown. These do not block independent Epoch
tests and are not grounds to re-open the already answered twenty questions.

## Housekeeping completed and deferred

Completed: this current-work index, verbatim supplied handoff, package-directory
pointer, and correction of catalog entries that still said 0.4 was unflashed
and 0.5 had not field-run. No historical decision text was rewritten.

Deferred: commit/organize incoming untracked packages with provenance, label the
older intermediate health patch clearly, and reconcile the old global project
instructions (which still describe Highline as the only active work). Those
changes should not move David's familiar sketch paths or erase rollback history.
