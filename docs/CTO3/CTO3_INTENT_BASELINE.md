# CTO3 Intent Baseline

Status: design-intent baseline; not an implementation specification  
Scope: NGR Lowline only  
Recorded: 2026-08-05

## Purpose

CTO3 exists to make NGR locomotives autonomous participants in railway
operations. It restores close-train automatic operation after the block-era CTO
and the unsuccessful 171-marker CTO2 experiments, using QUORUM's interpretation
of navigation evidence as the foundation for trustworthy position.

CTO3 is the **turntable, not the record**. The expanding-and-contracting bubble
is the first operating routine it must play. Bubble-specific choreography must
not be embedded in navigation, communications, motor control, or collision
protection in a way that prevents other routines from using those capabilities.

## Scope boundary

CTO3 is exclusively for the Lowline. Highline firmware, Highline operating
patterns, and trackside ESPs are outside its scope.

The current Lowline is a single loop with four identified station stops. Blocks
are gone and are not part of CTO3. Station stops are mapped locations, not
"QUORUM positions."

## Governing intent

1. Intelligence and operational control reside aboard the locomotives.
2. Locomotives communicate directly over ESP-NOW.
3. QUORUM provides navigation belief and evidence interpretation; CTO3 consumes
   that truth without reimplementing marker interpretation.
4. Collision avoidance protects physical consists, not point locations or Hall
   sensors.
5. Missing, stale, rejected, or inconsistent peer information is never evidence
   that traffic is clear.
6. Automatic routines are replaceable mission programs above common movement
   and safety services.
7. Higher-level requests may ask for movement but may not override navigation
   validity, physical-envelope protection, or e-stop.

## Bicameral locomotive authority

The locomotive has two distinct propulsion authorities:

- **Manual:** the operator directly controls propulsion.
- **AUTO:** onboard software controls propulsion while enrolled and running.

Navigation observes in both modes. E-stop crosses both authorities. Release to
Manual ends AUTO propulsion authority deterministically.

A manually driven, stopped, dispatcher-stopped, or e-stopped locomotive must
remain visible to other locomotives whenever it is powered. Manual authority
must not make a physical train disappear from traffic awareness. The locomotive
continues to broadcast its best available navigation truth, motion state, and
physical envelope, with uncertainty represented explicitly.

## Navigation and situational awareness

Each locomotive maintains one coherent operational picture including, at
minimum:

- identity;
- best position and its reference frame;
- direction of travel;
- measured or estimated speed;
- navigation state and confidence;
- expected observations and recent evidence;
- propulsion authority and motion state;
- consist geometry and configuration version;
- known peers, their freshness, and their occupied physical envelopes.

Marker observations are evidence evaluated against maintained belief. One odd,
ambiguous, missed, or rejected observation must not by itself teleport the
locomotive or erase credible operational identity. QUORUM is the present
interpretation architecture for this principle.

Hall observations provide accurate but spatially discrete evidence. The
developing IR wheel sensor is expected to become a second source of
navigational and speed truth. CTO3 must accept fused or independently qualified
navigation and speed information without redesigning its mission layer.

## Physical-envelope safety

The guiding collision constraint is the full consist extent measured from the
Hall sensor:

- 18 inches forward;
- 48 inches behind.

These are the current CTO3 design values and supersede earlier symmetric
two-foot/five-foot bubbles and marker-count approximations. The values must be
configuration, not assumptions scattered through routine logic.

Traffic reasoning concerns the gap between occupied envelopes along a valid
route. Point-to-point Hall distance is insufficient. Safety decisions must
account for direction, speed, stopping behavior, navigation confidence, peer
freshness, and the relevant route.

Peer loss, loss of navigation confidence, or inability to establish adequate
separation must cause deterministic conservative behavior. The exact state
machine and stopping policy remain for the implementation specification.

## Peer awareness and operating relationships

Every powered locomotive broadcasts self-truth independently of mode, motor
state, station state, or recent Hall activity. Each locomotive maintains a
registry capable of representing all relevant peers; a later report from one
locomotive must not overwrite another locomotive's record.

For the initial loop routine, locomotives must establish which train is in front
and which is behind and must coordinate their close-train activity. Whether the
implementation calls this pairing, enrollment, a session relationship, or a
derived topology is not yet settled. Historical CTO2 proposals that eliminated
pairing entirely are retained as alternatives, not adopted requirements.

The architecture must not make permanent leader/follower identity a prerequisite
for universal collision avoidance. Every credible physical consist remains
relevant to safety, including locomotives outside a selected routine.

## Initial routine: expanding and contracting bubble

The first CTO3 routine operates two same-direction trains around the present
Lowline while preserving their established front/rear order.

At each of the four identified stations, the front train proceeds to the next
station while the rear train is constrained to a safe earlier position. The
front train later departs, and the rear train advances into the station. The
separation therefore expands and contracts as the routine repeats.

The exact station choreography, departure handshake, stop profiles, and
recovery transitions belong in the routine specification. They must be stated
in terms of mapped stops, peer state, physical envelopes, and movement
primitives—not resurrected blocks.

## Reusable movement and mission capabilities

CTO3 should expose reusable primitives such as:

