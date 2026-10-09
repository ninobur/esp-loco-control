# NGR Architectural Note

## NAVI_EWO_0_1 — Architectural Inheritance and Simplification

**Date:** October 9, 2026  
**Status:** Mandatory primary architectural guide for all future NAVI firmware revisions, established by David's October 9, 2026 documentation-reconciliation instruction; documentation only.
**Scope:** NAVI navigation, geographic operating instructions, universal stopping, physical control, and future CTO operations.

This note applies the enduring decision-system principles by retaining NAVI's navigation judgment, keeping observation distinct from actuator action, and requiring evidence before additional control complexity. It does not authorize firmware changes, merging, flashing, or two-train activation.

### Governing authority and document precedence

Every NAVI firmware revision must begin by reading this guide and reconciling
the proposed work against its applicable requirements. This requirement applies
to CODEX, Claude, and every other developer, beyond `NAVI_EWO_0_1`.

This guide supersedes earlier architectural documents, implementation
instructions, and decisions wherever their provisions conflict with its
principles. Compatible earlier decisions remain binding, particularly Decisions
0116 and 0120. Report conflicts explicitly; do not restore superseded mechanisms
by relying on an older record. Any proposed departure requires David's explicit
approval before implementation. Every implementation review must assess
compliance with this guide and its Necessity and Simplicity Test.

Architectural precedence is:

`This guide → compatible existing NAVI decisions → implementation specifications → historical documentation`

The standing agent policy in `AGENTS.md` governs work authorization and
deployment. Mandatory architectural reference does not itself authorize
implementation, merging, flashing, or two-train activation.

This reconciliation considers both source histories:
`db329782d422559dcb886f93db8c1f3e3922a7ab` (the 14-section note) and
`7a31d913ef2218687b488936a998f82655f5d3c6` (simplified IR-loss fallback).
The fourteen numbered sections below remain the architectural foundation.

**Subsequent October 9 design decision — base cruise and single-overlay execution.**
Sections 4–6, 8–9, 11, and 13–14 incorporate David's later direction:
ordinary geographic tiles carry PWM cruise (initially 60); *all* exceptional
movement requirements, including station stops, are temporary overlays.
Only one highest-priority active overlay per locomotive has PWM authority at a time.
A universal, destination-anchored **STOP sequence**, invoked through an overlay,
is to be established and tested **before** any station-specific application. Stop completion is determined by
elapsed dwell **while applied PWM is concurrently zero**, not attainment of
the geographic aim point. These later provisions supersede conflicting earlier
statements in this guide and older documentation.

**Subsequent October 9 clarification — CTO is the negotiated bubble; CE is
within CTO.** David's [durable operating-arrangement decision note](NAVI_CTO_BUBBLE_AND_CIRCUIT_EXPRESS_DECISIONS_20261009.md)
is incorporated into this guide, especially §§8–9. Independent single-train
operation is not CTO. Normal Operations with Collision Control permits
independent operation; CTO is Toby and Otto's negotiated two-train relationship.
Circuit Express is a temporary coordinated role-reversal sequence within CTO:
**CTO → CE → role reversal → CTO**. Geographic collision protection is a
distinct highest-priority movement protection available under these operating
arrangements, not CTO's definition. One active PWM overlay **per locomotive**
does not replace shared negotiation. The note preserves the corrections to
earlier assistant interpretations and marks the Four-Station Local service
overlay packaging as a proposal, not a completed design.

### 1. Purpose

`NAVI_EWO_0_1` deliberately returns to demonstrated physical-control methods while retaining NAVI's architectural advances. The October 2, 2026 field operation demonstrated eight complete automatic station cycles using geographic PWM control without continuous closed-loop speed regulation. Document C subsequently established that geographic instructions, rather than procedural history, determine what NAVI must do.

**The objective is to combine proven physical behavior with geographic authority, without restoring obsolete station machinery or introducing unnecessary complexity.**

### 2. Fundamental separation of responsibilities

NAVI has three distinct responsibilities:

