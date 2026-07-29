# NGR CTO2 Thought and Logic Catalog — Initial Harvest

**Author:** Sam (ChatGPT), 2026-07-28
**Basis:** ROAD_TO_CTO.md, SOLONAV_2_14.ino, SOLONAV14.txt, prior CTO2 discussion history.

Preserved as written. Reconciliation with the Claude-side analysis is in
`CTO3_DESIGN_NOTES.md`; this document is the source and should not be edited to
match it.

---

## Purpose

Capture the CTO2 reasoning worth preserving while rebuilding toward a new CTO
lineage on top of SOLONAV rather than trying to patch CTO2 forward.

The governing conclusion is:

> CTO2 did not mainly fail because trains could not coordinate. It failed
> because coordination consumed position reports that could be confidently
> wrong.

Therefore the new CTO work should preserve the coordination ideas that were
sound, but replace every consumer of raw/unbounded position with bounded,
auditable truth.

---

## 1. Core lessons from CTO2

### 1.1 Coordination cannot exceed navigation truth

CTO2's rear-end failures were caused by correct clearance logic acting on
incorrect leader position. The follower's math could be internally correct and
still produce a collision if the leader report was wrong.

Design implication:
- Never consume a peer's bare marker as if it were track occupancy.
- Consume a bounded envelope: front bound, rear bound, uncertainty, freshness,
  and halted/moving state.
- A train whose position is unknown must be treated as occupying unresolved
  track, not as absent.

### 1.2 Dispatcher declaration is valid initial truth, but not permanent proof

CTO2 learned an important operational distinction:
- The dispatcher can declare a starting interval.
- That declaration is valid operational truth for startup and peer awareness.
- Hall/DNA/odometry should refine it.
- A bad read should not wipe it.
- A later trusted fix can replace it.

New CTO rule: **Declare → count → refine.** Do not leap from a bad pattern
match to certainty 1.000.

### 1.3 A train is an envelope, not a point

CTO2's boundary math was conceptually right: the Hall sensor is only a
reference point. Collision avoidance must operate on front and rear consist
boundaries.

Preserve: front boundary, rear boundary, touch gap, restart gap,
same-direction nearest-ahead evaluation.

Rewrite: any boundary derived from unbounded or untrusted marker truth.

### 1.4 Traffic is a temporary interruption, not a role

A key CTO2 simplification was treating traffic control as an interruption to
the local mission rather than a permanent identity.

Preserve:
- station mission continues to exist while traffic temporarily preempts it
- after traffic clearance, resume the interrupted local mission where practical
- traffic state machine should be local, explainable, and log-driven

### 1.5 Silent peer is not clear track

A powered-down, silent, or stale peer should become an unresolved obstruction.
CTO2 showed that absence of fresh radio data cannot be equivalent to empty
track.

New CTO rule:
- Fresh bounded peer = consumable occupancy.
- Stale known peer = unresolved obstruction until proved otherwise.
- Unknown peer = manual/operator problem, not autonomous clearance.

---

## 2. SOLONAV principles that should become CTO foundations

### 2.1 Position from history and map

SOLONAV inverts the old DNA matcher. The map and odometer are primary; a magnet
reading is a vote, not a verdict.

Preserve as CTO base: odometer-centered tracking, confidence score, marker
observations that can agree/disagree without teleporting the train, LOST state
instead of false certainty.

### 2.2 Detector does not make navigation decisions

The Hall decoder should detect events; navigation decides how to consume them.

Preserve: event floor / minimum event duration, median baseline, queue/task
telemetry, baseline and raw delta telemetry, no detector-level teleporting.

### 2.3 Lost behaviour must change under CTO

Solo LOST behaviour may be permissive. CTO LOST must not be.

New CTO rule:
- LOST in AUTO ends autonomous operation.
- Stop.
- Drop AUTO or hold it inert.
- Publish warning.
- Do not run stations.
- Do not resume without explicit operator declaration.

### 2.4 Stations must require trustworthy position

SOLONAV14 demonstrates promising station behaviour, but CTO must not allow
station logic to execute on unconfirmed or stale position.

Requirement:
- station decisions require nav state TRACKING with confidence above a defined
  threshold and odometer consistency.
- station action suppressed when LOST, REACQUIRING, or unbounded.

---

## 3. CTO2 components to harvest

| CTO2 component | Disposition | Reason |
|---|---|---|
| ESP-NOW transport | Port largely as-is after code audit | Transport itself was not the observed failure |
| Peer registry | Port after adding freshness/age semantics | Registry is useful, but stale peers must become obstructions |
| Same-direction nearest-ahead geometry | Preserve concept | Correct mental model for local traffic |
| Front/rear boundary calculations | Preserve concept, rewrite inputs | Boundary math is right; bare position inputs were unsafe |
| Traffic interruption state machine | Review and port selectively | Mission-interruption architecture is useful |
| Station-hold / rear-hold concepts | Review | May be useful for bubble/dispatcher behaviour |
| Pairing / role assignment | Review before port | Reasoning may be sound, but must not depend on unreliable position |
| Circuit Express reasoning | Document before coding | Expensive conclusion: pairing should be geometry-driven after CE |
| Speed coordination from Hall timing | Discard | Superseded by independent wheel/IR speed |
| Anything consuming position without bound | Rewrite | This is the root CTO2 failure mode |

