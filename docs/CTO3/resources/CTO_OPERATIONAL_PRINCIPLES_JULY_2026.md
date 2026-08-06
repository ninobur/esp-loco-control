# NGR CTO Operational Principles — July 2026

> Extracted from the original Word document for repository accessibility.

NGR CTO Operational Principles

Agreed baseline for navigation truth, traffic interaction, station behavior, and the next field-test revision

1. Operating Stance

The system is to be built for operational success, not for self-defeating refusal to act whenever evidence is imperfect.

Normal dispatcher setup is valid operational truth. The locomotive does not have to earn permission to exist through a new Hall hit, DNA exact lock, or confidence threshold.

Magnet and DNA evidence refine, confirm, or reconcile existing truth. They do not erase the locomotive’s operational identity because one observation is odd, ambiguous, or rejected.

Diagnostics may report uncertainty, alternate candidates, or map disagreement. They must not silently turn a known locomotive into MM000, UNKNOWN, SOLO, or radio-silent.

No new arbitrary buffer, brake rule, e-stop, PWM cap, or performance limiter is to be added without separate discussion and approval.

2. Persistent Operational Navigation Truth

Every locomotive maintains one persistent operational navigation state. All control and communications services read the same state; they do not each invent their own eligibility gate.

Dispatcher and magnet truth are complementary:

Dispatcher truth establishes where the locomotive is at setup and which way it is intended to travel.

Magnet truth updates the locomotive as it progresses through the fixed route.

A sequence such as 082 -> 083 -> 084 followed by an unexpected polarity at the next marker does not permit a teleportation conclusion. The best position remains the next plausible forward marker, with narrowly bounded alternatives if a marker may have been missed.

The locomotive always continues to publish its best current truth while carrying uncertainty as diagnostic information.

3. Declared Interval Plus Direction

The dispatcher does not report an interval by itself. Direction turns the interval into a valid marker reference, equivalent to the last marker passed in that direction.

The reported position is a marker reference with a stated truth source. It is not falsely claimed to be an exact Hall-on-magnet coordinate while physically between magnets.

AUTO may currently assume an initial operating direction, but traffic truth must always publish the actual declared or confirmed map direction. AUTO startup policy must not gate traffic broadcasting.

4. Always-On Universal Traffic Truth

Every powered locomotive broadcasts facts about itself so every other locomotive can receive and retain those facts. Broadcasting is not a pairing service and is not controlled by motor operation.

The broadcaster remains active while AUTO, manual, moving, stopped, at a station, dispatcher-stopped, or e-stopped.

A stopped locomotive must remain visible. A stopped Otto cannot become silent merely because no new Hall event has occurred.

Broadcast content is self-truth only: locomotive ID, sequence, truth validity/source, marker reference, actual map direction, running/stopped state, measured speed when available, front/rear boundaries, and consist configuration/version.

A station, traffic, or peer relationship may never suppress the local truth broadcaster.

5. All-Locomotive Peer Registry

Each locomotive maintains a registry for every other fresh broadcaster it hears. A third locomotive must not overwrite the stored truth for Otto or Toby.

The registry is universal awareness. It answers who exists, where they are, which way they are moving, and what physical space they occupy.

There is no current traffic prerequisite called paired, follower, trailing, or leader.

A peer may go stale in the registry, but stale status is a reportable fact; it must not erase the local locomotive’s own operational truth.

6. Traffic Proximity: Station Mission Remains Primary

Each locomotive is trying to travel from station to station. Its normal objective is the station M+2 stop. Traffic is a temporary interruption of that mission, not a new role or identity.

For proximity, each locomotive considers every fresh same-direction physical consist in the peer registry.

The relevant track condition is the nearest occupied consist ahead, derived from self front boundary to peer rear boundary in the current direction.

This is not a follower role. The locomotive is still executing its own route and station mission.

The nearest train ahead may change as topology changes. The controller responds to the current nearest relevant occupancy, not to a permanently assigned partner.

7. Marker-Count Traffic Geometry and Restart

The current traffic thresholds are marker-count rules, not claims that all marker intervals have identical physical length. Physical segment distance is used for speed estimation; the following coordinates are defined in marker counts.

Stopped lead: the approaching locomotive begins its established 20 -> 15 -> 10 pKPH arrival sequence at the 18-marker Hall-to-Hall point, then uses the existing paced 300 ms/PWM final ramp at the 10-marker touch coordinate.

When the obstruction has moved away and the separation reaches 12 markers Hall-to-Hall, the waiting rear locomotive restarts toward its unchanged station M+2 target.