- **Navigation:** determine geographic position and direction from Hall landmarks, IR distance, and NAVI's existing judgment.
- **Operating judgment:** determine the instruction applicable at the current geographic position, including base tiles and any authorized overlay.
- **Physical execution:** translate that instruction into motor operation with the established PWM actuator ramp.

The stopping mechanism does not independently navigate, interpret Hall observations, or determine dispatcher intent.

**NAVI determines where it is and what the railroad requires. A reusable physical maneuver executes that requirement. The sensors observe the result.**

### 3. Geographic authority replaces procedural admission

A maneuver must be executable from the locomotive's current geographic position and physical state. It must not require passage through an earlier admission point, an `ARMED` state, or completion of prior procedural phases.

The same location and physical circumstances should produce the same operating requirement regardless of how the locomotive arrived there, except for the minimal execution progress needed to distinguish an unfinished stop from an authorized departure. This applies to cold entry, STOP/GO, missed magnets, reversals, and interruptions.

**A stopping maneuver is a sequence of geographic objectives, not a procedure that must be entered at its beginning.**

### 4. Universal STOP sequence

**STOP sequence** is the name of the universal stopping maneuver. Every stop
is geographic; no geographic qualifier is necessary. Unlike the legacy
station-stop procedure, this sequence is **assignable to any target point
on the Lowline** and **permits entry at any point within the sequence**.
Its execution cannot depend on passing a starting marker, triggering an
admission latch, or completing an earlier procedural phase.

The STOP sequence is invoked under the **one active temporary overlay**.
The overlay supplies operating authority, the intended target and the
waiting/release condition. The STOP sequence supplies the reusable physical
maneuver:

**DECELERATE → STOP → WAIT → RESTART → ACCELERATE**

The target anchors a PWM approach constructed **backward toward the approaching
locomotive**. A geographic tile may contain many IR wheel-pulse positions,
each with its own PWM assignment (nominal installed pulse spacing ~9.652 mm).
The currently applicable pulse-position instruction determines the PWM
target, regardless of where the locomotive entered the sequence.

The initial design uses PWM 60 cruise in the base layer; the STOP sequence
uses moving creep at PWM 50 followed by a geographic final ramp toward
PWM 20 at its aim point. Actual stop location may differ from the target.
The target is not a stop-completion condition, and the locomotive does not
correct its position merely to reach the target.

**The established time-based PWM actuator ramp carries out its existing
command independently of further IR pulse arrivals.** When wheel pulses cease
before the geographic aim point, an already-established PWM ramp continues
toward zero; the STOP sequence does not need fabricated position, another
IR-based speed controller, or an entry latch. Mile-marker observations provide
geographic checkpoints and IR observations provide finer distance evidence.
Both inform NAVI's sole position judgment; their complementarity does
**not** revoke Decisions 0116 or 0120 or authorize Hall-only manufactured
travel.

Applied PWM=0 establishes the operational stop and begins timed dwell.
When dwell has expired and PWM is concurrently still zero, the timed STOP
sequence is complete. On authorized release of a stop-scoped overlay, retire it
and resume the established acceleration ramp regardless of the actual stop
coordinate. NAVI continues coherent geographic tracking and follows the
next applicable tile instruction. A collision-prevention overlay may use
the same STOP sequence but its release remains governed by required
separation, not by a station timer.

Completion of one STOP invocation must be distinguished from the lifetime of
the proposed whole-service overlay in §8. The physical STOP sequence does not
determine the CTO relationship or its coordinated release obligations (§9).

Develop and field-test this generic STOP sequence **first** at a non-station
target; only afterward apply it to Four-Station Local and then CTO.

### 5. Stop completion, dwell, and geographic accuracy

The intended target is a reference for constructing the stop's pulse-resolution
PWM profile, **not** an arrival or completion gate. For the later Four-Station
Local application, Station +1.5 MM remains the intended target. The earlier
+1.0 to +1.7 preferred range and Station 0 through +3 acceptance region remain
*diagnostic performance bands*, not authorization conditions. The actual stop
may be early or late. Do not move or restart simply to correct that error.

**Governing completion rule (David, October 9): a STOP is complete when its
dwell interval has expired and the applied PWM is concurrently zero.**

