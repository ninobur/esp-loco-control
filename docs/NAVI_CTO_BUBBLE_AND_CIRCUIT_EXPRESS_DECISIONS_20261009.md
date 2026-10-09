# NAVI operating arrangements: CTO bubble and Circuit Express

**Date:** October 9, 2026

**Authority:** David's settled architectural decisions, recorded at his explicit
request so future work does not require him to explain the relationship again.

**Status:** Governing clarification incorporated into the
[mandatory primary NAVI guide](NAVI_EWO_0_1_ARCHITECTURAL_NOTE_20261009.md),
especially §§8–9. The Four-Station Local overlay packaging in §5 below remains
an architectural proposal. This is documentation, not implementation authority.

This note applies the [enduring decision-system principles](NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md)
by separating operating judgment from physical execution, preserving historical
evidence, and declining to fill unresolved design details with invented rules.
It adopts no departure from those principles. The guide remains primary;
compatible Decisions 0116/0120 and the standing [agent policy](../AGENTS.md)
retain their existing scope.

## 1. Read this first: the settled relationship

**Each locomotive can operate independently. Independent single-train operation
is not CTO. CTO is the negotiated, coordinated two-locomotive bubble. Circuit
Express is a temporary coordinated sequence within CTO that enables
leader/follower reversal and resolves back into ordinary CTO.**

| Concept | Settled meaning |
|---|---|
| **Normal Operations with Collision Control** | An operating arrangement in which locomotives may run independently, using geographic collision protection where applicable. Independent operation remains a capability of each locomotive. |
| **CTO — the bubble** | Toby and Otto mutually negotiate and coordinate movement, leader/follower roles, station holding/waiting, release/departure, and synchronized travel. Their relationship is more than two independent operating programs avoiding a collision. |
| **Circuit Express (CE)** | An integral, temporary **two-train coordinated variant/sequence within CTO**, for changing/reversing leader/follower roles. It resolves back into ordinary CTO bubble operation. |
| **Geographic collision protection** | A distinct, highest-priority geographic movement protection available under operating arrangements, including Normal Operations and CTO/CE. It does not define CTO. |
| **One active PWM overlay per locomotive** | Local motor-command authority, distinct from the shared two-train negotiation and coordination. |
| **Cruise base and STOP sequence** | Common physical facilities: cruise-only base PWM 60 and a reusable, portable STOP sequence invoked under an overlay. They do not define the operating relationship. |

The conceptual CE sequence is:

**CTO → CE → role reversal → CTO**

CE is not a mutually exclusive peer of CTO, an independent train mode, or a
single-locomotive express program. Toby and Otto remain participants in one
coordinated CTO operation throughout CE, even while their temporary service
roles differ. This statement defines the operating relationship; it does not
specify a software pairing latch or a transition algorithm.

## 2. Cooperation and collision protection are distinct

The CTO bubble includes mutual operating obligations: how the two trains move
together, who leads/follows, how they hold or wait at stations, and how release,
departure, and travel are coordinated. Collision avoidance alone does not
establish that relationship.

Geographic collision protection can restrict movement in Normal Operations
as well as during CTO and CE. It has the highest geographic overlay priority.
That priority does not turn independently operating trains into a CTO bubble,
nor replace the negotiated relationship within CTO. Existing manual, E-stop,
and other control authorities retain the scope stated in the primary guide §10.

## 3. Shared coordination is not shared PWM authority

Each locomotive has **at most one active overlay commanding PWM at an instant**;
when no overlay applies, its cruise base supplies the instruction. Toby and
Otto can negotiate a shared operating relationship while each NAVI executes
its own resulting geographic instruction through its own single active overlay.
The local overlay invariant neither prohibits nor substitutes for cooperation.

The common base is **ordinary cruise only, initially PWM 60**. The portable
**STOP sequence** is invoked under an overlay and supplies the physical
maneuver **DECELERATE → STOP → WAIT → RESTART → ACCELERATE**. The governing
operation supplies why/where to stop and the applicable waiting/release
condition. STOP does not govern the CTO relationship or choose its roles.

The primary guide's established STOP rules remain in force, including arbitrary
Lowline targets and entry points, timed completion at elapsed dwell with
concurrently applied PWM=0, and collision release governed by separation.
Sharing the physical maneuver does not make different reasons for stopping
equivalent.

## 4. A traffic stop does not discharge an owed station stop

If the leader occupies the platform, the follower may make a traffic/spacing
stop behind it. **That hold is distinct from the follower's still-owed station
stop.** It is not platform service and does not consume the station obligation.

Once the leader departs and separation permits, the follower proceeds to its
own platform stop and performs its station service. Reevaluate the current
geographic instruction as the restriction changes, preserving that still-owed
service; do not interpret release of the traffic hold as permission to skip it.
This preserves the operating obligation without selecting an admission latch,
state machine, handshake, or other implementation mechanism.

## 5. Four-Station Local: architectural proposal, not a completed design