The next revision must give traffic approach the same proven station-arrival correction authority; ordinary cruise-maintenance corrections are too weak to ensure the requested 20 -> 15 -> 10 sequence is physically reached before the final ramp.

Full continuous speed-matching behind a moving train is a later extension. The 12-marker minimum is already the agreed operating rule and the restart threshold for the initial traffic interruption behavior.

8. Station Service and Traffic Authority

Goldcore station behavior is part of the normal mission and must be enabled in the next operating sketch. r8 suppressed station arming with PHASE1_TRAFFIC_TEST_ONLY, which is why the field test did not stop at stations.

Traffic may temporarily override the immediate stop target with an earlier traffic stop, but it does not complete or cancel the station visit.

After the 12-marker restart condition, traffic releases and the same station controller resumes the original M+2 objective.

The motor interface receives the current mission target from station service or temporary traffic control. It does not own peer discovery, navigation truth, or station synchronization.

9. MHE: Station Synchronization Only

MHE (MUST_HOLD_ELIGIBLE) is a dispatcher-set attribute used only for station departure/dwell synchronization. It is not used for ordinary proximity protection.

A locomotive that is not MHE never holds for station synchronization.

All locomotives, MHE or not, remain fully visible and fully relevant to traffic proximity and collision avoidance.

For MHE status, non-MHE locomotives are excluded from both sides of the comparison.

With two MHE+ locomotives, this self-sorts: only the locomotive with the other MHE+ locomotive closer behind than ahead holds.

With three MHE+ locomotives, each makes the same comparison using only the MHE+ topology.

With two MHE+ locomotives and an independent non-MHE third locomotive, the MHE calculation ignores the third locomotive entirely, while traffic proximity still respects all three.

The former leader/follower/pair vocabulary is not the basis of current traffic logic. MHE is a local station-hold status derived from current MHE topology.

10. Future Architecture Constraint: MHE Priority Follow

Not part of the immediate field-test revision, but the architecture must accommodate it: a dispatcher may later direct an MHE- locomotive to recognize MHE+ locomotives and preserve a larger separation behind them.

This is a dispatcher policy distance, not a new generic safety fallback.

It does not change universal proximity: all trains avoid all same-direction trains.

It does not make an MHE- locomotive participate in station hold unless its own MHE setting is enabled.

11. TeleMonitor and Diagnostics

TeleMonitor must not receive per-packet or per-marker traffic floods in normal operation.

Normal reports should be state changes and compact low-rate health: local truth source changes, peer added/stale, nearest occupancy ahead changes, traffic arm/release, target changes, final-ramp start, traffic stop, restart, station hold changes, and periodic radio health.

Detailed packet traces, candidate history, and accepted Hall-event sequence data belong behind a debug mode, not in normal operating telemetry.

Diagnostics explain disagreements; they do not authorize the system to discard declared setup truth or withdraw a locomotive from operations.

12. Required Repair Scope Before the Next Field Test

Replace split declared/odometry/DNA truth paths with the single persistent operational navigation state described above.

Make dispatcher interval plus direction create immediate marker-reference truth and boundary geometry; remove the traffic_seed_mm workaround.

Run the self-truth broadcaster independently of AUTO/manual, motor run state, dispatcher stop, e-stop, station state, peer receipt, and recent Hall activity.

Replace the single-peer CTO record with an all-locomotive peer registry.

Remove pairing, trailing, and follower as requirements for traffic. Select current nearest same-direction occupancy ahead from registry geometry.

Use operational navigation truth consistently for broadcast, peer geometry, station logic, and traffic logic. Do not require odomValid for the locomotive to be operationally known.

Re-enable Goldcore station service and preserve station M+2 as the standing mission when traffic temporarily stops a train early.

Implement traffic restart at 12 markers Hall-to-Hall and return control to the unchanged station objective.

Give stopped-lead traffic approach the established station-arrival correction authority for 20 -> 15 -> 10 pKPH, followed by the existing 300 ms/PWM final ramp at the touch coordinate.

Implement MHE as station-only topology, excluding non-MHE locomotives from MHE comparison while retaining them for traffic proximity.

Keep normal TeleMonitor output concise and provide detailed traffic packet output only under explicit debug control.

13. Immediate Test Claim and Boundary

The next sketch is not to be described as production-ready before it compiles and the field test demonstrates the behavior. The immediate field claim is limited to:

This is a statement of intended operating behavior, not a new NASA-style preflight procedure. Normal setup remains: place the locomotive, declare its direction and interval, and operate. The visible reports exist to confirm the system is behaving as designed, not to create new reasons to refuse operation.

14. Terminology to Retire or Restrict

Status: This document is the agreed design baseline. r8 does not yet meet it and should not be treated as the next operating test sketch.