The timer belongs to the active overlay that invokes the STOP sequence. For a timed station stop, the
initial dwell is five seconds. **Applied PWM=0 establishes NAVI's operating
criterion for no motive movement and starts the dwell.** Completion requires
the dwell to have expired **while applied PWM is concurrently still zero**.
There is **no separate IR pulse-absence, speed-estimation, or position test**
for recognizing a STOP or starting/completing dwell. The former PWM<25
STOPPED classification is not equivalent to the required PWM=0.

IR continues to provide valid position and movement observations when available,
so NAVI can report where the locomotive stopped and refine PWM geography.
NAVI does not wait to reach the intended coordinate, a Hall marker, or the
former Station +1 MM timer boundary. The former moving-approach +1 MM timer
trigger is superseded. Neither precise stopping position nor availability of
IR motion evidence gates the PWM-zero dwell.

**Decision 0120 remains binding:** an actual IR observation of displacement
while PWM=0 is an unexpected-movement event, not evidence that the PWM-zero
operational definition should be replaced with another stop detector. NAVI
must apply Decision 0120's hold and operator verification/redeclaration rule.
This is distinct from the normal stop-completion mechanism.

When the active overlay's completion **and release** requirements are
satisfied, withdraw that overlay and use the established ramp-up independently
of where the locomotive stopped. Do not let the same completed overlay issue a
second STOP in its own former footprint. Continue coherent IR-based geographic
tracking during restart. At the next applicable geographic tile instruction,
NAVI follows that instruction without demanding a departure landmark or
pretending an uncorroborated Hall pulse independently establishes position.

**Important:** a timed station service can release after dwell expiry with
PWM=0. A geographic collision hold cannot release merely because a dwell
expired; it remains governed by sufficient geographic separation or its
authoritative release instruction. The *same motor maneuver* serves both.

### 6. Preserve demonstrated PWM behavior; measure actual motion

The October 2 `NAVI_EYES_WIDE_OPEN_INTEGRATED_R2_FT2` field run remains
the physical-control reference: geographically assigned PWM, the existing
actuator ramp, grade-specific calibration where justified by field evidence,
PWM-zero stops, and automatic departures. The legacy `StationMachine` is
**not** an authority to restore.

**Geographic PWM assignments are the commands; IR motion and speed are the
measurements.** The system does not continuously regulate PWM to enforce a
physical-speed target. The cruise layer initially assigns PWM 60, and a
STOP overlay initially includes moving creep at PWM 50 and a final decrease
toward PWM 20 at the aim point. Applied PWM reaches zero to complete the
stop. Geographic assignments can later be tuned from observations of
overspeed, underspeed, grades, and early/late stops; adjustments require
deliberate firmware/configuration revision, not automatic adaptation.

IR pulse observations remain navigation evidence under NAVI, and allow
reporting pulse-resolved progress, actual speed, and actual stop location.
No separate geographic position estimator or IR homeostasis PID/step feedback
controller belongs to the STOP sequence.

Continuous IR speed homeostasis, adaptive glide-path correction, the
95-percent controller handoff, and PWM ticker-tape recording/playback are
**superseded requirements** and must not return through older documents.

**Simplified IR-loss fallback, retained from `7a31d913`:**

- In ordinary steady-speed operation, hold the existing PWM if usable IR
  speed feedback is unavailable, without inventing geographic progress.
- During an established monotonic deceleration, continue its already
  established actuator PWM reduction without initiating speed correction.
  Do not fabricate intervening pulse positions while IR distance is absent.
- The fallback preserves actuator continuity; the established time-based PWM ramp
  proceeds toward its already-commanded target without requiring further
  IR pulses. This cannot establish navigation position, stop completion, or
  authorization to depart.
- On recovery, applicable IR evidence resumes under existing validity and
  continuity rules. No ticker-tape, historical replay, or adaptive
  correction machinery is reinstated.

Decision 0116 continues to prohibit Hall-only position advancement,
manufactured travel, and Hall-only re-anchoring; Decision 0120 still governs
displacement observed at PWM zero. Lost/unreliable IR distinctions and any
safe geographic scheduling during such intervals remain open for decision.

### 7. NAVI owns continuous geographic position