The **entire Four-Station Local station-stop pattern may be one service overlay**,
with the reusable STOP sequence used for its station stops. Station service
remains outside the cruise-only base. Under this proposal, skipping a required
stop would require a higher-priority exception to that service overlay.

This is the level settled in the discussion. It does not establish a detailed
priority ladder below collision protection, nor decide service-overlay lifetime,
preemption/resumption, pending-request representation, or exception expiration.
Completing one STOP invocation must not silently be equated with consuming the
whole proposed four-station service pattern. Its representation remains open;
the rule against repeating a completed stop remains binding.

## 6. Explicitly superseded interpretations

These corrections govern future NAVI interpretation. Preserve the historical
texts and field evidence; do not reuse incompatible descriptions as current
architectural authority.

| Earlier description or interpretation | Disposition after David's clarification |
|---|---|
| Single-train independent operation is CTO | **Superseded/rejected.** It is an independent capability, not CTO. |
| CTO is independent operating programs plus collision avoidance; the guide's former §9 heading equates future CTO with a collision overlay | **Superseded.** CTO is the mutually negotiated two-train bubble; collision protection is a separate facility available under operating arrangements. |
| Normal Operations, CTO and CE form three mutually exclusive peer modes | **Superseded/rejected.** CE is within CTO. This was an intermediate conversation interpretation, corrected by David. |
| CE is merely a per-locomotive Express/PASS profile, an independent mode, or single-train express | **Superseded/rejected.** CE is the temporary coordinated two-train role-reversal sequence within CTO, returning to ordinary CTO. |
| One PWM overlay means there can be no shared two-train negotiation | **Superseded/rejected.** The rule is per locomotive and concerns PWM authority, not the operating relationship. |
| A traffic hold behind an occupied platform completes the follower's station service | **Superseded/rejected.** Its own platform stop remains owed after separation allows it to proceed. |
| STOP execution defines or controls the CTO relationship | **Superseded/rejected.** STOP is the common physical facility used under the governing operation. |

Specific historical reading corrections:

- [October 5 CTO/CE reconstruction (Document A)](NAVI_CLOSE_TRAIN_OPERATIONS_RECONSTRUCTION_ARCHITECTURE_20261005.md):
  §§2/10 preserve the dynamic bubble; §§11–13 and §15 items 9–12 must not
  reduce CE to unrelated service programs. Temporary service profiles belong
  to the coordinated CE sequence within CTO. The guide supersedes §5's
  whole-tile replacement model and §14's rejection of PWM as the operating
  language where they conflict with cruise-base/single-overlay PWM execution.
- [Position/overlay architecture (Document C)](NAVI_POSITION_OVERLAY_OPERATING_ARCHITECTURE_20261005.md):
  §28's “Circuit Express: PASS at all four stations” is not the definition
  of CE or a complete specification of its behavior. Its Four-Station Local
  **base** program is already superseded; §5 above records the broader service
  overlay proposal without making its detailed implementation a decision.
  §§22–23/30 preserve the distinction between traffic holding and station service.
- [September 9 CTO provenance proposal](NAVI_CTO_ARCHITECTURE_AND_PROVENANCE_20260909.md):
  traffic-only reductions and the proposed cap machinery do not define the
  current CTO relationship. Its mechanisms and staged plan remain historical
  proposals, not newly adopted algorithms or safety caps.

## 7. Provenance and boundaries

Source: David's October 9, 2026 discussion **“NAVI EWO DEV then Simplification”**
([conversation reference](https://chatgpt.com/c/6ac49898-d8f0-83e8-8943-ad347dc21bf6)),
and his explicit documentation instructions in the continuation. His final
correction controls the earlier tentative three-mode categorization:

> CE is actually a temporary variant of CTO that enables role reversal (leader follower). CE resolves into CTO. Definitely a two train coordinated sequence, and part of CTO

This note records the decisions in the repository so the conversation need not
be available to interpret them. Starting point verified live for this update:
`docs/navi-stop-overlay-20261009` at
`bd0b15f55f32a5dd92e939f2da26b61ae143dfd4`.

Historical evidence retained:

- [CTO2 bubble discussion](CTO3/resources/CTO2_BUBBLE_PRINCIPLE.txt), including
  leader holding, follower release, and the follower's later platform stop.
  Its old numerical profiles, timers, and mechanisms are not adopted here.
- [August 19 CE field record](../field-records/20260819_CE3_FIRST_CIRCUIT_EXPRESS.md)
  and [August 20 CE return record](../field-records/20260820_CE_RETURN_AND_THIRD_NODE.md),
  preserving two-train role changes and return to paired operation. Historical
  software severance/re-pairing and service patterns remain evidence of that
  implementation, not definitions or required transitions for future NAVI CTO.

**Not decided or authorized here:** a full bubble algorithm, detailed operating
transitions, negotiation protocol, packet format, new safety/performance caps,
or a complete overlay priority order. No firmware modification, merge, flash,
hardware operation, or two-train activation is authorized by this record.