---

## 4. New CTO packet model

Minimum self-report should include:

- `loco_id`
- sequence number
- timestamp / age
- direction
- nav state: TRACKING / LOST / DECLARED / REACQUIRING
- confidence
- last confirmed marker
- current estimated marker
- `front_bound_mm`
- `rear_bound_mm`
- `uncertainty_forward`
- `uncertainty_rear`
- `est_mm_s` from independent speed when available
- halted / moving / restarting / stopped-inferred
- source: dispatcher declared, odometer counted, map-refined, wheel-assisted
- `valid_for_traffic` boolean
- reason if not valid

**Rule:** Another train may act on a report only if it can prove the true
occupied train lies inside the reported envelope.

---

## 5. Audit questions for CTO2 code review

### 5.1 Peer transport
- How often are packets sent?
- What is the maximum observed interval?
- What fields are retained after peer silence?
- What age makes a peer stale?
- Does stale mean clear, ignored, or obstruction?

### 5.2 Registry and nearest-ahead
- Does nearest-ahead use same-direction topology?
- Does it handle wraparound?
- Does it use front/rear boundaries or Hall marker gaps?
- Can a behind peer be misclassified as ahead?
- How are stale peers represented?

### 5.3 Traffic control
- What event arms traffic approach?
- What event starts final ramp?
- What condition allows restart?
- Does restart require fresh later evidence, or can it self-release on stale data?
- Are all decisions traceable to packet sequence numbers?

### 5.4 Station and traffic interaction
- Can traffic preempt a station mission cleanly?
- Can station mission resume after traffic?
- Is the station state cleared too early?
- Are dwell/departure/restart phases explicit?
- Does traffic hold override station departure?

### 5.5 Operational truth
- Can dispatcher declaration be overwritten by a single bad read?
- Can a lost train still broadcast valid traffic truth?
- What truth source is published?
- Is certainty earned or merely asserted?

### 5.6 Safety under uncertainty
- What happens when peer radio disappears?
- What happens when self navigation is LOST?
- What happens when peer reports impossible speed or jump?
- What happens when bounds overlap?
- What happens if a peer powers off mid-lap?

---

## 6. Proposed CTO3 state machines

### 6.1 Self navigation state
- DECLARED
- TRACKING
- DEGRADED
- REACQUIRING
- LOST
- MANUAL_DECLARED

Traffic-valid only when DECLARED or TRACKING with bounded uncertainty, and
possibly DEGRADED if the published envelope has widened enough to remain
truthful.

### 6.2 Peer state
- FRESH_BOUNDED
- FRESH_UNBOUNDED
- STALE_KNOWN
- SILENT_KNOWN
- UNKNOWN

Only FRESH_BOUNDED can grant clearance. STALE_KNOWN and SILENT_KNOWN block
clearance.

### 6.3 Traffic state
- CLEAR
- APPROACHING_OBSTRUCTION
- FINAL_STOP
- WAIT_FOR_CLEARANCE
- RELEASED_RESTARTING
- MANUAL_REQUIRED

Every transition should log: self front/rear bounds, peer front/rear bounds,
gap, peer sequence, peer age, reason for transition.

---

## 7. CTO3 milestone interpretation

**M1 — trustworthy position.** Do not begin CTO behaviour until position cannot
be confidently wrong. M1 is not perfection; it is preventing false certainty.

**M2 — independent speed.** Required because Hall is currently both position
and speed source. A single sensor cannot check itself.

**M3 — closed-loop stops.** Station stops become distance/speed problems rather
than PWM rituals.

**M4 — CTO LOST behaviour.** Solo may keep crawling; CTO cannot. LOST in AUTO
must end autonomous operation.

**M5 — trustworthy self-report.** This is the real bridge from SOLONAV to CTO.
Not peer logic yet: first prove one locomotive can publish a bound that
contains the truth 100% of the time.

**M6 — peer awareness.** Only after M5. Audit first, port second.

**M7 — two-train operation.** Only after peer awareness proves bounded exchange
and stale-peer obstruction behaviour.

---

## 8. Immediate next work

1. **Assemble CTO2 source/log set:** latest CTO2 source, ideally
   `NGR_LL_DNA_CTO2_r12_CONTINUITY_FIRST.ino`; r9/r10/r11/r12 logs showing
   traffic stops, restarts, crashes, peer stale, virtual placement; any CE /
   bubble notes.
2. **Produce CTO2 audit table:** component, useful idea, failure assumption,
   port/rewrite/discard, dependency in SOLONAV/CTO3.
3. **Define M1 crossing test harness:** offline alignment, odometer comparison,
   phantom/missed marker detection, false-confidence detection.
4. **Define M5 packet format before porting ESP-NOW:** self bounds, freshness,
   truth source, uncertainty, valid-for-traffic state.
5. **Only then touch M6 code.**

---

## 9. Working rule

CTO3 should not be "CTO2 plus a better sensor." It should be "SOLONAV truth
plus harvested CTO2 coordination."

That preserves the hard-earned traffic ideas while removing the assumption that
ended CTO2.