A confirmed Hall landmark establishes a geographic reference. IR measures progress from that reference. Subsequent confirmed landmarks reconcile position against the surveyed map. A manual declaration must not manufacture an exact physical Hall anchor.

The stopping mechanism consumes NAVI's existing geographic judgment through a read-only interface. It must not create a second position estimator or permit Hall-only navigation advancement contrary to Decision 0116. NAVI's genuine Missed Magnet behavior, direction awareness, and Decision 0120 PWM-zero movement rules remain binding.

### 8. Cruise base layer and single prioritized overlay

The **base geographic layer is ordinary cruise only**, initially PWM **60**
at every base tile. It contains **no station stopping, dwell, or compulsory
service procedure**. Tiles retain geographic identity and pulse-position
resolution, so PWM can be calibrated by physical location in later revisions.
Changes to ordinary running, station service, scheduled station stops,
station PASS variations where needed, dispatcher directions, and traffic
restrictions are all expressed as **temporary overlays**, not persistent base
STOP tiles.

At any instant **at most one active overlay per locomotive may command PWM**;
when none applies, the cruise base governs. Select the highest
priority applicable overlay; **geographic collision prevention is always
highest**. The precise lower-priority order is not decided here and requires
David's approval. A pending lower-priority request is not automatically
forgotten, but its PWM instructions do not execute concurrently. Preemption,
resumption and request-expiration rules must be explicit and minimal; they
must not reproduce a procedural admission state machine.

An active overlay assigns geographic PWM commands at individual IR pulse
positions within its footprint and takes precedence over the base cruise
instructions. Overlays do **not** overwrite underlying tiles. Once the
current overlay completes and is authorized to release, it is retired;
the existing PWM ramp resumes movement and the locomotive obeys the next
applicable geographic instruction. The completed STOP footprint must not
reassert itself, and no base STOP remains to cause a second station stop.
Service-request identity/consumption must be minimal and must not become an
independent historical station-admission latch.

A temporary geographic collision stop may preempt a station stop. When the
collision overlay releases because separation permits, NAVI evaluates
whichever request is **then** geographically applicable and permitted by
priority. No old station sequence is blindly resumed merely because it was
previously active. A follower's traffic stop behind an occupied platform does
**not** consume its still-owed station stop: after the leader departs and
separation permits, the follower proceeds to its own platform stop and station
service. This obligation does not prescribe an admission latch or state machine.
One active PWM authority per locomotive is the invariant; it is distinct from
the shared two-train negotiation and coordination in §9.

**Architectural proposal — whole-service overlay:** the entire Four-Station
Local station-stop pattern may be one service overlay using the common STOP
sequence. Under that proposal, skipping a required stop would require a
higher-priority exception. This does not decide the detailed order below
collision protection, service-overlay lifetime, or preemption/resumption and
expiration rules. Completion of one STOP invocation is not automatically
completion of the whole service pattern; its representation remains open.

### 9. Operating arrangements: Normal Operations, CTO bubble, and CE within CTO

Every locomotive retains the ability to operate independently. **Independent
single-train operation is not CTO.** In **Normal Operations with Collision
Control**, locomotives may run independently and use geographic collision
protection where applicable.

**CTO is the negotiated, coordinated two-locomotive bubble.** Toby and Otto
mutually coordinate movement, leader/follower roles, station holding/waiting,
release/departure, and synchronized travel. CTO is not merely independent
operating programs plus collision avoidance. Geographic collision protection
is a distinct highest-priority geographic movement protection available under
operating arrangements; it does not define or replace the CTO relationship.

**Circuit Express (CE) is an integral temporary two-train coordinated variant
within CTO**, enabling leader/follower role reversal and resolving back into
ordinary CTO: **CTO → CE → role reversal → CTO**. It is not an independent
train mode, a mutually exclusive peer of CTO, or single-locomotive express.
Temporary Express/Local service roles remain part of the shared operation.
No full bubble algorithm, detailed transition or negotiation protocol is
specified by this clarification; see the
[durable decision note](NAVI_CTO_BUBBLE_AND_CIRCUIT_EXPRESS_DECISIONS_20261009.md).