- proceed along an authorized route;
- approach a mapped target;
- stop at a platform, hold point, or arbitrary mapped location;
- wait for time, state, or another locomotive's condition;
- resume when a condition becomes true;
- pass or skip a stop;
- reverse or switch when future topology permits it.

The bubble is one mission program built from these capabilities. Other records
must be addable without dismantling the architecture, including Circuit Express,
additional stops between established stations, scheduled service, conditional
meets, and switching routines.

A future mission may express: proceed to Bamboo and stop at its platform; wait
until Toby reaches Bamboo HOLD; then proceed, skip Arches, and stop at Grillers.
This must be representable as structured destinations, stopping positions,
dependencies, conditions, and skips—not special-case motor commands.

## Command sources and dispatcher evolution

The initial supervisory command surface is deliberately narrow:

- Start;
- Stop;
- eStop;
- Release to Manual.

These commands do not make the dispatcher the live traffic controller.
Movement permission and separation decisions remain onboard.

The architecture must later admit richer dispatcher orders and voice control.
Voice is another command-entry method: speech is translated into the same
validated structured missions used by other command sources. Neither voice nor
a future dispatcher commands motor PWM directly or bypasses onboard safety.

An external Raspberry Pi may collect telemetry, analyze logs, learn calibration
or speed models, and formulate missions. It is not required in the live
locomotive-to-locomotive safety loop.

## Speed-control direction

PWM is an actuator, not the operational quantity being controlled. Actual train
speed is the desired controlled variable. Hall timing currently provides sparse
speed evidence; IR may provide continuous evidence. The architecture must allow
the speed controller and learned locomotive models to improve without changing
mission semantics or physical-envelope protection.

## Expansion beyond the present loop

CTO3 must not permanently encode an unbranched circular railway. Future Lowline
expansion may add:

- turnouts and alternative routes;
- passing sidings and meets;
- switching sidings and reverse moves;
- route-specific platform, hold, fouling, and clearance points;
- turnout-state and route confirmation;
- reservations for shared or conflicting track;
- different consist lengths and clearance requirements.

Circular ahead/behind arithmetic is acceptable inside the initial loop-specific
implementation only if the permanent interfaces can later use a topology model
of segments, junctions, routes, and conflict areas. A future dispatcher may
request a route, but onboard locomotives retain authority to decide whether
movement is safe.

## Failure lessons that must survive CTO2

The revised CTO2 failure account records that both locomotives remained
`CTO_ROLE_SOLO`, peer geometry was unavailable, traffic remained
`TRAFFIC_CLEAR`, the stopped-lead approach state never activated, and the
following locomotive continued at cruise speed into a near-miss.

The proposed causes—pairing predicates, Manual/AUTO mixing, freshness and
sequence filtering, or session-direction setup—were debugging hypotheses, not
established root causes. Their suggested fixes belong to a superseded paradigm.
The durable requirement is that a failure to recognize or qualify traffic must
not be converted into clearance.

## Historical ideas retained but not adopted

The resource set deliberately preserves conflicting and superseded proposals:

- block occupancy and block-relative leader/follower rules;
- centralized West/East GO/HOLD dispatch;
- permanent role assignment;
- mandatory AUTO participation for traffic visibility;
- pairing as the only source of peer relevance;
- the proposal that pairing is unnecessary;
- symmetric protection bubbles and marker-count safety distances;
- global marker-pattern matching predating QUORUM;
- timer-dominated station choreography.

They remain useful design evidence. None becomes a CTO3 requirement merely by
appearing in an archived source.

## Architectural layers

CTO3 should preserve the following responsibility boundaries:

1. **Evidence acquisition:** Hall, IR, and future sensors.
2. **Navigation truth:** QUORUM and future sensor fusion.
3. **Self-truth and peer registry:** ESP-NOW publication, freshness, identity,
   and physical occupancy.
4. **Safety authority:** envelope separation, route conflicts, confidence, and
   stopping constraints.
5. **Movement services:** speed and stopping primitives.
6. **Mission engine:** replaceable routines, destinations, waits, dependencies,
   skips, and route requests.
7. **Command adapters:** current dispatcher controls, future mission orders,
   and voice input.

Dependencies flow downward. Higher layers request outcomes; lower layers retain
authority over truth and safety.

## Matters intentionally unresolved

This baseline does not yet decide:

- the pairing/session protocol for the initial two-locomotive routine;
- exact ESP-NOW packet fields, versioning, or freshness periods;
- initial ordering and reordering rules;
- numerical braking margins beyond the physical consist dimensions;
- behavior for each class of navigation or peer degradation;
- precise station platform and HOLD coordinates;
- whether route reservations begin onboard, cooperatively, or with future
  dispatcher assistance;
- the detailed IR/QUORUM fusion contract;
- the mission-description format.

Those decisions require explicit specifications and field-test evidence. They
must not be inferred from obsolete CTO2 constants.

## Source basis

The evidence supporting this baseline is indexed in
[`resources/README.md`](resources/README.md). The most authoritative sources are
the operator's current clarifications recorded on 2026-08-05, followed by the
July 2026 agreed operational principles and the preserved CTO/CTO2 records.
When sources disagree, the newer explicit operator clarification governs, and
the disagreement remains documented.