The cruise-only PWM 60 base and portable STOP sequence are common physical
facilities. Each locomotive executes the resulting instruction through its
own single active PWM overlay. The STOP sequence supplies the maneuver;
the governing operation supplies the reason, target, and waiting/release
condition. STOP does not govern the CTO relationship.

#### Common geographic collision protection and physical execution

The universal STOP sequence is to be established and field-tested **before**
Four-Station Local or Close Train Operations (CTO) are implemented.
Once proven, a station service simply supplies its stopping target and timed
dwell condition. A collision-prevention authority, applicable in Normal
Operations and CTO/CE, supplies the dynamically relevant geographic target or
restriction and a separation-dependent release condition. Within CTO, shared
negotiation also governs the pair's operating obligations; the collision
facility alone is not the bubble.

NAVI eventually needs the geographic boundaries of the whole consist,
not just the Hall location. The existing CTO architectural rule remains:

**Required clear separation = 600 mm + the standard stopping distance at
the follower's current physical speed.**

Measure separation from the follower's physical front to the leader's
physical rear. The leader's known geography determines the follower's
stop destination. Ordinary following may call for a reduced PWM overlay
without a full stop; when STOP is required, the same universal stopping
executor supplies the physical maneuver.

**Geographic collision prevention has top overlay priority.** Its restriction
is not removed just because a service dwell has expired; it releases only
when its geographical authority permits. The underlying base cruise
instruction continues to exist and is not overwritten.

The consist geometry and separation rule are retained design requirements
for later CTO work, not authorization for unvalidated two-train activation or
new performance restrictions in `NAVI_EWO_0_1`.

### 10. Preserve existing control authority

The new implementation must preserve manual throttle and direction; STOP/GO and AUTO admission; MQTT control and command responsiveness; E-stop and low-voltage protection; existing NAVI Hall/IR judgment; genuine Missed Magnet handling; Decisions 0116 and 0120; and the existing `requestPwm()` and `serviceRamp()` actuator path.

No new controller may compete for PWM authority.

### 11. Observe and report physical results, not imaginary compliance

For every stop, log the geographic target, starting tile, assigned pulse-
position PWM, requested/applied PWM, actual geographic progress from valid
NAVI/IR evidence, observed speed, estimated and observed stop coordinate
when valid, actual target error, dwell initiation/expiry, PWM-zero timing,
release authority, and departure/current-tile transition.

Log actual under-/overspeed and early/late stops so that geographic PWM
assignments can be refined through evidence-based revisions. A speed
measurement is **not** a continuous speed-correction command, and geographic
target error is **not** a reason to prevent a completed stop or launch a
corrective forward movement.

Telemetry must not compromise command responsiveness, MQTT, native
navigation evidence, or the actuator. Keep observation, judgment, and
PWM execution distinguishable.

### 12. Necessity and Simplicity Test

Before introducing a mechanism, algorithm, state variable, restriction, or dependency, ask:

**Question 1:** What does NAVI actually need to decide, and what can the existing physical system already accomplish without another decision?

**Question 2:** What is the simplest way to accomplish the required task?

**Screening principle:** Added complexity must justify its inclusion and must not increase system fragility.

Preserve demonstrated functionality. Add machinery only when it addresses an identified operational need and a simpler approach cannot adequately provide the benefit.

### 13. Implementation sequence and acceptance criteria

The development order is **(1) universal STOP sequence → (2) track
validation and PWM-geography calibration → (3) Four-Station Local service
using that same STOP sequence under overlay → (4) future CTO via prioritized geographic
restrictions and negotiated two-train coordination**. CE belongs within that
future CTO work. Do not begin by rebuilding station machinery.

Before `NAVI_EWO_0_1` is ready for field testing, implementation review must
verify at least:

1. Ordinary geographic base tiles initially assign cruise PWM 60, with no
   embedded station STOP or dwell.
2. At most one active overlay per locomotive commands PWM; collision prevention
   is highest priority when implemented, without inventing unapproved subordinate order.
3. The STOP sequence can be assigned to any Lowline target and entered
   anywhere in the backward-projected pulse-position PWM assignments, with
   creep PWM 50 and final geographic reduction toward PWM 20.
4. Existing PWM actuator ramps remain the sole physical executor, and the
   commanded stop reaches applied PWM zero.
5. A locomotive entering or restarting midway through the STOP sequence
   executes the current instruction without any ARMED/StationMachine admission or
   prior-event dependence.
6. Applied **PWM=0** establishes the stop condition and begins timed
   dwell; expiration with concurrently applied PWM=0 completes the timed STOP.
   IR-based immobility confirmation is not required. Actual stopping position
   is measured/reported, not made a completion gate.
7. A completed/released overlay cannot reissue the same stop before the
   next relevant instruction; the established ramp-up resumes independently
   of stop coordinate.
8. NAVI Hall/IR geographic authority, Decisions 0116/0120, manual/MQTT
   commands, STOP/GO, E-stop and existing actuator authority remain intact.
9. Host tests/compilation precede independent review; subsequent field
   tests are required to prove physical stopping, accuracy, and arbitrary
   positioning rather than relying solely on synthetic demonstrations.

Station-specific dwell and PASS overlays are **subsequent applications**, not
prerequisites for proving the universal STOP sequence. No implementation or
flashing is authorized by this guide.

**Initial test target, October 9:** David selected the vicinity of MM045 and
authorized choosing one point for approaches from both directions. The
[bidirectional STOP test definition](NAVI_STOP_MM045_BIDIRECTIONAL_TEST_DEFINITION_20261009.md)
places it at the midpoint of MM045–MM046: 150 mm from either marker along that
interval. It is one physical target, with the approach projected backward in
each direction; neither marker is a required admission trigger. This test
selection does not limit STOP's applicability anywhere on the Lowline.

### 14. Historical references, supersession, and open questions

**Physical-control evidence:** `ab0938b0f01531a38ce4d8a1d9d932ffad4f0334`
— October 2 `R2_FT2` successful station trials.

**Stationless firmware baseline:**
`c7c21163b016e443524eb9fa483f8b706ce76e22` — removes legacy
`StationMachine`; not itself an implemented numbered `NAVI_EWO_0_1` sketch.

**Discarded Pass 2 closed-loop candidate:**
`22b05a22d30d0ab76f5feb654a3e6f57a005545c` — historical prior art,
not the implementation foundation.

**Architectural records:** Document C, Decisions 0116/0120/0121/0123,
October 8 geographic-consist architecture, the October 9 primary-guide
reconciliation, and David's subsequent October 9 cruise-base / single-overlay
/ stop-completion decisions. Historical sources remain intact as evidence.
The subsequent [CTO bubble and Circuit Express decision note](NAVI_CTO_BUBBLE_AND_CIRCUIT_EXPRESS_DECISIONS_20261009.md)
records David's settled operating relationships and scoped historical
corrections. It distinguishes the Four-Station Local overlay proposal from
those settled decisions.

**Explicit supersession under the updated mandatory guide:**

| Earlier provision | Current governing disposition |
|---|---|
| Independent single-train operation is CTO; CTO is merely a collision overlay (including the former §9 framing) | Superseded. Independent operation remains a locomotive capability; Normal Operations with Collision Control can use it. CTO is the negotiated two-train bubble. Collision protection is distinct and available under operating arrangements. |
| CE is a third peer mode beside Normal Operations and CTO, or just a single-train Express/PASS profile | Superseded/rejected. CE is an integral temporary two-train coordinated sequence within CTO: CTO → CE → role reversal → CTO. |
| One active overlay removes shared two-train negotiation, or STOP execution governs the relationship | Superseded/rejected. Single PWM authority is per locomotive; shared coordination determines operating obligations, using common physical facilities. |
| A follower's traffic stop behind an occupied platform completes its station service | Superseded/rejected. The follower still owes its own platform stop once the leader departs and separation permits. |
| Base layer includes station stops and stop/dwell tiles (earlier guide §8 and Document C station-service examples) | Superseded. Base layer is cruise-only, initially PWM 60. Station service and all deviations from ordinary cruise are overlays. |
| Multiple potentially simultaneous overlay motor commands or station overlay plus base station STOP | Superseded. Only one active overlay per locomotive has PWM authority at a time; geographic collision prevention has highest priority. Lower-priority policy remains open. |
| First prove Four-Station Local and then extract generic stopping (earlier guide §§4/13) | Superseded. Prove the universal STOP sequence at arbitrary non-station destinations *first*; station and CTO use it later. |
| One PWM per tile or continuous physical-speed-target regulation (Document C; former Decision 0123) | Superseded. Individual IR pulse positions within tiles can carry geographic PWM assignments, with existing actuator ramp and diagnostic IR observations. |
| Station dwell clock starts at Station +1 MM while still moving (earlier guide §5, §13; Document C reconciliation) | Superseded. Dwell belongs to actual stop; stop service completes when dwell elapsed **AND** applied PWM=0. The target coordinate does not gate completion. |
| Preferred stop target and acceptance window function as required geographic stopping gates | Superseded. Any prescribed aim point, including Station +1.5 MM, guides the decel geography only. Stop location and error are recorded diagnostically. |
| Following PWM commands must retain procedural station sequence, entry latch, or destination acquisition | Superseded. Retire completed overlay and ramp up independent of stopping coordinate; obey the next applicable geographic tile instruction. |
| Continuous IR speed homeostasis, adaptive glide, controller 95% handoff, PWM ticker-tape | Superseded. Keep geographic PWM-first operation and simplified existing actuator continuity on temporary IR feedback loss, without violating 0116/0120. |
| Station service dwell release and collision hold release are identical | Superseded. Collision prevention remains controlled by geographic separation; timed service stop is released only after its own completion and applicable authorization. |

**Resolved by David:** a previously established time-based PWM ramp
continues even if IR pulses cease before the geographic target. MM provides
geographic checkpoints, IR provides finer measurements, and NAVI retains
sole navigation judgment under Decisions 0116 and 0120. Neither sensor
must continually authorize the already-established PWM ramp.

**Resolved by David:** applied PWM=0 is NAVI's no-motive-movement
indication and starts timed dwell. Timed STOP completion is dwell expired
concurrently with applied PWM still zero; no additional IR-rest test or
position gate is permitted. Decision 0120 independently governs actually
observed PWM-zero displacement.

**Still open — require David's explicit architectural decision before
coding a material behavior:**

1. The minimal representation, lifetime and reset of stop-overlay execution
   progress and one-time service consumption needed to prevent reissuing
   a completed stop, without a historical admission latch.
2. The priority **below** geographic collision prevention; pending request
   expiration and preemption/resumption semantics. Do not invent this order.
3. How an already-established geographical PWM decel behaves across
   gaps in usable IR-position evidence without inventing Hall-only progress,
   consistent with 0116 and 0120.

4. The field-measured PWM profiles, stop distance and consist-dependent
   stopping envelope across grades, directions and requested speeds.
   Do not mistake PWM target 20 for proof of physical rest or guaranteed
   positional stop accuracy.
5. Whether departure ramp authority, after release, holds until the next
   applicable tile boundary or yields immediately to the current base
   cruise assignment. David's intent is no repetition of the completed
   stop and compliance with the next tile; implementation representation
   must preserve that intent without hidden latches.
6. Full bubble algorithms, detailed CTO/CE transitions and negotiation
   protocol. Their absence does not reopen the settled definition of CTO or
   CE's place within it. Whole-service overlay packaging remains the proposal
   in §8; no new safety caps or subordinate priority ladder are adopted here.

No unapproved speed governors, safety/performance restrictions, adaptive
controllers, sensor thresholds, navigation shortcuts, or firmware changes
are established by this documentation update.

**Central principle**

> NAVI navigates and supplies the current geographic instruction.
> The base layer is ordinary cruise. The single highest-priority
> overlay, when present, modifies that instruction. A universal
> destination-anchored **STOP sequence** executes the physical maneuver
> through the single active overlay,
> accepts early or late stops, and completes timed service at dwell expiry
> with applied PWM concurrently zero. Upon release, ramp up without
> repositioning and follow the next applicable geographic tile.
>
> **Preserve what works. Add only what is necessary. Complexity must earn its place.**
